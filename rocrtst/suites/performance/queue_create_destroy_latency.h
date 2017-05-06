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

#ifndef __ROCRTST_SRC_INC_QUEUE_CREATE_DESTROY_LATENCY_H__
#define __ROCRTST_SRC_INC_QUEUE_CREATE_DESTROY_LATENCY_H__

#include "perf_common/perf_base.h"
#include "common/base_rocr.h"
#include "hsa/hsa.h"
#include <vector>

class QueueLatency: public rocrtst::BaseRocR, public PerfBase {
 public:
  //@Brief: Constructor
  QueueLatency();

  //@Brief: Destructor
  ~QueueLatency();

  //@Brief: Set up the teset environment
  virtual void SetUp();

  //@Brief: Run the test
  virtual void Run();

  //@Brief: Clean up and close the test
  virtual void Close();

  //@Brief: Display  results
  virtual void DisplayResults() const;

 private:
  //@Brief: A vector to store the pointers to multiple queues
  std::vector<hsa_queue_t*> queues_;

  //@Brief: Variable to store the mean time for both queue construction
  //  and destruction
  std::vector<double> construction_mean_;
  std::vector<double> destruction_mean_;

  //@Brief: Variable to store the max number of queue which are active for
  // device_
  uint32_t max_queue_;

  //@Brief: Pointer which points to original and destination vector memory
  // space
  uint8_t* in_;
  uint8_t* out_;

};

#endif //__ROCRTST_SRC_INC_QUEUE_CREATE_DESTROY_LATENCY_H__

