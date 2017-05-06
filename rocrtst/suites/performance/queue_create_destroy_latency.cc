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

#include "queue_create_destroy_latency.h"
#include "common/hsatimer.h"
#include "common/common.h"
#include "common/base_rocr_utils.h"
#include "common/helper_funcs.h"
#include "hsa/hsa_ext_amd.h"
#include "hsa/hsa_ext_finalize.h"
#include "gtest/gtest.h"
#include <stdio.h>

static const int kGridDimension = 1024;

// Construct the test case class
QueueLatency::QueueLatency() :
  BaseRocR() {
  max_queue_ = 0;
  in_ = NULL;
  out_ = NULL;
}

// Destruct the test case claa
QueueLatency::~QueueLatency() {

}

void QueueLatency::Close() {
  hsa_memory_free (in_);
  hsa_memory_free (out_);

  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  return;
}

// Set up the environment
void QueueLatency::SetUp() {
  hsa_status_t err;

  // We get hangs with vector_copy
  set_kernel_file_name("vector_copy.o");
  set_kernel_name("&__vector_copy_kernel");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();
  hsa_agent_t* cpu_dev = cpu_device();

  // Get the max queue which can be active for GPU device
  err = hsa_agent_get_info(*gpu_dev, HSA_AGENT_INFO_QUEUES_MAX, &max_queue_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Find system coarse grained region
  err = hsa_amd_agent_iterate_memory_pools(*cpu_dev, rocrtst::FindGlobalPool,
                                                                   &cpu_pool());
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  size_t pool_size;
  err = hsa_amd_memory_pool_get_info(cpu_pool(), HSA_AMD_MEMORY_POOL_INFO_SIZE,
                                                                    &pool_size);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(cpu_pool(),
                                     kGridDimension * kGridDimension * 4, 0,
                                                                (void**) &in_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(cpu_pool(),
                                     kGridDimension * kGridDimension * 4, 0,
                                                               (void**) &out_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  //rocrtst::LoadKernelFromObjFile(gpu_dev, "./"+ kernel_file_name() + ".o");
  rocrtst::LoadKernelFromObjFile(this);

  // Fill up the aql packet
  rocrtst::InitializeAQLPacket(this, &aql());
  aql().grid_size_x = kGridDimension * kGridDimension;

  // rocrtst::CommonCleanUp vector memory and register them
  //memset(in_, 1, kGridDimension*kGridDimension * 4);

  err = hsa_amd_memory_fill(in_, 1, kGridDimension * kGridDimension * 4);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  return;
}

void QueueLatency::Run() {
  hsa_agent_t* gpu_dev = gpu_device1();
  hsa_status_t err;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  // The outer for loop iterator represents the predefined queue number
  // After creating a queue, launch a kernel to train the queue, then destroy
  // TODO:Hardcode max_queue_ to 100
  max_queue_ = 20;

  for (uint32_t pre_defined_num = 0; pre_defined_num < max_queue_;
       pre_defined_num++) {
#ifdef DEBUG
    std::cout << "Existing queue number: " << pre_defined_num << std::endl;
#endif
    // vector to store the creation and destruction time
    std::vector<double> creation;
    std::vector<double> destruction;
    // Create pre_defined_num queues first
    hsa_queue_t* q;

    for (uint32_t i = 0; i < pre_defined_num; i++) {
      q = main_queue();
      rocrtst::CreateQueue(*gpu_dev, &q);

      queues_.push_back(q);
    }

    for (uint32_t i = 0; i < num_iteration(); i++) {
      rocrtst::PerfTimer p_timer;
      int id = p_timer.CreateTimer();

      uint32_t size = 0;
      err = hsa_agent_get_info(*gpu_dev, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &size);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      p_timer.StartTimer(id);
      hsa_queue_t* q = main_queue();

      err = hsa_queue_create(*gpu_dev, size, HSA_QUEUE_TYPE_MULTI, NULL, NULL,
                             UINT32_MAX, UINT32_MAX, &q);
      p_timer.StopTimer(id);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      creation.push_back(p_timer.ReadTimer(id));

      p_timer.ResetTimer(id);

      // Launch a kernel to the currently created queue
      // Allocate kernel parameter
      typedef struct args_t {
        void* in_buf;
        void* out_buf;
      } args;

      args* kern_ptr = NULL;
      err = hsa_amd_memory_pool_allocate(cpu_pool(), sizeof(args), 0,
                                         (void**) &kern_ptr);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      kern_ptr->in_buf = in_;
      kern_ptr->out_buf = out_;

      aql().kernarg_address = kern_ptr;

      // Obtain the current queue write index.
      uint64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

      // Write the aql packet at the calculated queue index address.
      const uint32_t queue_mask = main_queue()->size - 1;
      ((hsa_kernel_dispatch_packet_t*) (main_queue()->base_address))[index
          & queue_mask] = aql();

      ((hsa_kernel_dispatch_packet_t*) (main_queue()->base_address))[index
          & queue_mask].header |= HSA_PACKET_TYPE_KERNEL_DISPATCH
                                  << HSA_PACKET_HEADER_TYPE; 
      hsa_signal_store_screlease(main_queue()->doorbell_signal, index);

      // Wait on the dispatch signal until the kernel is finished.
      while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                       (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
        ;

      hsa_signal_store_screlease(signal(), 1);

      // Destroy the queue and record the timer
      p_timer.StartTimer(id);
      err = hsa_queue_destroy(main_queue());
      p_timer.StopTimer(id);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      destruction.push_back(p_timer.ReadTimer(id));

    }

#ifdef DEBUG
    std::cout << std::endl;
#endif

    // Destroy the predefined queue
    for (uint32_t i = 0; i < pre_defined_num; i++) {

      ASSERT_EQ(queues_.size(), pre_defined_num);

      err = hsa_queue_destroy(queues_[i]);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);
    }

    // Clear the queue vector
    queues_.clear();

    // Get the mean creation and detruction time and push back
    double creation_mean = rocrtst::CalcMean(creation);
    double destruction_mean = rocrtst::CalcMean(destruction);
    construction_mean_.push_back(creation_mean);
    destruction_mean_.push_back(destruction_mean);
  }
}

void QueueLatency::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  printf("======================================================\n");
  printf(" Existing queue#        Creation        Destroy\n");

  for (uint32_t i = 0; i < max_queue_; i++) {
    printf("      %d,         %fms          %fms\n", i,
           construction_mean_[i] * 1e3, destruction_mean_[i] * 1e3);
  }
}
