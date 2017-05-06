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

#include "dispatch_time.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/os.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "gtest/gtest.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_finalize.h"
#include <algorithm>

DispatchTime::DispatchTime() :
  BaseRocR() {
  use_default_ = false;
  launch_single_ = false;
  queue_size_ = 0;
  num_batch_ = 100000;
  memset(&aql(), 0, sizeof(hsa_kernel_dispatch_packet_t));
  single_default_mean_ = 0.0;
  single_interrupt_mean_ = 0.0;
  multi_default_mean_ = 0.0;
  multi_interrupt_mean_ = 0.0;
}

DispatchTime::~DispatchTime() {

}

void DispatchTime::SetUp() {
  // If it indicates to use default signal, set env var properly
  if (use_default_) {
    set_enable_interrupt(false);
  }
  else {
    set_enable_interrupt(true);
  }

  set_kernel_file_name("empty_kernel.o");
  set_kernel_name("&__Empty_kernel");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();

  // Create a queue
  hsa_queue_t* q = nullptr;
  rocrtst::CreateQueue(*gpu_dev, &q);
  ASSERT_NE(q, nullptr);
  set_main_queue(q);

  // Here, modify the batch size if it is larger than the queue size
  if (!launch_single_) {
    hsa_status_t err;
    uint32_t size = 0;
    err = hsa_agent_get_info(*gpu_dev, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &size);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    num_batch_ = num_batch_ > size ? size : num_batch_;
  }

  rocrtst::LoadKernelFromObjFile(this);

  // Fill up the kernel packet except header
  rocrtst::InitializeAQLPacket(this, &aql());
  aql().workgroup_size_x = 1;
  aql().grid_size_x = 1;
}

void DispatchTime::Run() {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  if (launch_single_) {
    RunSingle();
  }
  else {
    RunMulti();
  }
}

size_t DispatchTime::RealIterationNum() {
  return num_iteration() * 1.2 + 1;
}

void DispatchTime::RunSingle() {
  std::vector<double> timer;

  int it = RealIterationNum();
  const uint32_t queue_mask = main_queue()->size - 1;

  //queue should be empty
  ASSERT_EQ(hsa_queue_load_read_index_scacquire(main_queue()),
            hsa_queue_load_write_index_scacquire(main_queue()));

  void *q_base_addr = main_queue()->base_address;
  for (int i = 0; i < it; i++) {
    //Obtain the current queue write index.
    uint64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

    ASSERT_LT(index, main_queue()->size + index);

    //Write the aql packet at the calculated queue index address.

    ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask] = aql();

    //Get timing stamp and ring the doorbell to dispatch the kernel.
    rocrtst::PerfTimer p_timer;
    int id = p_timer.CreateTimer();
    p_timer.StartTimer(id);
    ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask].header |=
                      HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;
    hsa_signal_store_screlease(main_queue()->doorbell_signal, index);

    //Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;

    p_timer.StopTimer(id);

    timer.push_back(p_timer.ReadTimer(id));
    hsa_signal_store_screlease(signal(), 1);

#ifdef DEBUG
    std::cout << ".";
    fflush(stdout);
#endif
  }

  std::cout << std::endl;

  //Abandon the first result and after sort, delete the last 2% value
  timer.erase(timer.begin());
  std::sort(timer.begin(), timer.end());

  timer.erase(timer.begin() + num_iteration(), timer.end());

  if (use_default_) {
    single_default_mean_ = rocrtst::CalcMean(timer);
  }
  else {
    single_interrupt_mean_ = rocrtst::CalcMean(timer);
  }

  return;
}

void DispatchTime::RunMulti() {
  std::vector<double> timer;
  int it = RealIterationNum();
  const uint32_t queue_mask = main_queue()->size - 1;

  //queue should be empty
  ASSERT_EQ(hsa_queue_load_read_index_scacquire(main_queue()),
            hsa_queue_load_write_index_scacquire(main_queue()));

  for (int i = 0; i < it; i++) {
    uint64_t* index = (uint64_t*) malloc(sizeof(uint64_t) * num_batch_);

    hsa_signal_store_screlease(signal(), num_batch_);

    for (uint32_t j = 0; j < num_batch_; j++) {
      //index[j] = hsa_queue_add_write_index_scacq_screl(main_queue(), 1);
      index[j] = hsa_queue_add_write_index_relaxed(main_queue(), 1);

      //Write the aql packet at the calculated queue index address.
      ((hsa_kernel_dispatch_packet_t*) (main_queue()->base_address))[index[j]
          & queue_mask] = aql();

      if (j == num_batch_ - 1) {
        ((hsa_kernel_dispatch_packet_t*) (main_queue()->base_address))[index[j]
            & queue_mask].header |= 1 << HSA_PACKET_HEADER_BARRIER;

        //TODO: verify if the below is needed. I don't think it is. It should
        // already be initialized to signal().
        ((hsa_kernel_dispatch_packet_t*) (main_queue()->base_address))[index[j]
            & queue_mask].completion_signal = signal();
      }
    }

    // Set packet header reversly; set all headers except the very first
    // one, for now.
    for (uint32_t j = num_batch_ - 1; j > 0; j--) {

      ((hsa_kernel_dispatch_packet_t*) (main_queue()->base_address))[index[j]
          & queue_mask].header |= HSA_PACKET_TYPE_KERNEL_DISPATCH
                                  << HSA_PACKET_HEADER_TYPE;
    }

    //Get timing stamp and ring the doorbell to dispatch the kernel.
    rocrtst::PerfTimer p_timer;
    int id = p_timer.CreateTimer();
    p_timer.StartTimer(id);
    //Set the very first header...
    ((hsa_kernel_dispatch_packet_t*) (main_queue()->base_address))[index[0]
        & queue_mask].header |= HSA_PACKET_TYPE_KERNEL_DISPATCH
                                << HSA_PACKET_HEADER_TYPE;

    for (uint32_t j = 0; j < num_batch_; j++) {
      hsa_signal_store_screlease(main_queue()->doorbell_signal, index[j]);
    }

    //Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_EQ, 0,
                                     UINT64_MAX, HSA_WAIT_STATE_ACTIVE) != 0)
      ;

    p_timer.StopTimer(id);

    timer.push_back(p_timer.ReadTimer(id));
    hsa_signal_store_screlease(signal(), 1);

    free(index);

#ifdef DEBUG
    std::cout << ".";
    fflush(stdout);
#endif
  }

  std::cout << std::endl;

  // Abandon the first result and after sort, delete the last 2% value
  timer.erase(timer.begin());
  std::sort(timer.begin(), timer.end());

  timer.erase(timer.begin() + num_iteration(), timer.end());

  if (use_default_) {
    multi_default_mean_ = rocrtst::CalcMean(timer);
  }
  else {
    multi_interrupt_mean_ = rocrtst::CalcMean(timer);
  }

  return;
}

void DispatchTime::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  std::cout << "===================================================="
            << std::endl;

  if (use_default_) {
    if (launch_single_) {
      std::cout << "Single_Default:       " << single_default_mean_ * 1e6
                << std::endl;
    }
    else {
      std::cout << "Multi_Default:         "
                << multi_default_mean_ * 1e6 / num_batch_ << std::endl;
    }
  }
  else {
    if (launch_single_) {
      std::cout << "Single_Interrupt:       " << single_interrupt_mean_ * 1e6
                << std::endl;
    }
    else {
      std::cout << "Multi_Interrupt:         "
                << multi_interrupt_mean_ * 1e6 / num_batch_ << std::endl;
    }
  }

  std::cout << "====================================================="
            << std::endl;

  return;
}

void DispatchTime::Close() {
  hsa_status_t err;

  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  return;
}
