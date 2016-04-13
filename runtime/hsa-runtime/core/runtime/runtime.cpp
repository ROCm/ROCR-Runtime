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

#include "core/inc/runtime.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "core/common/shared.h"

#include "core/inc/hsa_ext_interface.h"
#include "core/inc/amd_cpu_agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/amd_topology.h"
#include "core/inc/signal.h"
#include "core/inc/interrupt_signal.h"

#include "core/inc/hsa_api_trace_int.h"

#define HSA_VERSION_MAJOR 1
#define HSA_VERSION_MINOR 0

const char rocrbuildid[] = "ROCR BUILD ID: " STRING(ROCR_BUILD_ID);

namespace core {
bool g_use_interrupt_wait = true;

Runtime* Runtime::runtime_singleton_ = NULL;

KernelMutex Runtime::bootstrap_lock_;

static bool loaded = true;

class RuntimeCleanup {
 public:
  ~RuntimeCleanup() {
    if (!Runtime::IsOpen()) {
      delete Runtime::runtime_singleton_;
    }

    loaded = false;
  }
};

static RuntimeCleanup cleanup_at_unload_;

bool Runtime::Acquire() {
  // Check to see if HSA has been cleaned up (process exit)
  if (!loaded) return false;

  // Handle initialization races
  ScopedAcquire<KernelMutex> boot(&bootstrap_lock_);

  if (runtime_singleton_ == NULL) {
    runtime_singleton_ = new Runtime();
  }

  // Serialize with release
  ScopedAcquire<KernelMutex> lock(&runtime_singleton_->kernel_lock_);

  if (runtime_singleton_->ref_count_ == INT32_MAX) {
    return false;
  }

  runtime_singleton_->ref_count_++;

  if (runtime_singleton_->ref_count_ == 1) {
    runtime_singleton_->Load();
  }

  return true;
}

bool Runtime::Release() {
  ScopedAcquire<KernelMutex> lock(&kernel_lock_);
  if (ref_count_ == 0) {
    return false;
  }

  if (ref_count_ == 1) {
    // Release all registered memory, then unload backends
    Unload();
  }

  ref_count_--;

  return true;
}

bool Runtime::IsOpen() {
  return (Runtime::runtime_singleton_ != NULL) &&
         (Runtime::runtime_singleton_->ref_count_ != 0);
}

void Runtime::RegisterAgent(Agent* agent) {
  if (agent->device_type() == Agent::DeviceType::kAmdCpuDevice) {
    cpu_agents_.push_back(agent);

    // Add cpu regions to the system region list.
    for (const core::MemoryRegion* region : agent->regions()) {
      if (region->fine_grain()) {
        system_regions_fine_.push_back(region);
      } else {
        system_regions_coarse_.push_back(region);
      }
    }

    assert(system_regions_fine_.size() > 0);

    // Init default fine grain system region allocator using fine grain
    // system region of the first discovered CPU agent.
    if (cpu_agents_.size() == 1) {
      if (system_regions_fine_[0]->full_profile()) {
        system_allocator_ = [](size_t size, size_t alignment) -> void * {
          return _aligned_malloc(size, alignment);
        };

        system_deallocator_ = [](void* ptr) { _aligned_free(ptr); };
      } else {
        // Might need memory pooling to cover allocation that
        // requires less than 4096 bytes.
        system_allocator_ = [&](size_t size, size_t alignment) -> void * {
          assert(alignment <= 4096);
          void* ptr = NULL;
          return (HSA_STATUS_SUCCESS ==
                  core::Runtime::runtime_singleton_->AllocateMemory(
                      system_regions_fine_[0], size, &ptr))
                     ? ptr
                     : NULL;
        };

        system_deallocator_ = [](void* ptr) {
          core::Runtime::runtime_singleton_->FreeMemory(ptr);
        };
      }

      BaseShared::SetAllocateAndFree(system_allocator_, system_deallocator_);
    }

    // Setup system clock frequency for the first time.
    if (sys_clock_freq_ == 0) {
      // Cache system clock frequency
      HsaClockCounters clocks;
      hsaKmtGetClockCounters(0, &clocks);
      sys_clock_freq_ = clocks.SystemClockFrequencyHz;
      host_agent_ = agent;
    }
  } else if (agent->device_type() == Agent::DeviceType::kAmdGpuDevice) {
    gpu_agents_.push_back(agent);

    gpu_ids_.push_back(agent->node_id());

    // Assign the first discovered gpu agent as blit agent that will provide
    // DMA operation for hsa_memory_copy.
    if (blit_agent_ == NULL) {
      blit_agent_ = agent;

      // Query the start and end address of the SVM address space in this
      // platform.
      if (reinterpret_cast<amd::GpuAgentInt*>(blit_agent_)->profile() ==
          HSA_PROFILE_BASE) {
        std::vector<const core::MemoryRegion*>::const_iterator it =
            std::find_if(blit_agent_->regions().begin(),
                         blit_agent_->regions().end(),
                         [](const core::MemoryRegion* region) {
              return (
                  reinterpret_cast<const amd::MemoryRegion*>(region)->IsSvm());
            });

        assert(it != blit_agent_->regions().end());

        const amd::MemoryRegion* svm_region =
            reinterpret_cast<const amd::MemoryRegion*>(*it);

        start_svm_address_ =
            static_cast<uintptr_t>(svm_region->GetBaseAddress());
        end_svm_address_ = start_svm_address_ + svm_region->GetPhysicalSize();

        // Bind VM fault handler when we detect the first GPU agent.
        // TODO: validate if it works on APU.
        BindVmFaultHandler();
      } else {
        start_svm_address_ = 0;
        end_svm_address_ = os::GetUserModeVirtualMemoryBase() +
                           os::GetUserModeVirtualMemorySize();
      }
    }
  }
}

void Runtime::DestroyAgents() {
  std::for_each(gpu_agents_.begin(), gpu_agents_.end(), DeleteObject());
  gpu_agents_.clear();

  gpu_ids_.clear();

  std::for_each(cpu_agents_.begin(), cpu_agents_.end(), DeleteObject());
  cpu_agents_.clear();

  blit_agent_ = NULL;

  system_regions_fine_.clear();
  system_regions_coarse_.clear();
}

void Runtime::SetLinkCount(size_t num_link) {
  const size_t last_index = GetIndexLinkInfo(0, num_link);
  link_matrix_.resize(last_index);

  memset(&link_matrix_[0], 0,
         link_matrix_.size() * sizeof(hsa_amd_memory_pool_link_info_t));
}

void Runtime::RegisterLinkInfo(uint32_t node_id_from, uint32_t node_id_to,
                               uint32_t num_hop,
                               hsa_amd_memory_pool_link_info_t& link_info) {
  const uint32_t idx = GetIndexLinkInfo(node_id_from, node_id_to);
  link_matrix_[idx].num_hop = num_hop;
  link_matrix_[idx].info = link_info;
}

const Runtime::LinkInfo Runtime::GetLinkInfo(uint32_t node_id_from,
                                             uint32_t node_id_to) {
  return (node_id_from != node_id_to)
             ? link_matrix_[GetIndexLinkInfo(node_id_from, node_id_to)]
             : LinkInfo();  // No link.
}

uint32_t Runtime::GetIndexLinkInfo(uint32_t node_id_from, uint32_t node_id_to) {
  const uint32_t node_id_max = std::max(node_id_from, node_id_to) - 1;
  const uint32_t node_id_min = std::min(node_id_from, node_id_to);
  return ((node_id_max * (node_id_max + 1) / 2) + node_id_min);
}

hsa_status_t Runtime::IterateAgent(hsa_status_t (*callback)(hsa_agent_t agent,
                                                            void* data),
                                   void* data) {
  if (!IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }

  std::vector<core::Agent*>* agent_lists[2] = {&cpu_agents_, &gpu_agents_};
  for (std::vector<core::Agent*>* agent_list : agent_lists) {
    for (size_t i = 0; i < agent_list->size(); ++i) {
      hsa_agent_t agent = Agent::Convert(agent_list->at(i));
      hsa_status_t status = callback(agent, data);

      if (status != HSA_STATUS_SUCCESS) {
        return status;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::AllocateMemory(const MemoryRegion* region, size_t size,
                                     void** ptr) {
  return AllocateMemory(false, region, size, ptr);
}

hsa_status_t Runtime::AllocateMemory(bool restrict_access,
                                     const MemoryRegion* region, size_t size,
                                     void** address) {
  const amd::MemoryRegion* amd_region =
      reinterpret_cast<const amd::MemoryRegion*>(region);
  hsa_status_t status = amd_region->Allocate(restrict_access, size, address);

  // Track the allocation result so that it could be freed properly.
  if (status == HSA_STATUS_SUCCESS) {
    ScopedAcquire<KernelMutex> lock(&memory_lock_);
    allocation_map_[*address] = AllocationRegion(region, size);
  }

  return status;
}

hsa_status_t Runtime::FreeMemory(void* ptr) {
  if (ptr == NULL) {
    return HSA_STATUS_SUCCESS;
  }

  const MemoryRegion* region = NULL;
  size_t size = 0;
  {
    ScopedAcquire<KernelMutex> lock(&memory_lock_);

    std::map<const void*, AllocationRegion>::const_iterator it =
        allocation_map_.find(ptr);

    if (it == allocation_map_.end()) {
      assert(false && "Can't find address in allocation map");
      return HSA_STATUS_ERROR;
    }

    region = it->second.region;
    size = it->second.size;

    allocation_map_.erase(it);
  }

  return region->Free(ptr, size);
}

hsa_status_t Runtime::CopyMemory(void* dst, const void* src, size_t size) {
  assert(dst != NULL && src != NULL && size != 0);

  bool is_src_system = false;
  bool is_dst_system = false;
  const uintptr_t src_uptr = reinterpret_cast<uintptr_t>(src);
  const uintptr_t dst_uptr = reinterpret_cast<uintptr_t>(dst);

  if ((reinterpret_cast<amd::GpuAgentInt*>(blit_agent_)->profile() ==
       HSA_PROFILE_FULL)) {
    is_src_system = (src_uptr < end_svm_address_);
    is_dst_system = (dst_uptr < end_svm_address_);
  } else {
    is_src_system =
        ((src_uptr < start_svm_address_) || (src_uptr >= end_svm_address_));
    is_dst_system =
        ((dst_uptr < start_svm_address_) || (dst_uptr >= end_svm_address_));

    if ((is_src_system && !is_dst_system) ||
        (!is_src_system && is_dst_system)) {
      // Use staging buffer or pin if either src or dst is gpuvm and the other
      // is system memory allocated via OS or C/C++ allocator.
      return CopyMemoryHostAlloc(dst, src, size, is_dst_system);
    }
  }

  if (is_src_system && is_dst_system) {
    memmove(dst, src, size);
    return HSA_STATUS_SUCCESS;
  }

  return blit_agent_->DmaCopy(dst, src, size);
}

hsa_status_t Runtime::CopyMemoryHostAlloc(void* dst, const void* src,
                                          size_t size, bool dst_malloc) {
  void* usrptr = (dst_malloc) ? dst : const_cast<void*>(src);
  void* agent_ptr = NULL;

  hsa_agent_t blit_agent = core::Agent::Convert(blit_agent_);

  const amd::MemoryRegion* system_region =
      reinterpret_cast<const amd::MemoryRegion*>(system_regions_fine_[0]);
  hsa_status_t stat =
      system_region->Lock(1, &blit_agent, usrptr, size, &agent_ptr);

  if (stat != HSA_STATUS_SUCCESS) {
    return stat;
  }

  stat = blit_agent_->DmaCopy((dst_malloc) ? agent_ptr : dst,
                              (dst_malloc) ? src : agent_ptr, size);

  system_region->Unlock(usrptr);

  return stat;
}

hsa_status_t Runtime::CopyMemory(void* dst, core::Agent& dst_agent,
                                 const void* src, core::Agent& src_agent,
                                 size_t size,
                                 std::vector<core::Signal*>& dep_signals,
                                 core::Signal& completion_signal) {
  const bool dst_gpu =
      (dst_agent.device_type() == core::Agent::DeviceType::kAmdGpuDevice);
  const bool src_gpu =
      (src_agent.device_type() == core::Agent::DeviceType::kAmdGpuDevice);
  if (dst_gpu || src_gpu) {
    core::Agent& copy_agent = (src_gpu) ? src_agent : dst_agent;
    return copy_agent.DmaCopy(dst, dst_agent, src, src_agent, size, dep_signals,
                              completion_signal);
  }

  // For cpu to cpu, fire and forget a copy thread.
  std::thread([](void* dst, const void* src, size_t size,
                 std::vector<core::Signal*> dep_signals,
                 core::Signal* completion_signal) {
                for (core::Signal* dep : dep_signals) {
                  dep->WaitRelaxed(HSA_SIGNAL_CONDITION_EQ, 0, UINT64_MAX,
                                   HSA_WAIT_STATE_BLOCKED);
                }

                memcpy(dst, src, size);

                completion_signal->SubRelease(1);
              },
              dst, src, size, dep_signals, &completion_signal).detach();

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::FillMemory(void* ptr, uint32_t value, size_t count) {
  assert(blit_agent_ != NULL);
  return blit_agent_->DmaFill(ptr, value, count);
}

hsa_status_t Runtime::AllowAccess(uint32_t num_agents,
                                  const hsa_agent_t* agents, const void* ptr) {
  const amd::MemoryRegion* amd_region = NULL;
  size_t alloc_size = 0;

  {
    ScopedAcquire<KernelMutex> lock(&memory_lock_);

    std::map<const void*, AllocationRegion>::const_iterator it =
        allocation_map_.find(ptr);

    if (it == allocation_map_.end()) {
      return HSA_STATUS_ERROR;
    }

    amd_region = reinterpret_cast<const amd::MemoryRegion*>(it->second.region);
    alloc_size = it->second.size;
  }

  return amd_region->AllowAccess(num_agents, agents, ptr, alloc_size);
}

hsa_status_t Runtime::GetSystemInfo(hsa_system_info_t attribute, void* value) {
  switch (attribute) {
    case HSA_SYSTEM_INFO_VERSION_MAJOR:
      *((uint16_t*)value) = HSA_VERSION_MAJOR;
      break;
    case HSA_SYSTEM_INFO_VERSION_MINOR:
      *((uint16_t*)value) = HSA_VERSION_MINOR;
      break;
    case HSA_SYSTEM_INFO_TIMESTAMP: {
      HsaClockCounters clocks;
      hsaKmtGetClockCounters(0, &clocks);
      *((uint64_t*)value) = clocks.SystemClockCounter;
      break;
    }
    case HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY: {
      assert(sys_clock_freq_ != 0 &&
             "Use of HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY before HSA "
             "initialization completes.");
      *(uint64_t*)value = sys_clock_freq_;
      break;
    }
    case HSA_SYSTEM_INFO_SIGNAL_MAX_WAIT:
      *((uint64_t*)value) = 0xFFFFFFFFFFFFFFFF;
      break;
    case HSA_SYSTEM_INFO_ENDIANNESS:
#if defined(HSA_LITTLE_ENDIAN)
      *((hsa_endianness_t*)value) = HSA_ENDIANNESS_LITTLE;
#else
      *((hsa_endianness_t*)value) = HSA_ENDIANNESS_BIG;
#endif
      break;
    case HSA_SYSTEM_INFO_MACHINE_MODEL:
#if defined(HSA_LARGE_MODEL)
      *((hsa_machine_model_t*)value) = HSA_MACHINE_MODEL_LARGE;
#else
      *((hsa_machine_model_t*)value) = HSA_MACHINE_MODEL_SMALL;
#endif
      break;
    case HSA_SYSTEM_INFO_EXTENSIONS:
      memset(value, 0, sizeof(uint8_t) * 128);

      if (extensions_.table.hsa_ext_program_finalize_fn != NULL) {
        *((uint8_t*)value) = 1 << HSA_EXTENSION_FINALIZER;
      }

      if (extensions_.table.hsa_ext_image_create_fn != NULL) {
        *((uint8_t*)value) |= 1 << HSA_EXTENSION_IMAGES;
      }

      *((uint8_t*)value) |= 1 << HSA_EXTENSION_AMD_PROFILER;

      break;
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
}

uint32_t Runtime::GetQueueId() { return atomic::Increment(&queue_count_); }

hsa_status_t Runtime::SetAsyncSignalHandler(hsa_signal_t signal,
                                            hsa_signal_condition_t cond,
                                            hsa_signal_value_t value,
                                            hsa_amd_signal_handler handler,
                                            void* arg) {
  // Asyncronous signal handler is only supported when KFD events are on.
  if (!core::g_use_interrupt_wait) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;

  // Indicate that this signal is in use.
  if (signal.handle != 0) hsa_signal_handle(signal)->Retain();

  ScopedAcquire<KernelMutex> scope_lock(&async_events_control_.lock);

  // Lazy initializer
  if (async_events_control_.async_events_thread_ == NULL) {
    // Create monitoring thread control signal
    auto err = HSA::hsa_signal_create(0, 0, NULL, &async_events_control_.wake);
    if (err != HSA_STATUS_SUCCESS) {
      assert(false && "Asyncronous events control signal creation error.");
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
    async_events_.PushBack(async_events_control_.wake, HSA_SIGNAL_CONDITION_NE,
                           0, NULL, NULL);

    // Start event monitoring thread
    async_events_control_.exit = false;
    async_events_control_.async_events_thread_ =
        os::CreateThread(AsyncEventsLoop, NULL);
    if (async_events_control_.async_events_thread_ == NULL) {
      assert(false && "Asyncronous events thread creation error.");
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  }

  new_async_events_.PushBack(signal, cond, value, handler, arg);

  hsa_signal_handle(async_events_control_.wake)->StoreRelease(1);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::InteropMap(uint32_t num_agents, Agent** agents,
                                 int interop_handle, uint32_t flags,
                                 size_t* size, void** ptr,
                                 size_t* metadata_size, const void** metadata) {
  HsaGraphicsResourceInfo info;

  HSAuint32 short_nodes[64];
  HSAuint32* nodes = short_nodes;
  if (num_agents > 64) {
    nodes = new HSAuint32[num_agents];
    if (nodes == NULL) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }
  MAKE_SCOPE_GUARD([&]() {
    if (num_agents > 64) delete[] nodes;
  });

  for (int i = 0; i < num_agents; i++)
    agents[i]->GetInfo((hsa_agent_info_t)HSA_AMD_AGENT_INFO_DRIVER_NODE_ID,
                       &nodes[i]);

  if (hsaKmtRegisterGraphicsHandleToNodes(interop_handle, &info, num_agents,
                                          nodes) != HSAKMT_STATUS_SUCCESS)
    return HSA_STATUS_ERROR;

  HSAuint64 altAddress;
  HsaMemMapFlags map_flags;
  map_flags.Value = 0;
  map_flags.ui32.PageSize = HSA_PAGE_SIZE_64KB;
  if (hsaKmtMapMemoryToGPUNodes(info.MemoryAddress, info.SizeInBytes,
                                &altAddress, map_flags, num_agents,
                                nodes) != HSAKMT_STATUS_SUCCESS) {
    map_flags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    if (hsaKmtMapMemoryToGPUNodes(info.MemoryAddress, info.SizeInBytes,
                                  &altAddress, map_flags, num_agents,
                                  nodes) != HSAKMT_STATUS_SUCCESS)
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  if (metadata_size != NULL) *metadata_size = info.MetadataSizeInBytes;
  if (metadata != NULL) *metadata = info.Metadata;

  *size = info.SizeInBytes;
  *ptr = info.MemoryAddress;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::InteropUnmap(void* ptr)
{
  if(hsaKmtUnmapMemoryToGPU(ptr)!=HSAKMT_STATUS_SUCCESS)
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  if(hsaKmtDeregisterMemory(ptr)!=HSAKMT_STATUS_SUCCESS)
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  return HSA_STATUS_SUCCESS;
}

void Runtime::AsyncEventsLoop(void*) {
  auto& async_events_control_ = runtime_singleton_->async_events_control_;
  auto& async_events_ = runtime_singleton_->async_events_;
  auto& new_async_events_ = runtime_singleton_->new_async_events_;

  while (!async_events_control_.exit) {
    // Wait for a signal
    hsa_signal_value_t value;
    uint32_t index = hsa_amd_signal_wait_any(
        uint32_t(async_events_.Size()), &async_events_.signal_[0],
        &async_events_.cond_[0], &async_events_.value_[0], uint64_t(-1),
        HSA_WAIT_STATE_BLOCKED, &value);

    // Reset the control signal
    if (index == 0) {
      hsa_signal_handle(async_events_control_.wake)->StoreRelaxed(0);
    } else if (index != -1) {
      // No error or timout occured, process the handler
      assert(async_events_.handler_[index] != NULL);
      bool keep =
          async_events_.handler_[index](value, async_events_.arg_[index]);
      if (!keep) {
        hsa_signal_handle(async_events_.signal_[index])->Release();
        async_events_.CopyIndex(index, async_events_.Size() - 1);
        async_events_.PopBack();
      }
    }

    // Check for dead signals
    index = 0;
    while (index != async_events_.Size()) {
      if (!hsa_signal_handle(async_events_.signal_[index])->IsValid()) {
        hsa_signal_handle(async_events_.signal_[index])->Release();
        async_events_.CopyIndex(index, async_events_.Size() - 1);
        async_events_.PopBack();
        continue;
      }
      index++;
    }

    // Insert new signals and find plain functions
    typedef std::pair<void (*)(void*), void*> func_arg_t;
    std::vector<func_arg_t> functions;
    {
      ScopedAcquire<KernelMutex> scope_lock(&async_events_control_.lock);
      for (size_t i = 0; i < new_async_events_.Size(); i++) {
        if (new_async_events_.signal_[i].handle == 0) {
          functions.push_back(
              func_arg_t((void (*)(void*))new_async_events_.handler_[i],
                         new_async_events_.arg_[i]));
          continue;
        }
        async_events_.PushBack(
            new_async_events_.signal_[i], new_async_events_.cond_[i],
            new_async_events_.value_[i], new_async_events_.handler_[i],
            new_async_events_.arg_[i]);
      }
      new_async_events_.Clear();
    }

    // Call plain functions
    for (size_t i = 0; i < functions.size(); i++)
      functions[i].first(functions[i].second);
    functions.clear();
  }

  // Release wait count of all pending signals
  for (size_t i = 1; i < async_events_.Size(); i++)
    hsa_signal_handle(async_events_.signal_[i])->Release();
  async_events_.Clear();

  for (size_t i = 0; i < new_async_events_.Size(); i++)
    hsa_signal_handle(new_async_events_.signal_[i])->Release();
  new_async_events_.Clear();
}

void Runtime::BindVmFaultHandler() {
  if (core::g_use_interrupt_wait) {
    // Create memory event with manual reset to avoid racing condition
    // with driver in case of multiple concurrent VM faults.
    vm_fault_event_ =
        core::InterruptSignal::CreateEvent(HSA_EVENTTYPE_MEMORY, true);

    // Create an interrupt signal object to contain the memory event.
    // This signal object will be registered with the async handler global
    // thread.
    vm_fault_signal_ = new core::InterruptSignal(0, vm_fault_event_);

    if (!vm_fault_signal_->IsValid() || vm_fault_signal_->EopEvent() == NULL) {
      assert(false && "Failed on creating VM fault signal");
      return;
    }

    SetAsyncSignalHandler(core::Signal::Convert(vm_fault_signal_),
                          HSA_SIGNAL_CONDITION_NE, 0, VMFaultHandler,
                          reinterpret_cast<void*>(vm_fault_signal_));
  }
}

bool Runtime::VMFaultHandler(hsa_signal_value_t val, void* arg) {
  core::InterruptSignal* vm_fault_signal =
      reinterpret_cast<core::InterruptSignal*>(arg);

  assert(vm_fault_signal != NULL);

  if (vm_fault_signal == NULL) {
    return false;
  }

  if (runtime_singleton_->flag().enable_vm_fault_message()) {
    HsaEvent* vm_fault_event = vm_fault_signal->EopEvent();

    const HsaMemoryAccessFault& fault =
        vm_fault_event->EventData.EventData.MemoryAccessFault;

    std::string reason = "";
    if (fault.Failure.NotPresent == 1) {
      reason += "Page not present or supervisor privilege";
    } else if (fault.Failure.ReadOnly == 1) {
      reason += "Write access to a read-only page";
    } else if (fault.Failure.NoExecute == 1) {
      reason += "Execute access to a page marked NX";
    } else if (fault.Failure.GpuAccess == 1) {
      reason += "Host access only";
    } else if (fault.Failure.ECC == 1) {
      reason += "ECC failure (if supported by HW)";
    } else {
      reason += "Unknown";
    }

    fprintf(stderr,
            "Memory access fault by GPU node-%u on address %p%s. Reason: %s.\n",
            fault.NodeId, reinterpret_cast<const void*>(fault.VirtualAddress),
            (fault.Failure.Imprecise == 1) ? "(may not be exact address)" : "",
            reason.c_str());
  } else {
    assert(false && "GPU memory access fault.");
  }

  std::abort();

  // No need to keep the signal because we are done.
  return false;
}

Runtime::Runtime()
    : host_agent_(NULL),
      blit_agent_(NULL),
      queue_count_(0),
      sys_clock_freq_(0),
      vm_fault_event_(NULL),
      vm_fault_signal_(NULL),
      ref_count_(0) {
  start_svm_address_ = 0;
#if defined(HSA_LARGE_MODEL)
  end_svm_address_ = UINT64_MAX;
#else
  end_svm_address_ = UINT32_MAX;
#endif
}

void Runtime::Load() {
  flag_.Refresh();

  g_use_interrupt_wait = flag_.enable_interrupt();

  if (!amd::Load()) {
    return;
  }

  loader_ = amd::hsa::loader::Loader::Create(&loader_context_);

  // Load extensions
  LoadExtensions();

  // Load tools libraries
  LoadTools();
}

void Runtime::Unload() {
  UnloadTools();
  UnloadExtensions();

  amd::hsa::loader::Loader::Destroy(loader_);
  loader_ = nullptr;

  async_events_control_.Shutdown();

  delete vm_fault_signal_;
  core::InterruptSignal::DestroyEvent(vm_fault_event_);

  DestroyAgents();

  CloseTools();

  amd::Unload();
}

void Runtime::LoadExtensions() {
// Load finalizer and extension library
#ifdef HSA_LARGE_MODEL
  static const std::string kFinalizerLib[] = {"hsa-ext-finalize64.dll",
                                              "libhsa-ext-finalize64.so.1"};
  static const std::string kImageLib[] = {"hsa-ext-image64.dll",
                                          "libhsa-ext-image64.so.1"};
#else
  static const std::string kFinalizerLib[] = {"hsa-ext-finalize.dll",
                                              "libhsa-ext-finalize.so.1"};
  static const std::string kImageLib[] = {"hsa-ext-image.dll",
                                          "libhsa-ext-image.so.1"};
#endif
  extensions_.Load(kFinalizerLib[os_index(os::current_os)]);
  extensions_.Load(kImageLib[os_index(os::current_os)]);
}

void Runtime::UnloadExtensions() { extensions_.Unload(); }

static std::vector<std::string> parse_tool_names(std::string tool_names) {
  std::vector<std::string> names;
  std::string name = "";
  bool quoted = false;
  while (tool_names.size() != 0) {
    auto index = tool_names.find_first_of(" \"\\");
    if (index == std::string::npos) {
      name += tool_names;
      break;
    }
    switch (tool_names[index]) {
      case ' ': {
        if (!quoted) {
          name += tool_names.substr(0, index);
          tool_names.erase(0, index + 1);
          names.push_back(name);
          name = "";
        } else {
          name += tool_names.substr(0, index + 1);
          tool_names.erase(0, index + 1);
        }
        break;
      }
      case '\"': {
        if (quoted) {
          quoted = false;
          name += tool_names.substr(0, index);
          tool_names.erase(0, index + 1);
          names.push_back(name);
          name = "";
        } else {
          quoted = true;
          tool_names.erase(0, index + 1);
        }
        break;
      }
      case '\\': {
        if (tool_names.size() > index + 1) {
          name += tool_names.substr(0, index) + tool_names[index + 1];
          tool_names.erase(0, index + 2);
        }
        break;
      }
    }  // end switch
  }    // end while

  if (name != "") names.push_back(name);
  return names;
}

void Runtime::LoadTools() {
  typedef bool (*tool_init_t)(::ApiTable*, uint64_t, uint64_t,
                              const char* const*);
  typedef Agent* (*tool_wrap_t)(Agent*);
  typedef void (*tool_add_t)(Runtime*);

  // Link extensions to API interception
  hsa_api_table_.LinkExts(&extensions_.table);

  // Load tool libs
  std::string tool_names = flag_.tools_lib_names();
  if (tool_names != "") {
    std::vector<std::string> names = parse_tool_names(tool_names);
    std::vector<const char*> failed;
    for (int i = 0; i < names.size(); i++) {
      os::LibHandle tool = os::LoadLib(names[i]);

      if (tool != NULL) {
        tool_libs_.push_back(tool);

        tool_init_t ld;
        ld = (tool_init_t)os::GetExportAddress(tool, "OnLoad");
        if (ld) {
          if (!ld(&hsa_api_table_.table, 0, failed.size(), &failed[0])) {
            failed.push_back(names[i].c_str());
            os::CloseLib(tool);
            continue;
          }
        }

        tool_wrap_t wrap;
        wrap = (tool_wrap_t)os::GetExportAddress(tool, "WrapAgent");
        if (wrap) {
          std::vector<core::Agent*>* agent_lists[2] = {&cpu_agents_,
                                                       &gpu_agents_};
          for (std::vector<core::Agent*>* agent_list : agent_lists) {
            for (size_t agent_idx = 0; agent_idx < agent_list->size();
                 ++agent_idx) {
              Agent* agent = wrap(agent_list->at(agent_idx));
              if (agent != NULL) {
                assert(agent->IsValid() &&
                       "Agent returned from WrapAgent is not valid");
                agent_list->at(agent_idx) = agent;
              }
            }
          }
        }

        tool_add_t add;
        add = (tool_add_t)os::GetExportAddress(tool, "AddAgent");
        if (add) add(this);
      }
    }
  }
}

void Runtime::UnloadTools() {
  typedef void (*tool_unload_t)();
  for (size_t i = tool_libs_.size(); i != 0; i--) {
    tool_unload_t unld;
    unld = (tool_unload_t)os::GetExportAddress(tool_libs_[i - 1], "OnUnload");
    if (unld) unld();
  }

  // Reset API table in case some tool doesn't cleanup properly
  hsa_api_table_.Reset();
}

void Runtime::CloseTools() {
  // Due to valgrind bug, runtime cannot dlclose extensions see:
  // http://valgrind.org/docs/manual/faq.html#faq.unhelpful
  if (!flag_.running_valgrind()) {
    for (int i = 0; i < tool_libs_.size(); i++) os::CloseLib(tool_libs_[i]);
  }
  tool_libs_.clear();
}

void Runtime::AsyncEventsControl::Shutdown() {
  if (async_events_thread_ != NULL) {
    exit = true;
    hsa_signal_handle(wake)->StoreRelaxed(1);
    os::WaitForThread(async_events_thread_);
    os::CloseThread(async_events_thread_);
    async_events_thread_ = NULL;
    HSA::hsa_signal_destroy(wake);
  }
}

void Runtime::AsyncEvents::PushBack(hsa_signal_t signal,
                                    hsa_signal_condition_t cond,
                                    hsa_signal_value_t value,
                                    hsa_amd_signal_handler handler, void* arg) {
  signal_.push_back(signal);
  cond_.push_back(cond);
  value_.push_back(value);
  handler_.push_back(handler);
  arg_.push_back(arg);
}

void Runtime::AsyncEvents::CopyIndex(size_t dst, size_t src) {
  signal_[dst] = signal_[src];
  cond_[dst] = cond_[src];
  value_[dst] = value_[src];
  handler_[dst] = handler_[src];
  arg_[dst] = arg_[src];
}

size_t Runtime::AsyncEvents::Size() { return signal_.size(); }

void Runtime::AsyncEvents::PopBack() {
  signal_.pop_back();
  cond_.pop_back();
  value_.pop_back();
  handler_.pop_back();
  arg_.pop_back();
}

void Runtime::AsyncEvents::Clear() {
  signal_.clear();
  cond_.clear();
  value_.clear();
  handler_.clear();
  arg_.clear();
}

}  // namespace core
