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

// AMD specific HSA backend.

#ifndef HSA_RUNTIME_CORE_INC_AMD_MEMORY_REGION_H_
#define HSA_RUNTIME_CORE_INC_AMD_MEMORY_REGION_H_

#include "hsakmt.h"

#include "core/inc/agent.h"
#include "core/inc/memory_region.h"

namespace amd {
class MemoryRegion : public core::MemoryRegion {
 public:
  /// @brief Allocate agent accessible memory (system / local memory).
  static void* AllocateKfdMemory(const HsaMemFlags& flag, HSAuint32 node_id,
                                 size_t size);

  /// @brief Free agent accessible memory (system / local memory).
  static void FreeKfdMemory(void* ptr, size_t size);

  static bool RegisterHostMemory(void* ptr, size_t size, size_t num_nodes,
                                 uint32_t* nodes);

  static void DeregisterHostMemory(void* ptr);

  /// @brief Pin memory.
  static bool MakeKfdMemoryResident(void* ptr, size_t size,
                                    uint64_t* alternate_va);

  /// @brief Unpin memory.
  static void MakeKfdMemoryUnresident(void* ptr);

  MemoryRegion(bool fine_grain, bool full_profile, uint32_t node_id,
               const HsaMemoryProperties& mem_props);

  ~MemoryRegion();

  hsa_status_t Allocate(size_t size, void** address) const;

  hsa_status_t Free(void* address, size_t size) const;

  hsa_status_t GetInfo(hsa_region_info_t attribute, void* value) const;

  HSAuint64 GetBaseAddress() const { return mem_props_.VirtualBaseAddress; }

  HSAuint64 GetPhysicalSize() const { return mem_props_.SizeInBytes; }

  HSAuint64 GetVirtualSize() const { return virtual_size_; }

  hsa_status_t AssignAgent(void* ptr, size_t size, const core::Agent& agent,
                           hsa_access_permission_t access) const;

  __forceinline bool IsLocalMemory() const {
    return ((mem_props_.HeapType == HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE) ||
            (mem_props_.HeapType == HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC));
  }

  __forceinline bool IsSystem() const {
    return mem_props_.HeapType == HSA_HEAPTYPE_SYSTEM;
  }

  __forceinline bool IsLDS() const {
    return mem_props_.HeapType == HSA_HEAPTYPE_GPU_LDS;
  }

  __forceinline bool IsGDS() const {
    return mem_props_.HeapType == HSA_HEAPTYPE_GPU_GDS;
  }

  __forceinline bool IsScratch() const {
    return mem_props_.HeapType == HSA_HEAPTYPE_GPU_SCRATCH;
  }

  __forceinline bool IsSvm() const {
    // TODO(bwicakso): uncomment this when available.
    //return mem_props_.HeapType == HSA_HEAPTYPE_DEVICE_SVM;

    // TODO(bwicakso): remove this when SVM memory prop is available.
    return false;
  }

  __forceinline core::Agent* owner() const {
    // Return NULL if it is system memory region.
    // Return non NULL on lds, scratch, local memory region.
    return owner_;
  }

  __forceinline void owner(core::Agent* o) {
    assert(o != NULL);
    owner_ = o;
  }

 private:
  uint32_t node_id_;

  core::Agent* owner_;

  const HsaMemoryProperties mem_props_;

  HsaMemFlags mem_flag_;

  size_t max_single_alloc_size_;

  HSAuint64 virtual_size_;

  static const size_t kPageSize_ = 4096;
};
}  // namespace

#endif  // header guard
