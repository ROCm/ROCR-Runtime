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

#include "core/inc/amd_memory_region.h"

#include <algorithm>
#include <set>

#include "core/inc/runtime.h"
#include "core/inc/amd_cpu_agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/util/utils.h"
#include "core/inc/exceptions.h"

namespace rocr {
namespace AMD {

// Tracks aggregate size of system memory available on platform
size_t MemoryRegion::max_sysmem_alloc_size_ = 0;

void* MemoryRegion::AllocateKfdMemory(const HsaMemFlags& flag,
                                      HSAuint32 node_id, size_t size) {
  void* ret = NULL;
  const HSAKMT_STATUS status = hsaKmtAllocMemory(node_id, size, flag, &ret);
  return (status == HSAKMT_STATUS_SUCCESS) ? ret : NULL;
}

void MemoryRegion::FreeKfdMemory(void* ptr, size_t size) {
  if (ptr == NULL || size == 0) {
    return;
  }

  HSAKMT_STATUS status = hsaKmtFreeMemory(ptr, size);
  assert(status == HSAKMT_STATUS_SUCCESS);
}

bool MemoryRegion::RegisterMemory(void* ptr, size_t size, const HsaMemFlags& MemFlags) {
  assert(ptr != NULL);
  assert(size != 0);

  const HSAKMT_STATUS status = hsaKmtRegisterMemoryWithFlags(ptr, size, MemFlags);
  return (status == HSAKMT_STATUS_SUCCESS);
}

void MemoryRegion::DeregisterMemory(void* ptr) { hsaKmtDeregisterMemory(ptr); }

bool MemoryRegion::MakeKfdMemoryResident(size_t num_node, const uint32_t* nodes, const void* ptr,
                                         size_t size, uint64_t* alternate_va,
                                         HsaMemMapFlags map_flag) {
  assert(num_node > 0);
  assert(nodes != NULL);

  *alternate_va = 0;
  const HSAKMT_STATUS status = hsaKmtMapMemoryToGPUNodes(
      const_cast<void*>(ptr), size, alternate_va, map_flag, num_node, const_cast<uint32_t*>(nodes));

  return (status == HSAKMT_STATUS_SUCCESS);
}

void MemoryRegion::MakeKfdMemoryUnresident(const void* ptr) {
  hsaKmtUnmapMemoryToGPU(const_cast<void*>(ptr));
}

MemoryRegion::MemoryRegion(bool fine_grain, bool kernarg, bool full_profile,
                           bool extended_scope_fine_grain, core::Agent* owner,
                           const HsaMemoryProperties& mem_props)
    : core::MemoryRegion(fine_grain, kernarg, full_profile, owner),
      mem_props_(mem_props),
      extended_scope_fine_grain_(extended_scope_fine_grain),
      max_single_alloc_size_(0),
      virtual_size_(0),
      fragment_allocator_(BlockAllocator(*this)) {
  virtual_size_ = GetPhysicalSize();

  // extended_scope_fine_grain and fine_grain memory regions are mutually exclusive
  assert(!(fine_grain && extended_scope_fine_grain));

  mem_flag_.Value = 0;
  map_flag_.Value = 0;
  static const HSAuint64 kGpuVmSize = (1ULL << 40);

  // Bind the memory region based on whether it is
  // coarse or fine grain or extended scope fine grain.
  mem_flag_.ui32.CoarseGrain = (fine_grain || extended_scope_fine_grain) ? 0 : 1;

  if (IsLocalMemory()) {
    mem_flag_.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    mem_flag_.ui32.NoSubstitute = 1;
    mem_flag_.ui32.HostAccess =
        (mem_props_.HeapType == HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE) ? 0 : 1;
    mem_flag_.ui32.NonPaged = 1;

    virtual_size_ = kGpuVmSize;

    // If memory region is extended scope fine grained
    // mark the page table entries for this memory region
    // as MTYPE_UC. Full read and write ordering are guaranteed
    // to this address.
    if (extended_scope_fine_grain) {
      AMD::GpuAgent* agent_ =
          const_cast<AMD::GpuAgent*>(reinterpret_cast<const AMD::GpuAgent*>(owner));
      if (agent_->isa()->GetVersion() == core::Isa::Version(9, 4, 0) ||
          agent_->isa()->GetVersion() == core::Isa::Version(9, 4, 1) ||
          agent_->isa()->GetVersion() == core::Isa::Version(9, 4, 2))
        mem_flag_.ui32.Uncached = 1;
    }

  } else if (IsSystem()) {
    mem_flag_.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    mem_flag_.ui32.NoSubstitute = 0;
    mem_flag_.ui32.HostAccess = 1;
    mem_flag_.ui32.CachePolicy = HSA_CACHING_CACHED;

    if (kernarg) mem_flag_.ui32.Uncached = 1;

    virtual_size_ =
        (full_profile) ? os::GetUserModeVirtualMemorySize() : kGpuVmSize;
  }


  // Adjust allocatable size per page align
  max_single_alloc_size_ = AlignDown(static_cast<size_t>(GetPhysicalSize()), kPageSize_);

  // Keep track of total system memory available
  // @note: System memory is surfaced as both coarse
  // and fine grain memory regions. To track total system
  // memory only fine grain is considered as it avoids
  // double counting
  if (IsSystem() && (fine_grain)) {
    max_sysmem_alloc_size_ += max_single_alloc_size_;
  }

  assert(GetVirtualSize() != 0);
  assert(GetPhysicalSize() <= GetVirtualSize());
  assert(IsMultipleOf(max_single_alloc_size_, kPageSize_));
}

MemoryRegion::~MemoryRegion() {}

hsa_status_t MemoryRegion::Allocate(size_t& size, AllocateFlags alloc_flags, void** address) const {
  ScopedAcquire<KernelMutex> lock(&owner()->agent_memory_lock_);
  return AllocateImpl(size, alloc_flags, address);
}

hsa_status_t MemoryRegion::AllocateImpl(size_t& size, AllocateFlags alloc_flags,
                                        void** address) const {
  if (address == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (!IsSystem() && !IsLocalMemory()) {
    return HSA_STATUS_ERROR_INVALID_ALLOCATION;
  }

  // Alocation requests for system memory considers aggregate
  // memory available on all CPU devices
  if (size > ((IsSystem() ?
                max_sysmem_alloc_size_ : max_single_alloc_size_))) {
    return HSA_STATUS_ERROR_INVALID_ALLOCATION;
  }

  size = AlignUp(size, kPageSize_);

  HsaMemFlags kmt_alloc_flags(mem_flag_);
  kmt_alloc_flags.ui32.ExecuteAccess =
      (alloc_flags & AllocateExecutable ? 1 : 0);
  kmt_alloc_flags.ui32.AQLQueueMemory =
      (alloc_flags & AllocateDoubleMap ? 1 : 0);
  if (IsSystem() && (alloc_flags & AllocateNonPaged))
      kmt_alloc_flags.ui32.NonPaged = 1;

  // Allocate pseudo fine grain memory
  kmt_alloc_flags.ui32.CoarseGrain = (alloc_flags & AllocatePCIeRW ? 0 : kmt_alloc_flags.ui32.CoarseGrain);

  // Only allow using the suballocator for ordinary VRAM.
  if (IsLocalMemory()) {
    bool subAllocEnabled = !core::Runtime::runtime_singleton_->flag().disable_fragment_alloc();
    // Avoid modifying executable or queue allocations.
    bool useSubAlloc = subAllocEnabled;
    useSubAlloc &= ((alloc_flags & (~AllocateRestrict)) == 0);
    if (useSubAlloc) {
      *address = fragment_allocator_.alloc(size);

      if ((alloc_flags & AllocateAsan) &&
          hsaKmtReplaceAsanHeaderPage(*address) != HSAKMT_STATUS_SUCCESS) {
        fragment_allocator_.free(*address);
        *address = NULL;
        return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
      }
      return HSA_STATUS_SUCCESS;
    }
  }

  // Allocate memory.
  // If it fails attempt to release memory from the block allocator and retry.
  *address = AllocateKfdMemory(kmt_alloc_flags, owner()->node_id(), size);
  if (*address == nullptr) {
    owner()->Trim();
    *address = AllocateKfdMemory(kmt_alloc_flags, owner()->node_id(), size);
  }

  if (*address != nullptr) {
    // Commit the memory.
    // For system memory, on non-restricted allocation, map it to all GPUs. On
    // restricted allocation, only CPU is allowed to access by default, so
    // no need to map
    // For local memory, only map it to the owning GPU. Mapping to other GPU,
    // if the access is allowed, is performed on AllowAccess.
    HsaMemMapFlags map_flag = map_flag_;
    size_t map_node_count = 1;
    const uint32_t owner_node_id = owner()->node_id();
    const uint32_t* map_node_id = &owner_node_id;

    if (IsSystem()) {
      if ((alloc_flags & AllocateRestrict) == 0) {
        // Map to all GPU agents.
        map_node_count = core::Runtime::runtime_singleton_->gpu_ids().size();

        if (map_node_count == 0) {
          // No need to pin since no GPU in the platform.
          return HSA_STATUS_SUCCESS;
        }

        map_node_id = &core::Runtime::runtime_singleton_->gpu_ids()[0];
      } else {
        // No need to pin it for CPU exclusive access.
        return HSA_STATUS_SUCCESS;
      }
    }

    uint64_t alternate_va = 0;
    const bool is_resident = MakeKfdMemoryResident(
        map_node_count, map_node_id, *address, size, &alternate_va, map_flag);

    const bool require_pinning =
        (!full_profile() || IsLocalMemory() || IsScratch());

    if (require_pinning && !is_resident) {
      FreeKfdMemory(*address, size);
      *address = NULL;
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }

    if ((alloc_flags & AllocateAsan) &&
        hsaKmtReplaceAsanHeaderPage(*address) != HSAKMT_STATUS_SUCCESS) {
      FreeKfdMemory(*address, size);
      *address = NULL;
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
    return HSA_STATUS_SUCCESS;
  }

  return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
}

hsa_status_t MemoryRegion::Free(void* address, size_t size) const {
  ScopedAcquire<KernelMutex> lock(&owner()->agent_memory_lock_);
  return FreeImpl(address, size);
}

hsa_status_t MemoryRegion::FreeImpl(void* address, size_t size) const {
  if (fragment_allocator_.free(address)) return HSA_STATUS_SUCCESS;

  MakeKfdMemoryUnresident(address);

  FreeKfdMemory(address, size);

  return HSA_STATUS_SUCCESS;
}

// TODO:  Look into a better name and/or making this process transparent to exporting.
hsa_status_t MemoryRegion::IPCFragmentExport(void* address) const {
  ScopedAcquire<KernelMutex> lock(&owner()->agent_memory_lock_);
  if (!fragment_allocator_.discardBlock(address)) return HSA_STATUS_ERROR_INVALID_ALLOCATION;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t MemoryRegion::GetInfo(hsa_region_info_t attribute,
                                   void* value) const {
  switch (attribute) {
    case HSA_REGION_INFO_SEGMENT:
      switch (mem_props_.HeapType) {
        case HSA_HEAPTYPE_SYSTEM:
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
          *((hsa_region_segment_t*)value) = HSA_REGION_SEGMENT_GLOBAL;
          break;
        case HSA_HEAPTYPE_GPU_LDS:
          *((hsa_region_segment_t*)value) = HSA_REGION_SEGMENT_GROUP;
          break;
        default:
          assert(false && "Memory region should only be global, group");
          break;
      }
      break;
    case HSA_REGION_INFO_GLOBAL_FLAGS:
      switch (mem_props_.HeapType) {
        case HSA_HEAPTYPE_SYSTEM:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE: {
          uint32_t ret = 0;

          ret = fine_grain()                ? HSA_REGION_GLOBAL_FLAG_FINE_GRAINED
              : extended_scope_fine_grain() ? HSA_REGION_GLOBAL_FLAG_EXTENDED_SCOPE_FINE_GRAINED
                                            : HSA_REGION_GLOBAL_FLAG_COARSE_GRAINED;

          if (kernarg()) ret |= HSA_REGION_GLOBAL_FLAG_KERNARG;
          *((uint32_t*)value) = ret;
          break;
        }
        default:
          *((uint32_t*)value) = 0;
          break;
      }
      break;
    case HSA_REGION_INFO_SIZE:
      *((size_t*)value) = static_cast<size_t>(GetPhysicalSize());
      break;
    case HSA_REGION_INFO_ALLOC_MAX_SIZE:
      switch (mem_props_.HeapType) {
        case HSA_HEAPTYPE_SYSTEM:
          *((size_t*)value) = max_sysmem_alloc_size_;
          break;
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
        case HSA_HEAPTYPE_GPU_SCRATCH:
          *((size_t*)value) = max_single_alloc_size_;
          break;
        default:
          *((size_t*)value) = 0;
      }
      break;
    case HSA_REGION_INFO_RUNTIME_ALLOC_ALLOWED:
      switch (mem_props_.HeapType) {
        case HSA_HEAPTYPE_SYSTEM:
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
          *((bool*)value) = true;
          break;
        default:
          *((bool*)value) = false;
          break;
      }
      break;
    case HSA_REGION_INFO_RUNTIME_ALLOC_GRANULE:
      switch (mem_props_.HeapType) {
        case HSA_HEAPTYPE_SYSTEM:
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
          *((size_t*)value) = kPageSize_;
          break;
        default:
          *((size_t*)value) = 0;
          break;
      }
      break;
    case HSA_REGION_INFO_RUNTIME_ALLOC_ALIGNMENT:
      switch (mem_props_.HeapType) {
        case HSA_HEAPTYPE_SYSTEM:
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
          *((size_t*)value) = kPageSize_;
          break;
        default:
          *((size_t*)value) = 0;
          break;
      }
      break;
    default:
      switch ((hsa_amd_region_info_t)attribute) {
        case HSA_AMD_REGION_INFO_HOST_ACCESSIBLE:
          *((bool*)value) =
              (mem_props_.HeapType == HSA_HEAPTYPE_SYSTEM) ? true : false;
          break;
        case HSA_AMD_REGION_INFO_BASE:
          *((void**)value) = reinterpret_cast<void*>(GetBaseAddress());
          break;
        case HSA_AMD_REGION_INFO_BUS_WIDTH:
          *((uint32_t*)value) = BusWidth();
          break;
        case HSA_AMD_REGION_INFO_MAX_CLOCK_FREQUENCY:
          *((uint32_t*)value) = MaxMemCloc();
          break;
        default:
          return HSA_STATUS_ERROR_INVALID_ARGUMENT;
          break;
      }
      break;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t MemoryRegion::GetPoolInfo(hsa_amd_memory_pool_info_t attribute,
                                       void* value) const {
  switch (attribute) {
    case HSA_AMD_MEMORY_POOL_INFO_SEGMENT:
    case HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS:
    case HSA_AMD_MEMORY_POOL_INFO_SIZE:
    case HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALLOWED:
    case HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_GRANULE:
    case HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALIGNMENT:
      return GetInfo(static_cast<hsa_region_info_t>(attribute), value);
    case HSA_AMD_MEMORY_POOL_INFO_ACCESSIBLE_BY_ALL:
      *((bool*)value) = IsSystem() ? true : false;
      break;
    case HSA_AMD_MEMORY_POOL_INFO_ALLOC_MAX_SIZE:
      switch (mem_props_.HeapType) {
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
        case HSA_HEAPTYPE_GPU_SCRATCH:
          return GetInfo(HSA_REGION_INFO_ALLOC_MAX_SIZE, value);
        case HSA_HEAPTYPE_SYSTEM:
          // Aggregate size available for allocation
          *((size_t*)value) = max_sysmem_alloc_size_;
          break;
        default:
          *((size_t*)value) = 0;
      }
      break;
    case HSA_AMD_MEMORY_POOL_INFO_LOCATION:
      if (IsLocalMemory())
        *((hsa_amd_memory_pool_location_t*)value) = HSA_AMD_MEMORY_POOL_LOCATION_GPU;
      else if (IsSystem())
        *((hsa_amd_memory_pool_location_t*)value) = HSA_AMD_MEMORY_POOL_LOCATION_CPU;
      else
        return HSA_STATUS_ERROR_INVALID_ARGUMENT;
      break;
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_amd_memory_pool_access_t MemoryRegion::GetAccessInfo(
    const core::Agent& agent, const core::Runtime::LinkInfo& link_info) const {

  // Return allowed by default if memory pool is owned by requesting device
  if (agent.public_handle().handle == owner()->public_handle().handle) {
    return HSA_AMD_MEMORY_POOL_ACCESS_ALLOWED_BY_DEFAULT;
  }

  // Requesting device does not have a link
  if (link_info.num_hop < 1) {
    return HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED;
  }

  // Determine access to fine and coarse grained system memory
  // Return allowed by default if requesting device is a CPU
  // Return disallowed by default if requesting device is not a CPU
  if (IsSystem()) {
    return (agent.device_type() == core::Agent::kAmdCpuDevice) ?
            (HSA_AMD_MEMORY_POOL_ACCESS_ALLOWED_BY_DEFAULT) :
            (HSA_AMD_MEMORY_POOL_ACCESS_DISALLOWED_BY_DEFAULT);
  }

  // Determine access type for device local memory which is
  // guaranteed to be HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC

  if (IsLocalMemory()) {
    // Return disallowed by default if memory is coarse
    // grained or extended scope fine grained without regard to link type
    if (fine_grain() == false) {
      return HSA_AMD_MEMORY_POOL_ACCESS_DISALLOWED_BY_DEFAULT;
    }

    // Return disallowed by default if memory is fine
    // grained and requesting device is connected via xGMI link
    if (agent.HiveId() == owner()->HiveId()) {
      return HSA_AMD_MEMORY_POOL_ACCESS_DISALLOWED_BY_DEFAULT;
    }

    // Return never allowed if memory is fine grained
    // link type is not xGMI i.e. link is PCIe
    return HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED;
  }

  // Return never allowed if above conditions are not satisified
  // This can happen when memory pool references neither system
  // or device local memory
  return HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED;
}

hsa_status_t MemoryRegion::GetAgentPoolInfo(
    const core::Agent& agent, hsa_amd_agent_memory_pool_info_t attribute,
    void* value) const {
  const uint32_t node_id_from = agent.node_id();
  const uint32_t node_id_to = owner()->node_id();

  const core::Runtime::LinkInfo link_info =
      core::Runtime::runtime_singleton_->GetLinkInfo(node_id_from, node_id_to);

  const hsa_amd_memory_pool_access_t access_type = GetAccessInfo(agent, link_info);

  switch (attribute) {
    case HSA_AMD_AGENT_MEMORY_POOL_INFO_ACCESS:
      *((hsa_amd_memory_pool_access_t*)value) = access_type;
      break;
    case HSA_AMD_AGENT_MEMORY_POOL_INFO_NUM_LINK_HOPS:
      *((uint32_t*)value) =
          (access_type != HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED)
              ? link_info.num_hop
              : 0;
      break;
    case HSA_AMD_AGENT_MEMORY_POOL_INFO_LINK_INFO:
      memset(value, 0, sizeof(hsa_amd_memory_pool_link_info_t));
      if ((access_type != HSA_AMD_MEMORY_POOL_ACCESS_NEVER_ALLOWED) &&
          (link_info.num_hop > 0)) {
        memcpy(value, &link_info.info, sizeof(hsa_amd_memory_pool_link_info_t));
      }
      break;
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t MemoryRegion::AllowAccess(uint32_t num_agents,
                                       const hsa_agent_t* agents,
                                       const void* ptr, size_t size) const {
  if (num_agents == 0 || agents == NULL || ptr == NULL || size == 0) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (!IsSystem() && !IsLocalMemory()) {
    return HSA_STATUS_ERROR;
  }

  // Adjust for fragments.  Make accessibility sticky for fragments since this will satisfy the
  // union of accessible agents between the fragments in the block.
  hsa_amd_pointer_info_t info;
  uint32_t agent_count = 0;
  hsa_agent_t* accessible = nullptr;
  MAKE_SCOPE_GUARD([&]() { free(accessible); });
  core::Runtime::PtrInfoBlockData blockInfo;
  std::vector<uint64_t> union_agents;
  info.size = sizeof(info);

  ScopedAcquire<KernelMutex> lock(&access_lock_);

  if (core::Runtime::runtime_singleton_->PtrInfo(const_cast<void*>(ptr), &info, malloc,
                                                 &agent_count, &accessible,
                                                 &blockInfo) == HSA_STATUS_SUCCESS) {
    /*  Thunk may return type = HSA_EXT_POINTER_TYPE_UNKNOWN for userptrs */
    if (info.type != HSA_EXT_POINTER_TYPE_UNKNOWN &&
        (blockInfo.length != size || info.sizeInBytes != size)) {
      for (int i = 0; i < num_agents; i++) union_agents.push_back(agents[i].handle);
      for (int i = 0; i < agent_count; i++) union_agents.push_back(accessible[i].handle);
      std::sort(union_agents.begin(), union_agents.end());
      const auto& last = std::unique(union_agents.begin(), union_agents.end());
      union_agents.erase(last, union_agents.end());

      agents = reinterpret_cast<hsa_agent_t*>(&union_agents[0]);
      num_agents = union_agents.size();
      size = blockInfo.length;
      ptr = blockInfo.base;
    }
  }

  bool cpu_in_list = false;

  std::set<GpuAgentInt*> whitelist_gpus;
  std::vector<uint32_t> whitelist_nodes;
  for (uint32_t i = 0; i < num_agents; ++i) {
    core::Agent* agent = core::Agent::Convert(agents[i]);
    if (agent == NULL || !agent->IsValid()) {
      return HSA_STATUS_ERROR_INVALID_AGENT;
    }

    if (agent->device_type() == core::Agent::kAmdGpuDevice) {
      whitelist_nodes.push_back(agent->node_id());
      whitelist_gpus.insert(reinterpret_cast<GpuAgentInt*>(agent));
    } else {
      cpu_in_list = true;
    }
  }

  if (whitelist_nodes.size() == 0 && IsSystem()) {
    assert(cpu_in_list);
    // This is a system region and only CPU agents in the whitelist.
    // Remove old mappings.
    AMD::MemoryRegion::MakeKfdMemoryUnresident(ptr);
    return HSA_STATUS_SUCCESS;
  }

  // If this is a local memory region, the owning gpu always needs to be in
  // the whitelist.
  if (IsLocalMemory() &&
      std::find(whitelist_nodes.begin(), whitelist_nodes.end(), owner()->node_id()) ==
          whitelist_nodes.end()) {
    whitelist_nodes.push_back(owner()->node_id());
    whitelist_gpus.insert(reinterpret_cast<GpuAgentInt*>(owner()));
  }

  HsaMemMapFlags map_flag = map_flag_;
  map_flag.ui32.HostAccess |= (cpu_in_list) ? 1 : 0;

  {  // Sequence with pointer info since queries to other fragments of the block may be adjusted by
     // this call.
    ScopedAcquire<KernelSharedMutex::Shared> lock(
        core::Runtime::runtime_singleton_->memory_lock_.shared());
    uint64_t alternate_va = 0;
    if (!AMD::MemoryRegion::MakeKfdMemoryResident(
      whitelist_nodes.size(), &whitelist_nodes[0], ptr,
      size, &alternate_va, map_flag)) {
        return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t MemoryRegion::CanMigrate(const MemoryRegion& dst,
                                      bool& result) const {
  // TODO: not implemented yet.
  result = false;
  return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
}

hsa_status_t MemoryRegion::Migrate(uint32_t flag, const void* ptr) const {
  // TODO: not implemented yet.
  return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
}

hsa_status_t MemoryRegion::Lock(uint32_t num_agents, const hsa_agent_t* agents,
                                void* host_ptr, size_t size,
                                void** agent_ptr) const {
  if (!IsSystem()) {
    return HSA_STATUS_ERROR;
  }

  if (full_profile()) {
    // For APU, any host pointer is always accessible by the gpu.
    *agent_ptr = host_ptr;
    return HSA_STATUS_SUCCESS;
  }

  std::set<core::Agent*> whitelist_gpus;
  std::vector<HSAuint32> whitelist_nodes;
  if (num_agents == 0 || agents == NULL) {
    // Map to all GPU agents.
    whitelist_nodes = core::Runtime::runtime_singleton_->gpu_ids();

    whitelist_gpus.insert(
        core::Runtime::runtime_singleton_->gpu_agents().begin(),
        core::Runtime::runtime_singleton_->gpu_agents().end());
  } else {
    for (uint32_t i = 0; i < num_agents; ++i) {
      core::Agent* agent = core::Agent::Convert(agents[i]);
      if (agent == NULL || !agent->IsValid()) {
        return HSA_STATUS_ERROR_INVALID_AGENT;
      }

      if (agent->device_type() == core::Agent::kAmdGpuDevice) {
        whitelist_nodes.push_back(agent->node_id());
        whitelist_gpus.insert(agent);
      }
    }
  }

  if (whitelist_nodes.size() == 0) {
    // No GPU agents in the whitelist. So no need to register and map since the
    // platform only has CPUs.
    *agent_ptr = host_ptr;
    return HSA_STATUS_SUCCESS;
  }

  // Call kernel driver to register and pin the memory.
  if (RegisterMemory(host_ptr, size, mem_flag_)) {
    uint64_t alternate_va = 0;
    if (MakeKfdMemoryResident(whitelist_nodes.size(), &whitelist_nodes[0],
                              host_ptr, size, &alternate_va, map_flag_)) {
      if (alternate_va != 0) {
        *agent_ptr = reinterpret_cast<void*>(alternate_va);
      } else {
        *agent_ptr = host_ptr;
      }

      return HSA_STATUS_SUCCESS;
    }
    AMD::MemoryRegion::DeregisterMemory(host_ptr);
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return HSA_STATUS_ERROR;
}

hsa_status_t MemoryRegion::Unlock(void* host_ptr) const {
  if (!IsSystem()) {
    return HSA_STATUS_ERROR;
  }

  if (full_profile()) {
    return HSA_STATUS_SUCCESS;
  }

  MakeKfdMemoryUnresident(host_ptr);
  DeregisterMemory(host_ptr);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t MemoryRegion::AssignAgent(void* ptr, size_t size,
                                       const core::Agent& agent,
                                       hsa_access_permission_t access) const {
  return HSA_STATUS_SUCCESS;
}

void MemoryRegion::Trim() const { fragment_allocator_.trim(); }

void* MemoryRegion::BlockAllocator::alloc(size_t request_size, size_t& allocated_size) const {
  void* ret;
  size_t bsize = AlignUp(request_size, block_size());

  hsa_status_t err = region_.AllocateImpl(
      bsize, core::MemoryRegion::AllocateRestrict | core::MemoryRegion::AllocateDirect, &ret);
  if (err != HSA_STATUS_SUCCESS)
    throw AMD::hsa_exception(err, "MemoryRegion::BlockAllocator::alloc failed.");
  assert(ret != nullptr && "Region returned nullptr on success.");

  allocated_size = bsize;
  return ret;
}

}  // namespace amd
}  // namespace rocr
