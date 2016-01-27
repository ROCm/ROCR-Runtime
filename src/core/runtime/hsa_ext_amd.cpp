////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
// 
// Copyright (c) 2014-2015, Advanced Micro Devices, Inc. All rights reserved.
// 
// Developed by:
// 
//                 AMD Research and AMD HSA Software Development
// 
//                 Advanced Micro Devices, Inc.
// 
//                 www.amd.com
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// 
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#include "hsakmt.h"

#include "core/inc/runtime.h"
#include "core/inc/agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/signal.h"

template <class T>
struct ValidityError;
template <>
struct ValidityError<core::Signal*> {
  enum { value = HSA_STATUS_ERROR_INVALID_SIGNAL };
};

template <>
struct ValidityError<core::Agent*> {
  enum { value = HSA_STATUS_ERROR_INVALID_AGENT };
};

template <>
struct ValidityError<core::MemoryRegion*> {
  enum { value = HSA_STATUS_ERROR_INVALID_REGION };
};

template <>
struct ValidityError<core::Queue*> {
  enum { value = HSA_STATUS_ERROR_INVALID_QUEUE };
};

template <class T>
struct ValidityError<const T*> {
  enum { value = ValidityError<T*>::value };
};

#define IS_BAD_PTR(ptr)                                          \
  do {                                                           \
    if ((ptr) == NULL) return HSA_STATUS_ERROR_INVALID_ARGUMENT; \
  } while (false)

#define IS_VALID(ptr)                                           \
  do {                                                          \
    if ((ptr) == NULL || !(ptr)->IsValid())                     \
      return hsa_status_t(ValidityError<decltype(ptr)>::value); \
  } while (false)

#define CHECK_ALLOC(ptr)                                         \
  do {                                                           \
    if ((ptr) == NULL) return HSA_STATUS_ERROR_OUT_OF_RESOURCES; \
  } while (false)

#define IS_OPEN()                                     \
  do {                                                \
    if (!core::Runtime::runtime_singleton_->IsOpen()) \
      return HSA_STATUS_ERROR_NOT_INITIALIZED;        \
  } while (false)

template <class T>
static __forceinline bool IsValid(T* ptr) {
  return (ptr == NULL) ? NULL : ptr->IsValid();
}

hsa_status_t HSA_API
    hsa_amd_coherency_get_type(hsa_agent_t agent_handle,
                               hsa_amd_coherency_type_t* type) {
  IS_OPEN();

  const core::Agent* agent = core::Agent::Convert(agent_handle);

  IS_VALID(agent);

  IS_BAD_PTR(type);

  if (agent->device_type() != core::Agent::kAmdGpuDevice) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  const amd::GpuAgentInt* gpu_agent =
      static_cast<const amd::GpuAgentInt*>(agent);

  *type = gpu_agent->current_coherency_type();

  return HSA_STATUS_SUCCESS;
}

hsa_status_t HSA_API hsa_amd_coherency_set_type(hsa_agent_t agent_handle,
                                                hsa_amd_coherency_type_t type) {
  IS_OPEN();

  core::Agent* agent = core::Agent::Convert(agent_handle);

  IS_VALID(agent);

  if (type < HSA_AMD_COHERENCY_TYPE_COHERENT ||
      type > HSA_AMD_COHERENCY_TYPE_NONCOHERENT) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (agent->device_type() != core::Agent::kAmdGpuDevice) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  amd::GpuAgent* gpu_agent = static_cast<amd::GpuAgent*>(agent);

  if (!gpu_agent->current_coherency_type(type)) {
    return HSA_STATUS_ERROR;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t HSA_API
    hsa_amd_memory_fill(void* ptr, uint32_t value, size_t count) {
  IS_OPEN();

  if (ptr == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (count == 0) {
    return HSA_STATUS_SUCCESS;
  }

  return core::Runtime::runtime_singleton_->FillMemory(ptr, value, count);
}

hsa_status_t HSA_API
    hsa_amd_memory_async_copy(void* dst, const void* src, size_t size,
                              hsa_agent_t copy_agent, uint32_t num_dep_signals,
                              const hsa_signal_t* dep_signals,
                              hsa_signal_t completion_signal) {
  // TODO(bwicakso): intermittent soft hang when interrupt signal is used on
  // the completion signal. The SDMA interrupt packet is not handled yet.
  if (dst == NULL || src == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if ((num_dep_signals == 0 && dep_signals != NULL) ||
      (num_dep_signals > 0 && dep_signals == NULL)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  core::Agent* agent = core::Agent::Convert(copy_agent);
  IS_VALID(agent);

  std::vector<core::Signal*> dep_signal_list(num_dep_signals);
  if (num_dep_signals > 0) {
    for (size_t i = 0; i < size; ++i) {
      core::Signal* dep_signal_obj = core::Signal::Convert(dep_signals[i]);
      IS_VALID(dep_signal_obj);
      dep_signal_list[i] = dep_signal_obj;
    }
  }

  core::Signal* out_signal_obj = core::Signal::Convert(completion_signal);
  IS_VALID(out_signal_obj);

  if (size > 0) {
    return core::Runtime::runtime_singleton_->CopyMemory(
        *agent, dst, src, size, dep_signal_list, *out_signal_obj);
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t HSA_API
    hsa_amd_profiling_set_profiler_enabled(hsa_queue_t* queue, int enable) {
  IS_OPEN();

  core::Queue* cmd_queue = core::Queue::Convert(queue);

  IS_VALID(cmd_queue);

  AMD_HSA_BITS_SET(cmd_queue->amd_queue_.queue_properties,
                   AMD_QUEUE_PROPERTIES_ENABLE_PROFILING, (enable != 0));

  return HSA_STATUS_SUCCESS;
}

hsa_status_t HSA_API hsa_amd_profiling_get_dispatch_time(
    hsa_agent_t agent_handle, hsa_signal_t hsa_signal,
    hsa_amd_profiling_dispatch_time_t* time) {
  IS_OPEN();

  IS_BAD_PTR(time);

  core::Agent* agent = core::Agent::Convert(agent_handle);

  IS_VALID(agent);

  core::Signal* signal = core::Signal::Convert(hsa_signal);

  IS_VALID(signal);

  if (agent->device_type() != core::Agent::kAmdGpuDevice) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  amd::GpuAgentInt* gpu_agent = static_cast<amd::GpuAgentInt*>(agent);

  gpu_agent->TranslateTime(signal, *time);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t HSA_API hsa_amd_profiling_convert_tick_to_system_domain(
    hsa_agent_t agent_handle, uint64_t agent_tick, uint64_t* system_tick) {
  IS_OPEN();

  IS_BAD_PTR(system_tick);

  core::Agent* agent = core::Agent::Convert(agent_handle);

  IS_VALID(agent);

  if (agent->device_type() != core::Agent::kAmdGpuDevice) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  amd::GpuAgentInt* gpu_agent = static_cast<amd::GpuAgentInt*>(agent);

  *system_tick = gpu_agent->TranslateTime(agent_tick);

  return HSA_STATUS_SUCCESS;
}

uint32_t HSA_API
    hsa_amd_signal_wait_any(uint32_t signal_count, hsa_signal_t* hsa_signals,
                            hsa_signal_condition_t* conds,
                            hsa_signal_value_t* values, uint64_t timeout_hint,
                            hsa_wait_state_t wait_hint,
                            hsa_signal_value_t* satisfying_value) {
  // Do not check for signal invalidation.  Invalidation may occur during async
  // signal handler loop and is not an error.
  for (uint i = 0; i < signal_count; i++)
    assert(hsa_signals[i].handle != 0 &&
           static_cast<core::Checked<0x71FCCA6A3D5D5276>*>(
               core::Signal::Convert(hsa_signals[i]))->IsValid() &&
           "Invalid signal.");

  return core::Signal::WaitAny(signal_count, hsa_signals, conds, values,
                               timeout_hint, wait_hint, satisfying_value);
}

hsa_status_t HSA_API
    hsa_amd_signal_async_handler(hsa_signal_t hsa_signal,
                                 hsa_signal_condition_t cond,
                                 hsa_signal_value_t value,
                                 hsa_amd_signal_handler handler, void* arg) {
  IS_OPEN();

  core::Signal* signal = core::Signal::Convert(hsa_signal);
  IS_VALID(signal);
  IS_BAD_PTR(handler);

  return core::Runtime::runtime_singleton_->SetAsyncSignalHandler(
      hsa_signal, cond, value, handler, arg);
}

hsa_status_t HSA_API hsa_amd_queue_cu_set_mask(const hsa_queue_t* queue,
                                               uint32_t num_cu_mask_count,
                                               const uint32_t* cu_mask) {
  IS_OPEN();
  IS_BAD_PTR(cu_mask);

  core::Queue* cmd_queue = core::Queue::Convert(queue);
  IS_VALID(cmd_queue);
  return cmd_queue->SetCUMasking(num_cu_mask_count, cu_mask);
}

hsa_status_t HSA_API hsa_amd_memory_lock(void* host_ptr, size_t size,
                                         hsa_agent_t* agents, int num_agent,
                                         void** agent_ptr) {
  *agent_ptr = NULL;

  IS_OPEN();

  if (size == 0 || host_ptr == NULL || agent_ptr == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if ((agents != NULL && num_agent == 0) ||
      (agents == NULL && num_agent != 0)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  static const size_t kCacheAlignment = 64;
  if (!IsMultipleOf(host_ptr, kCacheAlignment)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  std::vector<HSAuint32> nodes(num_agent);
  for (int i = 0; i < num_agent; ++i) {
    core::Agent* agent = core::Agent::Convert(agents[i]);
    if (agent == NULL || !agent->IsValid()) {
      return HSA_STATUS_ERROR_INVALID_AGENT;
    }

    nodes[i] = reinterpret_cast<amd::GpuAgentInt*>(agent)->node_id();
  }

  if (reinterpret_cast<amd::MemoryRegion*>(
          core::MemoryRegion::Convert(
              core::Runtime::runtime_singleton_->system_region()))
          ->full_profile()) {
    // For APU, any host pointer is always accessible by the gpu.
    *agent_ptr = host_ptr;
    return HSA_STATUS_SUCCESS;
  }

  const size_t num_node = nodes.size();
  uint32_t* node_array = (num_node > 0) ? &nodes[0] : NULL;
  if (amd::MemoryRegion::RegisterHostMemory(host_ptr, size, num_node,
                                            node_array)) {
    uint64_t alternate_va = 0;
    if (amd::MemoryRegion::MakeKfdMemoryResident(host_ptr, size,
                                                 &alternate_va)) {
      assert(alternate_va != 0);
      *agent_ptr = reinterpret_cast<void*>(alternate_va);
      return HSA_STATUS_SUCCESS;
    }
    amd::MemoryRegion::DeregisterHostMemory(host_ptr);
  }

  return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
}

hsa_status_t HSA_API hsa_amd_memory_unlock(void* host_ptr) {
  IS_OPEN();

  if (reinterpret_cast<amd::MemoryRegion*>(
          core::MemoryRegion::Convert(
              core::Runtime::runtime_singleton_->system_region()))
          ->full_profile()) {
    return HSA_STATUS_SUCCESS;
  }

  if (host_ptr != NULL) {
    amd::MemoryRegion::MakeKfdMemoryUnresident(host_ptr);
    amd::MemoryRegion::DeregisterHostMemory(host_ptr);
  }

  return HSA_STATUS_SUCCESS;
}
