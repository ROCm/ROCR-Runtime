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

#include "cp_process_time.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "common/os.h"
#include "gtest/gtest.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"
#include "hsa/hsa_ext_finalize.h"
#include <algorithm>

static const uint64_t kKernelIterations = 10000;
static const uint64_t kTestBadValue = 1234567891234567891;
//Set up some expectations for reasonable processing times
//For gfx803, Overhead time had a max of 18.208uS and a min of 7.82uS
static const double kGfx803MinOverhead = 7.78;
static const double kGfx803MaxOverhead = 21.064;
static const double kOverheadToleranceFactor = 0.25;

CpProcessTime::CpProcessTime() :
  BaseRocR() {
  // kernel_name_ = "&__simple_kernel";
  mean_ = 0.0;
}

CpProcessTime::~CpProcessTime() {
}

void CpProcessTime::SetUp() {
  hsa_status_t err;
  set_kernel_file_name("simple_kernel.o");
  set_kernel_name("&__simple_kernel");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }
  hsa_agent_t* gpu_dev = gpu_device1();

  // Create a queue
  hsa_queue_t* q = nullptr;
  rocrtst::CreateQueue(*gpu_dev, &q);
  ASSERT_NE(q, nullptr);
  set_main_queue(q);

  // Set profiling
  err = hsa_amd_profiling_set_profiler_enabled(q, 1);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Load and finalize the kernel
  err = rocrtst::LoadKernelFromObjFile(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  rocrtst::InitializeAQLPacket(this, &aql());
  aql().workgroup_size_x = 1;
  aql().grid_size_x = 1;
}

size_t CpProcessTime::RealIterationNum() {
  return num_iteration() * 1.2 + 1;
}

void CpProcessTime::Run() {
  hsa_status_t err;
  std::vector<double> timer;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();
  hsa_agent_t* cpu_dev = cpu_device();

  ASSERT_NE(gpu_dev, nullptr);
  ASSERT_NE(cpu_dev, nullptr);
  uint32_t it = RealIterationNum();

  typedef struct args_t {
    uint64_t* iteration;
    uint64_t* result;
  } args;

  err = rocrtst::SetPoolsTypical(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  uint64_t* iter = NULL;
  uint64_t* result = NULL;
  err = rocrtst::AllocAndAllowAccess(this, sizeof(uint64_t), cpu_pool(),
                                                               (void**)&iter);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = rocrtst::AllocAndAllowAccess(this, sizeof(uint64_t), cpu_pool(),
                                                             (void**)&result);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  
  *iter = kKernelIterations;
  *result = kTestBadValue;

  args  k_args;

  k_args.iteration = (uint64_t*)iter;
  k_args.result = (uint64_t*)result;

  err = rocrtst::AllocAndSetKernArgs(this, &k_args, sizeof(args));
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  rocrtst::WriteAQLToQueue(this);

  void * q_base_addr = main_queue()->base_address;
  const uint32_t queue_mask = main_queue()->size - 1;
  uint32_t aql_header = HSA_PACKET_TYPE_KERNEL_DISPATCH;
//  aql_header |= HSA_FENCE_SCOPE_SYSTEM <<
//                                    HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
//  aql_header |= HSA_FENCE_SCOPE_SYSTEM <<
//                                    HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;

  for (uint32_t i = 0; i < it; i++) {
    // uint64_t que_idx = hsa_queue_load_write_index_relaxed(main_queue());
    uint64_t que_idx = hsa_queue_add_write_index_relaxed(main_queue(), 1);

    //Get timing stamp an ring the doorbell to dispatch the kernel.
    rocrtst::PerfTimer p_timer;
    int id = p_timer.CreateTimer();
    p_timer.StartTimer(id);

    rocrtst::AtomicSetPacketHeader(aql_header, aql().setup,
             &((hsa_kernel_dispatch_packet_t*)(q_base_addr))[que_idx & queue_mask]);

    hsa_queue_store_write_index_relaxed(main_queue(), (que_idx + 1));
    hsa_signal_store_relaxed(main_queue()->doorbell_signal, que_idx);

    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;
//    hsa_signal_value_t value = hsa_signal_wait_scacquire(signal(),
//                HSA_SIGNAL_CONDITION_LT, 1, UINT64_MAX, HSA_WAIT_STATE_BLOCKED);
    // value should be 0, or we timed-out
    //ASSERT_EQ(value, 0);

    p_timer.StopTimer(id);

    hsa_amd_profiling_dispatch_time_t dispatch_time;
    err = hsa_amd_profiling_get_dispatch_time(*gpu_dev, signal(),
          &dispatch_time);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    uint64_t ticks = dispatch_time.end - dispatch_time.start;
    uint64_t freq;

    err = hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY, &freq);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    hsa_signal_store_screlease(signal(), 1);

    double execution_time = (double) ticks / freq * 1e6; //convert to us
    double temp = p_timer.ReadTimer(id) * 1e6;
    double cp_time = temp - execution_time;

#ifdef DEBUG
    std::cout << "Total:" << temp << "uS ";
    std::cout << "Execution:" << execution_time << "uS ";
    std::cout << "Overhead:" << cp_time << "uS ";
    std::cout << "Overhead %:" << cp_time / execution_time * 100 << std::endl;
#endif

    EXPECT_EQ(kKernelIterations, *result);
    timer.push_back(cp_time);

    //Assume overhead will not deviate too much from previously recorded
    // values. If this does happen and there is not a performance bug,
    // modify these constants

    //This may need to be made specific to the gpu being used
    EXPECT_GT(cp_time, kGfx803MinOverhead * (1 - kOverheadToleranceFactor));
    EXPECT_LT(cp_time, kGfx803MaxOverhead * (1 + kOverheadToleranceFactor));

    *result = 0;
  }

  //Abandon the first result and after sort, delete the last 2% value
  timer.erase(timer.begin());
  std::sort(timer.begin(), timer.end());

  timer.erase(timer.begin() + num_iteration(), timer.end());
  mean_ = rocrtst::CalcMean(timer);

  return;
}

void CpProcessTime::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  if (mean_ == 0.0) {
    return;
  }

  std::cout << "===================================================="
            << std::endl;
  std::cout << "The average Command Processor processing time is:  " << mean_
            << "us" << std::endl;
  std::cout << "===================================================="
            << std::endl;
  return;
}

void CpProcessTime::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}
