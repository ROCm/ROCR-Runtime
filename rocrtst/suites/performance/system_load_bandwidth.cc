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

#include "system_load_bandwidth.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "common/os.h"
#include "gtest/gtest.h"
#include <algorithm>

#if 0
static void initGlobalReadBuffer(uint32_t* in_data, uint32_t num_thrds,
                                 uint32_t num_ops, uint32_t num_loops) {

  // Populate input buffer with thread Id left shifted by 2.
  uint32_t value = 0;
  uint32_t val_idx = 0;

  for (int idx1 = 0; idx1 < num_loops; idx1++) {
    for (int idx2 = 0; idx2 < num_ops; idx2++) {
      // Write the value to be read by each thread
      for (int idx3 = 0; idx3 < num_thrds; idx3++) {
        value = idx3 << 2;
        in_data[val_idx++] = value;
      }
    }
  }

  return;
}

static bool verifyGlobalLoadKernel(uint32_t* data, uint32_t num_thrds,
                  uint32_t scale, const char* kernel_name, bool print_debug) {

  // Verify kernel operation i.e. validate the data in the output buffer.
  bool valid = true;
  uint32_t valid_value = 0;

  for (int idx = 0; idx < num_thrds; idx++) {

    valid_value = (idx << 2) * scale;

    if (print_debug) {
      std::cout << "Value expected = " << valid_value << std::endl;
      std::cout << "Value of data = " << data[idx] << std::endl;
    }

    if (data[idx] != valid_value) {
      std::cout << kernel_name << ": VALIDATION FAILED ! Bad index: " << idx
                << std::endl;
      std::cout << kernel_name << ": VALUE @ Bad index: " << data[idx]
                << std::endl;
      std::cout << std::endl;
      break;
    }
  }

#ifdef DEBUG
  std::cout << kernel_name << ": Passed validation" << std::endl;
  std::cout << std::endl;
#endif

  return true;
}
#endif

// Constructor
SystemLoadBandwidth::SystemLoadBandwidth() :
  BaseRocR() {
  set_group_size(0);
  num_group_ = 0;
  num_cus_ = 0;

  kernel_loop_count_ = 0;
  mean_ = 0.0;
  data_size_ = 0;
  set_enable_interrupt(0);
}

// Destructor
SystemLoadBandwidth::~SystemLoadBandwidth() {
}

// Set up the test environment
void SystemLoadBandwidth::SetUp() {
  set_kernel_file_name("sysMemRead.o");
  set_kernel_name("&__SysMemLoad");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }
 
  hsa_agent_t* gpu_dev = gpu_device1();
  SetWorkItemNum();

  //Create a queue with max number size
  hsa_queue_t* q = main_queue();
  rocrtst::CreateQueue(*gpu_dev, &q);

  rocrtst::LoadKernelFromObjFile(this);

  uint32_t total_work_items = num_cus_ * num_group_ * group_size();

  //Fill up part of aql
  rocrtst::InitializeAQLPacket(this, &aql());
  aql().workgroup_size_x = group_size();
  aql().grid_size_x = total_work_items;

  return;
}

// Run the test
void SystemLoadBandwidth::Run() {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  uint32_t total_workitems = num_cus_ * num_group_ * group_size();
  hsa_agent_t* gpu_dev = gpu_device1();
  hsa_status_t err;

  uint32_t ops_thrd = 32;
  uint64_t addr_step = (uint64_t) total_workitems * sizeof(uint32_t);
  uint64_t total_ops = (uint64_t) total_workitems * ops_thrd;
  uint64_t in_data_size = (uint64_t) total_ops * sizeof(uint32_t);
  //uint32_t *in_data = (uint32_t *)malloc(in_data_size);
  err = hsa_amd_agent_iterate_memory_pools(*gpu_dev, rocrtst::FindStandardPool,
                                                                &device_pool());
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  int32_t* in_data = NULL;
  err = hsa_amd_memory_pool_allocate(device_pool(), in_data_size, 0,
                                     (void**) &in_data);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  memset(in_data, 0, in_data_size);
  uint32_t out_data_size = total_workitems * sizeof(uint32_t);
  //uint32_t *out_data = (uint32_t *)malloc(out_data_size);
  uint32_t* out_data;
  err = hsa_amd_memory_pool_allocate(device_pool(), out_data_size, 0,
                                     (void**) &out_data);
  memset(out_data, 0, out_data_size);

  data_size_ = in_data_size;

  // initGlobalReadBuffer (in_data, total_workitems, ops_thrd,
  //                                                     kernel_loop_count_);

  typedef struct local_args_t {
    void* arg0;
    void* arg1;
    uint64_t arg2;
    void* arg3;
  } args;

  args* kern_ptr = NULL;
  err = hsa_amd_memory_pool_allocate(device_pool(), sizeof(args), 0,
                                     (void**) &kern_ptr);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // in_data is 32 bit ptr, so adding total_ops
  kern_ptr->arg0 = in_data;
  kern_ptr->arg1 = in_data + total_ops;
  kern_ptr->arg2 = addr_step;
  kern_ptr->arg3 = out_data;

  aql().kernarg_address = kern_ptr;

  std::vector<double> time;

  int it = num_iteration() * 1.2 + 1;

  void *q_base_addr = main_queue()->base_address;

  for (int i = 0; i < it; i++) {
    // Obtain the current queue write index
    uint64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

    // Write the aql packet at the calculated queue index address.
    const uint32_t queue_mask = main_queue()->size - 1;
    ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask] = aql();

    rocrtst::PerfTimer p_timer;
    int id = p_timer.CreateTimer();
    p_timer.StartTimer(id);

    ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask].header |=
                     HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;
    hsa_signal_store_screlease(main_queue()->doorbell_signal, index);

    // Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;

    p_timer.StopTimer(id);

#if DEBUG
    std::cout << ".";
    std::cout.flush();
#endif

    // Verify the results
    // uint32_t scale = kernel_loop_count_ * ops_thrd;
    //verifyGlobalLoadKernel(out_data, total_workitems, scale,
    //                                           kernel_name_.c_str(), false);

    time.push_back(p_timer.ReadTimer(id));

    hsa_signal_store_screlease(signal(), 1);
  }

  time.erase(time.begin());
  std::sort(time.begin(), time.end());
  time.erase(time.begin() + num_iteration(), time.end());
  mean_ = rocrtst::CalcMean(time);

  return;

}

void SystemLoadBandwidth::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void SystemLoadBandwidth::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  std::cout << "=======================================" << std::endl;
  std::cout << "System Load Bandwidth:     %f(GB/S)" <<
            data_size_ / mean_ / 1024 / 1024 / 1024 << std::endl;
}
