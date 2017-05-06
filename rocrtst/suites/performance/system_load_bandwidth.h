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

#ifndef __ROCRTST_SRC_INC_SYSTEM_LOAD_BANDWIDTH_H__
#define __ROCRTST_SRC_INC_SYSTEM_LOAD_BANDWIDTH_H__

#include "perf_common/perf_base.h"
#include "common/base_rocr.h"
#include "hsa/hsa.h"
#include <stdio.h>

class SystemLoadBandwidth: public rocrtst::BaseRocR, public PerfBase {
 public:
  //@Brief: Constructor
  SystemLoadBandwidth();

  //@Brief: Destructor
  ~SystemLoadBandwidth();

  //@Brief: Set up the testing environment
  virtual void SetUp();

  //@Brief: Run the test case
  virtual void Run();

  //@Brief: Close and clean up  the test enrionment
  virtual void Close();

  //@Brief: Display  load bandwidth
  virtual void DisplayResults() const;

  //@Brief: Set work-item configuration
  void SetWorkItemNum() {
#ifdef INTERACTIVE
    uint32_t tmp;
    printf("Please input the number of CUs you want to try:\n");
    scanf("%d", &num_cus_);

    printf("Please input the number of groups you want to try:\n");
    scanf("%d", &num_group_);

    printf("Please input the size of each group:\n");
    uint32_t sz = 0;
    scanf("%d", &tmp);
    set_group_size(tmp);

    printf("Please input the number of kernel loop you want to try:\n");
    scanf("%d", &kernel_loop_count_);
#else
    num_cus_ = 32;
    num_group_ = 128;
    set_group_size(256);
    kernel_loop_count_ = 16;
#endif
    return;
  }

 private:

  //@Brief: number of group
  uint32_t num_group_;

  //@Brief: number of CUs
  uint32_t num_cus_;

  //@Brief: number of kernel loop
  uint32_t kernel_loop_count_;

  //@Brief: Mean execution time
  double mean_;

  //@Brief: data size for test
  uint64_t data_size_;
};

#endif

