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

#ifndef __ROCRTST_SRC_DISPATCH_TIME_H__
#define __ROCRTST_SRC_DISPATCH_TIME_H__
#include "perf_common/perf_base.h"
#include "common/base_rocr.h"
#include "common/common.h"
#include "hsa/hsa.h"
#include <vector>

//@Brief: This class is defined to measure the mean latency of launching
//an empty kernel

class DispatchTime: public rocrtst::BaseRocR, public PerfBase {
 public:
  //@Brief: Constructor
  DispatchTime();

  //@Brief: Destructor
  virtual ~DispatchTime();

  //@Brief: Set up the environment for the test
  virtual void SetUp();

  //@Brief: Run the test case
  virtual void Run();

  //@Brief: Display  results we got
  virtual void DisplayResults() const;

  //@Brief: Clean up and close the runtime
  virtual void Close();

  //@Brief: Choose if use default signal or not
  void UseDefaultSignal(bool use_default = true) {
    use_default_ = use_default;
  }

  //@Brief; Choose to launch a single kernels or not
  void LaunchSingleKernel(bool launch_single = true) {
    launch_single_ = launch_single;
  }

 private:
  //@Brief: Get actual iteration number
  virtual size_t RealIterationNum();

  //@Brief: Launch single packet each time
  virtual void RunSingle();

  //@Brief: Launch multiple packets each time
  virtual void RunMulti();

  //@Brief: Indicate if use default signal or not
  bool use_default_;

  //@Brief: Indicate if launch single kernel or not
  bool launch_single_;

  //@Brief: Store the size of queue
  uint32_t queue_size_;

  //@Brief: Number of packets in a batch
  uint32_t num_batch_;

  //@Brief: Time of single default signal dispatch time
  double single_default_mean_;

  //@Brief: Time of single interrupt signal dispatch time
  double single_interrupt_mean_;

  //@Brief: Time of multi default signal dispatch time
  double multi_default_mean_;

  //@Brief: Time of multi interrupt signal dispatch time
  double multi_interrupt_mean_;

  char* orig_iterrupt_env_;
};

#endif

