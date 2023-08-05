////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2023, Advanced Micro Devices, Inc. All rights reserved.
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

#include "core/inc/amd_air_memory_region.h"

#include "core/inc/runtime.h"

namespace rocr {
namespace AMD {

AirMemoryRegion::AirMemoryRegion(bool fine_grain, bool kernarg,
                                 bool full_profile, core::MemProperties mprops,
                                 core::Agent* owner)
    : MemoryRegion(fine_grain, kernarg, full_profile, owner),
      fragment_allocator_(BlockAllocator(*this)), mprops_(mprops) {
  if (!(mprops.flags_ & MemFlagsHeapTypeDRAM)) {
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT,
                             "AirMemoryRegion: only supports DRAM heap type.");
  }

  void *mem(nullptr);
  auto status = AllocateAirMemory(mprops_.size_bytes_, &mem);

  if (status != HSA_STATUS_SUCCESS) {
    throw AMD::hsa_exception(status,
                             "AirMemoryRegion: unable to allocate memory.");
  }

  mprops_.virtual_base_addr_ = reinterpret_cast<uintptr_t>(mem);
}

AirMemoryRegion::~AirMemoryRegion() {
  FreeAirMemory(reinterpret_cast<void*>(mprops_.virtual_base_addr_));
}

hsa_status_t AirMemoryRegion::Allocate(size_t& size, AllocateFlags alloc_flags,
                                       void** address) const {
  if (IsLocalMemory() && !(alloc_flags & core::MemoryRegion::AllocateDirect)) {
    *address = fragment_allocator_.alloc(size);
    return HSA_STATUS_SUCCESS;
  } else if (IsLocalMemory() && !fragment_heap_allocated_) {
    *address = reinterpret_cast<void*>(mprops_.virtual_base_addr_);
    fragment_heap_allocated_ = true;
  } else {
    throw AMD::hsa_exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES,
                             "AirMemoryRegion: out of memory.");
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::Free(void *address, size_t size) const {
  fragment_allocator_.free(address);
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::IPCFragmentExport(void* address) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::GetInfo(hsa_region_info_t attribute,
                                      void* value) const  {
  switch (attribute) {
    case HSA_REGION_INFO_SEGMENT:
      if (IsLocalMemory())
       *static_cast<hsa_region_segment_t*>(value) = HSA_REGION_SEGMENT_GLOBAL;
      break;
    default:
      break;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::AssignAgent(void* ptr, size_t size,
                                          const core::Agent& agent,
                                          hsa_access_permission_t access) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::GetPoolInfo(hsa_amd_memory_pool_info_t attribute,
                         void* value) const {
  switch (attribute) {
    case HSA_AMD_MEMORY_POOL_INFO_SEGMENT:
    case HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS:
    case HSA_AMD_MEMORY_POOL_INFO_SIZE:
    case HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALLOWED:
    case HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_GRANULE:
    case HSA_AMD_MEMORY_POOL_INFO_RUNTIME_ALLOC_ALIGNMENT:
      return GetInfo(static_cast<hsa_region_info_t>(attribute), value);
    default:
      break;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::GetAgentPoolInfo(const core::Agent& agent,
                                               hsa_amd_agent_memory_pool_info_t attribute,
                                               void* value) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::AllowAccess(uint32_t num_agents, const hsa_agent_t* agents,
                           const void* ptr, size_t size) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::CanMigrate(const MemoryRegion& dst, bool& result) const {
  result = false;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::Migrate(uint32_t flag, const void* ptr) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::Lock(uint32_t num_agents, const hsa_agent_t* agents,
                    void* host_ptr, size_t size, void** agent_ptr) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AirMemoryRegion::Unlock(void* host_ptr) const {
  return HSA_STATUS_SUCCESS;
}

bool AirMemoryRegion::IsLocalMemory() const {
  return (mprops_.flags_ & MemFlagsHeapTypeDRAM);
}

uint64_t AirMemoryRegion::GetBaseAddress() const {
  return mprops_.virtual_base_addr_;
}

uint64_t AirMemoryRegion::GetPhysicalSize() const {
  return mprops_.size_bytes_;
}

uint64_t AirMemoryRegion::GetVirtualSize() const {
  return mprops_.size_bytes_;
}

hsa_status_t AirMemoryRegion::AllocateAirMemory(size_t& size, void** address) const {
  return core::Runtime::runtime_singleton_->AgentDriver(core::Agent::kAmdAieDevice)
      ->AllocateMemory(address, size, owner()->node_id(), mprops_.flags_);
}

hsa_status_t AirMemoryRegion::FreeAirMemory(void* address) const {
  return core::Runtime::runtime_singleton_->AgentDriver(core::Agent::kAmdAieDevice)
      ->FreeMemory(address, owner()->node_id());
}

void* AirMemoryRegion::BlockAllocator::alloc(size_t request_size, size_t& allocated_size) const {
  void* ptr(nullptr);
  size_t actual_size(AlignUp(request_size, block_size()));
  hsa_status_t status(region_.Allocate(actual_size,
                                       core::MemoryRegion::AllocateDirect,
                                       &ptr));

  if (status != HSA_STATUS_SUCCESS || !ptr) {
    throw AMD::hsa_exception(status,
                             "AirMemoryRegion: BlockAllocator alloc failed.");
  }

  allocated_size = actual_size;

  return ptr;
}

} // namespace AMD
} // namespace rocr
