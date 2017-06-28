/*
 * =============================================================================
 *   ROC Runtime Conformance Release License
 * =============================================================================
 * The University of Illinois/NCSA
 * Open Source License (NCSA)
 *
 * Copyright (c) 2017, Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Developed by:
 *
 *                 AMD Research and AMD ROC Software Development
 *
 *                 Advanced Micro Devices, Inc.
 *
 *                 www.amd.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimers in
 *    the documentation and/or other materials provided with the distribution.
 *  - Neither the names of <Name of Development Group, Name of Institution>,
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this Software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#include <vector>
#include <algorithm>

#include "common/base_rocr.h"
#include "suites/test_common/test_base.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"
#include "suites/performance/memory_async_copy.h"
#include "common/base_rocr_utils.h"
#include "gtest/gtest.h"

#define RET_IF_HSA_ERR(err) { \
  if ((err) != HSA_STATUS_SUCCESS) { \
    const char* msg = 0; \
    hsa_status_string(err, &msg); \
    std::cout << "hsa api call failure at line " << __LINE__ << ", file: " << \
                          __FILE__ << ". Call returned " << err << std::endl; \
    std::cout << msg << std::endl; \
    return (err); \
  } \
}

static const int kNumGranularity = 20;
const char* Str[kNumGranularity] = {"1k", "2K", "4K", "8K", "16K", "32K",
    "64K", "128K", "256K", "512K", "1M", "2M", "4M", "8M", "16M", "32M",
                                               "64M", "128M", "256M", "512M"};

const size_t Size[kNumGranularity] = {
    1024, 2*1024, 4*1024, 8*1024, 16*1024, 32*1024, 64*1024, 128*1024,
    256*1024, 512*1024, 1024*1024, 2048*1024, 4096*1024, 8*1024*1024,
    16*1024*1024, 32*1024*1024, 64*1024*1024, 128*1024*1024, 256*1024*1024,
    512*1024*1024};

static const int kMaxCopySize = Size[kNumGranularity - 1];

MemoryAsyncCopy::MemoryAsyncCopy(void) :
    TestBase() {
  static_assert(sizeof(Size)/sizeof(size_t) == kNumGranularity,
      "kNumGranularity does not match size of arrays");

  agent_index_ = 0;
  pool_index_ = 0;
  tran_.clear();
  agent_info()->clear();
  pool_info()->clear();
  node_info()->clear();
  verified_ = true;
  src_pool_id_ = -1;
  dst_pool_id_ = -1;
  do_full_test_ = false;
  set_num_iteration(10);  // Default value
  set_title("Asynchronous Memory Copy Bandwidth");
  set_description("This test measures bandwidth to/from Host from/to GPU "
      "and Peer to Peer using hsa_amd_memory_async_copy() to copy buffers "
      "of various length from memory pool to another.");
}

MemoryAsyncCopy::~MemoryAsyncCopy(void) {
  for (PoolInfo *p : pool_info_) {
    delete p;
  }

  for (AgentInfo *a : agent_info_) {
    delete a;
  }
}

void MemoryAsyncCopy::SetUp(void) {
  TestBase::SetUp();

  FindTopology();

  if (verbosity() >= VERBOSE_STANDARD) {
    PrintTopology();
  }
  ConstructTransactionList();
  return;
}

void MemoryAsyncCopy::Run(void) {
  TestBase::Run();

  for (Transaction t : tran_) {
    RunBenchmarkWithVerification(&t);
  }
}

void MemoryAsyncCopy::FindSystemPool(void) {
  hsa_status_t err;

  err = hsa_iterate_agents(rocrtst::FindCPUDevice, &cpu_agent_);
  ASSERT_EQ(HSA_STATUS_INFO_BREAK, err);

  err = hsa_amd_agent_iterate_memory_pools(cpu_agent_, rocrtst::FindGlobalPool,
        &sys_pool_);
  ASSERT_EQ(HSA_STATUS_INFO_BREAK, err);
}

static hsa_status_t AcquireAccess(hsa_agent_t agent,
                                    hsa_amd_memory_pool_t pool, void* ptr) {
  hsa_status_t err;

  hsa_amd_memory_pool_access_t access;
  err = hsa_amd_agent_memory_pool_get_info(agent, pool,
                              HSA_AMD_AGENT_MEMORY_POOL_INFO_ACCESS, &access);

  RET_IF_HSA_ERR(err);

  if (access == HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED) {
    return HSA_STATUS_ERROR;
  }

  if (access == HSA_AMD_MEMORY_POOL_ACCESS_DISALLOWED_BY_DEFAULT) {
    err = hsa_amd_agents_allow_access(1, &agent, NULL, ptr);
    RET_IF_HSA_ERR(err);
  }

  return err;
}

static hsa_agent_t *
AcquireAsyncCopyAccess(
         void *dst_ptr, hsa_amd_memory_pool_t dst_pool, hsa_agent_t *dst_ag,
         void *src_ptr, hsa_amd_memory_pool_t src_pool, hsa_agent_t *src_ag) {
  if (AcquireAccess(*src_ag, dst_pool, dst_ptr) != HSA_STATUS_SUCCESS) {
    if (AcquireAccess(*dst_ag, src_pool, src_ptr) == HSA_STATUS_SUCCESS) {
      return dst_ag;
    } else {
      return nullptr;
    }
  } else {
    return src_ag;
  }
}

void MemoryAsyncCopy::RunBenchmarkWithVerification(Transaction *t) {
  hsa_status_t err;
  void* ptr_src;
  void* ptr_dst;

  size_t size = t->max_size * 1024;

  hsa_amd_memory_pool_t src_pool =  pool_info_[t->src]->pool_;
  hsa_agent_t dst_agent = pool_info_[t->dst]->owner_agent_info()->agent();
  hsa_amd_memory_pool_t dst_pool = pool_info_[t->dst]->pool_;

  hsa_agent_t src_agent = pool_info_[t->src]->owner_agent_info()->agent();

  if (verbosity() >= VERBOSE_STANDARD) {
    printf("Executing Copy Path: From Pool %d To Pool %d ", t->src, t->dst);
    switch (t->type) {
      case H2D:
        printf("(Host-To-Device)\n");
        break;

      case D2H:
        printf("(Device-To-Host)\n");
        break;

      case P2P:
        printf("(Peer-To-Peer)\n");
        break;

      default:
        printf("**Unexpected path**\n");
        return;
    }
  }

  err = hsa_amd_memory_pool_allocate(src_pool, size, 0, &ptr_src);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  err = hsa_amd_memory_pool_allocate(dst_pool, size, 0,
                                     &ptr_dst);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  // rocrtst::CommonCleanUp data
  void* host_ptr_src = NULL;
  void* host_ptr_dst = NULL;
  err = hsa_amd_memory_pool_allocate(sys_pool_, size, 0,
                                     reinterpret_cast<void**>(&host_ptr_src));
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);
  err = hsa_amd_memory_pool_allocate(sys_pool_, size, 0,
                                     reinterpret_cast<void**>(&host_ptr_dst));
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  err = hsa_amd_memory_fill(host_ptr_src, 1, size/sizeof(uint32_t));
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  err = hsa_amd_memory_fill(host_ptr_dst, 0, size/sizeof(uint32_t));
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  hsa_signal_t s;
  err = hsa_signal_create(1, 0, NULL, &s);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  // **** First copy from the system buffer source to the test source pool
  // Acquire the appropriate access; prefer GPU agent over CPU where there
  // is a choice.
  hsa_agent_t *cpy_ag = nullptr;
  cpy_ag = AcquireAsyncCopyAccess(ptr_src, src_pool, &src_agent, host_ptr_src,
                                                     sys_pool_, &cpu_agent_);
  if (cpy_ag == nullptr) {
    std::cout << "Agents " << t->src << " and " << t->dst <<
                              "cannot access each other's pool." << std::endl;
  }
  ASSERT_NE(cpy_ag, nullptr);

  err = hsa_amd_memory_async_copy(ptr_src, *cpy_ag, host_ptr_src, *cpy_ag,
                                                            size, 0, NULL, s);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  while (hsa_signal_wait_scacquire(s, HSA_SIGNAL_CONDITION_LT, 1, uint64_t(-1),
                                   HSA_WAIT_STATE_ACTIVE))
  {}

  int iterations = RealIterationNum();

  // **** Next, copy from the test source pool to the test destination pool
  // Prefer a gpu agent to a cpu agent

  cpy_ag = AcquireAsyncCopyAccess(ptr_dst, dst_pool, &dst_agent, ptr_src,
                                                        src_pool, &src_agent);
  if (cpy_ag == nullptr) {
    std::cout << "Owner agents for pools" << t->src << " and " <<
                   t->dst << " cannot access each other's pool." << std::endl;
  }
  ASSERT_NE(cpy_ag, nullptr);

  for (int i = 0; i < kNumGranularity; i++) {
    if (Size[i] > size) {
      break;
    }

    std::vector<double> time;

    for (int it = 0; it < iterations; it++) {
      if (verbosity() >= VERBOSE_PROGRESS) {
        std::cout << ".";
        std::cout.flush();
      }

      hsa_signal_store_relaxed(t->signal, 1);

      rocrtst::PerfTimer copy_timer;
      int index = copy_timer.CreateTimer();

      copy_timer.StartTimer(index);
      err = hsa_amd_memory_async_copy(ptr_dst, *cpy_ag, ptr_src, *cpy_ag,
                                                 Size[i], 0, NULL, t->signal);
      ASSERT_EQ(HSA_STATUS_SUCCESS, err);

      while (hsa_signal_wait_scacquire(t->signal, HSA_SIGNAL_CONDITION_LT, 1,
                                         uint64_t(-1), HSA_WAIT_STATE_ACTIVE))
      {}

      copy_timer.StopTimer(index);

      hsa_signal_store_relaxed(s, 1);

      err = AcquireAccess(dst_agent, sys_pool_,
                    host_ptr_dst);
      ASSERT_EQ(HSA_STATUS_SUCCESS, err);


      err = hsa_amd_memory_async_copy(host_ptr_dst, cpu_agent_, ptr_dst,
                                                 dst_agent, size, 0, NULL, s);
      ASSERT_EQ(HSA_STATUS_SUCCESS, err);

      while (hsa_signal_wait_scacquire(s, HSA_SIGNAL_CONDITION_LT, 1,
                                       uint64_t(-1), HSA_WAIT_STATE_ACTIVE))
      {}

      err = AcquireAccess(cpu_agent_, sys_pool_, host_ptr_dst);
      ASSERT_EQ(HSA_STATUS_SUCCESS, err);

      if (memcmp(host_ptr_src, host_ptr_dst, Size[i])) {
        verified_ = false;
      }
      // Push the result back to vector time

      time.push_back(copy_timer.ReadTimer(index));
    }

    if (verbosity() >= VERBOSE_PROGRESS) {
      std::cout << std::endl;
    }

    // Get Min copy time
    t->min_time->push_back(*std::min_element(time.begin(), time.end()));
    // Get mean copy time and store to the array
    t->benchmark_copy_time->push_back(GetMeanTime(&time));
  }

  err = hsa_signal_destroy(s);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);
}

size_t MemoryAsyncCopy::RealIterationNum(void) {
  return num_iteration() * 1.2 + 1;
}

double MemoryAsyncCopy::GetMeanTime(std::vector<double> *vec) {
  std::sort(vec->begin(), vec->end());

  vec->erase(vec->begin());
  vec->erase(vec->begin(), vec->begin() + num_iteration() * 0.1);
  vec->erase(vec->begin() + num_iteration(), vec->end());

  double mean = 0.0;
  int num = vec->size();

  for (int it = 0; it < num; it++) {
    mean += (*vec)[it];
  }

  mean /= num;
  return mean;
}

void MemoryAsyncCopy::DisplayResults(void) const {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  TestBase::DisplayResults();

  for (Transaction t : tran_) {
    DisplayBenchmark(&t);
    delete t.benchmark_copy_time;
    delete t.min_time;
  }

  return;
}

void MemoryAsyncCopy::DisplayBenchmark(Transaction *t) const {
  size_t size = t->max_size * 1024;
  printf("=========================== PATH: From Pool %d To Pool %d (",
                                                              t->src, t->dst);

  switch (t->type) {
    case H2D:
      printf("Host-To-Device) ===========================\n");
      break;

    case D2H:
      printf("Device-To-Host) ===========================\n");
      break;

    case P2P:
      printf("Peer-To-Peer) =============================\n");
      break;

    default:
      ASSERT_EQ(t->type == H2D || t->type == D2H || t->type == P2P, true);
  }
  if (verified_) {
    std::cout << "Verification: Pass" << std::endl;
  } else {
    std::cout << "Verification: Fail" << std::endl;
  }

  if (verbosity() < VERBOSE_STANDARD) {
    return;
  }

  printf("Data Size             Avg Time(us)         Avg BW(GB/s)"
                           "          Min Time(us)          Peak BW(GB/s)\n");

  for (int i = 0; i < 20; i++) {
    if (Size[i] > size) {
      break;
    }

    double band_width =
    static_cast<double>(Size[i]/(*(t->benchmark_copy_time))[i]/1024/1024/1024);
    double peak_band_width =
       static_cast<double>(Size[i] / (*(t->min_time))[i]/ 1024 / 1024 / 1024);
    printf(
        "  %4s            %14lf        %14lf         %14lf         %14lf\n",
       Str[i], (*(t->benchmark_copy_time))[i] * 1e6, band_width,
                                  (*(t->min_time))[i] * 1e6, peak_band_width);
  }

  return;
}

void MemoryAsyncCopy::Close() {
  TestBase::Close();
}

static hsa_status_t GetPoolInfo(hsa_amd_memory_pool_t pool, void* data) {
  hsa_status_t err;
  MemoryAsyncCopy* ptr = reinterpret_cast<MemoryAsyncCopy*>(data);
  // Query pool segment, only report global one
  hsa_amd_segment_t region_segment;
  err = hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_SEGMENT,
                                     &region_segment);
  RET_IF_HSA_ERR(err);

  if (region_segment != HSA_AMD_SEGMENT_GLOBAL) {
    return HSA_STATUS_SUCCESS;
  }

  // Check if the pool is alloc allowed, if not, discard this pool
  bool alloc_allowed = false;
  err = hsa_amd_memory_pool_get_info(pool,
              HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALLOWED, &alloc_allowed);
  RET_IF_HSA_ERR(err);

  if (alloc_allowed != true) {
    return HSA_STATUS_SUCCESS;
  }

  // Query the max allocable size
  size_t alloc_max_size = 0;
  err = hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_SIZE,
                                     &alloc_max_size);
  RET_IF_HSA_ERR(err);

  // Check if the pool is fine-grained or coarse-grained
  uint32_t global_flag = 0;
  err = hsa_amd_memory_pool_get_info(pool,
                        HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &global_flag);
  RET_IF_HSA_ERR(err);

  bool is_fine_grained = HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED
                         & global_flag;

  int pool_i = ptr->pool_index();
  int ag_ind = ptr->agent_index();
  ptr->pool_info()->push_back(
    new PoolInfo(pool, pool_i, region_segment, is_fine_grained,
                                  alloc_max_size, ptr->agent_info()->back()));

  // Construct node_info and push back to agent_info_
  (*ptr->node_info())[ag_ind].pool.push_back(*ptr->pool_info()->back());
  ptr->set_pool_index(pool_i + 1);

  return HSA_STATUS_SUCCESS;
}

static hsa_status_t GetAgentInfo(hsa_agent_t agent, void* data) {
  MemoryAsyncCopy* ptr = reinterpret_cast<MemoryAsyncCopy*>(data);

  hsa_status_t err;
  char name[64];
  err = hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, name);
  RET_IF_HSA_ERR(err);

  // Get device type
  hsa_device_type_t device_type;
  err = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
  RET_IF_HSA_ERR(err);

  ptr->agent_info()->push_back(
                       new AgentInfo(agent, ptr->agent_index(), device_type));

  // Contruct a new NodeInfo structure and push back to agent_info_
  NodeInfo node;
  node.agent = *ptr->agent_info()->back();
  ptr->node_info()->push_back(node);

  err = hsa_amd_agent_iterate_memory_pools(agent, GetPoolInfo, ptr);
  ptr->set_agent_index(ptr->agent_index() + 1);
  return HSA_STATUS_SUCCESS;
}

void MemoryAsyncCopy::FindTopology() {
  hsa_status_t err;

  err = hsa_iterate_agents(GetAgentInfo, this);
  FindSystemPool();

  ASSERT_EQ(HSA_STATUS_SUCCESS, err);
}

void MemoryAsyncCopy::DisplayTestInfo(void) {
  TestBase::DisplayTestInfo();
}

void MemoryAsyncCopy::ConstructTransactionList(void) {
  hsa_status_t err;

  tran_.clear();

  int cpu_pool_indx = -1;
  int gpu1_pool_indx = -1;
  int gpu2_pool_indx = -1;

  auto push_trans = [&](int from_indx, int to_indx, TransType type) {
    Transaction t;
    t.src = from_indx;
    t.dst = to_indx;
    t.max_size = kMaxCopySize/1024;
    t.type = type;
    t.benchmark_copy_time = new  std::vector<double>;
    t.min_time = new std::vector<double>;
    err = hsa_signal_create(1, 0, NULL, &t.signal);
    ASSERT_EQ(HSA_STATUS_SUCCESS, err);

    tran_.push_back(t);
  };

  // Find the CPU Node and pool
  for (NodeInfo n : *node_info()) {
    if (cpu_pool_indx == -1 && n.agent.device_type() == HSA_DEVICE_TYPE_CPU) {
      cpu_pool_indx = n.pool[0].index_;
      continue;
    }
    if (gpu1_pool_indx == -1 && n.agent.device_type() == HSA_DEVICE_TYPE_GPU) {
      gpu1_pool_indx = n.pool[0].index_;
      continue;
    }
    if (gpu2_pool_indx == -1 &&  n.agent.device_type() == HSA_DEVICE_TYPE_GPU) {
      gpu2_pool_indx = n.pool[0].index_;
      break;
    }
  }

  ASSERT_NE(cpu_pool_indx, -1);
  ASSERT_NE(gpu1_pool_indx, -1);

  push_trans(cpu_pool_indx, gpu1_pool_indx, H2D);
  push_trans(gpu1_pool_indx, cpu_pool_indx, D2H);

  if (do_full_test_) {
    for (NodeInfo n : *node_info()) {
      if (n.agent.device_type() == HSA_DEVICE_TYPE_CPU) {
        continue;
      }

      for (PoolInfo p : n.pool) {
        if (p.index_ == gpu1_pool_indx) {
          continue;
        }
        push_trans(gpu1_pool_indx, p.index_, P2P);
        push_trans(p.index_, gpu1_pool_indx, P2P);
      }
    }
  } else {
    if (gpu2_pool_indx != -1) {
      push_trans(gpu1_pool_indx, gpu2_pool_indx, P2P);
      push_trans(gpu2_pool_indx, gpu1_pool_indx, P2P);
    }
  }
}

void MemoryAsyncCopy::PrintTopology(void) {
  size_t node_num = node_info()->size();

  for (uint32_t i = 0; i < node_num; i++) {
    NodeInfo node = node_info()->at(i);
    // Print agent info
    std::cout << std::endl;
    std::cout << "Agent #" << node.agent.index_ << ":" << std::endl;

    if (HSA_DEVICE_TYPE_CPU == node.agent.device_type())
      std::cout << "Agent Device Type:                             CPU"
                << std::endl;
    else if (HSA_DEVICE_TYPE_GPU == node.agent.device_type())
      std::cout << "Agent Device Type:                             GPU"
                << std::endl;

    // Print pool info
    size_t pool_num = node.pool.size();

    for (uint32_t j = 0; j < pool_num; j++) {
      std::cout << "    Memory Pool#" << node.pool.at(j).index_ << ":"
                << std::endl;
      std::cout << "        max allocable size in KB: \t\t"
                << node.pool.at(j).allocable_size_ / 1024 << std::endl;
      std::cout << "        is fine-grained: \t\t\t"
                << node.pool.at(j).is_fine_grained_ << std::endl;
    }
  }
}

#undef RET_IF_HSA_ERR
