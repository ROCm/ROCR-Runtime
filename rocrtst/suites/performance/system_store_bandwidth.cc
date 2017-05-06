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

#include "system_store_bandwidth.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "gtest/gtest.h"

static bool verifyGlobalStoreKernel(uint32_t* data, uint32_t num_thrds,
                                    uint32_t loop_cnt, uint32_t ops_loop,
                                    const char* kernel_name,
                                    bool print_debug) {

  // Verify kernel operation i.e. validate the data in the output buffer.
  for (uint32_t idx1 = 0; idx1 < loop_cnt; idx1++) {
    for (uint32_t idx2 = 0; idx2 < ops_loop; idx2++) {
      for (uint32_t idx3 = 0; idx3 < num_thrds; idx3++) {
        if (data[idx3] != (idx3 << 2)) {
          std::cout << kernel_name << ": VALIDATION FAILED ! Bad index: "
                    << idx3 << std::endl;
          std::cout << kernel_name << ": VALUE @ Bad index: " << data[idx3]
                    << std::endl;
          break;
        }
      }
    }
  }

#ifdef DEBUG
  std::cout << kernel_name << ": Passed validation" << std::endl;
  std::cout << std::endl;
#endif

  return true;
}

// Constructor
SystemStoreBandwidth::SystemStoreBandwidth() :
  BaseRocR() {

  set_group_size(0);
  num_group_ = 0;
  num_cus_ = 0;

  kernel_loop_count_ = 0;
  mean_ = 0.0;
  data_size_ = 0;
}

// Destructor
SystemStoreBandwidth::~SystemStoreBandwidth() {
}

// Set up the test environment
void SystemStoreBandwidth::SetUp() {

  set_kernel_file_name("sysMemWrite.o");
  set_kernel_name("&__SysMemStore");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }
  hsa_agent_t* gpu_dev = gpu_device1();

  SetWorkItemNum();

  //Create a queue with max number size
  hsa_queue_t* q = nullptr;
  rocrtst::CreateQueue(*gpu_dev, &q);
  set_main_queue(q);

  rocrtst::LoadKernelFromObjFile(this);

  uint32_t total_work_items = num_cus_ * num_group_ * group_size();

  //Fill up part of aql
  rocrtst::InitializeAQLPacket(this, &aql());
  aql().workgroup_size_x = group_size();
  aql().grid_size_x = total_work_items;

  return;
}

// Run the test
void SystemStoreBandwidth::Run() {
  hsa_status_t err;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  uint32_t total_workitems = num_cus_ * num_group_ * group_size();
  hsa_agent_t* gpu_dev = gpu_device1();

  uint32_t ops_thrd = 16;
  uint64_t addr_step = (uint64_t) total_workitems * sizeof(uint32_t);
  uint64_t total_ops = (uint64_t) total_workitems * kernel_loop_count_
                       * ops_thrd;
  uint64_t in_data_size = (uint64_t) total_ops * sizeof(uint32_t);
  err = hsa_amd_agent_iterate_memory_pools(*gpu_dev,
                                   rocrtst::FindStandardPool, &device_pool());
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  uint32_t* in_data = NULL;
  err = hsa_amd_memory_pool_allocate(device_pool(), in_data_size, 0,
                                     (void**) &in_data);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  //memset(in_data, 0, in_data_size);
  err = hsa_amd_memory_fill(in_data, 0, in_data_size);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  uint32_t out_data_size = total_workitems * sizeof(uint32_t);
  uint32_t* out_data = NULL;
  err = hsa_amd_memory_pool_allocate(device_pool(), out_data_size, 0,
                                     (void**) &out_data);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  //memset(out_data, 0, out_data_size);
  err = hsa_amd_memory_fill(out_data, 0, out_data_size);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  data_size_ = in_data_size;

  typedef struct local_args_t {
    void* arg0;
    void* arg1;
    uint64_t arg2;
    void* arg3;
  } args;

  // in_data is 32 bit ptr, so adding total_ops
  args* kern_ptr = NULL;
  err = hsa_amd_memory_pool_allocate(device_pool(), sizeof(args), 0,
                                     (void**) &kern_ptr);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  kern_ptr->arg0 = in_data;
  kern_ptr->arg1 = in_data + total_ops;
  kern_ptr->arg2 = addr_step;
  kern_ptr->arg3 = out_data;

  aql().kernarg_address = kern_ptr;

  std::vector<double> time;
  void *q_base_addr = main_queue()->base_address; 
  for (uint32_t i = 0; i < num_iteration(); i++) {
    // Obtain the current queue write index
    uint64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

    // Write the aql packet at the calculated queue index address.
    const uint32_t queue_mask = main_queue()->size - 1;
    ((hsa_kernel_dispatch_packet_t*)(q_base_addr))[index & queue_mask] = aql(); 

    rocrtst::PerfTimer p_timer;
    int id = p_timer.CreateTimer();
    p_timer.StartTimer(id);

    ((hsa_kernel_dispatch_packet_t*)(q_base_addr))[index & queue_mask].header |=
      HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;
    hsa_signal_store_screlease(main_queue()->doorbell_signal, index);

    // Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;

    p_timer.StopTimer(id);

    // Verify the results
    verifyGlobalStoreKernel(in_data, total_workitems, kernel_loop_count_,
                                     ops_thrd, kernel_name().c_str(), false);

    time.push_back(p_timer.ReadTimer(id));

    hsa_signal_store_screlease(signal(), 1);
  }

  time.erase(time.begin());
  mean_ = rocrtst::CalcMean(time);

  return;
}

void SystemStoreBandwidth::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  return;
}

void SystemStoreBandwidth::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  std::cout << "=======================================" << std::endl;
  std::cout << "System Load Bandwidth:     %f(GB/S)"
            << data_size_ / mean_ / 1024 / 1024 / 1024 << std::endl;
}
