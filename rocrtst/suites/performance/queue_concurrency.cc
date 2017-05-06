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
#include "queue_concurrency.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "common/os.h"
#include "hsa/hsa_ext_finalize.h"
#include "gtest/gtest.h"

#include <math.h>
#include <thread>

QueueConcurrency::QueueConcurrency() :
  BaseRocR(), execution_time_(8) {
  queue_num_ = 0;
  std_time_ = 0.0;

  set_enable_interrupt(true);
  set_requires_profile (HSA_PROFILE_FULL);
}

QueueConcurrency::~QueueConcurrency() {
}

void QueueConcurrency::SetUp() {
  hsa_status_t err;

  set_kernel_file_name("test_kernel.o");
  set_kernel_name("&__OpenCL_vec_assign_kernel");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }
 
  rocrtst::LoadKernelFromObjFile(this);

  hsa_agent_t* gpu_dev = gpu_device1();

  // Fill up part of aql pakcet which are the same cross the threads
  rocrtst::InitializeAQLPacket(this, &aql());

  // Create a queue
  hsa_queue_t* q = main_queue();
  rocrtst::CreateQueue(*gpu_dev, &q);

  for (int i = 0; i < 2; i++) {
    // Output of kernel
    int output = 0;

    // Iteration number
    int iterations = 1024 * 1024; // * 1024;

    struct ALIGNED_(16)
    args_t {
      void* arg0;
      int arg1;
    } local_args;

    local_args.arg0 = (void*) &output;
    local_args.arg1 = iterations;

    err = hsa_memory_register(&local_args, sizeof(local_args));
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    //Obtain the current queue write index.
    uint64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

    //Write the aql packet at the calculated queue index address.

    const uint32_t queue_mask = main_queue()->size - 1;
    hsa_kernel_dispatch_packet_t* pkt_addr =
      (hsa_kernel_dispatch_packet_t*) (main_queue()->base_address);

    (pkt_addr)[index & queue_mask] = aql();
    (pkt_addr)[index & queue_mask].completion_signal = signal();
    (pkt_addr)[index & queue_mask].kernarg_address = &local_args;

    //Get timing stamp and ring the doorbell to dispatch the kernel.
    rocrtst::PerfTimer p_timer;
    int id = p_timer.CreateTimer();
    p_timer.StartTimer(id);

    //.type = HSA_PACKET_TYPE_DISPATCH;
    (pkt_addr)[index & queue_mask].header |= HSA_PACKET_TYPE_KERNEL_DISPATCH
        << HSA_PACKET_HEADER_TYPE;
    hsa_signal_store_screlease(main_queue()->doorbell_signal, index);

    //Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;

    p_timer.StopTimer(id);
    hsa_signal_store_screlease(signal(), 1);

    if (1 == i) {
      std_time_ = p_timer.ReadTimer(id);
    }
  }

  //Destroy the queue
  err = hsa_queue_destroy(main_queue());
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void QueueConcurrency::Run() {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  // Launch 8 child threads
  std::vector < std::thread > threads;

  for (int i = 0; i < 8; i++) {
    threads.push_back(std::thread(&QueueConcurrency::ThreadFunc, this, i));
  }

  // Wait for join
  for (int i = 0; i < 8; i++) {
    threads[i].join();
  }

  CalculateQueueNum();
}

void QueueConcurrency::CalculateQueueNum() {
  for (int i = 0; i < 8; i++) {
    double expected_time = execution_time_[0] / (1 << i);
    double deviation = sqrt(
                         (expected_time - execution_time_[i])
                         * (expected_time - execution_time_[i]));

    if (deviation < 0.1 * expected_time) {
      queue_num_++;
    }
  }
}

void QueueConcurrency::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  for (int i = 0; i < 8; i++) {
    std::cout << execution_time_[i] << std::endl;
  }

  std::cout << "Number of Concurrent Queue is: " << queue_num_ << std::endl;

  ASSERT_EQ(queue_num_, 3);

  return;
}

void QueueConcurrency::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void QueueConcurrency::ThreadFunc(int threadID) {
  // Define local queue and signal
  hsa_queue_t* queue;
  hsa_signal_t signal;
  hsa_status_t err;
  hsa_agent_t* gpu_dev = gpu_device1();

  // Create a signal
  err = hsa_signal_create(1, 0, NULL, &signal);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  rocrtst::CreateQueue(*gpu_dev, &queue);

  std::vector<double> time;

  for (uint32_t i = 0; i < num_iteration(); i++) {
    // Output of kernel
    int output = 0;

    // Iteration number
    int iterations = 1024 * 1024 / (1 << threadID);

    struct ALIGNED_(16)
    args_t {
      void* arg0;
      int arg1;
    } local_args;

    local_args.arg0 = (void*) &output;
    local_args.arg1 = iterations;

    err = hsa_memory_register(&local_args, sizeof(local_args));
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    //Obtain the current queue write index.
    uint64_t index = hsa_queue_add_write_index_relaxed(queue, 1);

    //Write the aql packet at the calculated queue index address.

    const uint32_t queue_mask = queue->size - 1;
    hsa_kernel_dispatch_packet_t* pkt_addr =
      (hsa_kernel_dispatch_packet_t*) (queue->base_address);
    (pkt_addr)[index & queue_mask] = aql();
    (pkt_addr)[index & queue_mask].completion_signal = signal;
    (pkt_addr)[index & queue_mask].kernarg_address = &local_args;

    //Get timing stamp and ring the doorbell to dispatch the kernel.
    rocrtst::PerfTimer p_timer;
    int id = p_timer.CreateTimer();
    p_timer.StartTimer(id);

    //.type = HSA_PACKET_TYPE_DISPATCH;
    (pkt_addr)[index & queue_mask].header |= HSA_PACKET_TYPE_KERNEL_DISPATCH
        << HSA_PACKET_HEADER_TYPE;
    hsa_signal_store_screlease(queue->doorbell_signal, index);

    //Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal, HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;

    p_timer.StopTimer(id);
    hsa_signal_store_screlease(signal, 1);

    time.push_back(p_timer.ReadTimer(id));

    EXPECT_EQ(output, iterations);

    if (1 == i) {
      execution_time_[threadID] = p_timer.ReadTimer(id);
    }
  }

  time.erase(time.begin());
  execution_time_[threadID] = rocrtst::CalcMean(time);
  return;
}

