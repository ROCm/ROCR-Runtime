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

#include "flush_latency.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "common/os.h"
#include "gtest/gtest.h"
#include <algorithm>

static const int kWorkItem = 1024 * 1204;
// Constructor
FlushLatency::FlushLatency() :
  BaseRocR() {
  set_group_size(0);
  num_group_ = 0;
  num_cus_ = 0;

  kernel_loop_count_ = 0;
  mean_ = 0.0;
  data_size_ = 0;

  set_requires_profile (HSA_PROFILE_BASE);
}

// Destructor
FlushLatency::~FlushLatency() {
}

// Set up the test environment
void FlushLatency::SetUp() {
  hsa_status_t err;

  SetWorkItemNum();

  set_kernel_file_name("flush_latency.o");
  set_kernel_name("&main");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();

  //Create a queue with max number size
  hsa_queue_t* q;
  rocrtst::CreateQueue(*gpu_dev, &q);
  set_main_queue(q);

  //Enable profiling
  err = hsa_amd_profiling_set_profiler_enabled(main_queue(), 1);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  rocrtst::LoadKernelFromObjFile(this);

  uint32_t total_work_items = kWorkItem * 0.3;

  //Fill up part of aql
  rocrtst::InitializeAQLPacket(this, &aql());
  aql().workgroup_size_x = group_size();
  aql().grid_size_x = total_work_items;

  return;
}

// Run the test
void FlushLatency::Run() {
  hsa_status_t err;
  hsa_amd_memory_pool_t cpu_pool;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();
  hsa_agent_t* cpu_dev = cpu_device();

  err = hsa_amd_agent_iterate_memory_pools(*gpu_dev, rocrtst::FindStandardPool,
                                                                &device_pool());
  ASSERT_EQ(err, HSA_STATUS_INFO_BREAK);

  ASSERT_NE(device_pool().handle, 0);

  cpu_pool.handle = 0;
  err = hsa_amd_agent_iterate_memory_pools(*cpu_dev, rocrtst::FindGlobalPool,
        &cpu_pool);
  ASSERT_EQ(err, HSA_STATUS_INFO_BREAK);

  ASSERT_NE(cpu_pool.handle, 0);

#if DEBUG
  std::cout << "Device Pool Properties:" << std::endl;
  err = rocrtst::DumpMemoryPoolInfo(device_pool());
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  std::cout << "Global Pool Properties:" << std::endl;
  err = rocrtst::DumpMemoryPoolInfo(cpu_pool);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
#endif
  uint32_t out_data_size = 1024 * 1024 * sizeof(uint32_t);

  std::vector<double> time_none;
  std::vector<double> time_release;

  std::vector < uint64_t > time_none_stamp;
  std::vector < uint64_t > time_release_stamp;

  //Query system timestamp frequency
  uint64_t freq;
  err = hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY, &freq);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  void* out = NULL;
  uint32_t* out_data;
  const uint32_t queue_mask = main_queue()->size - 1;
  typedef struct local_args_t {
    void* arg0;
  } args;

  // Warm up
  uint16_t header = 0;
  header |= HSA_FENCE_SCOPE_AGENT << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;
  header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
  header |= HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;
  aql().header = header;

  err = hsa_amd_memory_pool_allocate(device_pool(), out_data_size, 0,
                                     (void**) &out_data);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  args* kern_ptr = NULL;
  err = hsa_amd_memory_pool_allocate(cpu_pool, sizeof(args), 0,
                                     (void**) &kern_ptr);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  kern_ptr->arg0 = out_data;

  aql().kernarg_address = kern_ptr;

  // Obtain the current queue write index
  int64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

  void *q_base_addr = main_queue()->base_address;
  // Write the aql packet at the calculated queue index address.
  ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask] = aql();

  hsa_signal_store_screlease(main_queue()->doorbell_signal, index);

  // Wait on the dispatch signal until the kernel is finished.
  while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                   (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
    ;

  hsa_signal_store_screlease(signal(), 1);

  for (int i = 0; i < 1000; i++) {
    err = hsa_amd_memory_pool_allocate(device_pool(), out_data_size, 0,
                                       (void**) &out_data);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    args* kern_ptr = NULL;
    err = hsa_amd_memory_pool_allocate(cpu_pool, sizeof(args), 0,
                                       (void**) &kern_ptr);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    kern_ptr->arg0 = out_data;

    aql().kernarg_address = kern_ptr;

    // Obtain the current queue write index
    int64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

    // Write the aql packet at the calculated queue index address.
    ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask] = aql();

    hsa_signal_store_screlease(main_queue()->doorbell_signal, index);

    // Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;

    hsa_amd_profiling_dispatch_time_t dispatch_time;
    err = hsa_amd_profiling_get_dispatch_time(*gpu_dev, signal(),
          &dispatch_time);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    uint64_t sys_start = 0;
    uint64_t sys_end = 0;
    err = hsa_amd_profiling_convert_tick_to_system_domain(*gpu_dev,
          dispatch_time.start, &sys_start);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);
    err = hsa_amd_profiling_convert_tick_to_system_domain(*gpu_dev,
          dispatch_time.end, &sys_end);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    uint64_t stamp = dispatch_time.end - dispatch_time.start;
    double execution_time = (double) stamp / freq * 1e6; // convert to us.

    time_none.push_back(execution_time);
    time_none_stamp.push_back(stamp);

    hsa_signal_store_screlease(signal(), 1);

    if (out != NULL) {
      err = hsa_memory_free(out);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);
    }

    out = out_data;
    out_data = NULL;
  }

  header = 0;
  header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;
  header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
  header |= HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;
  aql().header = header;

  for (int i = 0; i < 1000; i++) {
    err = hsa_amd_memory_pool_allocate(device_pool(), out_data_size, 0,
                                       (void**) &out_data);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    args* kern_ptr = NULL;
    err = hsa_amd_memory_pool_allocate(cpu_pool, sizeof(args), 0,
                                       (void**) &kern_ptr);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    kern_ptr->arg0 = out_data;

    aql().kernarg_address = kern_ptr;

    // Obtain the current queue write index
    uint64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

    // Write the aql packet at the calculated queue index address.
    ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask] = aql();

    hsa_signal_store_screlease(main_queue()->doorbell_signal, index);

    // Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;

    hsa_signal_store_screlease(signal(), 1);

    hsa_amd_profiling_dispatch_time_t dispatch_time;
    err = hsa_amd_profiling_get_dispatch_time(*gpu_dev, signal(),
          &dispatch_time);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    uint64_t sys_start = 0;
    uint64_t sys_end = 0;
    err = hsa_amd_profiling_convert_tick_to_system_domain(*gpu_dev,
          dispatch_time.start, &sys_start);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);
    err = hsa_amd_profiling_convert_tick_to_system_domain(*gpu_dev,
          dispatch_time.end, &sys_end);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    uint64_t stamp = dispatch_time.end - dispatch_time.start;
    double execution_time = (double) stamp / freq * 1e6; // convert to us.
    time_release.push_back(execution_time);
    time_release_stamp.push_back(stamp);

    if (out != NULL) {
      err = hsa_memory_free(out);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);
    }

    out = out_data;
    out_data = NULL;
  }

  std::sort(time_none.begin(), time_none.end());
  std::sort(time_release.begin(), time_release.end());

  time_none.erase(time_none.begin(), time_none.begin() + 50);
  time_none.erase(time_none.end() - 50, time_none.end());
  time_release.erase(time_release.begin(), time_release.begin() + 50);
  time_release.erase(time_release.end() - 50, time_release.end());

  mean_ = rocrtst::CalcMean(time_none, time_release);

  return;
}

void FlushLatency::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void FlushLatency::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  std::cout << std::endl << "======================================="
            << std::endl;
  std::cout << "Average cache flush overhead:     " << mean_ << "uS"
            << std::endl;
  std::cout << "=======================================" << std::endl;
  return;
}
