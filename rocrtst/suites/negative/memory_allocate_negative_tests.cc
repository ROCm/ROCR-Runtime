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


#include <fcntl.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <memory>

#include "suites/negative/memory_allocate_negative_tests.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "gtest/gtest.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_finalize.h"

static const uint32_t kNumBufferElements = 256;





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



MemoryAllocateNegativeTest::MemoryAllocateNegativeTest(void) :
    TestBase() {
  set_num_iteration(10);  // Number of iterations to execute of the main test;
                          // This is a default value which can be overridden
                          // on the command line.

  set_title("RocR Memory Allocate Negative Test");
  set_description("This series of tests are Negative tests "
    "that do check memory allocation on GPU and CPU, "
    "i.e. requesting an allocation of more than max "
    "pool size or 0 size.");
}

MemoryAllocateNegativeTest::~MemoryAllocateNegativeTest(void) {
}

// Any 1-time setup involving member variables used in the rest of the test
// should be done here.
void MemoryAllocateNegativeTest::SetUp(void) {
  hsa_status_t err;

  TestBase::SetUp();

  err = rocrtst::SetDefaultAgents(this);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  err = rocrtst::SetPoolsTypical(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  return;
}

void MemoryAllocateNegativeTest::Run(void) {
  // Compare required profile for this test case with what we're actually
  // running on
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  TestBase::Run();
}

void MemoryAllocateNegativeTest::DisplayTestInfo(void) {
  TestBase::DisplayTestInfo();
}

void MemoryAllocateNegativeTest::DisplayResults(void) const {
  // Compare required profile for this test case with what we're actually
  // running on
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  return;
}

void MemoryAllocateNegativeTest::Close() {
  // This will close handles opened within rocrtst utility calls and call
  // hsa_shut_down(), so it should be done after other hsa cleanup
  TestBase::Close();
}




static const char kSubTestSeparator[] = "  **************************";

static void PrintMemorySubtestHeader(const char *header) {
  std::cout << "  *** Memory Subtest: " << header << " ***" << std::endl;
}

static void PrintAgentNameAndType(hsa_agent_t agent) {
  hsa_status_t err;

  char ag_name[64];
  hsa_device_type_t ag_type;

  err = hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, ag_name);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &ag_type);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  std::cout << "  Agent: " << ag_name << " (";
  switch (ag_type) {
    case HSA_DEVICE_TYPE_CPU:
      std::cout << "CPU)";
      break;
    case HSA_DEVICE_TYPE_GPU:
      std::cout << "GPU)";
      break;
    case HSA_DEVICE_TYPE_DSP:
      std::cout << "DSP)";
      break;
    }
  std::cout << std::endl;
  return;
}

static const int kMemoryAllocSize = 1024;

// This test verify that hsa_memory_allocate can't allocate
// memory more than POOL_INFO_SIZE
void MemoryAllocateNegativeTest::MaxMemoryAllocateTest(hsa_agent_t agent,
                                               hsa_amd_memory_pool_t pool) {
  hsa_status_t err;

  rocrtst::pool_info_t pool_i;
  err = rocrtst::AcquirePoolInfo(pool, &pool_i);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  if (verbosity() > 0) {
    PrintAgentNameAndType(agent);
  }

  // Determine if allocation is allowed in this pool
  bool alloc = false;
  err = hsa_amd_memory_pool_get_info(pool,
                   HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALLOWED, &alloc);

  if (alloc) {
    size_t max_size;
    err = hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_SIZE,
                                      &max_size);
    char *memoryPtr;
    err = hsa_amd_memory_pool_allocate(pool, (max_size + 16), 0,
                                       reinterpret_cast<void**>(&memoryPtr));
    ASSERT_EQ(err, HSA_STATUS_ERROR_INVALID_ALLOCATION);
  }
  return;
}




// This test verify that requesting an allocation
// of 0 size is valid on memory pool or not
void MemoryAllocateNegativeTest::ZeroMemoryAllocateTest(hsa_agent_t agent,
                                                hsa_amd_memory_pool_t pool) {
  hsa_status_t err;

  rocrtst::pool_info_t pool_i;
  err = rocrtst::AcquirePoolInfo(pool, &pool_i);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  if (verbosity() > 0) {
    PrintAgentNameAndType(agent);
  }

  // Determine if allocation is allowed in this pool
  bool alloc = false;
  err = hsa_amd_memory_pool_get_info(pool,
                   HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALLOWED, &alloc);

  if (alloc) {
    char *memoryPtr;
    err = hsa_amd_memory_pool_allocate(pool, 0, 0,
                                       reinterpret_cast<void**>(&memoryPtr));
    ASSERT_EQ(err, HSA_STATUS_ERROR_INVALID_ARGUMENT);
  }
  return;
}


void MemoryAllocateNegativeTest::MaxMemoryAllocateTest(void) {
  hsa_status_t err;
  std::vector<std::shared_ptr<rocrtst::agent_pools_t>> agent_pools;

  PrintMemorySubtestHeader("MaxMemoryAllocateTest in Memory Pools");

  err = rocrtst::GetAgentPools(&agent_pools);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  auto pool_idx = 0;
  for (auto a : agent_pools) {
    for (auto p : a->pools) {
      std::cout << "  Pool " << pool_idx++ << ":" << std::endl;
      MaxMemoryAllocateTest(a->agent, p);
    }
  }

  if (verbosity() > 0) {
    std::cout << "subtest Passed" << std::endl;
    std::cout << kSubTestSeparator << std::endl;
  }
}

void MemoryAllocateNegativeTest::ZeroMemoryAllocateTest(void) {
  hsa_status_t err;
  std::vector<std::shared_ptr<rocrtst::agent_pools_t>> agent_pools;

  PrintMemorySubtestHeader("ZeroMemoryAllocateTest in Memory Pools");

  err = rocrtst::GetAgentPools(&agent_pools);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  auto pool_idx = 0;
  for (auto a : agent_pools) {
    for (auto p : a->pools) {
      std::cout << "  Pool " << pool_idx++ << ":" << std::endl;
      ZeroMemoryAllocateTest(a->agent, p);
    }
  }

  if (verbosity() > 0) {
    std::cout << "subtest Passed" << std::endl;
    std::cout << kSubTestSeparator << std::endl;
  }
}

#undef RET_IF_HSA_ERR
