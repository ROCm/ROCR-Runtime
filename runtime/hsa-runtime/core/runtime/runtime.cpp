////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
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
#include <vector>
#include <list>

#include "core/common/shared.h"
#include "core/inc/hsa_ext_interface.h"
#include "core/inc/amd_cpu_agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/amd_topology.h"
#include "core/inc/signal.h"
#include "core/inc/interrupt_signal.h"
#include "core/inc/hsa_ext_amd_impl.h"
#include "core/inc/hsa_api_trace_int.h"
#include "core/util/os.h"
#include "core/inc/exceptions.h"
#include "inc/hsa_ven_amd_aqlprofile.h"

#define HSA_VERSION_MAJOR 1
#define HSA_VERSION_MINOR 1

const char rocrbuildid[] __attribute__((used)) = "ROCR BUILD ID: " STRING(ROCR_BUILD_ID);

namespace rocr {
namespace core {
bool g_use_interrupt_wait = true;
bool g_use_mwaitx = true;

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

hsa_status_t Runtime::Acquire() {
  // Check to see if HSA has been cleaned up (process exit)
  if (!loaded) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;

  ScopedAcquire<KernelMutex> boot(&bootstrap_lock_);

  if (runtime_singleton_ == NULL) {
    runtime_singleton_ = new Runtime();
  }

  if (runtime_singleton_->ref_count_ == INT32_MAX) {
    return HSA_STATUS_ERROR_REFCOUNT_OVERFLOW;
  }

  runtime_singleton_->ref_count_++;
  MAKE_NAMED_SCOPE_GUARD(refGuard, [&]() { runtime_singleton_->ref_count_--; });

  if (runtime_singleton_->ref_count_ == 1) {
    hsa_status_t status = runtime_singleton_->Load();

    if (status != HSA_STATUS_SUCCESS) {
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  }

  refGuard.Dismiss();
  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::Release() {
  // Check to see if HSA has been cleaned up (process exit)
  if (!loaded) return HSA_STATUS_SUCCESS;

  ScopedAcquire<KernelMutex> boot(&bootstrap_lock_);

  if (runtime_singleton_ == nullptr) return HSA_STATUS_ERROR_NOT_INITIALIZED;

  if (runtime_singleton_->ref_count_ == 1) {
    // Release all registered memory, then unload backends
    runtime_singleton_->Unload();
  }

  runtime_singleton_->ref_count_--;

  if (runtime_singleton_->ref_count_ == 0) {
    delete runtime_singleton_;
    runtime_singleton_ = nullptr;
  }

  return HSA_STATUS_SUCCESS;
}

bool Runtime::IsOpen() {
  return (Runtime::runtime_singleton_ != NULL) &&
         (Runtime::runtime_singleton_->ref_count_ != 0);
}

// Register agent information only.  Must not call anything that may use the registered information
// since those tables are incomplete.
void Runtime::RegisterAgent(Agent* agent, bool Enabled) {
  // Record the agent in the node-to-agent reverse lookup table.
  agents_by_node_[agent->node_id()].push_back(agent);

  // Process agent as a cpu or gpu device.
  if (agent->device_type() == Agent::DeviceType::kAmdCpuDevice) {
    cpu_agents_.push_back(agent);

    agents_by_gpuid_[0] = agent;

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
      // Might need memory pooling to cover allocation that
      // requires less than 4096 bytes.

      // Default system pool must support kernarg
      for (auto pool : system_regions_fine_) {
        if (pool->kernarg()) {
          system_allocator_ = [pool](size_t size, size_t alignment,
                                     MemoryRegion::AllocateFlags alloc_flags) -> void* {
            assert(alignment <= 4096);
            void* ptr = NULL;
            return (HSA_STATUS_SUCCESS ==
                    core::Runtime::runtime_singleton_->AllocateMemory(pool, size, alloc_flags,
                                                                      &ptr))
                ? ptr
                : NULL;
          };

          system_deallocator_ = [](void* ptr) {
            core::Runtime::runtime_singleton_->FreeMemory(ptr);
          };

          BaseShared::SetAllocateAndFree(system_allocator_, system_deallocator_);
          break;
        }
      }
    }
  } else if (agent->device_type() == Agent::DeviceType::kAmdGpuDevice) {
    if (Enabled) {
      gpu_agents_.push_back(agent);
      gpu_ids_.push_back(agent->node_id());
      agents_by_gpuid_[((AMD::GpuAgent*)agent)->KfdGpuID()] = agent;

      // Assign the first discovered gpu agent as region gpu.
      if (region_gpu_ == NULL) region_gpu_ = agent;
    } else {
      disabled_gpu_agents_.push_back(agent);
    }
  }
}

void Runtime::DestroyAgents() {
  agents_by_node_.clear();

  std::for_each(gpu_agents_.begin(), gpu_agents_.end(), DeleteObject());
  gpu_agents_.clear();

  std::for_each(disabled_gpu_agents_.begin(), disabled_gpu_agents_.end(), DeleteObject());
  disabled_gpu_agents_.clear();

  gpu_ids_.clear();

  std::for_each(cpu_agents_.begin(), cpu_agents_.end(), DeleteObject());
  cpu_agents_.clear();

  region_gpu_ = NULL;

  system_regions_fine_.clear();
  system_regions_coarse_.clear();
}

void Runtime::SetLinkCount(size_t num_nodes) {
  num_nodes_ = num_nodes;
  link_matrix_.resize(num_nodes * num_nodes);
}

void Runtime::RegisterLinkInfo(uint32_t node_id_from, uint32_t node_id_to,
                               uint32_t num_hop,
                               hsa_amd_memory_pool_link_info_t& link_info) {
  const uint32_t idx = GetIndexLinkInfo(node_id_from, node_id_to);
  link_matrix_[idx].num_hop = num_hop;
  link_matrix_[idx].info = link_info;

  // Limit the number of hop to 1 since the runtime does not have enough
  // information to share to the user about each hop.
  link_matrix_[idx].num_hop = std::min(link_matrix_[idx].num_hop , 1U);
}

const Runtime::LinkInfo Runtime::GetLinkInfo(uint32_t node_id_from,
                                             uint32_t node_id_to) {
  return (node_id_from != node_id_to)
             ? link_matrix_[GetIndexLinkInfo(node_id_from, node_id_to)]
             : LinkInfo();  // No link.
}

uint32_t Runtime::GetIndexLinkInfo(uint32_t node_id_from, uint32_t node_id_to) {
  return ((node_id_from * num_nodes_) + node_id_to);
}

hsa_status_t Runtime::IterateAgent(hsa_status_t (*callback)(hsa_agent_t agent,
                                                            void* data),
                                   void* data) {
  AMD::callback_t<decltype(callback)> call(callback);

  std::vector<core::Agent*>* agent_lists[2] = {&cpu_agents_, &gpu_agents_};
  for (std::vector<core::Agent*>* agent_list : agent_lists) {
    for (size_t i = 0; i < agent_list->size(); ++i) {
      hsa_agent_t agent = Agent::Convert(agent_list->at(i));
      hsa_status_t status = call(agent, data);

      if (status != HSA_STATUS_SUCCESS) {
        return status;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::AllocateMemory(const MemoryRegion* region, size_t size,
                                     MemoryRegion::AllocateFlags alloc_flags,
                                     void** address) {
  size_t size_requested = size;  // region->Allocate(...) may align-up size to granularity
  hsa_status_t status = region->Allocate(size, alloc_flags, address);
  // Track the allocation result so that it could be freed properly.
  if (status == HSA_STATUS_SUCCESS) {
    ScopedAcquire<KernelSharedMutex> lock(&memory_lock_);
    allocation_map_[*address] = AllocationRegion(region, size, size_requested, alloc_flags);
  }

  return status;
}

hsa_status_t Runtime::FreeMemory(void* ptr) {
  if (ptr == nullptr) {
    return HSA_STATUS_SUCCESS;
  }

  const MemoryRegion* region = nullptr;
  size_t size = 0;
  std::unique_ptr<std::vector<AllocationRegion::notifier_t>> notifiers;
  MemoryRegion::AllocateFlags alloc_flags = core::MemoryRegion::AllocateNoFlags;

  {
    ScopedAcquire<KernelSharedMutex> lock(&memory_lock_);

    std::map<const void*, AllocationRegion>::iterator it = allocation_map_.find(ptr);

    if (it == allocation_map_.end()) {
      debug_warning(false && "Can't find address in allocation map");
      return HSA_STATUS_ERROR_INVALID_ALLOCATION;
    }
    region = it->second.region;
    size = it->second.size;
    alloc_flags = it->second.alloc_flags;

    // Imported fragments can't be released with FreeMemory.
    if (region == nullptr) {
      assert(false && "Can't release imported memory with free.");
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }

    notifiers = std::move(it->second.notifiers);

    allocation_map_.erase(it);
  }

  // Notifiers can't run while holding the lock or the callback won't be able to manage memory.
  // The memory triggering the notification has already been removed from the memory map so can't
  // be double released during the callback.
  if (notifiers) {
    for (auto& notifier : *notifiers) {
      notifier.callback(notifier.ptr, notifier.user_data);
    }
  }

  if (alloc_flags & core::MemoryRegion::AllocateAsan)
    assert(hsaKmtReturnAsanHeaderPage(ptr) == HSAKMT_STATUS_SUCCESS);

  return region->Free(ptr, size);
}

hsa_status_t Runtime::RegisterReleaseNotifier(void* ptr, hsa_amd_deallocation_callback_t callback,
                                              void* user_data) {
  ScopedAcquire<KernelSharedMutex> lock(&memory_lock_);
  auto mem = allocation_map_.upper_bound(ptr);
  if (mem != allocation_map_.begin()) {
    mem--;

    // No support for imported fragments yet.
    if (mem->second.region == nullptr) return HSA_STATUS_ERROR_INVALID_ALLOCATION;

    if ((mem->first <= ptr) &&
        (ptr < reinterpret_cast<const uint8_t*>(mem->first) + mem->second.size)) {
      auto& notifiers = mem->second.notifiers;
      if (!notifiers) notifiers.reset(new std::vector<AllocationRegion::notifier_t>);
      AllocationRegion::notifier_t notifier = {
          ptr, AMD::callback_t<hsa_amd_deallocation_callback_t>(callback), user_data};
      notifiers->push_back(notifier);
      return HSA_STATUS_SUCCESS;
    }
  }
  return HSA_STATUS_ERROR_INVALID_ALLOCATION;
}

hsa_status_t Runtime::DeregisterReleaseNotifier(void* ptr,
                                                hsa_amd_deallocation_callback_t callback) {
  hsa_status_t ret = HSA_STATUS_ERROR_INVALID_ARGUMENT;
  ScopedAcquire<KernelSharedMutex> lock(&memory_lock_);
  auto mem = allocation_map_.upper_bound(ptr);
  if (mem != allocation_map_.begin()) {
    mem--;
    if ((mem->first <= ptr) &&
        (ptr < reinterpret_cast<const uint8_t*>(mem->first) + mem->second.size)) {
      auto& notifiers = mem->second.notifiers;
      if (!notifiers) return HSA_STATUS_ERROR_INVALID_ARGUMENT;
      for (size_t i = 0; i < notifiers->size(); i++) {
        if (((*notifiers)[i].ptr == ptr) && ((*notifiers)[i].callback) == callback) {
          (*notifiers)[i] = std::move((*notifiers)[notifiers->size() - 1]);
          notifiers->pop_back();
          i--;
          ret = HSA_STATUS_SUCCESS;
        }
      }
    }
  }
  return ret;
}

hsa_status_t Runtime::CopyMemory(void* dst, const void* src, size_t size) {
  void* source = const_cast<void*>(src);

  // Choose agents from pointer info
  bool is_src_system = false;
  bool is_dst_system = false;
  core::Agent* src_agent;
  core::Agent* dst_agent;

  // Fetch ownership
  const auto& is_system_mem = [&](void* ptr, core::Agent*& agent, bool& need_lock) {
    hsa_amd_pointer_info_t info;
    uint32_t count;
    hsa_agent_t* accessible = nullptr;
    MAKE_SCOPE_GUARD([&]() { free(accessible); });
    info.size = sizeof(info);
    hsa_status_t err = PtrInfo(ptr, &info, malloc, &count, &accessible);
    if (err != HSA_STATUS_SUCCESS)
      throw AMD::hsa_exception(err, "PtrInfo failed in hsa_memory_copy.");
    ptrdiff_t endPtr = (ptrdiff_t)ptr + size;
    if (info.agentBaseAddress <= ptr &&
        endPtr <= (ptrdiff_t)info.agentBaseAddress + info.sizeInBytes) {
      if (info.agentOwner.handle == 0) info.agentOwner = accessible[0];
      agent = core::Agent::Convert(info.agentOwner);
      need_lock = false;
      return agent->device_type() != core::Agent::DeviceType::kAmdGpuDevice;
    } else {
      need_lock = true;
      agent = cpu_agents_[0];
      return true;
    }
  };

  bool src_lock, dst_lock;
  is_src_system = is_system_mem(source, src_agent, src_lock);
  is_dst_system = is_system_mem(dst, dst_agent, dst_lock);

  // CPU-CPU
  if (is_src_system && is_dst_system) {
    memcpy(dst, source, size);
    return HSA_STATUS_SUCCESS;
  }

  // Same GPU
  if (src_agent->node_id() == dst_agent->node_id()) return dst_agent->DmaCopy(dst, source, size);

  // GPU-CPU
  // Must ensure that system memory is visible to the GPU during the copy.
  const AMD::MemoryRegion* system_region =
      static_cast<const AMD::MemoryRegion*>(system_regions_fine_[0]);

  void* gpuPtr = nullptr;
  const auto& locked_copy = [&](void*& ptr, core::Agent* locking_agent) {
    void* tmp;
    hsa_agent_t agent = locking_agent->public_handle();
    hsa_status_t err = system_region->Lock(1, &agent, ptr, size, &tmp);
    if (err != HSA_STATUS_SUCCESS) throw AMD::hsa_exception(err, "Lock failed in hsa_memory_copy.");
    gpuPtr = ptr;
    ptr = tmp;
  };

  MAKE_SCOPE_GUARD([&]() {
    if (gpuPtr != nullptr) system_region->Unlock(gpuPtr);
  });

  if (src_lock) locked_copy(source, dst_agent);
  if (dst_lock) locked_copy(dst, src_agent);
  if (is_src_system) return dst_agent->DmaCopy(dst, source, size);
  if (is_dst_system) return src_agent->DmaCopy(dst, source, size);

  /*
  GPU-GPU - functional support, not a performance path.

  This goes through system memory because we have to support copying between non-peer GPUs
  and we can't use P2P pointers even if the GPUs are peers.  Because hsa_amd_agents_allow_access
  requires the caller to specify all allowed agents we can't assume that a peer mapped pointer
  would remain mapped for the duration of the copy.
  */
  void* temp = system_allocator_(size, 0, core::MemoryRegion::AllocateNoFlags);
  MAKE_SCOPE_GUARD([&]() { system_deallocator_(temp); });
  hsa_status_t err = src_agent->DmaCopy(temp, source, size);
  if (err == HSA_STATUS_SUCCESS) err = dst_agent->DmaCopy(dst, temp, size);
  return err;
}

hsa_status_t Runtime::CopyMemory(void* dst, core::Agent* dst_agent, const void* src,
                                 core::Agent* src_agent, size_t size,
                                 std::vector<core::Signal*>& dep_signals,
                                 core::Signal& completion_signal) {
  auto lookupAgent = [this](core::Agent* agent, const void* ptr) {
    hsa_amd_pointer_info_t info;
    PtrInfoBlockData block;
    info.size = sizeof(info);
    PtrInfo(ptr, &info, nullptr, nullptr, nullptr, &block);
    // Limit to IPC and GFX types for now.  These are the only types for which the application may
    // not posess a proper agent handle.
    if ((info.type != HSA_EXT_POINTER_TYPE_IPC) && (info.type != HSA_EXT_POINTER_TYPE_GRAPHICS)) {
      return agent;
    }
    return block.agentOwner;
  };

  const bool src_gpu = (src_agent->device_type() == core::Agent::DeviceType::kAmdGpuDevice);
  core::Agent* copy_agent = (src_gpu) ? src_agent : dst_agent;

  // Lookup owning agent if blit kernel is selected or if flag override is set.
  if ((dst_agent == src_agent) || flag().discover_copy_agents()) {
    dst_agent = lookupAgent(dst_agent, dst);
    src_agent = lookupAgent(src_agent, src);
  }
  return copy_agent->DmaCopy(dst, *dst_agent, src, *src_agent, size, dep_signals,
                             completion_signal);
}

hsa_status_t Runtime::CopyMemoryOnEngine(void* dst, core::Agent* dst_agent, const void* src,
                                 core::Agent* src_agent, size_t size,
                                 std::vector<core::Signal*>& dep_signals,
                                 core::Signal& completion_signal,
                                 hsa_amd_sdma_engine_id_t engine_id, bool force_copy_on_sdma) {
  const bool src_gpu = (src_agent->device_type() == core::Agent::DeviceType::kAmdGpuDevice);
  core::Agent* copy_agent = (src_gpu) ? src_agent : dst_agent;

  // engine_id is single bitset unique.
  int engine_offset = ffs(engine_id);
  if (!engine_id || !!((engine_id >> engine_offset))) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return copy_agent->DmaCopyOnEngine(dst, *dst_agent, src, *src_agent, size, dep_signals,
                             completion_signal, engine_offset, force_copy_on_sdma);
}

hsa_status_t Runtime::CopyMemoryStatus(core::Agent* dst_agent, core::Agent* src_agent,
                                       uint32_t *engine_ids_mask) {
  const bool src_gpu = (src_agent->device_type() == core::Agent::DeviceType::kAmdGpuDevice);
  core::Agent* copy_agent = (src_gpu) ? src_agent : dst_agent;

  if (dst_agent == src_agent) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  return copy_agent->DmaCopyStatus(*dst_agent, *src_agent, engine_ids_mask);
}

hsa_status_t Runtime::FillMemory(void* ptr, uint32_t value, size_t count) {
  // Choose blit agent from pointer info
  hsa_amd_pointer_info_t info;
  uint32_t agent_count;
  hsa_agent_t* accessible = nullptr;
  info.size = sizeof(info);
  MAKE_SCOPE_GUARD([&]() { free(accessible); });
  hsa_status_t err = PtrInfo(ptr, &info, malloc, &agent_count, &accessible);
  if (err != HSA_STATUS_SUCCESS) return err;

  ptrdiff_t endPtr = (ptrdiff_t)ptr + count * sizeof(uint32_t);

  // Check for GPU fill
  // Selects GPU fill for SVM and Locked allocations if a GPU address is given and is mapped.
  if (info.agentBaseAddress <= ptr &&
      endPtr <= (ptrdiff_t)info.agentBaseAddress + info.sizeInBytes) {
    core::Agent* blit_agent = core::Agent::Convert(info.agentOwner);
    if (blit_agent->device_type() != core::Agent::DeviceType::kAmdGpuDevice) {
      blit_agent = nullptr;
      for (uint32_t i = 0; i < agent_count; i++) {
        if (core::Agent::Convert(accessible[i])->device_type() ==
            core::Agent::DeviceType::kAmdGpuDevice) {
          blit_agent = core::Agent::Convert(accessible[i]);
          break;
        }
      }
    }
    if (blit_agent) return blit_agent->DmaFill(ptr, value, count);
  }

  // Host and unmapped SVM addresses copy via host.
  if (info.hostBaseAddress <= ptr && endPtr <= (ptrdiff_t)info.hostBaseAddress + info.sizeInBytes) {
    memset(ptr, value, count * sizeof(uint32_t));
    return HSA_STATUS_SUCCESS;
  }

  return HSA_STATUS_ERROR_INVALID_ALLOCATION;
}

hsa_status_t Runtime::AllowAccess(uint32_t num_agents,
                                  const hsa_agent_t* agents, const void* ptr) {
  const AMD::MemoryRegion* amd_region = NULL;
  size_t alloc_size = 0;

  {
    ScopedAcquire<KernelSharedMutex> lock(&memory_lock_);

    std::map<const void*, AllocationRegion>::const_iterator it = allocation_map_.find(ptr);

    if (it == allocation_map_.end()) {
      return HSA_STATUS_ERROR;
    }

    amd_region = reinterpret_cast<const AMD::MemoryRegion*>(it->second.region);

    // Imported IPC handle entries inside allocation_map_ do not have an amd_region because they
    // were allocated in the other process. Access is already granted during IPCAttach().
    if (!amd_region)
      return HSA_STATUS_SUCCESS;

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
      *((uint64_t*)value) = os::ReadSystemClock();
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
    case HSA_SYSTEM_INFO_EXTENSIONS: {
      memset(value, 0, sizeof(uint8_t) * 128);

      auto setFlag = [&](uint32_t bit) {
        assert(bit < 128 * 8 && "Extension value exceeds extension bitmask");
        uint index = bit / 8;
        uint subBit = bit % 8;
        ((uint8_t*)value)[index] |= 1 << subBit;
      };

      if (hsa_internal_api_table_.finalizer_api.hsa_ext_program_finalize_fn != NULL) {
        setFlag(HSA_EXTENSION_FINALIZER);
      }

      if (hsa_internal_api_table_.image_api.hsa_ext_image_create_fn != NULL) {
        setFlag(HSA_EXTENSION_IMAGES);
      }

      if (os::LibHandle lib = os::LoadLib(kAqlProfileLib)) {
        os::CloseLib(lib);
        setFlag(HSA_EXTENSION_AMD_AQLPROFILE);
      }

      setFlag(HSA_EXTENSION_AMD_PROFILER);

      break;
    }
    case HSA_AMD_SYSTEM_INFO_BUILD_VERSION: {
      *(const char**)value = STRING(ROCR_BUILD_ID);
      break;
    }
    case HSA_AMD_SYSTEM_INFO_SVM_SUPPORTED: {
      bool ret = true;
      for (auto agent : gpu_agents_) {
        AMD::GpuAgent* gpu = (AMD::GpuAgent*)agent;
        ret &= (gpu->properties().Capability.ui32.SVMAPISupported == 1);
      }
      *(bool*)value = ret;
      break;
    }
    case HSA_AMD_SYSTEM_INFO_SVM_ACCESSIBLE_BY_DEFAULT: {
      bool ret = true;
      for(auto agent : gpu_agents_)
        ret &= (agent->isa()->GetXnack() == IsaFeature::Enabled);
      *(bool*)value = ret;
      break;
    }
    case HSA_AMD_SYSTEM_INFO_MWAITX_ENABLED: {
      *((bool*)value) = g_use_mwaitx;
      break;
    }
    case HSA_AMD_SYSTEM_INFO_DMABUF_SUPPORTED: {
      auto kfd_version = core::Runtime::runtime_singleton_->KfdVersion().version;

      // Implemented in KFD in 1.12
      if (kfd_version.KernelInterfaceMajorVersion > 1 ||
          (kfd_version.KernelInterfaceMajorVersion == 1 &&
              kfd_version.KernelInterfaceMinorVersion >= 12))
        *(reinterpret_cast<bool*>(value)) = true;
      else
        *(reinterpret_cast<bool*>(value)) = false;
      break;
    }
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::SetAsyncSignalHandler(hsa_signal_t signal,
                                            hsa_signal_condition_t cond,
                                            hsa_signal_value_t value,
                                            hsa_amd_signal_handler handler,
                                            void* arg) {
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
  static const int tinyArraySize=8;
  HsaGraphicsResourceInfo info;

  HSAuint32 short_nodes[tinyArraySize];
  HSAuint32* nodes = short_nodes;
  if (num_agents > tinyArraySize) {
    nodes = new HSAuint32[num_agents];
    if (nodes == NULL) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }
  MAKE_SCOPE_GUARD([&]() {
    if (num_agents > tinyArraySize) delete[] nodes;
  });

  for (uint32_t i = 0; i < num_agents; i++)
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
    if (hsaKmtMapMemoryToGPUNodes(info.MemoryAddress, info.SizeInBytes, &altAddress, map_flags,
                                  num_agents, nodes) != HSAKMT_STATUS_SUCCESS) {
      hsaKmtDeregisterMemory(info.MemoryAddress);
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  }

  if (metadata_size != NULL) *metadata_size = info.MetadataSizeInBytes;
  if (metadata != NULL) *metadata = info.Metadata;

  *size = info.SizeInBytes;
  *ptr = info.MemoryAddress;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::InteropUnmap(void* ptr) {
  if(hsaKmtUnmapMemoryToGPU(ptr)!=HSAKMT_STATUS_SUCCESS)
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  if(hsaKmtDeregisterMemory(ptr)!=HSAKMT_STATUS_SUCCESS)
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::PtrInfo(const void* ptr, hsa_amd_pointer_info_t* info, void* (*alloc)(size_t),
                              uint32_t* num_agents_accessible, hsa_agent_t** accessible,
                              PtrInfoBlockData* block_info) {
  static_assert(static_cast<int>(HSA_POINTER_UNKNOWN) == static_cast<int>(HSA_EXT_POINTER_TYPE_UNKNOWN),
                "Thunk pointer info mismatch");
  static_assert(static_cast<int>(HSA_POINTER_ALLOCATED) == static_cast<int>(HSA_EXT_POINTER_TYPE_HSA),
                "Thunk pointer info mismatch");
  static_assert(static_cast<int>(HSA_POINTER_REGISTERED_USER) == static_cast<int>(HSA_EXT_POINTER_TYPE_LOCKED),
                "Thunk pointer info mismatch");
  static_assert(static_cast<int>(HSA_POINTER_REGISTERED_GRAPHICS) == static_cast<int>(HSA_EXT_POINTER_TYPE_GRAPHICS),
                "Thunk pointer info mismatch");

  HsaPointerInfo thunkInfo;
  uint32_t* mappedNodes;

  hsa_amd_pointer_info_t retInfo = {0};

  // check output struct has an initialized size.
  if (info->size == 0) return HSA_STATUS_ERROR_INVALID_ARGUMENT;

  retInfo.size = Min(size_t(info->size), sizeof(hsa_amd_pointer_info_t));

  bool returnListData =
      ((alloc != nullptr) && (num_agents_accessible != nullptr) && (accessible != nullptr));

  bool allocation_map_entry_found = false;

  {  // memory_lock protects access to the NMappedNodes array and fragment user data since these may
     // change with calls to memory APIs.
    ScopedAcquire<KernelSharedMutex> lock(&memory_lock_);

    // We don't care if this returns an error code.
    // The type will be HSA_EXT_POINTER_TYPE_UNKNOWN if so.
    auto err = hsaKmtQueryPointerInfo(ptr, &thunkInfo);
    if (err != HSAKMT_STATUS_SUCCESS || thunkInfo.Type == HSA_POINTER_UNKNOWN) {
      retInfo.type = HSA_EXT_POINTER_TYPE_UNKNOWN;
      memcpy(info, &retInfo, retInfo.size);
      return HSA_STATUS_SUCCESS;
    }

    if (returnListData) {
      assert(thunkInfo.NMappedNodes <= agents_by_node_.size() &&
             "PointerInfo: Thunk returned more than all agents in NMappedNodes.");
      mappedNodes = (uint32_t*)alloca(thunkInfo.NMappedNodes * sizeof(uint32_t));
      memcpy(mappedNodes, thunkInfo.MappedNodes, thunkInfo.NMappedNodes * sizeof(uint32_t));
    }
    retInfo.type = (hsa_amd_pointer_type_t)thunkInfo.Type;
    retInfo.agentBaseAddress = reinterpret_cast<void*>(thunkInfo.GPUAddress);
    retInfo.hostBaseAddress = thunkInfo.CPUAddress;
    retInfo.sizeInBytes = thunkInfo.SizeInBytes;
    retInfo.userData = thunkInfo.UserData;
    retInfo.global_flags = thunkInfo.MemFlags.ui32.CoarseGrain
        ? HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED
        : HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED;
    retInfo.global_flags |=
        thunkInfo.MemFlags.ui32.Uncached ? HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT : 0;
    if (block_info != nullptr) {
      // Block_info reports the thunk allocation from which we may have suballocated.
      // For locked memory we want to return the host address since hostBaseAddress is used to
      // manipulate locked memory and it is possible that hostBaseAddress is different from
      // agentBaseAddress.
      // For device memory, hostBaseAddress is either equal to agentBaseAddress or is NULL when the
      // CPU does not have access.
      assert((retInfo.hostBaseAddress || retInfo.agentBaseAddress) && "Thunk pointer info returned no base address.");
      block_info->base = (retInfo.hostBaseAddress ? retInfo.hostBaseAddress : retInfo.agentBaseAddress);
      block_info->length = retInfo.sizeInBytes;

      // Report the owning agent, even if such an agent is not usable in the process.
      auto nodeAgents = agents_by_node_.find(thunkInfo.Node);
      assert(nodeAgents != agents_by_node_.end() && "Node id not found!");
      block_info->agentOwner = nodeAgents->second[0];
    }
    auto fragment = allocation_map_.upper_bound(ptr);
    if (fragment != allocation_map_.begin()) {
      fragment--;
      if ((fragment->first <= ptr) &&
          (ptr < reinterpret_cast<const uint8_t*>(fragment->first) + fragment->second.size_requested)) {
        // agent and host address must match here. Only lock memory is allowed to have differing
        // addresses but lock memory has type HSA_EXT_POINTER_TYPE_LOCKED and cannot be
        // suballocated.
        retInfo.agentBaseAddress = const_cast<void*>(fragment->first);
        retInfo.hostBaseAddress = retInfo.agentBaseAddress;
        retInfo.sizeInBytes = fragment->second.size_requested;
        retInfo.userData = fragment->second.user_ptr;
        allocation_map_entry_found = true;
      }
    }
  }  // end lock scope

  // Return type UNKNOWN for released fragments.  Do not report the underlying block info to users!
  if ((!allocation_map_entry_found) &&
      ((retInfo.type == HSA_EXT_POINTER_TYPE_HSA) || (retInfo.type == HSA_EXT_POINTER_TYPE_IPC))) {
    retInfo.type = HSA_EXT_POINTER_TYPE_UNKNOWN;
  }

  // IPC and Graphics memory may come from a node that does not have an agent in this process.
  // Ex. ROCR_VISIBLE_DEVICES or peer GPU is not supported by ROCm.
  retInfo.agentOwner.handle = 0;
  auto nodeAgents = agents_by_node_.find(thunkInfo.Node);
  assert(nodeAgents != agents_by_node_.end() && "Node id not found!");
  for (auto agent : nodeAgents->second) {
    if (agent->Enabled()) {
      retInfo.agentOwner = agent->public_handle();
      break;
    }
  }

  // Correct agentOwner for locked memory.  Thunk reports the GPU that owns the
  // alias but users are expecting to see a CPU when the memory is system.
  if (retInfo.type == HSA_EXT_POINTER_TYPE_LOCKED) {
    if ((nodeAgents == agents_by_node_.end()) ||
        (nodeAgents->second[0]->device_type() != core::Agent::kAmdCpuDevice)) {
      retInfo.agentOwner = cpu_agents_[0]->public_handle();
    }
  }

  memcpy(info, &retInfo, retInfo.size);

  if (returnListData) {
    uint32_t count = 0;
    for (HSAuint32 i = 0; i < thunkInfo.NMappedNodes; i++) {
      assert(mappedNodes[i] <= max_node_id() &&
             "PointerInfo: Invalid node ID returned from thunk.");
      count += agents_by_node_[mappedNodes[i]].size();
    }

    AMD::callback_t<decltype(alloc)> Alloc(alloc);
    *accessible = (hsa_agent_t*)Alloc(sizeof(hsa_agent_t) * count);
    if ((*accessible) == nullptr) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    *num_agents_accessible = count;

    uint32_t index = 0;
    for (HSAuint32 i = 0; i < thunkInfo.NMappedNodes; i++) {
      auto& list = agents_by_node_[mappedNodes[i]];
      for (auto agent : list) {
        (*accessible)[index] = agent->public_handle();
        index++;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::SetPtrInfoData(const void* ptr, void* userptr) {
  {  // Use allocation map if possible to handle fragments.
    ScopedAcquire<KernelSharedMutex> lock(&memory_lock_);
    const auto& it = allocation_map_.find(ptr);
    if (it != allocation_map_.end()) {
      it->second.user_ptr = userptr;
      return HSA_STATUS_SUCCESS;
    }
  }
  // Cover entries not in the allocation map (graphics, lock,...)
  if (hsaKmtSetMemoryUserData(ptr, userptr) == HSAKMT_STATUS_SUCCESS)
    return HSA_STATUS_SUCCESS;
  return HSA_STATUS_ERROR_INVALID_ARGUMENT;
}

hsa_status_t Runtime::IPCCreate(void* ptr, size_t len, hsa_amd_ipc_memory_t* handle) {
  static_assert(sizeof(hsa_amd_ipc_memory_t) == sizeof(HsaSharedMemoryHandle),
                "Thunk IPC mismatch.");

  static const size_t pageSize = 4096;

  // Reject sharing allocations larger than ~8TB due to thunk limitations.
  if (len > 0x7FFFFFFF000ull) return HSA_STATUS_ERROR_INVALID_ARGUMENT;

  // Check for fragment sharing.
  PtrInfoBlockData block;
  hsa_amd_pointer_info_t info;
  info.size = sizeof(info);
  if (PtrInfo(ptr, &info, nullptr, nullptr, nullptr, &block) != HSA_STATUS_SUCCESS)
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;

  // Temporary: Previous versions of HIP will call hsa_amd_ipc_memory_create with the len aligned to
  // granularity. We need to maintain backward compatibility for 2 releases so we temporarily allow
  // this. After 2 releases, we will only allow info.sizeInBytes != len.
  if ((info.agentBaseAddress != ptr) ||
      (info.sizeInBytes != len && AlignUp(info.sizeInBytes, pageSize) != len)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  if ((block.base != ptr) || (block.length != len)) {
    if (!IsMultipleOf(block.base, 2 * 1024 * 1024)) {
      assert(false && "Fragment's block not aligned to 2MB!");
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }
    if (hsaKmtShareMemory(block.base, block.length, reinterpret_cast<HsaSharedMemoryHandle*>(
                                                        handle)) != HSAKMT_STATUS_SUCCESS)
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    uint32_t offset =
        (reinterpret_cast<uint8_t*>(ptr) - reinterpret_cast<uint8_t*>(block.base)) / 4096;
    // Holds size in (4K?) pages in thunk handle: Mark as a fragment and denote offset.
    handle->handle[6] |= 0x80000000 | offset;
    // Mark block for IPC.  Prevents reallocation of exported memory.
    ScopedAcquire<KernelSharedMutex::Shared> lock(memory_lock_.shared());
    hsa_status_t err = allocation_map_[ptr].region->IPCFragmentExport(ptr);
    assert(err == HSA_STATUS_SUCCESS && "Region inconsistent with address map.");
    return err;
  } else {
    if (hsaKmtShareMemory(ptr, len, reinterpret_cast<HsaSharedMemoryHandle*>(handle)) !=
        HSAKMT_STATUS_SUCCESS)
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::IPCAttach(const hsa_amd_ipc_memory_t* handle, size_t len, uint32_t num_agents,
                                Agent** agents, void** mapped_ptr) {
  static const int tinyArraySize = 8;
  void* importAddress;
  HSAuint64 importSize;
  HSAuint64 altAddress;

  hsa_amd_ipc_memory_t importHandle;
  importHandle = *handle;

  // Extract fragment info
  bool isFragment = false;
  uint32_t fragOffset = 0;

  auto fixFragment = [&]() {
    if (isFragment) {
      importAddress = reinterpret_cast<uint8_t*>(importAddress) + fragOffset;
      len = Min(len, importSize - fragOffset);
    }
    ScopedAcquire<KernelSharedMutex> lock(&memory_lock_);
    allocation_map_[importAddress] =
        AllocationRegion(nullptr, len, len, core::MemoryRegion::AllocateNoFlags);
  };

  if ((importHandle.handle[6] & 0x80000000) != 0) {
    isFragment = true;
    fragOffset = (importHandle.handle[6] & 0x1FF) * 4096;
    importHandle.handle[6] &= ~(0x80000000 | 0x1FF);
  }

  if (num_agents == 0) {
    if (hsaKmtRegisterSharedHandle(reinterpret_cast<const HsaSharedMemoryHandle*>(&importHandle),
                                   &importAddress, &importSize) != HSAKMT_STATUS_SUCCESS)
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    if (hsaKmtMapMemoryToGPU(importAddress, importSize, &altAddress) != HSAKMT_STATUS_SUCCESS) {
      hsaKmtDeregisterMemory(importAddress);
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
    fixFragment();
    *mapped_ptr = importAddress;
    return HSA_STATUS_SUCCESS;
  }

  HSAuint32* nodes = nullptr;
  if (num_agents > tinyArraySize)
    nodes = new HSAuint32[num_agents];
  else
    nodes = (HSAuint32*)alloca(sizeof(HSAuint32) * num_agents);
  if (nodes == NULL) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;

  MAKE_SCOPE_GUARD([&]() {
    if (num_agents > tinyArraySize) delete[] nodes;
  });

  for (uint32_t i = 0; i < num_agents; i++)
    agents[i]->GetInfo((hsa_agent_info_t)HSA_AMD_AGENT_INFO_DRIVER_NODE_ID, &nodes[i]);

  if (hsaKmtRegisterSharedHandleToNodes(
          reinterpret_cast<const HsaSharedMemoryHandle*>(&importHandle), &importAddress,
          &importSize, num_agents, nodes) != HSAKMT_STATUS_SUCCESS)
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;

  HsaMemMapFlags map_flags;
  map_flags.Value = 0;
  map_flags.ui32.PageSize = HSA_PAGE_SIZE_64KB;
  if (hsaKmtMapMemoryToGPUNodes(importAddress, importSize, &altAddress, map_flags, num_agents,
                                nodes) != HSAKMT_STATUS_SUCCESS) {
    map_flags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    if (hsaKmtMapMemoryToGPUNodes(importAddress, importSize, &altAddress, map_flags, num_agents,
                                  nodes) != HSAKMT_STATUS_SUCCESS) {
      hsaKmtDeregisterMemory(importAddress);
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  }

  fixFragment();
  *mapped_ptr = importAddress;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::IPCDetach(void* ptr) {
  {  // Handle imported fragments.
    ScopedAcquire<KernelSharedMutex> lock(&memory_lock_);
    const auto& it = allocation_map_.find(ptr);
    if (it != allocation_map_.end()) {
      if (it->second.region != nullptr) return HSA_STATUS_ERROR_INVALID_ARGUMENT;
      allocation_map_.erase(it);
      lock.Release();  // Can't hold memory lock when using pointer info.

      PtrInfoBlockData block;
      hsa_amd_pointer_info_t info;
      info.size = sizeof(info);
      if (PtrInfo(ptr, &info, nullptr, nullptr, nullptr, &block) != HSA_STATUS_SUCCESS)
        return HSA_STATUS_ERROR_INVALID_ARGUMENT;
      ptr = block.base;
    }
  }
  if (hsaKmtUnmapMemoryToGPU(ptr) != HSAKMT_STATUS_SUCCESS)
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  if (hsaKmtDeregisterMemory(ptr) != HSAKMT_STATUS_SUCCESS)
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
    uint32_t index = AMD::hsa_amd_signal_wait_any(
        uint32_t(async_events_.Size()), &async_events_.signal_[0],
        &async_events_.cond_[0], &async_events_.value_[0], uint64_t(-1),
        HSA_WAIT_STATE_BLOCKED, &value);

    // Reset the control signal
    if (index == 0) {
      hsa_signal_handle(async_events_control_.wake)->StoreRelaxed(0);
    } else if (index != -1) {
      // No error or timout occured, process the handlers
      // Call handler for the known satisfied signal.
      assert(async_events_.handler_[index] != NULL);
      bool keep = async_events_.handler_[index](value, async_events_.arg_[index]);
      if (!keep) {
        hsa_signal_handle(async_events_.signal_[index])->Release();
        async_events_.CopyIndex(index, async_events_.Size() - 1);
        async_events_.PopBack();
      }
      // Check remaining signals before sleeping.
      for (size_t i = index; i < async_events_.Size(); i++) {
        hsa_signal_handle sig(async_events_.signal_[i]);

        value = atomic::Load(&sig->signal_.value, std::memory_order_relaxed);
        bool condition_met = false;

        switch (async_events_.cond_[i]) {
          case HSA_SIGNAL_CONDITION_EQ: {
            condition_met = (value == async_events_.value_[i]);
            break;
          }
          case HSA_SIGNAL_CONDITION_NE: {
            condition_met = (value != async_events_.value_[i]);
            break;
          }
          case HSA_SIGNAL_CONDITION_GTE: {
            condition_met = (value >= async_events_.value_[i]);
            break;
          }
          case HSA_SIGNAL_CONDITION_LT: {
            condition_met = (value < async_events_.value_[i]);
            break;
          }
        }

        if (condition_met) {
          assert(async_events_.handler_[i] != NULL);
          bool keep = async_events_.handler_[i](value, async_events_.arg_[i]);
          if (!keep) {
            hsa_signal_handle(async_events_.signal_[i])->Release();
            async_events_.CopyIndex(i, async_events_.Size() - 1);
            async_events_.PopBack();
            i--;
          }
        }
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
  if (core::g_use_interrupt_wait && !gpu_agents_.empty()) {
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

  HsaEvent* vm_fault_event = vm_fault_signal->EopEvent();

  HsaMemoryAccessFault& fault =
      vm_fault_event->EventData.EventData.MemoryAccessFault;

  hsa_status_t custom_handler_status = HSA_STATUS_ERROR;
  auto system_event_handlers = runtime_singleton_->GetSystemEventHandlers();
  // If custom handler is registered, pack the fault info and call the handler
  if (!system_event_handlers.empty()) {
    hsa_amd_event_t memory_fault_event;
    memory_fault_event.event_type = HSA_AMD_GPU_MEMORY_FAULT_EVENT;
    hsa_amd_gpu_memory_fault_info_t& fault_info = memory_fault_event.memory_fault;

    // Find the faulty agent
    auto it = runtime_singleton_->agents_by_node_.find(fault.NodeId);
    assert(it != runtime_singleton_->agents_by_node_.end() && "Can't find faulty agent.");
    Agent* faulty_agent = it->second.front();
    fault_info.agent = Agent::Convert(faulty_agent);

    fault_info.virtual_address = fault.VirtualAddress;
    fault_info.fault_reason_mask = 0;
    if (fault.Failure.NotPresent == 1) {
      fault_info.fault_reason_mask |= HSA_AMD_MEMORY_FAULT_PAGE_NOT_PRESENT;
    }
    if (fault.Failure.ReadOnly == 1) {
      fault_info.fault_reason_mask |= HSA_AMD_MEMORY_FAULT_READ_ONLY;
    }
    if (fault.Failure.NoExecute == 1) {
      fault_info.fault_reason_mask |= HSA_AMD_MEMORY_FAULT_NX;
    }
    if (fault.Failure.GpuAccess == 1) {
      fault_info.fault_reason_mask |= HSA_AMD_MEMORY_FAULT_HOST_ONLY;
    }
    if (fault.Failure.Imprecise == 1) {
      fault_info.fault_reason_mask |= HSA_AMD_MEMORY_FAULT_IMPRECISE;
    }
    if (fault.Failure.ECC == 1 && fault.Failure.ErrorType == 0) {
      fault_info.fault_reason_mask |= HSA_AMD_MEMORY_FAULT_DRAMECC;
    }
    if (fault.Failure.ErrorType == 1) {
      fault_info.fault_reason_mask |= HSA_AMD_MEMORY_FAULT_SRAMECC;
    }
    if (fault.Failure.ErrorType == 2) {
      fault_info.fault_reason_mask |= HSA_AMD_MEMORY_FAULT_DRAMECC;
    }
    if (fault.Failure.ErrorType == 3) {
      fault_info.fault_reason_mask |= HSA_AMD_MEMORY_FAULT_HANG;
    }

    for (auto& callback : system_event_handlers) {
      hsa_status_t err = callback.first(&memory_fault_event, callback.second);
      if (err == HSA_STATUS_SUCCESS) custom_handler_status = HSA_STATUS_SUCCESS;
    }
  }

  // No custom VM fault handler registered or it failed.
  if (custom_handler_status != HSA_STATUS_SUCCESS) {
    if (runtime_singleton_->flag().enable_vm_fault_message()) {
      std::string reason = "";
      if (fault.Failure.NotPresent == 1) {
        reason += "Page not present or supervisor privilege";
      } else if (fault.Failure.ReadOnly == 1) {
        reason += "Write access to a read-only page";
      } else if (fault.Failure.NoExecute == 1) {
        reason += "Execute access to a page marked NX";
      } else if (fault.Failure.GpuAccess == 1) {
        reason += "Host access only";
      } else if ((fault.Failure.ECC == 1 && fault.Failure.ErrorType == 0) ||
                 fault.Failure.ErrorType == 2) {
        reason += "DRAM ECC failure";
      } else if (fault.Failure.ErrorType == 1) {
        reason += "SRAM ECC failure";
      } else if (fault.Failure.ErrorType == 3) {
        reason += "Generic hang recovery";
      } else {
        reason += "Unknown";
      }

      core::Agent* faultingAgent = runtime_singleton_->agents_by_node_[fault.NodeId][0];

      fprintf(
          stderr,
          "Memory access fault by GPU node-%u (Agent handle: %p) on address %p%s. Reason: %s.\n",
          fault.NodeId, reinterpret_cast<void*>(faultingAgent->public_handle().handle),
          reinterpret_cast<const void*>(fault.VirtualAddress),
          (fault.Failure.Imprecise == 1) ? "(may not be exact address)" : "", reason.c_str());

#ifndef NDEBUG
      PrintMemoryMapNear(reinterpret_cast<void*>(fault.VirtualAddress));
#endif
    }
    assert(false && "GPU memory access fault.");
    std::abort();
  }
  // No need to keep the signal because we are done.
  return false;
}

void Runtime::PrintMemoryMapNear(void* ptr) {
  runtime_singleton_->memory_lock_.Acquire();
  auto it = runtime_singleton_->allocation_map_.upper_bound(ptr);
  for (int i = 0; i < 2; i++) {
    if (it != runtime_singleton_->allocation_map_.begin()) it--;
  }
  fprintf(stderr, "Nearby memory map:\n");
  auto start = it;
  for (int i = 0; i < 3; i++) {
    if (it == runtime_singleton_->allocation_map_.end()) break;
    std::string kind = "Non-HSA";
    if (it->second.region != nullptr) {
      const AMD::MemoryRegion* region = static_cast<const AMD::MemoryRegion*>(it->second.region);
      if (region->IsSystem())
        kind = "System";
      else if (region->IsLocalMemory())
        kind = "VRAM";
      else if (region->IsScratch())
        kind = "Scratch";
      else if (region->IsLDS())
        kind = "LDS";
    }
    fprintf(stderr, "%p, 0x%lx, %s\n", it->first, it->second.size, kind.c_str());
    it++;
  }
  fprintf(stderr, "\n");
  it = start;
  runtime_singleton_->memory_lock_.Release();
  hsa_amd_pointer_info_t info;
  PtrInfoBlockData block;
  uint32_t count;
  hsa_agent_t* canAccess;
  info.size = sizeof(info);
  for (int i = 0; i < 3; i++) {
    if (it == runtime_singleton_->allocation_map_.end()) break;
    runtime_singleton_->PtrInfo(const_cast<void*>(it->first), &info, malloc, &count, &canAccess,
                                &block);
    fprintf(stderr, "PtrInfo:\n\tAddress: %p-%p/%p-%p\n\tSize: 0x%lx\n\tType: %u\n\tOwner: %p\n",
            info.agentBaseAddress, (char*)info.agentBaseAddress + info.sizeInBytes,
            info.hostBaseAddress, (char*)info.hostBaseAddress + info.sizeInBytes, info.sizeInBytes,
            info.type, reinterpret_cast<void*>(info.agentOwner.handle));
    fprintf(stderr, "\tCanAccess: %u\n", count);
    for (int t = 0; t < count; t++)
      fprintf(stderr, "\t\t%p\n", reinterpret_cast<void*>(canAccess[t].handle));
    fprintf(stderr, "\tIn block: %p, 0x%lx\n", block.base, block.length);
    free(canAccess);
    it++;
  }
}

Runtime::Runtime()
    : region_gpu_(nullptr),
      sys_clock_freq_(0),
      vm_fault_event_(nullptr),
      vm_fault_signal_(nullptr),
      ref_count_(0),
      kfd_version{0} {}

hsa_status_t Runtime::Load() {
  os::cpuid_t cpuinfo;

  // Assume features are not supported if parse CPUID fails
  if (!os::ParseCpuID(&cpuinfo)) {
    /*
     * This is not a failure, in some environments such as SRIOV, not all CPUID info is
     * exposed inside the guest
     */
    debug_warning("Parsing CPUID failed.");
  }

  flag_.Refresh();
  g_use_interrupt_wait = flag_.enable_interrupt();
  g_use_mwaitx = flag_.check_mwaitx(cpuinfo.mwaitx);

  if (!AMD::Load()) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // Setup system clock frequency for the first time.
  if (sys_clock_freq_ == 0) {
    sys_clock_freq_ = os::SystemClockFrequency();
    if (sys_clock_freq_ < 100000) debug_warning("System clock resolution is low.");
  }

  BindVmFaultHandler();

  loader_ = amd::hsa::loader::Loader::Create(&loader_context_);

  // Load extensions
  LoadExtensions();

  // Initialize per GPU scratch, blits, and trap handler
  for (core::Agent* agent : gpu_agents_) {
    hsa_status_t status =
        reinterpret_cast<AMD::GpuAgentInt*>(agent)->PostToolsInit();

    if (status != HSA_STATUS_SUCCESS) {
      return status;
    }
  }

  // Load tools libraries
  LoadTools();

  // Load svm profiler
  svm_profile_.reset(new AMD::SvmProfileControl);

  return HSA_STATUS_SUCCESS;
}

void Runtime::Unload() {
  svm_profile_.reset(nullptr);

  UnloadTools();
  UnloadExtensions();

  amd::hsa::loader::Loader::Destroy(loader_);
  loader_ = nullptr;

  std::for_each(gpu_agents_.begin(), gpu_agents_.end(), DeleteObject());
  gpu_agents_.clear();

  std::for_each(disabled_gpu_agents_.begin(), disabled_gpu_agents_.end(), DeleteObject());
  disabled_gpu_agents_.clear();

  async_events_control_.Shutdown();

  if (vm_fault_signal_ != nullptr) {
    vm_fault_signal_->DestroySignal();
    vm_fault_signal_ = nullptr;
  }
  core::InterruptSignal::DestroyEvent(vm_fault_event_);
  vm_fault_event_ = nullptr;

  SharedSignalPool.clear();

  EventPool.clear();

  DestroyAgents();

  CloseTools();

  AMD::Unload();
}

void Runtime::LoadExtensions() {
// Load finalizer and extension library
#ifdef HSA_LARGE_MODEL
  static const std::string kFinalizerLib[] = {"hsa-ext-finalize64.dll",
                                              "libhsa-ext-finalize64.so.1"};
#else
  static const std::string kFinalizerLib[] = {"hsa-ext-finalize.dll",
                                              "libhsa-ext-finalize.so.1"};
#endif

  // Update Hsa Api Table with handle of Finalizer extension Apis
  // Skipping finalizer loading since finalizer is no longer distributed.
  // LinkExts will expose the finalizer-not-present implementation.
  // extensions_.LoadFinalizer(kFinalizerLib[os_index(os::current_os)]);
  hsa_api_table_.LinkExts(&extensions_.finalizer_api,
                          core::HsaApiTable::HSA_EXT_FINALIZER_API_TABLE_ID);

  // Update Hsa Api Table with handle of Image extension Apis
  extensions_.LoadImage();
  hsa_api_table_.LinkExts(&extensions_.image_api,
                          core::HsaApiTable::HSA_EXT_IMAGE_API_TABLE_ID);
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
  typedef bool (*tool_init_t)(::HsaApiTable*, uint64_t, uint64_t,
                              const char* const*);
  typedef Agent* (*tool_wrap_t)(Agent*);
  typedef void (*tool_add_t)(Runtime*);

  std::vector<const char*> failed;

  //Get loaded libs and filter to tool libraries.
  struct lib_t {
    lib_t(os::LibHandle lib, uint32_t order, std::string name) : lib_(lib), order_(order), name_(name) {}
    os::LibHandle lib_;
    uint32_t order_;
    std::string name_;
  };

  std::list<lib_t> sorted;
  uint32_t env_count=0;

  // Load env var tool lib names and determine ordering offset.
  std::string tool_names = flag_.tools_lib_names();
  std::vector<std::string> names;
  if (tool_names != "") {
    names = parse_tool_names(tool_names);
    env_count = names.size();
  }

  // Discover loaded tools.
  std::vector<os::LibHandle> loaded = os::GetLoadedToolsLib();
  for(auto& handle : loaded) {
    const uint32_t* order = (const uint32_t*)os::GetExportAddress(handle, "HSA_AMD_TOOL_PRIORITY");
    if(order) {
      sorted.push_back(lib_t(handle, *order+env_count, os::GetLibraryName(handle)));
    } else {
      os::CloseLib(handle);
    }
  }
  
  // Load env var tools.
  env_count=0;
  for (auto& name : names) {
    os::LibHandle tool = os::LoadLib(name);

    if (tool != nullptr) {
      sorted.push_back(lib_t(tool, env_count, name));
      env_count++;
    } else {
      failed.push_back(name.c_str());
      if (flag().report_tool_load_failures())
        fprintf(stderr, "Tool lib \"%s\" failed to load.\n", name.c_str());
    }
  }

  if(!sorted.empty()) {
    // Close duplicate handles
    sorted.sort([](const lib_t& lhs, const lib_t& rhs) {
      if(lhs.lib_ == rhs.lib_)
        return lhs.order_ < rhs.order_;
      return lhs.lib_ < rhs.lib_;
    });

    os::LibHandle current = sorted.front().lib_;
    auto it = sorted.begin();
    it++;
    while(it != sorted.end()) {
      if(it->lib_==current) {
        os::CloseLib(current);
        auto rem = it;
        it = sorted.erase(rem);
      } else {
        current = it->lib_;
        it++;
      }
    }

    // Sort to load order
    sorted.sort([](const lib_t& lhs, const lib_t& rhs) {
      return lhs.order_ < rhs.order_;
    });
    
    for(auto& lib : sorted) {
      auto& tool = lib.lib_;

      rocr::AMD::callback_t<tool_init_t> ld = (tool_init_t)os::GetExportAddress(tool, "OnLoad");
      if (!ld) {
        failed.push_back(lib.name_.c_str());
        os::CloseLib(tool);
        continue;
      }
      if (!ld(&hsa_api_table_.hsa_api,
        hsa_api_table_.hsa_api.version.major_id,
        failed.size(), failed.data())) {
          failed.push_back(lib.name_.c_str());
          os::CloseLib(tool);
          continue;
      }
      tool_libs_.push_back(tool);

      rocr::AMD::callback_t<tool_wrap_t> wrap =
        (tool_wrap_t)os::GetExportAddress(tool, "WrapAgent");
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

      rocr::AMD::callback_t<tool_add_t> add = (tool_add_t)os::GetExportAddress(tool, "AddAgent");
      if (add) add(this);
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
    for (auto& lib : tool_libs_) os::CloseLib(lib);
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

hsa_status_t Runtime::SetCustomSystemEventHandler(hsa_amd_system_event_callback_t callback,
                                                  void* data) {
  ScopedAcquire<KernelMutex> lock(&system_event_lock_);
  system_event_handlers_.push_back(
      std::make_pair(AMD::callback_t<hsa_amd_system_event_callback_t>(callback), data));
  return HSA_STATUS_SUCCESS;
}

std::vector<std::pair<AMD::callback_t<hsa_amd_system_event_callback_t>, void*>>
Runtime::GetSystemEventHandlers() {
  ScopedAcquire<KernelMutex> lock(&system_event_lock_);
  return system_event_handlers_;
}

hsa_status_t Runtime::SetInternalQueueCreateNotifier(hsa_amd_runtime_queue_notifier callback,
                                                     void* user_data) {
  if (internal_queue_create_notifier_) {
    return HSA_STATUS_ERROR;
  } else {
    internal_queue_create_notifier_ = callback;
    internal_queue_create_notifier_user_data_ = user_data;
    return HSA_STATUS_SUCCESS;
  }
}

void Runtime::InternalQueueCreateNotify(const hsa_queue_t* queue, hsa_agent_t agent) {
  if (internal_queue_create_notifier_)
    internal_queue_create_notifier_(queue, agent, internal_queue_create_notifier_user_data_);
}

hsa_status_t Runtime::SetSvmAttrib(void* ptr, size_t size,
                                   hsa_amd_svm_attribute_pair_t* attribute_list,
                                   size_t attribute_count) {
  uint32_t set_attribs = 0;
  std::vector<bool> agent_seen(max_node_id() + 1, false);

  std::vector<HSA_SVM_ATTRIBUTE> attribs;
  attribs.reserve(attribute_count);
  uint32_t set_flags = 0;
  uint32_t clear_flags = 0;

  auto Convert = [&](uint64_t value) -> Agent* {
    hsa_agent_t handle = {value};
    Agent* agent = Agent::Convert(handle);
    if ((agent == nullptr) || !agent->IsValid())
      throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_AGENT,
                               "Invalid agent handle in Runtime::SetSvmAttrib.");
    return agent;
  };

  auto ConvertAllowNull = [&](uint64_t value) -> Agent* {
    hsa_agent_t handle = {value};
    Agent* agent = Agent::Convert(handle);
    if ((agent != nullptr) && (!agent->IsValid()))
      throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_AGENT,
                               "Invalid agent handle in Runtime::SetSvmAttrib.");
    return agent;
  };

  auto ConfirmNew = [&](Agent* agent) {
    if (agent_seen[agent->node_id()])
      throw AMD::hsa_exception(
          HSA_STATUS_ERROR_INCOMPATIBLE_ARGUMENTS,
          "Multiple attributes given for the same agent in Runtime::SetSvmAttrib.");
    agent_seen[agent->node_id()] = true;
  };

  auto Check = [&](uint64_t attrib) {
    if (set_attribs & (1 << attrib))
      throw AMD::hsa_exception(HSA_STATUS_ERROR_INCOMPATIBLE_ARGUMENTS,
                               "Attribute given multiple times in Runtime::SetSvmAttrib.");
    set_attribs |= (1 << attrib);
  };

  auto kmtPair = [](uint32_t attrib, uint32_t value) {
    HSA_SVM_ATTRIBUTE pair = {attrib, value};
    return pair;
  };

  for (uint32_t i = 0; i < attribute_count; i++) {
    auto attrib = attribute_list[i].attribute;
    auto value = attribute_list[i].value;

    switch (attrib) {
      case HSA_AMD_SVM_ATTRIB_GLOBAL_FLAG: {
        Check(attrib);
        switch (value) {
          case HSA_AMD_SVM_GLOBAL_FLAG_FINE_GRAINED:
            set_flags |= HSA_SVM_FLAG_COHERENT;
            break;
          case HSA_AMD_SVM_GLOBAL_FLAG_COARSE_GRAINED:
            clear_flags |= HSA_SVM_FLAG_COHERENT;
            break;
          default:
            throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT,
                                     "Invalid HSA_AMD_SVM_ATTRIB_GLOBAL_FLAG value.");
        }
        break;
      }
      case HSA_AMD_SVM_ATTRIB_READ_ONLY: {
        Check(attrib);
        if (value)
          set_flags |= HSA_SVM_FLAG_GPU_RO;
        else
          clear_flags |= HSA_SVM_FLAG_GPU_RO;
        break;
      }
      case HSA_AMD_SVM_ATTRIB_HIVE_LOCAL: {
        Check(attrib);
        if (value)
          set_flags |= HSA_SVM_FLAG_HIVE_LOCAL;
        else
          clear_flags |= HSA_SVM_FLAG_HIVE_LOCAL;
        break;
      }
      case HSA_AMD_SVM_ATTRIB_MIGRATION_GRANULARITY: {
        Check(attrib);
        // Max migration size is 1GB.
        if (value > 18) value = 18;
        attribs.push_back(kmtPair(HSA_SVM_ATTR_GRANULARITY, value));
        break;
      }
      case HSA_AMD_SVM_ATTRIB_PREFERRED_LOCATION: {
        Check(attrib);
        Agent* agent = ConvertAllowNull(value);
        if (agent == nullptr)
          attribs.push_back(kmtPair(HSA_SVM_ATTR_PREFERRED_LOC, INVALID_NODEID));
        else
          attribs.push_back(kmtPair(HSA_SVM_ATTR_PREFERRED_LOC, agent->node_id()));
        break;
      }
      case HSA_AMD_SVM_ATTRIB_READ_MOSTLY: {
        Check(attrib);
        if (value)
          set_flags |= HSA_SVM_FLAG_GPU_READ_MOSTLY;
        else
          clear_flags |= HSA_SVM_FLAG_GPU_READ_MOSTLY;
        break;
      }
      case HSA_AMD_SVM_ATTRIB_GPU_EXEC: {
        Check(attrib);
        if (value)
          set_flags |= HSA_SVM_FLAG_GPU_EXEC;
        else
          clear_flags |= HSA_SVM_FLAG_GPU_EXEC;
        break;
      }
      case HSA_AMD_SVM_ATTRIB_AGENT_ACCESSIBLE: {
        Agent* agent = Convert(value);
        ConfirmNew(agent);
        if (agent->device_type() == Agent::kAmdCpuDevice) {
          set_flags |= HSA_SVM_FLAG_HOST_ACCESS;
        } else {
          attribs.push_back(kmtPair(HSA_SVM_ATTR_ACCESS, agent->node_id()));
        }
        break;
      }
      case HSA_AMD_SVM_ATTRIB_AGENT_ACCESSIBLE_IN_PLACE: {
        Agent* agent = Convert(value);
        ConfirmNew(agent);
        if (agent->device_type() == Agent::kAmdCpuDevice) {
          set_flags |= HSA_SVM_FLAG_HOST_ACCESS;
        } else {
          attribs.push_back(kmtPair(HSA_SVM_ATTR_ACCESS_IN_PLACE, agent->node_id()));
        }
        break;
      }
      case HSA_AMD_SVM_ATTRIB_AGENT_NO_ACCESS: {
        Agent* agent = Convert(value);
        ConfirmNew(agent);
        if (agent->device_type() == Agent::kAmdCpuDevice) {
          clear_flags |= HSA_SVM_FLAG_HOST_ACCESS;
        } else {
          attribs.push_back(kmtPair(HSA_SVM_ATTR_NO_ACCESS, agent->node_id()));
        }
        break;
      }
      default:
        throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT,
                                 "Illegal or invalid attribute in Runtime::SetSvmAttrib");
    }
  }

  // Merge CPU access properties - grant access if any CPU needs access.
  // Probably wrong.
  if (set_flags & HSA_SVM_FLAG_HOST_ACCESS) clear_flags &= ~HSA_SVM_FLAG_HOST_ACCESS;

  // Add flag updates
  if (clear_flags) attribs.push_back(kmtPair(HSA_SVM_ATTR_CLR_FLAGS, clear_flags));
  if (set_flags) attribs.push_back(kmtPair(HSA_SVM_ATTR_SET_FLAGS, set_flags));

  uint8_t* base = AlignDown((uint8_t*)ptr, 4096);
  uint8_t* end = AlignUp((uint8_t*)ptr + size, 4096);
  size_t len = end - base;
  HSAKMT_STATUS error = hsaKmtSVMSetAttr(base, len, attribs.size(), &attribs[0]);
  if (error != HSAKMT_STATUS_SUCCESS)
    throw AMD::hsa_exception(HSA_STATUS_ERROR, "hsaKmtSVMSetAttr failed.");

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::GetSvmAttrib(void* ptr, size_t size,
                                   hsa_amd_svm_attribute_pair_t* attribute_list,
                                   size_t attribute_count) {
  std::vector<HSA_SVM_ATTRIBUTE> attribs;
  attribs.reserve(attribute_count);

  std::vector<int> kmtIndices(attribute_count);

  bool getFlags = false;

  auto Convert = [&](uint64_t value) -> Agent* {
    hsa_agent_t handle = {value};
    Agent* agent = Agent::Convert(handle);
    if ((agent == nullptr) || !agent->IsValid())
      throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_AGENT,
                               "Invalid agent handle in Runtime::GetSvmAttrib.");
    return agent;
  };

  auto kmtPair = [](uint32_t attrib, uint32_t value) {
    HSA_SVM_ATTRIBUTE pair = {attrib, value};
    return pair;
  };

  for (uint32_t i = 0; i < attribute_count; i++) {
    auto& attrib = attribute_list[i].attribute;
    auto& value = attribute_list[i].value;

    switch (attrib) {
      case HSA_AMD_SVM_ATTRIB_GLOBAL_FLAG:
      case HSA_AMD_SVM_ATTRIB_READ_ONLY:
      case HSA_AMD_SVM_ATTRIB_HIVE_LOCAL:
      case HSA_AMD_SVM_ATTRIB_READ_MOSTLY: {
        getFlags = true;
        kmtIndices[i] = -1;
        break;
      }
      case HSA_AMD_SVM_ATTRIB_MIGRATION_GRANULARITY: {
        kmtIndices[i] = attribs.size();
        attribs.push_back(kmtPair(HSA_SVM_ATTR_GRANULARITY, 0));
        break;
      }
      case HSA_AMD_SVM_ATTRIB_PREFERRED_LOCATION: {
        kmtIndices[i] = attribs.size();
        attribs.push_back(kmtPair(HSA_SVM_ATTR_PREFERRED_LOC, 0));
        break;
      }
      case HSA_AMD_SVM_ATTRIB_PREFETCH_LOCATION: {
        value = Agent::Convert(GetSVMPrefetchAgent(ptr, size)).handle;
        kmtIndices[i] = -1;
        break;
      }
      case HSA_AMD_SVM_ATTRIB_ACCESS_QUERY: {
        Agent* agent = Convert(value);
        if (agent->device_type() == Agent::kAmdCpuDevice) {
          getFlags = true;
          kmtIndices[i] = -1;
        } else {
          kmtIndices[i] = attribs.size();
          attribs.push_back(kmtPair(HSA_SVM_ATTR_ACCESS, agent->node_id()));
        }
        break;
      }
      default:
        throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT,
                                 "Illegal or invalid attribute in Runtime::SetSvmAttrib");
    }
  }

  if (getFlags) {
    // Order is important to later code.
    attribs.push_back(kmtPair(HSA_SVM_ATTR_CLR_FLAGS, 0));
    attribs.push_back(kmtPair(HSA_SVM_ATTR_SET_FLAGS, 0));
  }

  uint8_t* base = AlignDown((uint8_t*)ptr, 4096);
  uint8_t* end = AlignUp((uint8_t*)ptr + size, 4096);
  size_t len = end - base;
  if (attribs.size() != 0) {
    HSAKMT_STATUS error = hsaKmtSVMGetAttr(base, len, attribs.size(), &attribs[0]);
    if (error != HSAKMT_STATUS_SUCCESS)
      throw AMD::hsa_exception(HSA_STATUS_ERROR, "hsaKmtSVMGetAttr failed.");
  }

  for (uint32_t i = 0; i < attribute_count; i++) {
    auto& attrib = attribute_list[i].attribute;
    auto& value = attribute_list[i].value;

    switch (attrib) {
      case HSA_AMD_SVM_ATTRIB_GLOBAL_FLAG: {
        if (attribs[attribs.size() - 1].value & HSA_SVM_FLAG_COHERENT) {
          value = HSA_AMD_SVM_GLOBAL_FLAG_FINE_GRAINED;
          break;
        }
        if (attribs[attribs.size() - 2].value & HSA_SVM_FLAG_COHERENT)
          value = HSA_AMD_SVM_GLOBAL_FLAG_COARSE_GRAINED;
        else
          value = HSA_AMD_SVM_GLOBAL_FLAG_INDETERMINATE;
        break;
      }
      case HSA_AMD_SVM_ATTRIB_READ_ONLY: {
        value = (attribs[attribs.size() - 1].value & HSA_SVM_FLAG_GPU_RO);
        break;
      }
      case HSA_AMD_SVM_ATTRIB_HIVE_LOCAL: {
        value = (attribs[attribs.size() - 1].value & HSA_SVM_FLAG_HIVE_LOCAL);
        break;
      }
      case HSA_AMD_SVM_ATTRIB_MIGRATION_GRANULARITY: {
        value = attribs[kmtIndices[i]].value;
        break;
      }
      case HSA_AMD_SVM_ATTRIB_PREFERRED_LOCATION: {
        uint64_t node = attribs[kmtIndices[i]].value;
        Agent* agent = nullptr;
        if (node != INVALID_NODEID) agent = agents_by_node_[node][0];
        value = Agent::Convert(agent).handle;
        break;
      }
      case HSA_AMD_SVM_ATTRIB_PREFETCH_LOCATION: {
        break;
      }
      case HSA_AMD_SVM_ATTRIB_READ_MOSTLY: {
        value = (attribs[attribs.size() - 1].value & HSA_SVM_FLAG_GPU_READ_MOSTLY);
        break;
      }
      case HSA_AMD_SVM_ATTRIB_ACCESS_QUERY: {
        if (kmtIndices[i] == -1) {
          if (attribs[attribs.size() - 1].value & HSA_SVM_FLAG_HOST_ACCESS)
            attrib = HSA_AMD_SVM_ATTRIB_AGENT_ACCESSIBLE;
        } else {
          switch (attribs[kmtIndices[i]].type) {
            case HSA_SVM_ATTR_ACCESS:
              attrib = HSA_AMD_SVM_ATTRIB_AGENT_ACCESSIBLE;
              break;
            case HSA_SVM_ATTR_ACCESS_IN_PLACE:
              attrib = HSA_AMD_SVM_ATTRIB_AGENT_ACCESSIBLE_IN_PLACE;
              break;
            case HSA_SVM_ATTR_NO_ACCESS:
              attrib = HSA_AMD_SVM_ATTRIB_AGENT_NO_ACCESS;
              break;
            default:
              assert(false && "Bad agent accessibility from KFD.");
          }
        }
        break;
      }
      default:
        throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT,
                                 "Illegal or invalid attribute in Runtime::GetSvmAttrib");
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::SvmPrefetch(void* ptr, size_t size, hsa_agent_t agent,
                                  uint32_t num_dep_signals, const hsa_signal_t* dep_signals,
                                  hsa_signal_t completion_signal) {
  uintptr_t base = reinterpret_cast<uintptr_t>(AlignDown(ptr, 4096));
  uintptr_t end = AlignUp(reinterpret_cast<uintptr_t>(ptr) + size, 4096);
  size_t len = end - base;

  PrefetchOp* op = new PrefetchOp();
  MAKE_NAMED_SCOPE_GUARD(OpGuard, [&]() { delete op; });

  Agent* dest = Agent::Convert(agent);
  if (dest->device_type() == Agent::kAmdCpuDevice)
    op->node_id = 0;
  else
    op->node_id = dest->node_id();

  op->base = reinterpret_cast<void*>(base);
  op->size = len;
  op->completion = completion_signal;
  if (num_dep_signals > 1) {
    op->remaining_deps = num_dep_signals - 1;
    for (int i = 0; i < num_dep_signals - 1; i++) op->dep_signals.push_back(dep_signals[i]);
  } else {
    op->remaining_deps = 0;
  }

  {
    ScopedAcquire<KernelMutex> lock(&prefetch_lock_);
    // Remove all fully overlapped and trim partially overlapped ranges.
    // Get iteration bounds
    auto start = prefetch_map_.upper_bound(base);
    if (start != prefetch_map_.begin()) start--;
    auto stop = prefetch_map_.lower_bound(end);

    auto isEndNode = [&](decltype(start) node) { return node->second.next == prefetch_map_.end(); };
    auto isFirstNode = [&](decltype(start) node) {
      return node->second.prev == prefetch_map_.end();
    };

    // Trim and remove old ranges.
    while (start != stop) {
      uintptr_t startBase = start->first;
      uintptr_t startEnd = startBase + start->second.bytes;

      auto ibase = Max(startBase, base);
      auto iend = Min(startEnd, end);
      // Check for overlap
      if (ibase < iend) {
        // Second range check
        if (iend < startEnd) {
          auto ret = prefetch_map_.insert(
              std::make_pair(iend, PrefetchRange(startEnd - iend, start->second.op)));
          assert(ret.second && "Prefetch map insert failed during range split.");

          auto it = ret.first;
          it->second.prev = start;
          it->second.next = start->second.next;
          start->second.next = it;
          if (!isEndNode(it)) it->second.next->second.prev = it;
        }

        // Is the first interval of the old range valid
        if (startBase < ibase) {
          start->second.bytes = ibase - startBase;
        } else {
          if (isFirstNode(start)) {
            start->second.op->prefetch_map_entry = start->second.next;
            if (!isEndNode(start)) start->second.next->second.prev = prefetch_map_.end();
          } else {
            start->second.prev->second.next = start->second.next;
            if (!isEndNode(start)) start->second.next->second.prev = start->second.prev;
          }
          start = prefetch_map_.erase(start);
          continue;
        }
      }
      start++;
    }

    // Insert new range.
    auto ret = prefetch_map_.insert(std::make_pair(base, PrefetchRange(len, op)));
    assert(ret.second && "Prefetch map insert failed.");

    auto it = ret.first;
    op->prefetch_map_entry = it;
    it->second.next = it->second.prev = prefetch_map_.end();
  }

  // Remove the prefetch's ranges from the map.
  static auto removePrefetchRanges = [](PrefetchOp* op) {
    ScopedAcquire<KernelMutex> lock(&Runtime::runtime_singleton_->prefetch_lock_);
    auto it = op->prefetch_map_entry;
    while (it != Runtime::runtime_singleton_->prefetch_map_.end()) {
      auto next = it->second.next;
      Runtime::runtime_singleton_->prefetch_map_.erase(it);
      it = next;
    }
  };

  // Prefetch Signal handler for synchronization.
  static hsa_amd_signal_handler signal_handler = [](hsa_signal_value_t value, void* arg) {
    PrefetchOp* op = reinterpret_cast<PrefetchOp*>(arg);

    if (op->remaining_deps > 0) {
      op->remaining_deps--;
      Runtime::runtime_singleton_->SetAsyncSignalHandler(
          op->dep_signals[op->remaining_deps], HSA_SIGNAL_CONDITION_EQ, 0, signal_handler, arg);
      return false;
    }

    HSA_SVM_ATTRIBUTE attrib;
    attrib.type = HSA_SVM_ATTR_PREFETCH_LOC;
    attrib.value = op->node_id;
    HSAKMT_STATUS error = hsaKmtSVMSetAttr(op->base, op->size, 1, &attrib);
    assert(error == HSAKMT_STATUS_SUCCESS && "KFD Prefetch failed.");

    removePrefetchRanges(op);

    if (op->completion.handle != 0) Signal::Convert(op->completion)->SubRelaxed(1);
    delete op;

    return false;
  };

  auto no_dependencies = [](void* arg) { signal_handler(0, arg); };

  MAKE_NAMED_SCOPE_GUARD(RangeGuard, [&]() { removePrefetchRanges(op); });

  hsa_status_t err;
  if (num_dep_signals == 0)
    err = AMD::hsa_amd_async_function(no_dependencies, op);
  else
    err = SetAsyncSignalHandler(dep_signals[num_dep_signals - 1], HSA_SIGNAL_CONDITION_EQ, 0,
                                signal_handler, op);
  if (err != HSA_STATUS_SUCCESS) throw AMD::hsa_exception(err, "Signal handler unable to be set.");

  RangeGuard.Dismiss();
  OpGuard.Dismiss();
  return HSA_STATUS_SUCCESS;
}

Agent* Runtime::GetSVMPrefetchAgent(void* ptr, size_t size) {
  uintptr_t base = reinterpret_cast<uintptr_t>(AlignDown(ptr, 4096));
  uintptr_t end = AlignUp(reinterpret_cast<uintptr_t>(ptr) + size, 4096);

  std::vector<std::pair<uintptr_t, uintptr_t>> holes;

  ScopedAcquire<KernelMutex> lock(&Runtime::runtime_singleton_->prefetch_lock_);
  auto start = prefetch_map_.upper_bound(base);
  if (start != prefetch_map_.begin()) start--;
  auto stop = prefetch_map_.lower_bound(end);

  // KFD returns -1 for no or mixed destinations.
  uint32_t prefetch_node = -2;
  if (start != stop) {
    prefetch_node = start->second.op->node_id;
  }

  while (start != stop) {
    uintptr_t startBase = start->first;
    uintptr_t startEnd = startBase + start->second.bytes;

    auto ibase = Max(base, startBase);
    auto iend = Min(end, startEnd);
    // Check for intersection with the query
    if (ibase < iend) {
      // If prefetch locations are different then we report null agent.
      if (prefetch_node != start->second.op->node_id) return nullptr;

      // Push leading gap to an array for checking KFD.
      if (base < ibase) holes.push_back(std::make_pair(base, ibase - base));

      // Trim query range.
      base = iend;
    }
    start++;
  }
  if (base < end) holes.push_back(std::make_pair(base, end - base));

  HSA_SVM_ATTRIBUTE attrib;
  attrib.type = HSA_SVM_ATTR_PREFETCH_LOC;
  for (auto& range : holes) {
    HSAKMT_STATUS error =
        hsaKmtSVMGetAttr(reinterpret_cast<void*>(range.first), range.second, 1, &attrib);
    assert(error == HSAKMT_STATUS_SUCCESS && "KFD prefetch query failed.");

    if (attrib.value == -1) return nullptr;
    if (prefetch_node == -2) prefetch_node = attrib.value;
    if (prefetch_node != attrib.value) return nullptr;
  }

  assert(prefetch_node != -2 && "prefetch_node was not updated.");
  assert(prefetch_node != -1 && "Should have already returned.");
  return agents_by_node_[prefetch_node][0];
}

hsa_status_t Runtime::DmaBufExport(const void* ptr, size_t size, int* dmabuf, uint64_t* offset) {
#ifdef __linux__
  ScopedAcquire<KernelSharedMutex::Shared> lock(memory_lock_.shared());
  // Lookup containing allocation.
  auto mem = allocation_map_.upper_bound(ptr);
  if (mem != allocation_map_.begin()) {
    mem--;
    if ((mem->first <= ptr) &&
        (ptr < reinterpret_cast<const uint8_t*>(mem->first) + mem->second.size)) {
      // Check size is in bounds.
      if (uintptr_t(ptr) - uintptr_t(mem->first) + size <= mem->second.size) {
        // Check allocation is on GPU
        if (mem->second.region->owner()->device_type() != Agent::kAmdGpuDevice)
          return HSA_STATUS_ERROR_INVALID_AGENT;

        int fd;
        uint64_t off;
        HSAKMT_STATUS err = hsaKmtExportDMABufHandle(const_cast<void*>(ptr), size, &fd, &off);
        if (err == HSAKMT_STATUS_SUCCESS) {
          *dmabuf = fd;
          *offset = off;
          return HSA_STATUS_SUCCESS;
        }

        assert((err != HSAKMT_STATUS_INVALID_PARAMETER) &&
               "Thunk does not recognize an expected allocation.");
        if (err == HSAKMT_STATUS_ERROR) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
        return HSA_STATUS_ERROR;
      }
    }
  }
  return HSA_STATUS_ERROR_INVALID_ALLOCATION;
#else
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
#endif
}

hsa_status_t Runtime::DmaBufClose(int dmabuf) {
#ifdef __linux__
  int err = close(dmabuf);
  if (err == 0) return HSA_STATUS_SUCCESS;
  return HSA_STATUS_ERROR_RESOURCE_FREE;
#else
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
#endif
}

}  // namespace core
}  // namespace rocr
