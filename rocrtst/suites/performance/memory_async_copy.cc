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

#include "memory_async_copy.h"
#include "common/base_rocr_utils.h"
#include "gtest/gtest.h"

const char* Str[20] = {"1k", "2K", "4K", "8K", "16K", "32K", "64K", "128K",
                       "256K", "512K", "1M", "2M", "4M", "8M", "16M", "32M",
                                               "64M", "128M", "256M", "512M"
                      };
const size_t Size[20] = {1024, 2 * 1024, 4 * 1024, 8 * 1024, 16 * 1024, 32
                         * 1024, 64 * 1024, 128 * 1024, 256 * 1024, 512 * 1024,
                         1024 * 1024, 2048 * 1024, 4096 * 1024, 8 * 1024 * 1024,
                         16 * 1024 * 1024, 32 * 1024 * 1024, 64 * 1024 * 1024,
                         128 * 1024 * 1024, 256 * 1024 * 1024, 512 * 1024 * 1024
                        };

MemoryAsyncCopy::MemoryAsyncCopy() :
  BaseRocR() {
//  argc_ = argc;
//  argv_ = argv;
  bench_mark_mode_ = false;
  verification_ = false;
  agent_index_ = 0;
  region_index_ = 0;
  tran_.clear();
  agent_info_.clear();
  region_info_.clear();
  node_info_.clear();
  verified_ = true;
}

MemoryAsyncCopy::~MemoryAsyncCopy() {
  size_t size = tran_.size();

  if (size != 0) {
    for (size_t i = 0; i < size; i++) {
      if (tran_.at(i).dep_signal != nullptr)
        ;

      delete[] tran_.at(i).dep_signal;
    }
  }
}

void MemoryAsyncCopy::SetUp() {

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }
  FindTopology();

  ParseArgument();
  return;
}

void MemoryAsyncCopy::Run() {
  if (bench_mark_mode_)
    if (verification_) {
      RunBenchmarkWithVerification();
    }
    else {
      RunBenchmark();
    }
  else {
    RunNormal();
  }
}

void MemoryAsyncCopy::FindSystemRegion() {
  hsa_status_t err;

  err = hsa_iterate_agents(rocrtst::FindCPUDevice, &cpu_agent_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_agent_iterate_memory_pools(cpu_agent_, rocrtst::FindGlobalPool,
        &sys_region_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void MemoryAsyncCopy::AcquireAccess(hsa_agent_t agent,
                                    hsa_amd_memory_pool_t pool, void* ptr) {
  hsa_status_t err;

  hsa_amd_memory_pool_access_t access;
  err = hsa_amd_agent_memory_pool_get_info(agent, pool,
        HSA_AMD_AGENT_MEMORY_POOL_INFO_ACCESS, &access);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  ASSERT_NE(HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED, access);

  if (HSA_AMD_MEMORY_POOL_ACCESS_DISALLOWED_BY_DEFAULT == access) {
    err = hsa_amd_agents_allow_access(1, &agent, NULL, ptr);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  }
}

void MemoryAsyncCopy::RunBenchmarkWithVerification() {
  hsa_status_t err;
  void* ptr_src;
  void* ptr_dst;

  transaction& t = tran_.at(0);
  size_t size = t.size * 1024;

  FindSystemRegion();

  err = hsa_amd_memory_pool_allocate(region_info_[t.src].region_, size, 0,
                                     &ptr_src);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(region_info_[t.dst].region_, size, 0,
                                     &ptr_dst);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // rocrtst::CommonCleanUp data
  void* host_ptr_src = NULL;
  void* host_ptr_dst = NULL;
  err = hsa_amd_memory_pool_allocate(sys_region_, size, 0,
                                     (void**) &host_ptr_src);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  err = hsa_amd_memory_pool_allocate(sys_region_, size, 0,
                                     (void**) &host_ptr_dst);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  memset(host_ptr_src, 1, size);
  memset(host_ptr_dst, 0, size);

  hsa_signal_t s;
  err = hsa_signal_create(1, 0, NULL, &s);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  AcquireAccess(region_info_[t.src].owner_agent_, sys_region_, host_ptr_src);
  AcquireAccess(cpu_agent_, region_info_[t.src].region_, ptr_src);

  err = hsa_amd_memory_async_copy(ptr_src, region_info_[t.src].owner_agent_,
                                  host_ptr_src, cpu_agent_, size, 0, NULL, s);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  while (hsa_signal_wait_scacquire(s, HSA_SIGNAL_CONDITION_LT, 1, uint64_t(-1),
                                   HSA_WAIT_STATE_ACTIVE))
    ;

  int iterations = RealIterationNum();

  AcquireAccess(region_info_[t.dst].owner_agent_, region_info_[t.src].region_,
                ptr_src);

  for (int i = 0; i < 20; i++) {
    if (Size[i] > size) {
      break;
    }

    std::vector<double> time;

    for (int it = 0; it < iterations; it++) {
#if DEBUG
      std::cout << ".";
      std::cout.flush();
#endif
      // Check access to memory pool region
      AcquireAccess(region_info_[t.src].owner_agent_,
                    region_info_[t.dst].region_, ptr_dst);

      hsa_signal_store_relaxed(t.signal, 1);

      rocrtst::PerfTimer copy_timer;
      int index = copy_timer.CreateTimer();

      copy_timer.StartTimer(index);
      err = hsa_amd_memory_async_copy(ptr_dst, region_info_[t.dst].owner_agent_,
                                      ptr_src, region_info_[t.src].owner_agent_,
                                                    Size[i], 0, NULL, t.signal);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      while (hsa_signal_wait_scacquire(t.signal, HSA_SIGNAL_CONDITION_LT, 1,
                                       uint64_t(-1), HSA_WAIT_STATE_ACTIVE))
        ;

      copy_timer.StopTimer(index);

      hsa_signal_store_relaxed(s, 1);

      AcquireAccess(region_info_[t.dst].owner_agent_, sys_region_,
                    host_ptr_dst);
      AcquireAccess(cpu_agent_, region_info_[t.dst].region_, ptr_dst);

      err = hsa_amd_memory_async_copy(host_ptr_dst, cpu_agent_, ptr_dst,
                          region_info_[t.dst].owner_agent_, size, 0, NULL, s);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      while (hsa_signal_wait_scacquire(s, HSA_SIGNAL_CONDITION_LT, 1,
                                       uint64_t(-1), HSA_WAIT_STATE_ACTIVE))
        ;

      err = hsa_memory_copy(host_ptr_dst, ptr_dst, size);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      if (memcmp(host_ptr_src, host_ptr_dst, Size[i])) {
        verified_ = false;
      }

      // Push the result back to vector time
      time.push_back(copy_timer.ReadTimer(index));
    }

#if DEBUG
    std::cout << std::endl;
#endif

    // Get Min copy time
    min_time_.push_back(GetMinTime(time));
    // Get mean copy time and store to the array
    benchmark_copy_time_.push_back(GetMeanTime(time));
  }

  DisplayBenchmark();
}

void MemoryAsyncCopy::RunBenchmark() {
  hsa_status_t err;
  void* ptr_src;
  void* ptr_dst;

  transaction& t = tran_.at(0);
  size_t size = t.size * 1024;

  FindSystemRegion();

  err = hsa_amd_memory_pool_allocate(region_info_[t.src].region_, size, 0,
                                     &ptr_src);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(region_info_[t.dst].region_, size, 0,
                                     &ptr_dst);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Check access to memory pool region
  AcquireAccess(region_info_[t.src].owner_agent_, region_info_[t.dst].region_,
                ptr_dst);
  AcquireAccess(region_info_[t.dst].owner_agent_, region_info_[t.src].region_,
                ptr_src);

  int iterations = RealIterationNum();

  for (int i = 0; i < 20; i++) {
    if (Size[i] > size) {
      break;
    }

    std::vector<double> time;

    for (int it = 0; it < iterations; it++) {
#if DEBUG
      std::cout << ".";
      std::cout.flush();
#endif

      hsa_signal_store_relaxed(t.signal, 1);

      rocrtst::PerfTimer copy_timer;
      int index = copy_timer.CreateTimer();

      copy_timer.StartTimer(index);
      err = hsa_amd_memory_async_copy(ptr_dst, region_info_[t.dst].owner_agent_,
                                      ptr_src, region_info_[t.src].owner_agent_,
                                                    Size[i], 0, NULL, t.signal);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      while (hsa_signal_wait_scacquire(t.signal, HSA_SIGNAL_CONDITION_LT, 1,
                                       uint64_t(-1), HSA_WAIT_STATE_ACTIVE))
        ;

      copy_timer.StopTimer(index);

      // Push the result back to vector time
      time.push_back(copy_timer.ReadTimer(index));
    }

#if DEBUG
    std::cout << std::endl;
#endif

    // Get Min copy time
    min_time_.push_back(GetMinTime(time));
    // Get mean copy time and store to the array
    benchmark_copy_time_.push_back(GetMeanTime(time));
  }

  DisplayBenchmark();
}

void MemoryAsyncCopy::RunNormal() {
  int num_transaction = tran_.size();
  hsa_status_t err;
  std::vector<void*> ptr_src;
  std::vector<void*> ptr_dst;

  for (int i = 0; i < num_transaction; i++) {
    void* ptr_src_temp;
    void* ptr_dst_temp;
    transaction& t = tran_[i];
    hsa_amd_memory_pool_t region_src = region_info_[t.src].region_;
    hsa_amd_memory_pool_t region_dst = region_info_[t.dst].region_;
    size_t size = t.size * 1024;

    // Allocate memory
    err = hsa_amd_memory_pool_allocate(region_src, size, 0,
                                       (void**) &ptr_src_temp);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);
    err = hsa_amd_memory_pool_allocate(region_dst, size, 0,
                                       (void**) &ptr_dst_temp);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    AcquireAccess(region_info_[t.dst].owner_agent_, region_src, ptr_src_temp);
    AcquireAccess(region_info_[t.src].owner_agent_, region_dst, ptr_dst_temp);

    ptr_src.push_back(ptr_src_temp);
    ptr_dst.push_back(ptr_dst_temp);
  }

  int iterations = RealIterationNum();
  std::vector<double> time;

  for (int i = 0; i < iterations; i++) {
    for (int j = 0; j < num_transaction; j++) {
      transaction& t = tran_[j];
      hsa_signal_store_relaxed(t.signal, 1);
    }

    rocrtst::PerfTimer copy_timer;
    int index = copy_timer.CreateTimer();
    copy_timer.StartTimer(index);

    for (int j = 0; j < num_transaction; j++) {
      transaction& t = tran_[j];
      err = hsa_amd_memory_async_copy(ptr_dst[j],
             region_info_[t.dst].owner_agent_, ptr_src[j],
             region_info_[t.src].owner_agent_, t.size * 1024, t.num_dep_signal,
                                                        t.dep_signal, t.signal);
    }

    // Wait on the last transaction to finish
    while (hsa_signal_wait_scacquire(tran_[num_transaction - 1].signal,
              HSA_SIGNAL_CONDITION_LT, 1, uint64_t(-1), HSA_WAIT_STATE_ACTIVE))
      ;

    copy_timer.StopTimer(index);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    time.push_back(copy_timer.ReadTimer(index));
  }

  user_copy_time_ = GetMeanTime(time);
  DisplayResults();
}

size_t MemoryAsyncCopy::RealIterationNum() {
  return num_iteration() * 1.2 + 1;
}

double MemoryAsyncCopy::GetMinTime(std::vector<double>& vec) {
  std::sort(vec.begin(), vec.end());
  return vec.at(0);
}
double MemoryAsyncCopy::GetMeanTime(std::vector<double>& vec) {
  std::sort(vec.begin(), vec.end());

  vec.erase(vec.begin());
  vec.erase(vec.begin(), vec.begin() + num_iteration() * 0.1);
  vec.erase(vec.begin() + num_iteration(), vec.end());

  double mean = 0.0;
  int num = vec.size();

  for (int it = 0; it < num; it++) {
    mean += vec[it];
  }

  mean /= num;
  return mean;
}

void MemoryAsyncCopy::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  printf("================ User-Defined  Mode Result "
         "===================================\n");
  double band_width = (double) tran_.back().size / user_copy_time_ / 1024
                      / 1024;
  printf("  %zuKB                             %lf\n", tran_.back().size,
         band_width);
  return;
}

void MemoryAsyncCopy::DisplayBenchmark() {
  transaction& t = tran_.at(0);
  size_t size = t.size * 1024;
  printf("================ Benchmark Mode Result "
         "===================================\n");

  printf("Data Size             Avg Time(us)         Avg BW(GB/s)"
                              "          Min Time(us)         Peak BW(GB/s)\n");

  for (int i = 0; i < 20; i++) {
    if (Size[i] > size) {
      break;
    }

    double band_width = (double) Size[i] / benchmark_copy_time_[i] / 1024 / 1024
                        / 1024;
    double peak_band_width = (double) Size[i] / min_time_[i] / 1024 / 1024
                             / 1024;
    printf("  %4s            %14lf        %14lf         %14lf         %14lf\n",
          Str[i], benchmark_copy_time_[i] * 1e6, band_width, min_time_[i] * 1e6,
           peak_band_width);
  }

  if (verification_) {
    if (verified_) {
      std::cout << "Verification: Pass" << std::endl;
    }
    else {
      std::cout << "Verification: Fail" << std::endl;
    }
  }
  return;
}

void MemoryAsyncCopy::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void MemoryAsyncCopy::FindTopology() {
  hsa_status_t err;
  err = hsa_iterate_agents(AgentInfo, this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void MemoryAsyncCopy::ParseArgument() {
  bool print_help_info = false;
  hsa_status_t err;

  opterr = 0;
  int c;
  int src_region = 0;
  int dst_region = 0;
  size_t data_size = 512 * 1024;
  size_t opt_num = 0;
  char rec = 'n';

  while ((c = getopt(argc_, argv_, "hbvs:f:t:i:r:")) != -1) {
    switch (c) {
      case 'h':
        print_help_info = true;
        break;

      case 'f':
        src_region = std::stoi(optarg);
        opt_num++;
        break;

      case 't':
        dst_region = std::stoi(optarg);
        opt_num++;
        break;

      case 's':
        data_size = std::stoi(optarg);
        break;

      case 'i':
        set_num_iteration(std::stoi(optarg));
        break;

      case 'r':
        rec = tolower(*optarg);
        break;

      case 'b':
        bench_mark_mode_ = true;
        break;

      case 'v':
        verification_ = true;
        break;

      case '?':
        if (optopt == 'f' || optopt == 't' || optopt == 's' || optopt == 'i'
            || optopt == 'r') {
          std::cout << "Error: Option -f -t -s -i and -r ALL requires argument"
                    << std::endl;
          std::cout << help_info << std::endl;
        }

        ASSERT_NE("Error: Option -f -t -s -i and -r ALL requires argument", "");
        break;

      default:
        std::cout << "Error: Please set option argument properly!" << std::endl;
        std::cout << help_info << std::endl;
        ASSERT_NE("Error: Please set option argument properly!", "");
    }
  }

  //-h option has the highest priority
  if (print_help_info) {
    std::cout << help_info << std::endl;
    PrintTopology();
    ASSERT_NE("Exit on -h", "");
  }

  if (opt_num != 2) {
    std::cout << "You must specify all of -f -t" << std::endl;
    std::cout << help_info << std::endl;
    PrintTopology();
    ASSERT_NE("You must specify all of -f -t", "");
  }

  // Set transaction
  transaction trans;
  trans.src = src_region;
  trans.dst = dst_region;
  trans.size = data_size;
  trans.num_dep_signal = 0;
  trans.dep_signal = nullptr;
  err = hsa_signal_create(1, 0, NULL, &trans.signal);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  tran_.push_back(trans);

  if (!bench_mark_mode_) {
    while (rec != 'n') {
      int dep = 0;
      ;
      std::cout
          << "You will add another copy transaction, which will depends on "
          "previous ones." << std::endl;
      std::cout << "There are " << tran_.size() <<
                         " copy transactions already, how many transactions"
                                 " you want the new transaction depends on?"
                << std::endl;
      std::cin >> dep;
      std::cout
          << "Please specify which one you want to depend on, separate with "
          "whitespace, index from 0:" << std::endl;
      int* dep_ptr = new int[dep];

      for (int i = 0; i < dep; i++) {
        std::cin >> dep_ptr[i];
      }

      std::cout << "Please specify the dst memory pool:" << std::endl;
      std::cin >> dst_region;
      std::cout << "Please specify the src memory pool:" << std::endl;
      std::cin >> src_region;
      std::cout << "Please specify the data size:" << std::endl;
      std::cin >> data_size;
      std::cout << "Do you want to add more copy transaction: \"y\" or \"n\"?"
                << std::endl;
      char temp;
      std::cin >> temp;
      rec = tolower(temp);

      transaction t;
      t.dst = dst_region;
      t.src = src_region;
      t.size = data_size;
      err = hsa_signal_create(1, 0, NULL, &t.signal);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      t.num_dep_signal = dep;
      hsa_signal_t* signal_ptr = nullptr;

      if (dep != 0) {
        signal_ptr = new hsa_signal_t[dep];
      }

      for (int i = 0; i < dep; i++) {
        signal_ptr[i] = tran_.at(dep_ptr[i]).signal;
      }

      t.dep_signal = signal_ptr;
      tran_.push_back(t);

      delete[] dep_ptr;
    }
  }
}

void MemoryAsyncCopy::PrintTopology() {
  size_t node_num = node_info_.size();

  for (uint32_t i = 0; i < node_num; i++) {
    node_info node = node_info_.at(i);
    // Print agent info
    std::cout << std::endl;
    std::cout << "Agent #" << node.agent.index_ << ":" << std::endl;

    if (HSA_DEVICE_TYPE_CPU == node.agent.device_type_)
      std::cout << "Agent Device Type:                             CPU"
                << std::endl;
    else if (HSA_DEVICE_TYPE_GPU == node.agent.device_type_)
      std::cout << "Agent Device Type:                             GPU"
                << std::endl;

    // Print region info
    size_t region_num = node.region.size();

    for (uint32_t j = 0; j < region_num; j++) {
      std::cout << "    Memory Pool#" << node.region.at(j).index_ << ":"
                << std::endl;
      std::cout << "        max allocable size in KB: 		"
                << node.region.at(j).allocable_size_ / 1024 << std::endl;
      std::cout << "        is fine-grained: 			"
                << node.region.at(j).is_fine_grained_ << std::endl;
    }
  }
}

#define RET_IF_MEM_ASYNC_ERR(err) { \
  if ((err) != HSA_STATUS_SUCCESS) { \
    std::cout << "hsa api call failure at line " << __LINE__ << ", file: " << \
              __FILE__ << ". Call returned " << err << std::endl; \
    return (err); \
  } \
}

hsa_status_t RegionInfo(hsa_amd_memory_pool_t region, void* data) {
  hsa_status_t err;
  MemoryAsyncCopy* ptr = reinterpret_cast<MemoryAsyncCopy*>(data);
  // Query region segment, only report global one
  hsa_amd_segment_t region_segment;
  err = hsa_amd_memory_pool_get_info(region, HSA_AMD_MEMORY_POOL_INFO_SEGMENT,
                                     &region_segment);
  RET_IF_MEM_ASYNC_ERR(err);

  if (HSA_AMD_SEGMENT_GLOBAL != region_segment) {
    return HSA_STATUS_SUCCESS;
  }

  // Check if the region is alloc allowed, if not, discard this region
  bool alloc_allowed = false;
  err = hsa_amd_memory_pool_get_info(region,
              HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALLOWED, &alloc_allowed);
  RET_IF_MEM_ASYNC_ERR(err);

  if (alloc_allowed != true) {
    return HSA_STATUS_SUCCESS;
  }

  // Query the max allocable size
  size_t alloc_max_size = 0;
  err = hsa_amd_memory_pool_get_info(region, HSA_AMD_MEMORY_POOL_INFO_SIZE,
                                     &alloc_max_size);
  RET_IF_MEM_ASYNC_ERR(err);

  // Check if the region is fine-grained or coarse-grained
  uint32_t global_flag = 0;
  err = hsa_amd_memory_pool_get_info(region,
                        HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &global_flag);
  RET_IF_MEM_ASYNC_ERR(err);

  bool is_fine_grained = HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED
                         & global_flag;
  // ptr->region_info_.push_back(region_info(region, ptr->region_index_,
  // region_segment, is_fine_grained, host_accessible, alloc_max_size));

  ptr->region_info_.push_back(
    region_info(region, ptr->region_index_, region_segment, is_fine_grained,
                alloc_max_size, ptr->agent_info_.back().agent_));

  // Construct node_info and push back to node_info_
  ptr->node_info_[ptr->agent_index_].region.push_back(ptr->region_info_.back());
  ptr->region_index_++;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AgentInfo(hsa_agent_t agent, void* data) {
  MemoryAsyncCopy* ptr = reinterpret_cast<MemoryAsyncCopy*>(data);

  hsa_status_t err;
  char name[64];
  err = hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, name);
  RET_IF_MEM_ASYNC_ERR(err);

  // Get device type
  hsa_device_type_t device_type;
  err = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
  RET_IF_MEM_ASYNC_ERR(err);

  ptr->agent_info_.push_back(agent_info(agent, ptr->agent_index_, device_type));

  // Contruct an new node_info structure and push back to node_info_
  node_info node;
  node.agent = ptr->agent_info_.back();
  ptr->node_info_.push_back(node);

  err = hsa_amd_agent_iterate_memory_pools(agent, RegionInfo, ptr);
  ptr->agent_index_++;

  return HSA_STATUS_SUCCESS;
}

#undef RET_IF_MEM_ASYNC_ERR
