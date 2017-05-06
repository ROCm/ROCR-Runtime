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

/// \file
/// Contains counter related functionality that can be used by samples and
/// tests.
#ifndef ROCRTST_COMMON_HSA_PERF_CNTRS_H_
#define ROCRTST_COMMON_HSA_PERF_CNTRS_H_

#include "hsa/hsa.h"
#include "hsa/hsa_ext_profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <string>

namespace rocrtst {


typedef enum CntrValCnfType {
  ///< no counter value validation should be performed
  CntrValCnf_None,

  ///< counter value should be an exact match to expectedResult
  CntrValCnf_Exact,

  ///< counter value should be greater than expectedResult
  CntrValCnf_GreaterThan,

  ///< counter value should be less than expectedResult
  CntrValCnf_LessThan
} CntrValCnfType;

/// Struct used to encapsulate Counter Info
typedef struct CntrInfo {
  ///< Id of counter in hardware block
  uint32_t cntrId;

  ///< Name of counter
  char cntrName[72];

  ///< Handle of perf counter
  hsa_ext_tools_counter_t cntrHndl;

  ///< Id of hardware block containing the counter
  uint32_t blkId;

  ///< Handle of counter block
  hsa_ext_tools_counter_block_t blkHndl;

  ///< Expected value of perf counte
  uint64_t  expectedResult;

  ///< Value of perf counter expected
  uint64_t cntrResult;

  ///< Type of validation upon completion of dispatch
  CntrValCnfType cnfType;

  CntrInfo(uint32_t cntrId, const char* cntrName, void* cntrHndl,
           uint32_t blkId, void* blkHndl,
           uint64_t expResult, uint64_t result, CntrValCnfType cnfType) {
    this->cntrId = cntrId;
    this->cntrHndl = cntrHndl;
    this->blkId = blkId;
    this->blkHndl = blkHndl;
    this->expectedResult = expResult;
    this->cntrResult = result;
    this->cnfType = cnfType;
    memcpy(this->cntrName, cntrName, strlen(cntrName));
  }
} CntrInfo;

class RocrPerfCntrApp {
 public:
  // Constructor of the class. Will initialize the list of perf counters
  // that will be used to program the device
  RocrPerfCntrApp();

  //  Destructor of the class
  ~RocrPerfCntrApp();

  // Return the number of perf counters
  uint32_t GetNumPerfCntrs();

  // Return the handle of perf counter at specified index
  CntrInfo* GetPerfCntr(uint32_t idx);

  // Print the list of perf counters
  bool PrintCntrs();

  // Initialize the list of perf counters
  hsa_status_t Init(hsa_agent_t agent);

  // Register Pre and Post dispatch callbacks
  void RegisterCallbacks(hsa_queue_t* queue);

  // Wait for perf counter collection to complete
  hsa_status_t Wait();

  // Validate perf counter values
  hsa_status_t Validate();

 private:
  //  Number of queues to create
  std::vector<CntrInfo*> cntrList_;

  //  Handle of Perf Cntr Manager
  hsa_ext_tools_pmu_t perfMgr_;
};

}  // namespace rocrtst

#endif  // ROCRTST_COMMON_HSA_PERF_CNTRS_H_
