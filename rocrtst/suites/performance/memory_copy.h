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

#ifndef __ROCRTST_SRC_MEMORY_MEM_COPY_H__
#define __ROCRTST_SRC_MEMORY_MEM_COPY_H__

#include "common/base_rocr.h"
#include "perf_common/perf_base.h"
#include "hsa/hsa.h"
#include "common/hsatimer.h"
#include <vector>

class MemoryCopy: public rocrtst::BaseRocR, public PerfBase {

 public:
  //@Brief: Constructor for test case of MemoryCopy
  MemoryCopy(size_t num = 100);

  //@Brief: Destructor for test case of MemoryCopy
  virtual ~MemoryCopy();

  //@Brief: Setup the environment for measurement
  virtual void SetUp();

  //@Brief: Core measurement execution
  virtual void Run();

  //@Brief: Clean up and retrive the resource
  virtual void Close();

  //@Brief: Display  results
  virtual void DisplayResults() const;

 private:
  //@Brief: Define copy data size and corresponding string
  static const size_t Size[16];
  static const char* Str[16];

  //@Brief: Get real iteration number
  virtual size_t RealIterationNum();

  //@Brief: Get the mean copy time
  virtual double GetMeanTime(std::vector<double>& vec);

 protected:
  //@Brief: More variables declared for testing
  //@Brief: Source pointer from which data copy
  void* ptr_src_;

  //@Brief: Destination pointer to which data copy
  void* ptr_dst_;

  //@Brief: Pointer to device memory
  void* ptr_dev_src_;
  void* ptr_dev_dst_;

  //@Brief: Array to store the timer results for each data size
  std::vector<double> sys2sys_copy_time_;
  std::vector<double> sys2dev_copy_time_;
  std::vector<double> dev2sys_copy_time_;
  std::vector<double> dev2dev_copy_time_;

  //@Brief: Device memory region
  hsa_region_t device_region_;
};

#endif
