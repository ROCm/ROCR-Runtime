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
#include "core/inc/amd_cpu_agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/signal.h"
#include "core/inc/interrupt_signal.h"

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
struct ValidityError<amd::MemoryRegion*> {
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

namespace AMD {

hsa_status_t 
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

hsa_status_t hsa_amd_coherency_set_type(hsa_agent_t agent_handle,
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

hsa_status_t
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

hsa_status_t
    hsa_amd_memory_async_copy(void* dst, hsa_agent_t dst_agent_handle,
                              const void* src, hsa_agent_t src_agent_handle,
                              size_t size, uint32_t num_dep_signals,
                              const hsa_signal_t* dep_signals,
                              hsa_signal_t completion_signal) {
  if (dst == NULL || src == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if ((num_dep_signals == 0 && dep_signals != NULL) ||
      (num_dep_signals > 0 && dep_signals == NULL)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  core::Agent* dst_agent = core::Agent::Convert(dst_agent_handle);
  IS_VALID(dst_agent);

  core::Agent* src_agent = core::Agent::Convert(src_agent_handle);
  IS_VALID(src_agent);

  std::vector<core::Signal*> dep_signal_list(num_dep_signals);
  if (num_dep_signals > 0) {
    for (size_t i = 0; i < num_dep_signals; ++i) {
      core::Signal* dep_signal_obj = core::Signal::Convert(dep_signals[i]);
      IS_VALID(dep_signal_obj);
      dep_signal_list[i] = dep_signal_obj;
    }
  }

  core::Signal* out_signal_obj = core::Signal::Convert(completion_signal);
  IS_VALID(out_signal_obj);

  if (size > 0) {
    return core::Runtime::runtime_singleton_->CopyMemory(
        dst, *dst_agent, src, *src_agent, size, dep_signal_list,
        *out_signal_obj);
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t
    hsa_amd_profiling_set_profiler_enabled(hsa_queue_t* queue, int enable) {
  IS_OPEN();

  core::Queue* cmd_queue = core::Queue::Convert(queue);

  IS_VALID(cmd_queue);

  AMD_HSA_BITS_SET(cmd_queue->amd_queue_.queue_properties,
                   AMD_QUEUE_PROPERTIES_ENABLE_PROFILING, (enable != 0));

  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_amd_profiling_async_copy_enable(bool enable) {
  IS_OPEN();

  return core::Runtime::runtime_singleton_->IterateAgent(
      [](hsa_agent_t agent_handle, void* data) -> hsa_status_t {
        const bool enable = *(reinterpret_cast<bool*>(data));
        return core::Agent::Convert(agent_handle)->profiling_enabled(enable);
      },
      reinterpret_cast<void*>(&enable));
}

hsa_status_t hsa_amd_profiling_get_dispatch_time(
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

  // Translate timestamp from GPU to system domain.
  gpu_agent->TranslateTime(signal, *time);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_amd_profiling_get_async_copy_time(
    hsa_signal_t hsa_signal, hsa_amd_profiling_async_copy_time_t* time) {
  IS_OPEN();

  IS_BAD_PTR(time);

  core::Signal* signal = core::Signal::Convert(hsa_signal);

  IS_VALID(signal);

  core::Agent* agent = signal->async_copy_agent();

  if (agent == NULL) {
    return HSA_STATUS_ERROR;
  }

  if (agent->device_type() == core::Agent::DeviceType::kAmdGpuDevice) {
    // Translate timestamp from GPU to system domain.
    static_cast<amd::GpuAgentInt*>(agent)->TranslateTime(signal, *time);
    return HSA_STATUS_SUCCESS;
  }

  // The timestamp is already in system domain.
  time->start = signal->signal_.start_ts;
  time->end = signal->signal_.end_ts;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t
    hsa_amd_profiling_convert_tick_to_system_domain(hsa_agent_t agent_handle,
                                                    uint64_t agent_tick,
                                                    uint64_t* system_tick) {
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

uint32_t
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

hsa_status_t
    hsa_amd_signal_async_handler(hsa_signal_t hsa_signal,
                                 hsa_signal_condition_t cond,
                                 hsa_signal_value_t value,
                                 hsa_amd_signal_handler handler, void* arg) {
  IS_OPEN();

  core::Signal* signal = core::Signal::Convert(hsa_signal);
  IS_VALID(signal);
  IS_BAD_PTR(handler);
  if (core::g_use_interrupt_wait && (!core::InterruptSignal::IsType(signal)))
    return HSA_STATUS_ERROR_INVALID_SIGNAL;
  return core::Runtime::runtime_singleton_->SetAsyncSignalHandler(
      hsa_signal, cond, value, handler, arg);
}

hsa_status_t
    hsa_amd_async_function(void (*callback)(void* arg), void* arg) {
  IS_OPEN();

  IS_BAD_PTR(callback);
  static const hsa_signal_t null_signal = {0};
  return core::Runtime::runtime_singleton_->SetAsyncSignalHandler(
      null_signal, HSA_SIGNAL_CONDITION_EQ, 0, (hsa_amd_signal_handler)callback,
      arg);
}

hsa_status_t hsa_amd_queue_cu_set_mask(const hsa_queue_t* queue,
                                               uint32_t num_cu_mask_count,
                                               const uint32_t* cu_mask) {
  IS_OPEN();
  IS_BAD_PTR(cu_mask);

  core::Queue* cmd_queue = core::Queue::Convert(queue);
  IS_VALID(cmd_queue);
  return cmd_queue->SetCUMasking(num_cu_mask_count, cu_mask);
}

hsa_status_t hsa_amd_memory_lock(void* host_ptr, size_t size,
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

  const amd::MemoryRegion* system_region =
      reinterpret_cast<const amd::MemoryRegion*>(
          core::Runtime::runtime_singleton_->system_regions_fine()[0]);

  return system_region->Lock(num_agent, agents, host_ptr, size, agent_ptr);
}

hsa_status_t hsa_amd_memory_unlock(void* host_ptr) {
  IS_OPEN();

  const amd::MemoryRegion* system_region =
      reinterpret_cast<const amd::MemoryRegion*>(
          core::Runtime::runtime_singleton_->system_regions_fine()[0]);

  return system_region->Unlock(host_ptr);
}

hsa_status_t
    hsa_amd_memory_pool_get_info(hsa_amd_memory_pool_t memory_pool,
                                 hsa_amd_memory_pool_info_t attribute,
                                 void* value) {
  IS_OPEN();
  IS_BAD_PTR(value);

  hsa_region_t region = {memory_pool.handle};
  const amd::MemoryRegion* mem_region = amd::MemoryRegion::Convert(region);
  if (mem_region == NULL) {
    return (hsa_status_t)HSA_STATUS_ERROR_INVALID_MEMORY_POOL;
  }

  return mem_region->GetPoolInfo(attribute, value);
}

hsa_status_t hsa_amd_agent_iterate_memory_pools(
    hsa_agent_t agent_handle,
    hsa_status_t (*callback)(hsa_amd_memory_pool_t memory_pool, void* data),
    void* data) {
  IS_OPEN();
  IS_BAD_PTR(callback);
  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  if (agent->device_type() == core::Agent::kAmdCpuDevice) {
    return reinterpret_cast<const amd::CpuAgent*>(agent)->VisitRegion(
        false, reinterpret_cast<hsa_status_t (*)(hsa_region_t memory_pool,
                                                 void* data)>(callback),
        data);
  }

  return reinterpret_cast<const amd::GpuAgentInt*>(agent)->VisitRegion(
      false,
      reinterpret_cast<hsa_status_t (*)(hsa_region_t memory_pool, void* data)>(
          callback),
      data);
}

hsa_status_t
    hsa_amd_memory_pool_allocate(hsa_amd_memory_pool_t memory_pool, size_t size,
                                 uint32_t flags, void** ptr) {
  IS_OPEN();

  if (size == 0 || ptr == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  hsa_region_t region = {memory_pool.handle};
  const core::MemoryRegion* mem_region = core::MemoryRegion::Convert(region);

  if (mem_region == NULL || !mem_region->IsValid()) {
    return (hsa_status_t)HSA_STATUS_ERROR_INVALID_MEMORY_POOL;
  }

  return core::Runtime::runtime_singleton_->AllocateMemory(
      mem_region, size, core::MemoryRegion::AllocateRestrict, ptr);
}

hsa_status_t hsa_amd_memory_pool_free(void* ptr) {
  return HSA::hsa_memory_free(ptr);
}

hsa_status_t
    hsa_amd_agents_allow_access(uint32_t num_agents, const hsa_agent_t* agents,
                                const uint32_t* flags, const void* ptr) {
  IS_OPEN();

  if (num_agents == 0 || agents == NULL || flags != NULL || ptr == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return core::Runtime::runtime_singleton_->AllowAccess(num_agents, agents,
                                                        ptr);
}

hsa_status_t
    hsa_amd_memory_pool_can_migrate(hsa_amd_memory_pool_t src_memory_pool,
                                    hsa_amd_memory_pool_t dst_memory_pool,
                                    bool* result) {
  IS_OPEN();

  if (result == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  hsa_region_t src_region_handle = {src_memory_pool.handle};
  const amd::MemoryRegion* src_mem_region =
      amd::MemoryRegion::Convert(src_region_handle);

  if (src_mem_region == NULL || !src_mem_region->IsValid()) {
    return static_cast<hsa_status_t>(HSA_STATUS_ERROR_INVALID_MEMORY_POOL);
  }

  hsa_region_t dst_region_handle = {dst_memory_pool.handle};
  const amd::MemoryRegion* dst_mem_region =
      amd::MemoryRegion::Convert(dst_region_handle);

  if (dst_mem_region == NULL || !dst_mem_region->IsValid()) {
    return static_cast<hsa_status_t>(HSA_STATUS_ERROR_INVALID_MEMORY_POOL);
  }

  return src_mem_region->CanMigrate(*dst_mem_region, *result);
}

hsa_status_t hsa_amd_memory_migrate(const void* ptr,
                                            hsa_amd_memory_pool_t memory_pool,
                                            uint32_t flags) {
  IS_OPEN();

  if (ptr == NULL || flags != 0) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  hsa_region_t dst_region_handle = {memory_pool.handle};
  const amd::MemoryRegion* dst_mem_region =
      amd::MemoryRegion::Convert(dst_region_handle);

  if (dst_mem_region == NULL || !dst_mem_region->IsValid()) {
    return static_cast<hsa_status_t>(HSA_STATUS_ERROR_INVALID_MEMORY_POOL);
  }

  return dst_mem_region->Migrate(flags, ptr);
}

hsa_status_t hsa_amd_agent_memory_pool_get_info(
    hsa_agent_t agent_handle, hsa_amd_memory_pool_t memory_pool,
    hsa_amd_agent_memory_pool_info_t attribute, void* value) {
  IS_OPEN();

  if (value == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  hsa_region_t region_handle = {memory_pool.handle};
  const amd::MemoryRegion* mem_region =
      amd::MemoryRegion::Convert(region_handle);

  if (mem_region == NULL || !mem_region->IsValid()) {
    return static_cast<hsa_status_t>(HSA_STATUS_ERROR_INVALID_MEMORY_POOL);
  }

  return mem_region->GetAgentPoolInfo(*agent, attribute, value);
}

hsa_status_t hsa_amd_interop_map_buffer(uint32_t num_agents,
                                        hsa_agent_t* agents, int interop_handle,
                                        uint32_t flags, size_t* size,
                                        void** ptr, size_t* metadata_size,
                                        const void** metadata) {
  static const int tinyArraySize=8;
  IS_OPEN();
  IS_BAD_PTR(agents);
  IS_BAD_PTR(size);
  IS_BAD_PTR(ptr);
  if (flags != 0) return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  if (num_agents == 0) return HSA_STATUS_ERROR_INVALID_ARGUMENT;

  core::Agent* short_agents[tinyArraySize];
  core::Agent** core_agents = short_agents;
  if (num_agents > tinyArraySize) {
    core_agents = new core::Agent* [num_agents];
    if (core_agents == NULL) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  for (unsigned i = 0; i < num_agents; i++) {
    core::Agent* device = core::Agent::Convert(agents[i]);
    IS_VALID(device);
    core_agents[i] = device;
  }

  auto ret = core::Runtime::runtime_singleton_->InteropMap(
      num_agents, core_agents, interop_handle, flags, size, ptr, metadata_size,
      metadata);

  if (num_agents > tinyArraySize) delete[] core_agents;
  return ret;
}

hsa_status_t hsa_amd_interop_unmap_buffer(void* ptr) {
  IS_OPEN();
  if (ptr != NULL) core::Runtime::runtime_singleton_->InteropUnmap(ptr);
  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_amd_pointer_info(void* ptr, hsa_amd_pointer_info_t* info, void* (*alloc)(size_t),
                              uint32_t* num_accessible, hsa_agent_t** accessible) {
  IS_OPEN();
  IS_BAD_PTR(ptr);
  IS_BAD_PTR(info);
  return core::Runtime::runtime_singleton_->PtrInfo(ptr, info, alloc, num_accessible, accessible);
}

hsa_status_t hsa_amd_pointer_info_set_userdata(void* ptr, void* userdata) {
  IS_OPEN();
  IS_BAD_PTR(ptr);
  return core::Runtime::runtime_singleton_->SetPtrInfoData(ptr, userdata);
}

hsa_status_t hsa_amd_ipc_memory_create(void* ptr, size_t len, hsa_amd_ipc_memory_t* handle) {
  IS_OPEN();
  IS_BAD_PTR(ptr);
  IS_BAD_PTR(handle);
  return core::Runtime::runtime_singleton_->IPCCreate(ptr, len, handle);
}

hsa_status_t hsa_amd_ipc_memory_attach(const hsa_amd_ipc_memory_t* ipc, size_t len,
                                       uint32_t num_agents, const hsa_agent_t* mapping_agents,
                                       void** mapped_ptr) {
  static const int tinyArraySize = 8;
  IS_OPEN();
  IS_BAD_PTR(mapped_ptr);
  if (num_agents != 0) IS_BAD_PTR(mapping_agents);

  core::Agent** core_agents = nullptr;
  if (num_agents > tinyArraySize)
    core_agents = new core::Agent*[num_agents];
  else
    core_agents = (core::Agent**)alloca(sizeof(core::Agent*) * num_agents);
  if (core_agents == NULL) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  MAKE_SCOPE_GUARD([&]() {
    if (num_agents > tinyArraySize) delete[] core_agents;
  });

  for (unsigned i = 0; i < num_agents; i++) {
    core::Agent* device = core::Agent::Convert(mapping_agents[i]);
    IS_VALID(device);
    core_agents[i] = device;
  }

  return core::Runtime::runtime_singleton_->IPCAttach(ipc, len, num_agents, core_agents,
                                                      mapped_ptr);
}

hsa_status_t hsa_amd_ipc_memory_detach(void* mapped_ptr) {
  IS_OPEN();
  IS_BAD_PTR(mapped_ptr);
  return core::Runtime::runtime_singleton_->IPCDetach(mapped_ptr);
}

} // end of AMD namespace
