/*
 * =============================================================================
 *   ROC Runtime Conformance Release License
 * =============================================================================
 * The University of Illinois/NCSA
 * Open Source License (NCSA)
 *
 * Copyright (c) 2018, Advanced Micro Devices, Inc.
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

#include <algorithm>
#include <iostream>
#include "suites/functional/concurrent_init_shutdown.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "gtest/gtest.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_finalize.h"

void* TestHSAInitShutdownFunction(void* args) {
  // This is callback function for each thread
  // This will initialize the HSA runtime and shutdown
  hsa_status_t status;

  // Initialize hsa runtime
  status = hsa_init();
  if (status != HSA_STATUS_SUCCESS) std::cout << "Failed" << std::endl;

  // Shutdown hsa runtime
  status = hsa_shut_down();
  if (status != HSA_STATUS_SUCCESS) {
    std::cout << "Failed" << std::endl;
  }
  pthread_exit(NULL);
}

static const int NumOfThreads = 100;  // Number of thread to be created

#define RET_IF_HSA_ERR(err) { \
  if ((err) != HSA_STATUS_SUCCESS) { \
    const char* msg = 0; \
    hsa_status_string(err, &msg); \
    std::cout << "hsa api call failure at line " << __LINE__ << ", file: " << \
                          __FILE__ << ". Call returned " << err << std::endl; \
    std::cout << msg << std::endl; \
    return (err); \
  } \
}

ConcurrentInitShutdownTest::ConcurrentInitShutdownTest(void) : TestBase() {
  set_num_iteration(10);  // Number of iterations to execute of the main test;
                        // This is a default value which can be overridden
                        // on the command line.
  set_title("RocR Concurrent Init Test");
  set_description("This test initializes HSA runtime concurrently");
}

// Any 1-time setup involving member variables used in the rest of the test
// should be done here.
ConcurrentInitShutdownTest::~ConcurrentInitShutdownTest(void) {
}

// Compare required profile for this test case with what we're actually
// running on
void ConcurrentInitShutdownTest::SetUp(void) {
  return;  // hsa runtime initalized pthread callback function
}


// Compare required profile for this test case with what we're actually
// running on
void ConcurrentInitShutdownTest::Run(void) {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  TestBase::Run();
}

// Compare required profile for this test case with what we're actually
// running on
void ConcurrentInitShutdownTest::DisplayTestInfo(void) {
  TestBase::DisplayTestInfo();
}

void ConcurrentInitShutdownTest::DisplayResults(void) const {
  // Compare required profile for this test case with what we're actually
  // running on
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  return;
}

void ConcurrentInitShutdownTest::Close() {
  // This will close handles opened within rocrtst utility calls and call
  // hsa_shut_down(), so it should be done after other hsa cleanup
  TestBase::Close();
}

void ConcurrentInitShutdownTest::TestConcurrentInitShutdown(void) {
  pthread_t ThreadId[NumOfThreads];

  pthread_attr_t attr;
  pthread_attr_init(&attr);

  // Setting the attribute to PTHREAD_CREATE_JOINABLE
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  // This is to create threads concurrently
  // HSA runtime will be initialized for each thread
  for (int Id = 0; Id < NumOfThreads; ++Id) {
    int ThreadStatus = pthread_create(ThreadId + Id, &attr,
                                      TestHSAInitShutdownFunction, &Id);

    // Check if the thread is created successfully
    if (ThreadStatus < 0) {
      std::cout << Id << "Thread creation failed " << std::endl;
    }
  }
}
#undef RET_IF_HSA_ERR
