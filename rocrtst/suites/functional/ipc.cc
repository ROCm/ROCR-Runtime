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

// The purpose of this test is to provide an example of the use of the
// common RocrTest classes and utilities that are used in many examples.
// It can be used as a template to start off with when writing new tests.
// In many cases, the existing boilerplate code will be sufficient as is.
// Otherwise, the boilerplate code can be either supplemented or replaced
// by your own code in your example, as necessary.
//
// The comments provided are focused more on the use of the common rocrtst
// utilities and boilerplate code, rather than the example app. itself.
//
// The boilerplate code includes code for:
// * hsa initialization and clean up
// * code to load pre-built kernels
// * creating queues
// * populating AQL packets
// * checking for required profiles
// * finding cpu and gpu agents (callbacks for common use cases)
// * finding pools (having common requirements)
// * allocating and setting kernel arguments
// * somewhat standardized output
// * handling additional command line arguments, beyond google-test arguments
// * support for various level of verbosity, controlled from command line arg
// * support for building OpenCL kernels
// * timer support
//
// Overview of RocrTst code organization:
// Classes:
// * class BaseRocR (base_rocr.h) -- base class for all rocrtst examples and
//   tests. Most of the rocrtst common utilities act on BaseRocR objects
//
// * TestBase (test_base.h)  -- derives from BaseRocR and is the base class
//   for all tests under <rocrtst root>/suites. The implementation in TestBase
//   methods are typically actions that are required for most/all tests and
//   should therefore be called from the derived implementions of the methods.
//
// Utilities:
// * <rocrtst root>/common/base_rocr_utils.<cc/h> contains a set of utilities
//   that act on BaseRocR objects.
//
// * <rocrtst root>/common/common.<cc/h> contain other non-BaseRocR utilities
//
// Special Files:
// * main.cc -- The main google test file from which the tests are invoked.
//     There should be an entry for each test to be run there.
//
// * kernels -- OpenCL kernel source files should go in the kernels directory
//
// * CMakeLists.txt -- Host code (*.cc and *.h files) should build without
//     modifying the CMakeList.txt file, if the files are place in the
//     "performance" directory. However, an entry for OpenCL kernels. For
//     each kernel to be built, the bitcode libraries must be indicated before
//     the call to "build_kernel()" is made. See existing code for examples.

#include <sys/mman.h>

#include <algorithm>
#include <iostream>
#include <vector>
#include <atomic>

#include "suites/functional/ipc.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "gtest/gtest.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_finalize.h"

static const uint32_t kNumBufferElements = 256;

struct callback_args {
  hsa_agent_t host;
  hsa_agent_t device;
  hsa_amd_memory_pool_t cpu_pool;
  hsa_amd_memory_pool_t gpu_pool;
  size_t gpu_mem_granule;
};

// Wrap printf to add first or second process indicator
#define PROCESS_LOG(format, ...)  { \
    if (verbosity() >= VERBOSE_STANDARD || !processOne_) { \
      fprintf(stdout, "line:%d P%u: " format, \
                   __LINE__, static_cast<int>(!processOne_), ##__VA_ARGS__); \
    } \
}

IPCTest::IPCTest(void) :
    TestBase() {
  set_num_iteration(10);  // Number of iterations to execute of the main test;
                          // This is a default value which can be overridden
                          // on the command line.
  set_title("IPC Test");
  set_description("IPCTest verifies that the IPC feature of RocR is "
      "functioning as expected. The test first forks off second process. The "
      "2 processes share pointers to RocR allocated memory and also share "
      "signal handles");
}

IPCTest::~IPCTest(void) {
}

// See if the other process wrote an error value to the token; if not, write
// the newVal to the token.
static int CheckAndSetToken(std::atomic<int> *token, int newVal) {
  if (*token == -1) {
    return -1;
  } else {
    *token = newVal;
  }

  return 0;
}

// Any 1-time setup involving member variables used in the rest of the test
// should be done here.
void IPCTest::SetUp(void) {
  hsa_status_t err;

  int ret;
  // We must fork process before doing HSA stuff, specifically, hsa_init, as
  // each process needs to do this.
  // Allocate linux shared_ memory.
  shared_ = reinterpret_cast<Shared*>(
      mmap(nullptr, sizeof(Shared), PROT_READ | PROT_WRITE,
                                          MAP_SHARED | MAP_ANONYMOUS, -1, 0));
  ASSERT_NE(shared_, MAP_FAILED) << "mmap failed to allocated shared_ memory";

  // "token" is used to signal state changes between the 2 processes.
  std::atomic<int> * token = &shared_->token;
  *token = 0;

  // Spawn second process and verify communication
  child_ = 0;
  child_ = fork();
  ASSERT_NE(child_, -1) << "fork failed";
  if (child_ != 0) {
    processOne_ = true;

    // Signal to other process we are waiting, and then wait...
    *token = 1;
    while (*token == 1) {
      sched_yield();
    }

    PROCESS_LOG("Second process observed, handshake...\n");
    *token = 1;
    while (*token == 1) {
      sched_yield();
    }

  } else {
    processOne_ = false;
    set_verbosity(0);
    PROCESS_LOG("Second process running.\n");

    while (*token == 0) {
      sched_yield();
    }

    ret = CheckAndSetToken(token, 0);
    ASSERT_EQ(ret, 0) << "Error detected in other process";
    // Wait for handshake
    while (*token == 0) {
      sched_yield();
    }
    ret = CheckAndSetToken(token, 0);
    ASSERT_EQ(ret, 0) << "Error detected in other process";
  }
  // TestBase::SetUp() will set HSA_ENABLE_INTERRUPT if enable_interrupt() is
  // true, and call hsa_init(). It also prints the SetUp header.
  TestBase::SetUp();

  // SetDefaultAgents(this) will assign the first CPU and GPU found on
  // iterating through the agents and assign them to cpu_device_ and
  // gpu_device1_, respectively (cpu_device() and gpu_device1()). These
  // BaseRocR member variables are used in some utilities. Additionally,
  // SetDefaultAgents() checks the profile of the gpu and compares this
  // to any required profile.
  err = rocrtst::SetDefaultAgents(this);
  ASSERT_EQ(HSA_STATUS_SUCCESS, err);

  // Find and assign HSA_AMD_SEGMENT_GLOBAL pools for cpu, gpu and a kern_arg
  // pool
  err = rocrtst::SetPoolsTypical(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  return;
}

// This wrapper atomically writes the provided header and setup to the
// provided AQL packet. The provided AQL packet address should be in the
// queue memory space.
static inline void AtomicSetPacketHeader(uint16_t header, uint16_t setup,
                                  hsa_kernel_dispatch_packet_t* queue_packet) {
  __atomic_store_n(reinterpret_cast<uint32_t*>(queue_packet),
                   header | (setup << 16), __ATOMIC_RELEASE);
}

// Do a few extra iterations as we toss out some of the inital and final
// iterations when calculating statistics
uint32_t IPCTest::RealIterationNum(void) {
  return num_iteration() * 1.2 + 1;
}

void IPCTest::Run(void) {
  hsa_status_t err;
  std::atomic<int> *token = &shared_->token;
  TestBase::Run();

  // Print out name of the device.
  char name1[64] = {0};
  char name2[64] = {0};
  err = hsa_agent_get_info(*cpu_device(), HSA_AGENT_INFO_NAME, name1);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS) << "hsa_agent_get_info() failed";
  err = hsa_agent_get_info(*gpu_device1(), HSA_AGENT_INFO_NAME, name2);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS) << "hsa_agent_get_info() failed";

  uint16_t loc1, loc2;
  err = hsa_agent_get_info(*cpu_device(),
                           (hsa_agent_info_t)HSA_AMD_AGENT_INFO_BDFID, &loc1);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  err = hsa_agent_get_info(*gpu_device1(),
                           (hsa_agent_info_t)HSA_AMD_AGENT_INFO_BDFID, &loc2);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  size_t gpu_mem_granule;
#ifdef ROCRTST_EMULATOR_BUILD
  gpu_mem_granule = 4;
#else
  err = hsa_amd_memory_pool_get_info(device_pool(),
    HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_GRANULE, &gpu_mem_granule);
#endif

  if (verbosity() >= VERBOSE_STANDARD) {
    fprintf(stdout, "Using: %s (%d) and %s (%d)\n", name1, loc1, name2, loc2);
  }

  hsa_agent_t ag_list[2] = {*gpu_device1(), *cpu_device()};

  auto CheckAndFillBuffer = [&](void *gpu_src_ptr, uint32_t exp_cur_val,
                                                   uint32_t new_val) -> void {
    hsa_signal_t copy_signal;
    size_t sz = gpu_mem_granule;
    hsa_status_t err;
    hsa_signal_value_t sig;

    err = hsa_signal_create(1, 0, NULL, &copy_signal);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    uint32_t *sysBuf;

    err = hsa_amd_memory_pool_allocate(cpu_pool(), sz, 0,
                                          reinterpret_cast<void **>(&sysBuf));
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    hsa_agent_t ag_list[2] = {*gpu_device1(), *cpu_device()};
    err = hsa_amd_agents_allow_access(2, ag_list, NULL, sysBuf);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    err = hsa_amd_memory_async_copy(sysBuf, *cpu_device(), gpu_src_ptr,
                                    *gpu_device1(), sz, 0, NULL, copy_signal);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    sig = hsa_signal_wait_relaxed(copy_signal, HSA_SIGNAL_CONDITION_LT,
                                               1, -1, HSA_WAIT_STATE_BLOCKED);
    ASSERT_EQ(sig, 0) << "Expected signal 0, but got " << sig;

    uint32_t count = sz/sizeof(uint32_t);

    for (uint32_t i = 0; i < count; ++i) {
      ASSERT_EQ(sysBuf[i], exp_cur_val);
      sysBuf[i] = new_val;
    }

    hsa_signal_store_relaxed(copy_signal, 1);

    err = hsa_amd_memory_async_copy(gpu_src_ptr, *gpu_device1(), sysBuf,
                                     *cpu_device(), sz, 0, NULL, copy_signal);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    sig = hsa_signal_wait_relaxed(copy_signal, HSA_SIGNAL_CONDITION_LT,
                                         1, -1, HSA_WAIT_STATE_BLOCKED);
    ASSERT_EQ(sig, 0) << "Expected signal 0, but got " << sig;

    err = hsa_signal_destroy(copy_signal);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    err = hsa_amd_memory_pool_free(sysBuf);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  };
  if (processOne_) {
    // Ignoring the first allocation to exercise fragment allocation.
    uint32_t* discard = NULL;
    err = hsa_amd_memory_pool_allocate(device_pool(), gpu_mem_granule, 0,
                                          reinterpret_cast<void**>(&discard));
    ASSERT_EQ(err, HSA_STATUS_SUCCESS) <<
                                    "Failed to allocate memory from gpu pool";

    // Allocate some VRAM and fill it with 1's
    uint32_t* gpuBuf = NULL;
    err = hsa_amd_memory_pool_allocate(device_pool(), gpu_mem_granule, 0,
                                            reinterpret_cast<void**>(&gpuBuf));
    ASSERT_EQ(err, HSA_STATUS_SUCCESS) <<
                                     "Failed to allocate memory from gpu pool";

    err = hsa_amd_memory_pool_free(discard);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS) << "Failed to free GPU memory";

    PROCESS_LOG("Allocated local memory buffer at %p\n", gpuBuf);

    err = hsa_amd_agents_allow_access(2, ag_list, NULL, gpuBuf);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    err = hsa_amd_ipc_memory_create(gpuBuf, gpu_mem_granule,
                          const_cast<hsa_amd_ipc_memory_t*>(&shared_->handle));
    PROCESS_LOG(
    "Created IPC handle associated with gpu-local buffer at P0 address %p\n",
                                                                      gpuBuf);

    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    uint32_t count = gpu_mem_granule/sizeof(uint32_t);
    shared_->size = gpu_mem_granule;
    shared_->count = count;

    err = hsa_amd_memory_fill(gpuBuf, 1, count);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    // Get IPC capable signal
    hsa_signal_t ipc_signal;
    err = hsa_amd_signal_create(1, 0, NULL, HSA_AMD_SIGNAL_IPC, &ipc_signal);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    err = hsa_amd_ipc_signal_create(ipc_signal,
                  const_cast<hsa_amd_ipc_signal_t*>(&shared_->signal_handle));
    PROCESS_LOG("Created IPC handle associated with ipc_signal\n");
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    // Signal Process 2 that the gpu buffer is ready to read.
    CheckAndSetToken(token, 1);
    PROCESS_LOG("Allocated buffer and filled it with 1's. Wait for P1...\n");
    hsa_signal_value_t ret =
        hsa_signal_wait_acquire(ipc_signal, HSA_SIGNAL_CONDITION_NE, 1, -1,
                                                     HSA_WAIT_STATE_BLOCKED);

    ASSERT_EQ(ret, 2) << "Expected signal value of 2, but got " << ret;

    CheckAndFillBuffer(gpuBuf, 2, 0);
    PROCESS_LOG("Confirmed P1 filled buffer with 2\n")
    PROCESS_LOG("PASSED on P0\n");

    hsa_signal_store_relaxed(ipc_signal, 0);

    err = hsa_signal_destroy(ipc_signal);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    err = hsa_amd_memory_pool_free(gpuBuf);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    waitpid(child_, nullptr, 0);
    munmap(shared_, sizeof(Shared));

    // Note: Close() (and hsa_shut_down()) will be called from main() in
    // RunCustomEpilog()
  } else {  // "ProcessTwo"
    PROCESS_LOG("Waiting for process 0 to write 1 to token...\n");
    while (*token == 0) {
      sched_yield();
    }
    if (*token != 1) {
      *token = -1;
    }
    ASSERT_EQ(*token, 1) << "Error detected in signaling token";

    PROCESS_LOG("Process 0 wrote 1 to token...\n");

    // Attach shared_ VRAM
    void* ptr;
    err = hsa_amd_ipc_memory_attach(
      const_cast<hsa_amd_ipc_memory_t*>(&shared_->handle), shared_->size, 1,
                                                               ag_list, &ptr);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    PROCESS_LOG(
     "Attached to IPC handle; P1 buffer address gpu-local memory is %p\n",
                                                                         ptr);
    // Attach shared_ signal
    hsa_signal_t ipc_signal;
    err = hsa_amd_ipc_signal_attach(
     const_cast<hsa_amd_ipc_signal_t*>(&shared_->signal_handle), &ipc_signal);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    PROCESS_LOG("Attached to signal IPC handle\n");

    CheckAndFillBuffer(reinterpret_cast<uint32_t *>(ptr), 1, 2);

    PROCESS_LOG(
      "Confirmed P0 filled buffer with 1; P1 re-filled buffer with 2\n");
    PROCESS_LOG("PASSED on P1\n");

    hsa_signal_store_release(ipc_signal, 2);


    // Test ptrinfo - allocation is a single granule so this tests imported
    // fragment info.
    err = hsa_amd_pointer_info_set_userdata(ptr,
                                         reinterpret_cast<void*>(0xDEADBEEF));
    ASSERT_EQ(err, HSA_STATUS_SUCCESS) <<
                                 "hsa_amd_pointer_info_set_userdata() failed";

    hsa_amd_pointer_info_t info;
    info.size = sizeof(info);
    err = hsa_amd_pointer_info(
                reinterpret_cast<uint8_t*>(ptr) + shared_->size / 2, &info,
                                                   nullptr, nullptr, nullptr);
    if ((info.sizeInBytes != shared_->size) ||
        (info.userData != reinterpret_cast<void*>(0xDEADBEEF)) ||
                                             (info.agentBaseAddress != ptr)) {
      PROCESS_LOG("Pointer Info check failed.\n");
    } else {
      PROCESS_LOG("PointerInfo check PASSED.\n");
    }




    err = hsa_amd_ipc_memory_detach(ptr);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    hsa_signal_wait_relaxed(ipc_signal, HSA_SIGNAL_CONDITION_NE, 2, -1,
                                                      HSA_WAIT_STATE_BLOCKED);

    err = hsa_signal_destroy(ipc_signal);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);
    Close();
    exit(0);
  }

  return;
}

void IPCTest::DisplayTestInfo(void) {
  TestBase::DisplayTestInfo();
}

void IPCTest::DisplayResults(void) const {
  TestBase::DisplayResults();
  return;
}

void IPCTest::Close() {
  // This will close handles opened within rocrtst utility calls and call
  // hsa_shut_down(), so it should be done after other hsa cleanup
  TestBase::Close();
}

#undef PROCESS_LOG
