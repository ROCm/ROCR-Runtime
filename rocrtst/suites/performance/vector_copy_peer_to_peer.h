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

#ifndef __ROCRTST_SRC_VECTOR_COPY_P2P_H__
#define __ROCRTST_SRC_VECTOR_COPY_P2P_H__

#include "perf_common/perf_base.h"
#include "common/base_rocr.h"
#include "common/common.h"
#include "common/hsatimer.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"
#include "hsa/hsa_ext_finalize.h"
#include <algorithm>
#include <vector>

//@Brief: This class is defined to measure the mean latency of launching
//an empty kernel

class VectorCopyP2P: public rocrtst::BaseRocR, public PerfBase {
 public:
  //@Brief: Constructor
  VectorCopyP2P();

  //@Brief: Destructor
  virtual ~VectorCopyP2P();

  //@Brief: Set up the environment for the test
  virtual void SetUp();

  //@Brief: Run the test case
  virtual void Run();

  //@Brief: Display  results we got
  virtual void DisplayResults() const;

  //@Brief: Clean up and close the runtime
  virtual void Close();

 private:
  //@Brief: Get actual iteration number
  virtual size_t RealIterationNum();

  //@Brief: Create Queue
  virtual void CreateQueue();

  //@Brief: Store the size of queue
  uint32_t queue_size_;

  //@Brief: The mean time of CP Processing
  double mean_;

  //@Brief: The group memory region
  hsa_region_t group_region_;

  //@Brief: Pointer to cu_id array
  uint32_t* cu_;

  uint32_t manual_input;
  uint32_t group_input;
};

#endif

