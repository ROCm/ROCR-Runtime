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

#include "memory_allocation.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "hsa/hsa.h"
#include "gtest/gtest.h"
#include <algorithm>

MemoryAllocation::MemoryAllocation(uint32_t num_iters) :
  BaseRocR(), allocation_time_ {0.0}, mem_pool_flag_(0) {
  ptr = NULL;
}

MemoryAllocation::~MemoryAllocation() {

}

const char* MemoryAllocation::Str[16] = {"64K", "128K", "256K", "512K", "1M",
                                         "2M", "4M", "8M", "16M", "32M",
                                         "64M", "128M", "256M", "512M", "1G",
                                         "2G" 
                                        };
const size_t MemoryAllocation::Size[16] = {64*1024, 128*1024,
                                           256*1024,512*1024, 1024*1024,
                                           2048*1024, 4096*1024, 8*1024*1024,
                                           16*1024*1024, 32*1024*1024,
                                           64*1024*1024, 128*1024*1024,
                                           256 * 1024*1024, 512*1024*1024,
                                           1024*1024*1024,
                                           (size_t)2*1024*1024*1024
                                          };

void MemoryAllocation::SetUp() {
  hsa_status_t err;

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  hsa_agent_t* cpu_dev = cpu_device();

  err = hsa_amd_agent_iterate_memory_pools(*cpu_dev, rocrtst::FindGlobalPool,
                                                                  &cpu_pool());

  EXPECT_EQ(err, HSA_STATUS_INFO_BREAK);

  if (err != HSA_STATUS_INFO_BREAK) {
    std::cout << "Unable to find global pool. Test will not be run."
              << std::endl;
    return;
  }

  //At this point, cpu_pool() should be in the global segment
  err = hsa_amd_memory_pool_get_info(cpu_pool(),
         (hsa_amd_memory_pool_info_t) HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS,
                                                             &mem_pool_flag_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void MemoryAllocation::Run() {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  if (cpu_pool().handle == 0) {
    return;
  }

  size_t iterations = RealIterationNum();
  hsa_status_t err;

  //Iterate over the different data size
  for (int i = 0; i < 16; i++) {
    std::vector<double> time;

    for (uint32_t it = 0; it < iterations; it++) {
#if DEBUG
      std::cout << "." << std::flush;
#endif

      rocrtst::PerfTimer allocation_timer;
      int index = allocation_timer.CreateTimer();

      allocation_timer.StartTimer(index);
      err = hsa_amd_memory_pool_allocate(cpu_pool(), Size[i], 0, &ptr);
      allocation_timer.StopTimer(index);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      //Free the memory which was allocated
      err = hsa_amd_memory_pool_free(ptr);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);
      ptr = NULL;

      // PUsh the results back to vector time
      time.push_back(allocation_timer.ReadTimer(index));
    }

#if DEBUG
    std::cout << std::endl;
#endif

    //Get mean copy time and store to the array
    allocation_time_[i] = GetMeanTime(time);
  }
}

size_t MemoryAllocation::RealIterationNum() {
  return num_iteration() * 1.2 + 1;
}

double MemoryAllocation::GetMeanTime(std::vector<double>& vec) {
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

void MemoryAllocation::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  fprintf(stdout, "==============================================\n");
  fprintf(stdout, "  Data Size  Allocation_time   BandWidth(GB/s)\n");

  for (int i = 0; i < 16; i++) {
    fprintf(stdout, "  %9s  %15.6f   %15.6f\n", Str[i], allocation_time_[i],
            2 * Size[i] / allocation_time_[i] / 1024 / 1024 / 1024);
  }

  fprintf(stdout, "==============================================\n");

  return;
}

void MemoryAllocation::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  return;
}
