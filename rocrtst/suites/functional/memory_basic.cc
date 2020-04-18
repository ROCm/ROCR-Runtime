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

#include <algorithm>
#include <iostream>
#include <vector>
#include <memory>

#include "suites/functional/memory_basic.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "gtest/gtest.h"
#include "hsa/hsa.h"

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


MemoryTest::MemoryTest(void) :
    TestBase() {
  set_num_iteration(10);  // Number of iterations to execute of the main test;
                          // This is a default value which can be overridden
                          // on the command line.
  set_title("RocR Memory Tests");
  set_description("This series of tests check memory allocation limits, extent"
    " of GPU access to system memory and other memory related functionality.");
}

MemoryTest::~MemoryTest(void) {
}

// Any 1-time setup involving member variables used in the rest of the test
// should be done here.
void MemoryTest::SetUp(void) {
  hsa_status_t err;

  TestBase::SetUp();

  err = rocrtst::SetDefaultAgents(this);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  err = rocrtst::SetPoolsTypical(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  return;
}

void MemoryTest::Run(void) {
  // Compare required profile for this test case with what we're actually
  // running on
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  TestBase::Run();
}

void MemoryTest::DisplayTestInfo(void) {
  TestBase::DisplayTestInfo();
}

void MemoryTest::DisplayResults(void) const {
  // Compare required profile for this test case with what we're actually
  // running on
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  return;
}

void MemoryTest::Close() {
  // This will close handles opened within rocrtst utility calls and call
  // hsa_shut_down(), so it should be done after other hsa cleanup
  TestBase::Close();
}

hsa_status_t MemoryTest::TestAllocate(hsa_amd_memory_pool_t pool, size_t sz) {
  void *ptr;
  hsa_status_t err;

  err = hsa_amd_memory_pool_allocate(pool, sz, 0, &ptr);

  if (err == HSA_STATUS_SUCCESS) {
    err = hsa_memory_free(ptr);
  }

  return err;
}

static const char kSubTestSeparator[] = "  **************************";

static void PrintMemorySubtestHeader(const char *header) {
  std::cout << "  *** Memory Subtest: " << header << " ***" << std::endl;
}

// Test Fixtures
void MemoryTest::MaxSingleAllocationTest(hsa_agent_t ag,
                                                 hsa_amd_memory_pool_t pool) {
  hsa_status_t err;

  rocrtst::pool_info_t pool_i;
  char ag_name[64];
  hsa_device_type_t ag_type;

  err = hsa_agent_get_info(ag, HSA_AGENT_INFO_NAME, ag_name);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_agent_get_info(ag, HSA_AGENT_INFO_DEVICE, &ag_type);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  if (verbosity() > 0) {
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
  }

  err = rocrtst::AcquirePoolInfo(pool, &pool_i);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  if (verbosity() > 0) {
      rocrtst::DumpMemoryPoolInfo(&pool_i, 2);
  }

  if (!pool_i.alloc_allowed || pool_i.alloc_granule == 0 ||
                                           pool_i.alloc_alignment == 0) {
    if (verbosity() > 0) {
      std::cout << "  Test not applicable. Skipping." << std::endl;
      std::cout << kSubTestSeparator << std::endl;
    }
    return;
  }
  // Do everything in "granule" units
  auto gran_sz = pool_i.alloc_granule;
  auto pool_sz = pool_i.aggregate_alloc_max / gran_sz;

  // Neg. test: Try to allocate more than the pool size
  err = TestAllocate(pool, pool_sz*gran_sz + gran_sz);
  EXPECT_EQ(HSA_STATUS_ERROR_INVALID_ALLOCATION, err);

  auto max_alloc_size = pool_sz/2;
  uint64_t upper_bound = pool_sz;
  uint64_t lower_bound = 0;

  while (true) {
    err = TestAllocate(pool, max_alloc_size * gran_sz);
    ASSERT_TRUE(err == HSA_STATUS_SUCCESS ||
                                    err == HSA_STATUS_ERROR_OUT_OF_RESOURCES);
    if (err == HSA_STATUS_SUCCESS) {
      lower_bound = max_alloc_size;
      max_alloc_size += (upper_bound - lower_bound)/2;
    } else if (err == HSA_STATUS_ERROR_OUT_OF_RESOURCES) {
      upper_bound = max_alloc_size;
      max_alloc_size -= (upper_bound - lower_bound)/2;
    }

    if ((upper_bound - lower_bound) < 2) {
      break;
    }
    ASSERT_GT(upper_bound, lower_bound);
  }

  if (verbosity() > 0) {
    std::cout << "  Biggest single allocation size for this pool is " <<
                        (max_alloc_size * gran_sz)/1024 << "KB." << std::endl;
    std::cout << "  This is " <<
                  static_cast<float>(max_alloc_size)/pool_sz*100 <<
                                               "% of the total." << std::endl;
  }

  if (ag_type == HSA_DEVICE_TYPE_GPU) {
    EXPECT_GE((float)max_alloc_size/pool_sz, (float)15/16);
  }
  if (verbosity() > 0) {
    std::cout << kSubTestSeparator << std::endl;
  }
}

void MemoryTest::MaxSingleAllocationTest(void) {
  hsa_status_t err;
  std::vector<std::shared_ptr<rocrtst::agent_pools_t>> agent_pools;

  PrintMemorySubtestHeader("Maximum Single Allocation in Memory Pools");

  err = rocrtst::GetAgentPools(&agent_pools);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  auto pool_idx = 0;
  for (auto a : agent_pools) {
    for (auto p : a->pools) {
      std::cout << "  Pool " << pool_idx++ << ":" << std::endl;
      MaxSingleAllocationTest(a->agent, p);
    }
  }
}

#undef RET_IF_HSA_ERR
