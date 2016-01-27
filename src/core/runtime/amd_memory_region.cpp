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

#include "core/inc/amd_memory_region.h"

#include <algorithm>

#include "core/inc/runtime.h"
#include "core/inc/memory_database.h"
#include "core/inc/amd_cpu_agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/util/utils.h"

namespace amd {
void* MemoryRegion::AllocateKfdMemory(const HsaMemFlags& flag,
                                      HSAuint32 node_id, size_t size) {
  void* ret = NULL;
  const HSAKMT_STATUS status = hsaKmtAllocMemory(node_id, size, flag, &ret);
  if (status != HSAKMT_STATUS_SUCCESS) return NULL;

  core::Runtime::runtime_singleton_->Register(ret, size, false);

  return ret;
}

void MemoryRegion::FreeKfdMemory(void* ptr, size_t size) {
  if (ptr == NULL || size == 0) {
    return;
  }

  // Completely deregister ptr (could be two references on the registration due
  // to an explicit registration call)
  while (core::Runtime::runtime_singleton_->Deregister(ptr))
    ;

  HSAKMT_STATUS status = hsaKmtFreeMemory(ptr, size);
  assert(status == HSAKMT_STATUS_SUCCESS);
}

bool MemoryRegion::RegisterHostMemory(void* ptr, size_t size, size_t num_nodes,
                                      uint32_t* nodes) {
  assert(ptr != NULL);
  assert(size != 0);

  HSAKMT_STATUS status = HSAKMT_STATUS_ERROR;
  if (num_nodes == 0 || nodes == NULL) {
    status = hsaKmtRegisterMemory(ptr, size);
  }

  // TODO(bwicakso): uncomment call to new registration api with specific agents
  // when available.
  //status = hsaKmtRegisterMemoryToNodes(ptr, size, num_nodes, nodes);

  return status == HSAKMT_STATUS_SUCCESS;
}

void MemoryRegion::DeregisterHostMemory(void* ptr) {
  if (ptr != NULL) {
    HSAKMT_STATUS status = hsaKmtDeregisterMemory(ptr);
    assert(status == HSAKMT_STATUS_SUCCESS);
  }
}

bool MemoryRegion::MakeKfdMemoryResident(void* ptr, size_t size,
                                         uint64_t* alternate_va) {
  *alternate_va = 0;
  HSAKMT_STATUS status = hsaKmtMapMemoryToGPU(ptr, size, alternate_va);
  return (status == HSAKMT_STATUS_SUCCESS);
}

void MemoryRegion::MakeKfdMemoryUnresident(void* ptr) {
  hsaKmtUnmapMemoryToGPU(ptr);
}

MemoryRegion::MemoryRegion(bool fine_grain, bool full_profile, uint32_t node_id,
                           const HsaMemoryProperties& mem_props)
    : core::MemoryRegion(fine_grain, full_profile),
      node_id_(node_id),
      mem_props_(mem_props),
      max_single_alloc_size_(0),
      virtual_size_(0) {
  virtual_size_ = GetPhysicalSize();

  mem_flag_.Value = 0;

  static const HSAuint64 kGpuVmSize = (1ULL << 40);
  static const HSAuint64 kUsedGpuVmSize = 256ULL * 1024 * 1024;

  if (IsLocalMemory()) {
    mem_flag_.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    mem_flag_.ui32.NoSubstitute = 1;
    mem_flag_.ui32.HostAccess = 0;
    mem_flag_.ui32.NonPaged = 1;

    assert(GetPhysicalSize() > kUsedGpuVmSize);

    max_single_alloc_size_ = AlignDown(
        static_cast<size_t>(GetPhysicalSize() - kUsedGpuVmSize), kPageSize_);

    virtual_size_ = kGpuVmSize;
  } else if (IsSystem()) {
    mem_flag_.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    mem_flag_.ui32.NoSubstitute = 1;
    mem_flag_.ui32.HostAccess = 1;
    mem_flag_.ui32.CachePolicy = HSA_CACHING_CACHED;

    if (full_profile) {
      max_single_alloc_size_ =
          AlignDown(static_cast<size_t>(GetPhysicalSize()), kPageSize_);

      virtual_size_ = os::GetUserModeVirtualMemorySize();
    } else {
      max_single_alloc_size_ = AlignDown(
          static_cast<size_t>(GetPhysicalSize() - kUsedGpuVmSize), kPageSize_);

      virtual_size_ = kGpuVmSize;
    }
  }

  mem_flag_.ui32.CoarseGrain = (fine_grain) ? 0 : 1;

  assert(GetVirtualSize() != 0);
  assert(GetPhysicalSize() <= GetVirtualSize());
  assert(IsMultipleOf(max_single_alloc_size_, kPageSize_));
}

MemoryRegion::~MemoryRegion() {}

hsa_status_t MemoryRegion::Allocate(size_t size, void** address) const {
  if (address == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  *address = AllocateKfdMemory(mem_flag_, node_id_, size);

  if (*address != NULL) {
    uint64_t alternate_va = 0;
    if (full_profile()) {
      // Not mandatory on APU, so we can ignore the result.
      MakeKfdMemoryResident(*address, size, &alternate_va);
    } else {
      // TODO: remove immediate pinning when HSA API to
      // explicitly unpin memory is available.
      if (!MakeKfdMemoryResident(*address, size, &alternate_va)) {
        FreeKfdMemory(*address, size);
        *address = NULL;
        return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
      }
    }

    return HSA_STATUS_SUCCESS;
  }

  return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
}

hsa_status_t MemoryRegion::Free(void* address, size_t size) const {
  MakeKfdMemoryUnresident(address);

  FreeKfdMemory(address, size);

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
          *((uint32_t*)value) = fine_grain()
                                    ? (HSA_REGION_GLOBAL_FLAG_KERNARG |
                                       HSA_REGION_GLOBAL_FLAG_FINE_GRAINED)
                                    : HSA_REGION_GLOBAL_FLAG_COARSE_GRAINED;
          break;
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
          *((uint32_t*)value) = HSA_REGION_GLOBAL_FLAG_COARSE_GRAINED;
          break;
        default:
          *((uint32_t*)value) = 0;
          break;
      }
      break;
    case HSA_REGION_INFO_SIZE:
      switch (mem_props_.HeapType) {
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
          // TODO: report the actual physical size of local memory until API to
          // explicitly unpin memory is available.
          *((size_t*)value) = static_cast<size_t>(GetPhysicalSize());
          break;
        default:
          *((size_t*)value) = static_cast<size_t>(GetVirtualSize());
          break;
      }
      break;
    case HSA_REGION_INFO_ALLOC_MAX_SIZE:
      switch (mem_props_.HeapType) {
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
        case HSA_HEAPTYPE_SYSTEM:
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
      // TODO: remove the hardcoded value.
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
      // TODO: remove the hardcoded value.
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
        default:
          return HSA_STATUS_ERROR_INVALID_ARGUMENT;
          break;
      }
      break;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t MemoryRegion::AssignAgent(void* ptr, size_t size,
                                       const core::Agent& agent,
                                       hsa_access_permission_t access) const {
  if (fine_grain()) {
    return HSA_STATUS_SUCCESS;
  }

  if (std::find(agent.regions().begin(), agent.regions().end(), this) ==
      agent.regions().end()) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  HSAuint64 u_ptr = reinterpret_cast<HSAuint64>(ptr);
  if (u_ptr >= GetBaseAddress() &&
      u_ptr < (GetBaseAddress() + GetVirtualSize())) {
    // TODO: only support agent allocation buffer.

    // TODO: commented until API to explicitly unpin memory is available.
    // if (!MakeKfdMemoryResident(ptr, size)) {
    //  return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    //}

    return HSA_STATUS_SUCCESS;
  } else {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return HSA_STATUS_SUCCESS;
}

}  // namespace
