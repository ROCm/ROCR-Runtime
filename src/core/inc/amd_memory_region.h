////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2023, Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef HSA_RUNTIME_CORE_INC_AMD_MEMORY_REGION_H_
#define HSA_RUNTIME_CORE_INC_AMD_MEMORY_REGION_H_

#include "core/inc/memory_region.h"
#include "inc/hsa_ext_amd.h"

namespace rocr {
namespace AMD {

class MemoryRegion : public core::MemoryRegion {
 public:
  MemoryRegion::MemoryRegion(bool fine_grain, bool kernarg, bool full_profile,
                             core::Agent* owner)
      : core::MemoryRegion(fine_grain, kernarg, full_profile, owner) {}

  virtual ~MemoryRegion() {}

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

  /// @brief  Convert hsa_region_t into AMD::MemoryRegion *.
  static __forceinline MemoryRegion* Convert(hsa_region_t region) {
    return reinterpret_cast<MemoryRegion*>(region.handle);
  }

  virtual hsa_status_t GetPoolInfo(hsa_amd_memory_pool_info_t attribute,
                           void* value) const = 0;
  virtual hsa_status_t GetAgentPoolInfo(const core::Agent& agent,
                                        hsa_amd_agent_memory_pool_info_t attribute,
                                        void* value) const = 0;

  virtual hsa_status_t AllowAccess(uint32_t num_agents, const hsa_agent_t* agents,
                                   const void* ptr, size_t size) const = 0;
  virtual hsa_status_t CanMigrate(const MemoryRegion& dst, bool& result) const = 0;
  virtual hsa_status_t Migrate(uint32_t flag, const void* ptr) const = 0;
  virtual hsa_status_t Lock(uint32_t num_agents, const hsa_agent_t* agents,
                            void* host_ptr, size_t size, void** agent_ptr) const = 0;
  virtual hsa_status_t Unlock(void* host_ptr) const = 0;

  virtual uint64_t GetBaseAddress() const = 0;
  virtual uint64_t GetPhysicalSize() const = 0;
  virtual uint64_t GetVirtualSize() const = 0;
  virtual uint64_t GetCacheSize() const = 0;

  virtual bool IsLocalMemory() const = 0;
  virtual bool IsPublic() const = 0;
  virtual bool IsSystem() const = 0;
  virtual bool IsLDS() const = 0;
  virtual bool IsGDS() const = 0;
  virtual bool IsScratch() const = 0;
};

} // namespace AMD
} // namespace rocr

#endif // header guard
