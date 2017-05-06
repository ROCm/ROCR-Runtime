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

#ifndef __ROCRTST_SRC_MEMORY_MEM_ALLOCATION_H__
#define __ROCRTST_SRC_MEMORY_MEM_ALLOCATION_H__

#include "perf_common/perf_base.h"
#include "common/base_rocr.h"
#include "common/hsatimer.h"
#include "hsa/hsa.h"
#include <vector>

class MemoryAllocation: public rocrtst::BaseRocR, public PerfBase {

 public:
  //@Brief: Constructor for test case of MemoryAllocation
  MemoryAllocation(uint32_t num_iters = 100);

  //@Brief: Destructor for test case of MemoryAllocation
  virtual ~MemoryAllocation();

  //@Brief: Set up the environment for the test
  virtual void SetUp();

  //@Brief: Execute the test
  virtual void Run();

  //@Brief: Display  results
  virtual void DisplayResults() const;

  //@Brief: Clean up and close the environment
  virtual void Close();

 protected:
  //@Brief: Pointer to the memory space which is allocated by HSA Memory
  // allocation API
  void* ptr;

  //@Brief: Array to store the timers results for each data size
  double allocation_time_[16];

 private:
  //@Brief: Define allocated data size and corresponding string
  static const size_t Size[16];
  static const char* Str[16];

  uint32_t mem_pool_flag_;

  //@Brief: Get the actual iteration number
  size_t RealIterationNum();

  //@Brief: Get mean execution time
  double GetMeanTime(std::vector<double>& vec);

};
#endif
