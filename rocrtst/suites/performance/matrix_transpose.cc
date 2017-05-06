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

#include "matrix_transpose.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"
#include "hsa/hsa_ext_finalize.h"
#include "gtest/gtest.h"
#include <stdlib.h>
#include <algorithm>

static const unsigned int NUM_BLOCK_SIZES = 2;
static const unsigned int blockSizes[NUM_BLOCK_SIZES] = {8, 16};
static const unsigned int NUM_MATRIX_DIMS = 2;
static const unsigned int matrixDims[NUM_MATRIX_DIMS] = {1024, 64};

MatrixTranspose::MatrixTranspose(void) :
  BaseRocR() {
  in_buffer_sys_ = NULL;
  out_buffer_sys_ = NULL;
  in_buffer_ = NULL;
  out_buffer_ = NULL;
  width_ = 0;
  height_ = 0;
  buf_size_ = 0;
  block_size_ = 0;
  time_mean_ = 0.0;
}

MatrixTranspose::~MatrixTranspose(void) {

}

void MatrixTranspose::SetUp(void) {
  hsa_status_t err;

  InitializeData();

  set_kernel_file_name("transpose_kernel.o");
  set_kernel_name("&__OpenCL_matrixTranspose_kernel");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();
  hsa_agent_t* cpu_dev = cpu_device();

  err = hsa_amd_agent_iterate_memory_pools(*cpu_dev, rocrtst::FindGlobalPool,
                                                                  &cpu_pool());
  ASSERT_EQ(err, HSA_STATUS_INFO_BREAK);

  err = hsa_amd_memory_pool_allocate(cpu_pool(), buf_size_, 0,
                                     (void**) &in_buffer_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(cpu_pool(), buf_size_, 0,
                                     (void**) &out_buffer_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_agents_allow_access(1, gpu_dev, NULL, in_buffer_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_agents_allow_access(1, gpu_dev, NULL, out_buffer_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Create a queue
  hsa_queue_t* q = nullptr;
  rocrtst::CreateQueue(*gpu_dev, &q);
  set_main_queue(q);

  rocrtst::LoadKernelFromObjFile(this);

  // Fill up aql packet
  rocrtst::InitializeAQLPacket(this, &aql());
  aql().setup = 0;
  aql().setup |= 2 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
  aql().workgroup_size_x = block_size_;
  aql().workgroup_size_y = block_size_;
  aql().grid_size_x = width_;
  aql().grid_size_y = height_;
  aql().group_segment_size = sizeof(uint) * block_size_ * block_size_;

  // Debug
#ifdef DEBUG
  std::cout << "workgroup size: " << block_size_ << ", " << block_size_
            << ", " << 1 << std::endl;
  std::cout << "grid size: " << aql().grid_size_x << ", " <<
            aql().grid_size_y << ", " << aql().grid_size_z << std::endl;
  std::cout << "group segment size: " << aql().group_segment_size << std::endl;
#endif
}

void MatrixTranspose::Run(void) {
  hsa_status_t err;
  hsa_agent_t* gpu_dev = gpu_device1();

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  // Allocate kernel parameter
  typedef struct args_t {
    uint* offset_0;
    uint* offset_1;
    uint* offset_2;
    uint* printf_buffer;
    uint* vqueue_buffer;
    uint* aqlwrap_pointer;

    uint* in_buf;
    uint* out_buf;
    uint* local_buf;
    uint iblock_size;
    uint iwidth;
    uint iheight;
  } args;

  args* kern_ptr = NULL;
  err = hsa_amd_memory_pool_allocate(cpu_pool(), sizeof(args), 0,
                                     (void**) &kern_ptr);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_agents_allow_access(1, gpu_dev, NULL, kern_ptr);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  kern_ptr->offset_0 = 0;
  kern_ptr->offset_1 = 0;
  kern_ptr->offset_2 = 0;
  kern_ptr->printf_buffer = 0;
  kern_ptr->vqueue_buffer = 0;
  kern_ptr->aqlwrap_pointer = 0;

  kern_ptr->in_buf = in_buffer_sys_;
  kern_ptr->out_buf = out_buffer_sys_;
  kern_ptr->local_buf = 0;
  kern_ptr->iblock_size = block_size_;
  kern_ptr->iwidth = width_;
  kern_ptr->iheight = height_;

  aql().kernarg_address = kern_ptr;

  //Obtain the current queue write index.
  uint64_t idx = hsa_queue_add_write_index_relaxed(main_queue(), 1);

  ((hsa_kernel_dispatch_packet_t*)(main_queue()->base_address))[idx] = aql();

  rocrtst::PerfTimer p_timer;
  int id = p_timer.CreateTimer();
  p_timer.StartTimer(id);

  ((hsa_kernel_dispatch_packet_t*)(main_queue()->base_address))[idx].header |=
                     HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;

  hsa_signal_store_release(main_queue()->doorbell_signal, idx);

  //Wait on the dispatch signal until the kernel is finished.
  hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                       (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE);
  p_timer.StopTimer(id);

  hsa_amd_profiling_dispatch_time_t dispatch_time;
  err = hsa_amd_profiling_get_dispatch_time(*gpu_dev, signal(), &dispatch_time);

  uint64_t stamp = dispatch_time.end - dispatch_time.start;
  uint64_t freq;

  err = hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY, &freq);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  std::cout << "Kernel time is: " <<
            (double) stamp / (double) freq * 1000.0 << std::endl;
  hsa_signal_store_release(signal(), 1);


  // Verify Results
  VerifyResults (out_buffer_sys_);

  // Abandon the first result which is warm up

  time_mean_ = p_timer.ReadTimer(id); //rocrtst::CalcMean(timer);
}

void MatrixTranspose::DisplayResults(void) const {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  std::cout << "============================================" << std::endl;
  std::cout << "Matrix Transpose Mean Time:       " << time_mean_ << std::endl;

  return;
}

void MatrixTranspose::Close(void) {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void MatrixTranspose::InitializeData(void) {
  // int openTest = 1;
  block_size_ = 16; //blockSizes[openTest % NUM_BLOCK_SIZES];
  width_ = 1920; //matrixDims[openTest / NUM_BLOCK_SIZES];
  height_ = width_;

  buf_size_ = width_ * height_ * sizeof(uint);

  in_buffer_sys_ = (uint*) aligned_alloc(256, buf_size_);

  SetData (in_buffer_sys_);
  out_buffer_sys_ = (uint*) aligned_alloc(256, buf_size_);

  FillData(out_buffer_sys_, 0xdeadbeef);

  return;
}

void MatrixTranspose::SetData(uint* buffer) {
  for (unsigned int i = 0; i < height_; i++) {
    for (unsigned int j = 0; j < width_; j++) {
      *(buffer + i * width_ + j) = i * width_ + j;
    }
  }
}

void MatrixTranspose::FillData(uint* buffer, unsigned int val) {
  for (unsigned int i = 0; i < width_ * height_; i++) {
    buffer[i] = val;
  }
}

void MatrixTranspose::VerifyResults(uint* buffer) {
  bool err = false;

  for (unsigned int i = 0; (i < width_) && !err; i++) {
    for (unsigned int j = 0; (j < height_) && !err; j++) {
      ASSERT_EQ(*(buffer + i * height_ + j), j * width_ + i);
    }
  }

  std::cout << "PASSED!" << std::endl;
}
