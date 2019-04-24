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

// HSA runtime C++ interface file.

#ifndef HSA_RUNTME_CORE_INC_MEMORY_REGION_H_
#define HSA_RUNTME_CORE_INC_MEMORY_REGION_H_

#include <vector>

#include "core/inc/agent.h"
#include "core/inc/checked.h"

namespace core {
class Agent;

class MemoryRegion : public Checked<0x9C961F19EE175BB3> {
 public:
  MemoryRegion(bool fine_grain, bool full_profile, core::Agent* owner)
      : fine_grain_(fine_grain), full_profile_(full_profile), owner_(owner) {
    assert(owner_ != NULL);
  }

  virtual ~MemoryRegion() {}

  // Convert this object into hsa_region_t.
  static __forceinline hsa_region_t Convert(MemoryRegion* region) {
    const hsa_region_t region_handle = {
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(region))};
    return region_handle;
  }

  static __forceinline const hsa_region_t Convert(const MemoryRegion* region) {
    const hsa_region_t region_handle = {
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(region))};
    return region_handle;
  }

  // Convert hsa_region_t into MemoryRegion *.
  static __forceinline MemoryRegion* Convert(hsa_region_t region) {
    return reinterpret_cast<MemoryRegion*>(region.handle);
  }

  enum AllocateEnum {
    AllocateNoFlags = 0,
    AllocateRestrict = (1 << 0),    // Don't map system memory to GPU agents
    AllocateExecutable = (1 << 1),  // Set executable permission
    AllocateDoubleMap = (1 << 2),   // Map twice VA allocation to backing store
    AllocateDirect = (1 << 3),      // Bypass fragment cache.
    AllocateIPC = (1 << 4),         // System memory that can be IPC-shared
  };

  typedef uint32_t AllocateFlags;

  virtual hsa_status_t Allocate(size_t& size, AllocateFlags alloc_flags, void** address) const = 0;

  virtual hsa_status_t Free(void* address, size_t size) const = 0;

  // Translate memory properties into HSA region attribute.
  virtual hsa_status_t GetInfo(hsa_region_info_t attribute,
                               void* value) const = 0;

  virtual hsa_status_t AssignAgent(void* ptr, size_t size, const Agent& agent,
                                   hsa_access_permission_t access) const = 0;

  __forceinline bool fine_grain() const { return fine_grain_; }

  __forceinline bool full_profile() const { return full_profile_; }

  __forceinline core::Agent* owner() const { return owner_; }

 private:
  const bool fine_grain_;
  const bool full_profile_;

  core::Agent* owner_;
};
}  // namespace core

#endif  // header guard
