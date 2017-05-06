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

#include "memory_copy.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "hsa/hsa.h"
#include "gtest/gtest.h"
#include <algorithm>

MemoryCopy::MemoryCopy(size_t num) :
  BaseRocR() {
  ptr_src_ = NULL;
  ptr_dst_ = NULL;
  ptr_dev_src_ = NULL;
  ptr_dev_dst_ = NULL;
  device_region_.handle = 0;
  set_requires_profile (HSA_PROFILE_BASE);
}

MemoryCopy::~MemoryCopy() {
}

const char* MemoryCopy::Str[16] = {"64K", "128K", "256K", "512K", "1M", "2M",
                                   "4M", "8M", "16M", "32M", "64M", "128M",
                                   "256M", "512M", "1G", "2G"
                                  };
const size_t MemoryCopy::Size[16] = {64*1024, 128*1024, 256*1024, 512*1024,
                                     1024*1024, 2048*1024, 4096*1024,
                                     8*1024*1024, 16*1024* 1024, 32*1024*1024,
                                     64*1024*1024, 128*1024*1024, 256*1024*1024,
                                     512*1024*1024, 1024*1024*1024,
                                     (size_t)2*1024*1024* 1024
                                    };


void MemoryCopy::SetUp() {
  hsa_status_t err;

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();
  hsa_agent_t* cpu_dev = cpu_device();

  // Find system memory pool for kernarg allocation.
  // hsa_amd_memory_pool_t sys_coarse_grained_pool;
  err = hsa_amd_agent_iterate_memory_pools(*cpu_dev, rocrtst::FindGlobalPool,
        &cpu_pool());
  ASSERT_EQ(err, HSA_STATUS_INFO_BREAK);

  ASSERT_NE(cpu_pool().handle, 0);

  // Get local memory pool of the first GPU.
  // hsa_amd_memory_pool_t gpu_pool_;
  err = hsa_amd_agent_iterate_memory_pools(*gpu_dev, rocrtst::FindStandardPool,
        &device_pool());
  ASSERT_EQ(err, HSA_STATUS_INFO_BREAK);
  ASSERT_NE(device_pool().handle, 0);

  //Allocate buffers whose size is 2GB
  err = hsa_amd_memory_pool_allocate(cpu_pool(), Size[12], 0, &ptr_src_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(cpu_pool(), Size[12], 0, &ptr_dst_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(device_pool(), Size[11], 0, &ptr_dev_src_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(device_pool(), Size[11], 0, &ptr_dev_dst_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  //Assign the region ownership to GPU
  err = hsa_memory_assign_agent(ptr_dev_src_, *gpu_dev,
                                HSA_ACCESS_PERMISSION_RW);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_memory_assign_agent(ptr_dev_dst_, *gpu_dev,
                                HSA_ACCESS_PERMISSION_RW);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  //rocrtst::CommonCleanUp the two buffer, src to 1 each byte and dst to 0
  err = hsa_amd_memory_fill(ptr_src_, 1, Size[12]);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  //Check if the initialization is correct
#if DEBUG
  std::cout << "Value after setting source buffer is: "
            << (int)((uint8_t*)ptr_src_)[0] << std::endl;
#endif

  return;
}

void MemoryCopy::Run() {
  hsa_status_t err;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  uint32_t iterations = RealIterationNum();

  //Iteration over the different data size on system memory
  for (int i = 0; i < 13; i++) {
    std::vector<double> time;

    for (uint32_t it = 0; it < iterations; it++) {
#if DEBUG
      std::cout << ".";
      fflush(stdout);
#endif

      rocrtst::PerfTimer copy_timer;
      int index = copy_timer.CreateTimer();

      copy_timer.StartTimer(index);
      err = hsa_memory_copy(ptr_dst_, ptr_src_, Size[i]);
      copy_timer.StopTimer(index);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      // Push the result back to vector time
      time.push_back(copy_timer.ReadTimer(index));

#if DEBUG
      //Check if the data copied is correct
      uint8_t* temp_ptr = (uint8_t*)ptr_dst_;

      for (uint32_t j = 0; j < Size[i]; j++) {
        ASSERT_EQ(temp_ptr[j], 1);
      }

#endif
    }

#if DEBUG
    std::cout << std::endl;
#endif

    //Get mean copy time and store to the array
    sys2sys_copy_time_.push_back(GetMeanTime(time));
  }

  //Copy from system memory to device memory
  for (int i = 0; i < 12; i++) {
    std::vector<double> time;

    for (uint32_t it = 0; it < iterations; it++) {
#if DEBUG
      std::cout << ".";
      fflush(stdout);
#endif

      rocrtst::PerfTimer copy_timer;
      int index = copy_timer.CreateTimer();

      copy_timer.StartTimer(index);
      err = hsa_memory_copy(ptr_dev_src_, ptr_src_, Size[i]);
      copy_timer.StopTimer(index);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      // Push the result back to vector time
      time.push_back(copy_timer.ReadTimer(index));

#if DEBUG
      //Check if the data copied is correct
      uint8_t* temp_ptr = (uint8_t*)ptr_dst_;

      for (uint32_t j = 0; j < Size[i]; j++) {
        ASSERT_EQ(temp_ptr[j], 1);
      }

#endif
    }

#if DEBUG
    std::cout << std::endl;
#endif

    //Get mean copy time and store to the array
    sys2dev_copy_time_.push_back(GetMeanTime(time));
  }

  //Copy from device memory to device memory
  for (int i = 0; i < 12; i++) {
    std::vector<double> time;

    for (uint32_t it = 0; it < iterations; it++) {
#if DEBUG
      std::cout << ".";
      fflush(stdout);
#endif

      rocrtst::PerfTimer copy_timer;
      int index = copy_timer.CreateTimer();

      copy_timer.StartTimer(index);
      err = hsa_memory_copy(ptr_dev_dst_, ptr_dev_src_, Size[i]);
      copy_timer.StopTimer(index);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      // Push the result back to vector time
      time.push_back(copy_timer.ReadTimer(index));

#if DEBUG
      //Check if the data copied is correct
      uint8_t* temp_ptr = (uint8_t*)ptr_dst_;

      for (uint32_t j = 0; j < Size[i]; j++) {
        ASSERT_EQ(temp_ptr[j], 1);
      }

#endif
    }

#if DEBUG
    std::cout << std::endl;
#endif

    //Get mean copy time and store to the array
    dev2dev_copy_time_.push_back(GetMeanTime(time));
  }

  //Copy from device memory to system memory
  for (int i = 0; i < 12; i++) {
    std::vector<double> time;

    for (uint32_t it = 0; it < iterations; it++) {
#if DEBUG
      std::cout << ".";
      fflush(stdout);
#endif

      rocrtst::PerfTimer copy_timer;
      int index = copy_timer.CreateTimer();

      copy_timer.StartTimer(index);
      err = hsa_memory_copy(ptr_dst_, ptr_dev_src_, Size[i]);
      copy_timer.StopTimer(index);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      // Push the result back to vector time
      time.push_back(copy_timer.ReadTimer(index));

#if DEBUG
      //Check if the data copied is correct
      uint8_t* temp_ptr = (uint8_t*)ptr_dst_;

      for (uint32_t j = 0; j < Size[i]; j++) {
        if (temp_ptr[j] != 1) {
          ASSERT_EQ(temp_ptr[j], 1);
        }
      }

#endif
    }

#if DEBUG
    std::cout << std::endl;
#endif

    //Get mean copy time and store to the array
    dev2sys_copy_time_.push_back(GetMeanTime(time));
  }
}

size_t MemoryCopy::RealIterationNum() {
  return num_iteration() * 1.2 + 1;
}

double MemoryCopy::GetMeanTime(std::vector<double>& vec) {
  std::sort(vec.begin(), vec.end());

  vec.erase(vec.begin());
  vec.erase(vec.begin(), vec.begin() + num_iteration() * 0.1);
  vec.erase(vec.begin() + num_iteration(), vec.end());

  double mean = 0.0;
  int num = vec.size();

  for (int it = 0; it < num; it++) {
    //        printf("%f\n", vec[it]);
    mean += vec[it];
  }

  mean /= num;
  return mean;
}

void MemoryCopy::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  printf(
    "================ System to System ==================================\n");
  printf("  Data Size                      BandWidth(GB/s)\n");

  //Output the BW of system memory to system memory
  for (int i = 0; i < 13; i++) {
    double band_width = (double) Size[i] / sys2sys_copy_time_[i] / 1024 / 1024
                        / 1024 * 2;
#ifdef DEBUG
    printf("size: %zu      time: %f\n", Size[i], sys2sys_copy_time_[i]);
#endif
    printf("  %s                             %lf\n", Str[i], band_width);
  }

  printf(
    "================ System to Device ===================================\n");

  for (int i = 0; i < 12; i++) {
    double band_width = (double) Size[i] / sys2dev_copy_time_[i] / 1024 / 1024
                        / 1024 * 2;
#ifdef DEBUG
    printf("size: %zu      time: %f\n", Size[i], sys2dev_copy_time_[i]);
#endif
    printf("  %s                             %lf\n", Str[i], band_width);
  }

  printf(
    "================ Device to Device ===================================\n");

  for (int i = 0; i < 12; i++) {
    double band_width = (double) Size[i] / dev2dev_copy_time_[i] / 1024 / 1024
                        / 1024 * 2;
#ifdef DEBUG
    printf("size: %zu      time: %f\n", Size[i], dev2dev_copy_time_[i]);
#endif
    printf("  %s                             %lf\n", Str[i], band_width);
  }

  printf(
    "================ Device to System ===================================\n");

  for (int i = 0; i < 12; i++) {
    double band_width = (double) Size[i] / dev2sys_copy_time_[i] / 1024 / 1024
                        / 1024 * 2;
#ifdef DEBUG
    printf("size: %zu      time: %f\n", Size[i], dev2sys_copy_time_[i]);
#endif
    printf("  %s                             %lf\n", Str[i], band_width);
  }

  printf("===================================================\n");
  return;
}

void MemoryCopy::Close() {
  hsa_status_t err;

  //Free the memory allocated
  err = hsa_memory_free(ptr_src_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_memory_free(ptr_dst_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  ptr_src_ = NULL;
  ptr_dst_ = NULL;

  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  return;
}
