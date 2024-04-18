/*
 * =============================================================================
 *   ROC Runtime Conformance Release License
 * =============================================================================
 * The University of Illinois/NCSA
 * Open Source License (NCSA)
 *
 * Copyright (c) 2024, Advanced Micro Devices, Inc.
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

#include "suites/functional/trap_handler.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/os.h"
#include "common/helper_funcs.h"
#include "gtest/gtest.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"

#include <thread>

#define RETRY_LIMIT 5
#define DELAY_IN_MILLISECONDS 1
#define TIMEOUT_LIMIT 5000  // milliseconds

static const char kSubTestSeparator[] = "  **************************";
static void PrintDebugSubtestHeader(const char* header) {
  std::cout << "  *** TrapHandler Subtest: " << header << " ***" << std::endl;
}

static const uint32_t kNumBufferElements = 256;

typedef struct q_callback_data_t {
  hsa_queue_t** qptr;
  bool trap;
} q_callback_data;

static void CallbackQueueErrorHandler(hsa_status_t status, hsa_queue_t* source, void* data);
static hsa_status_t CallbackEventHandler(const hsa_amd_event_t* event, void* data);

TrapHandler::TrapHandler(bool trigger_s_trap, bool trigger_memory_violation) : TestBase() {
  std::string name;
  std::string desc;

  name = "ROCr Trap Handler Test";
  desc =
      "This set of tests intentionally trigger software exceptions and verify "
      "that the GPU can handle abnormal situations.";

  if (trigger_s_trap) {
    name += ": Trigger a software trap";
    desc +=
        "\n\nCurrent sub-test intentionally triggers a software exception using"
        " the 's_trap' instruction, to validate if the queue's error handling"
        " callback is triggered.";
  } else if (trigger_memory_violation) {
    name += ": Trigger illegal memory access";
    desc +=
        "\n\nCurrent sub-test intentionally triggers a memory violation error"
        " to attempt accessing an invalid memory address. It verifies if the "
        " GPU Memory protection exception is triggered.";
  }

  set_title(name);
  set_description(desc);
  set_kernel_file_name("trap_handler_kernels.hsaco");
  kernel_names_ = {"trigger_s_trap", "trigger_memory_violation"};
}

void TrapHandler::SetUp() {
  hsa_status_t err;

  TestBase::SetUp();

  err = rocrtst::SetDefaultAgents(this);
  CHECK(err);

  err = rocrtst::SetPoolsTypical(this);
  CHECK(err)

  return;
}

static inline void AtomicSetPacketHeader(uint16_t header, uint16_t setup,
                                         hsa_kernel_dispatch_packet_t* queue_packet) {
  __atomic_store_n(reinterpret_cast<uint32_t*>(queue_packet), header | (setup << 16),
                   __ATOMIC_RELEASE);
}

void TrapHandler::Run(void) {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }
  TestBase::Run();
}

void TrapHandler::TriggerSoftwareTrap(void) {
  std::vector<hsa_agent_t> cpus, gpus;
  hsa_status_t err = hsa_iterate_agents(rocrtst::IterateCPUAgents, &cpus);
  CHECK(err)

  err = hsa_iterate_agents(rocrtst::IterateGPUAgents, &gpus);
  CHECK(err)

  // Check if there are any GPUs available
  if (gpus.empty()) {
    std::cerr << "No GPUs found." << std::endl;
    return;
  }

  // Select the first GPU in the vector
  hsa_agent_t selectedGpuAgent = gpus[0];
  uint32_t node_id;
  err = hsa_agent_get_info(selectedGpuAgent, HSA_AGENT_INFO_NODE, &node_id);
  CHECK(err);

  std::cout << "*** Running test on GPU node ID: 0x" << std::hex << node_id << "***\n" << std::endl;
  execute_kernel("trigger_s_trap", cpus[0], selectedGpuAgent);
  return;
}

void TrapHandler::TriggerMemoryViolation(void) {
  std::vector<hsa_agent_t> cpus, gpus;
  hsa_status_t err = hsa_iterate_agents(rocrtst::IterateCPUAgents, &cpus);
  CHECK(err)

  err = hsa_iterate_agents(rocrtst::IterateGPUAgents, &gpus);
  CHECK(err)

  // Check if there are any GPUs available
  if (gpus.empty()) {
    std::cerr << "No GPUs found." << std::endl;
    return;
  }

  // Select the first GPU from the list
  hsa_agent_t selectedGpuAgent = gpus[0];
  uint32_t node_id;
  err = hsa_agent_get_info(selectedGpuAgent, HSA_AGENT_INFO_NODE, &node_id);
  CHECK(err);

  std::cout << "*** Running test on GPU node ID: 0x" << std::hex << node_id << "***\n" << std::endl;
  execute_kernel("trigger_memory_violation", cpus[0], selectedGpuAgent);
  return;
}

void TrapHandler::execute_kernel(const char* kernel_name, hsa_agent_t cpuAgent,
                                 hsa_agent_t gpuAgent) {
  hsa_signal_t signal;
  uint32_t queue_size = 0;

  set_kernel_name(kernel_name);

  hsa_status_t err = hsa_agent_get_info(gpuAgent, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size);
  CHECK(err)
  q_callback_data data;
  data.qptr = &queue;
  data.trap = false;

  // Create queue and register queue error handler callback function
  err = hsa_queue_create(gpuAgent, queue_size, HSA_QUEUE_TYPE_MULTI, CallbackQueueErrorHandler,
                         &data, 0, 0, &queue);
  CHECK(err)
  ASSERT_NE(queue, nullptr);
  set_main_queue(queue);

  // Register system event handler callback function
  hsa_status_t status = hsa_amd_register_system_event_handler(&CallbackEventHandler, this);
  ASSERT_EQ(status, HSA_STATUS_SUCCESS);

  hsa_amd_memory_pool_t kernarg_pool;
  err = hsa_amd_agent_iterate_memory_pools(cpuAgent, rocrtst::GetKernArgMemoryPool, &kernarg_pool);
  CHECK(err)

  hsa_amd_memory_pool_t global_pool;
  err = hsa_amd_agent_iterate_memory_pools(cpuAgent, rocrtst::GetGlobalMemoryPool, &global_pool);
  CHECK(err)

  err = hsa_amd_memory_pool_allocate(global_pool, kNumBufferElements * sizeof(uint32_t), 0,
                                     reinterpret_cast<void**>(&src_buffer_));

  CHECK(err)

  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, src_buffer_);
  CHECK(err)

  for (uint32_t i = 0; i < kNumBufferElements; ++i) {
    reinterpret_cast<uint32_t*>(src_buffer_)[i] = i;
  }

  err = hsa_amd_memory_pool_allocate(global_pool, kNumBufferElements * sizeof(uint32_t), 0,
                                     reinterpret_cast<void**>(&dst_buffer_));
  CHECK(err)
  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, dst_buffer_);
  CHECK(err)

  typedef struct __attribute__((aligned(16))) local_args_t {
    uint32_t* dstArray;
    uint32_t* srcArray;
    uint32_t size;
  } local_args;

  local_args* KernArgs = NULL;
  err = hsa_amd_memory_pool_allocate(kernarg_pool, sizeof(local_args), 0,
                                     reinterpret_cast<void**>(&KernArgs));
  CHECK(err)

  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, KernArgs);
  CHECK(err)

  KernArgs->dstArray = reinterpret_cast<uint32_t*>(dst_buffer_);
  KernArgs->srcArray = reinterpret_cast<uint32_t*>(src_buffer_);
  KernArgs->size = kNumBufferElements;

  err = rocrtst::LoadKernelFromObjFile(this, &gpuAgent);
  CHECK(err)

  err = hsa_signal_create(1, 0, NULL, &signal);
  CHECK(err)

  hsa_kernel_dispatch_packet_t aql;
  memset(&aql, 0, sizeof(aql));

  aql.header = 0;
  aql.setup = 1;
  aql.workgroup_size_x = kNumBufferElements;
  aql.workgroup_size_y = 1;
  aql.workgroup_size_z = 1;
  aql.grid_size_x = kNumBufferElements;
  aql.grid_size_y = 1;
  aql.grid_size_z = 1;
  aql.private_segment_size = 0;
  aql.group_segment_size = 0;
  aql.kernel_object = kernel_object();
  aql.kernarg_address = KernArgs;
  aql.completion_signal = signal;

  const uint32_t queue_mask = queue->size - 1;

  uint64_t index = hsa_queue_load_write_index_relaxed(queue);

  hsa_queue_store_write_index_relaxed(queue, index + 1);

  rocrtst::WriteAQLToQueueLoc(queue, index, &aql);

  uint32_t aql_header = HSA_PACKET_TYPE_KERNEL_DISPATCH;

  aql_header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
  aql_header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;

  void* q_base = queue->base_address;
  rocrtst::AtomicSetPacketHeader(
      aql_header, aql.setup,
      &(reinterpret_cast<hsa_kernel_dispatch_packet_t*>(q_base))[index & queue_mask]);

  hsa_signal_store_relaxed(queue->doorbell_signal, index);

  hsa_signal_value_t completion = 0;
  int retry_count = 0;

  if (strcmp(kernel_name, "trigger_s_trap") == 0) {
    completion = hsa_signal_wait_scacquire(signal, HSA_SIGNAL_CONDITION_LT, 1, TIMEOUT_LIMIT,
                                           HSA_WAIT_STATE_BLOCKED);
    ASSERT_EQ(completion, 1);

    while (data.trap != true && retry_count < RETRY_LIMIT) {
      ++retry_count;
      sleep(DELAY_IN_MILLISECONDS);
    }
    ASSERT_EQ(data.trap, true);
  }

  else if (strcmp(kernel_name, "trigger_memory_violation") == 0) {
    while (event_occured != 1 && retry_count < RETRY_LIMIT) {
      ++retry_count;
      sleep(DELAY_IN_MILLISECONDS);
    }
    ASSERT_EQ(event_occured, 1);
  }

  // Cleanup
  if (KernArgs) hsa_memory_free(KernArgs);
  if (src_buffer_) hsa_memory_free(src_buffer_);
  if (dst_buffer_) hsa_memory_free(dst_buffer_);
  if (signal.handle) hsa_signal_destroy(signal);

  if (!event_occured && queue) {
    hsa_queue_destroy(queue);
    queue = nullptr;
  }

  return;
}

void TrapHandler::DisplayTestInfo(void) { TestBase::DisplayTestInfo(); }

void TrapHandler::DisplayResults(void) const {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  TestBase::DisplayResults();
  return;
}

void TrapHandler::Close() { TestBase::Close(); }

hsa_status_t CallbackEventHandler(const hsa_amd_event_t* event, void* data) {
  TrapHandler* ptr = reinterpret_cast<TrapHandler*>(data);

  switch (event->event_type) {
    case HSA_AMD_GPU_MEMORY_FAULT_EVENT:
      ptr->event_occured = true;
      printf("Subtest Passed: Runtime caught GPU Memory Fault Event successfully!\n");
      break;

    case HSA_AMD_GPU_HW_EXCEPTION_EVENT:
      ptr->event_occured = true;
      printf("Subtest Passed: Runtime caught GPU HW Exception Event successfully!\n");
      break;
    default:
      // Unknown event type
      printf("Subtest Failed: Unknown event type occurred\n");
      break;
  };

  std::cout << kSubTestSeparator << std::endl;
  // Returning success to indicate that event was handled
  return HSA_STATUS_SUCCESS;
}

void CallbackQueueErrorHandler(hsa_status_t status, hsa_queue_t* source, void* data) {
  std::cout << "Subtest Passed: Runtime caught trap instruction successfully!" << std::endl;

  ASSERT_NE(source, nullptr);
  ASSERT_NE(data, nullptr);

  q_callback_data* debug_data = reinterpret_cast<q_callback_data*>(data);
  hsa_queue_t* queue = *(debug_data->qptr);
  //  check the queue id and user data
  ASSERT_EQ(source->id, queue->id);
  debug_data->trap = true;

  std::cout << kSubTestSeparator << std::endl;
  return;
}
