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

#ifndef HSA_RUNTIME_CORE_INC_AIE_MEMORY_REGION_H_
#define HSA_RUNTIME_CORE_INC_AIE_MEMORY_REGION_H_

#include "core/inc/driver.h"
#include "core/inc/amd_memory_region.h"
#include "core/util/simple_heap.h"
#include "inc/hsa_ext_amd.h"

namespace rocr {
namespace AMD {

class AirMemoryRegion : public MemoryRegion {
 public:
  AirMemoryRegion(bool fine_grain, bool kernarg, bool full_profile,
                  core::MemProperties mprops, core::Agent* owner);

  ~AirMemoryRegion();

  static __forceinline hsa_region_t Convert(MemoryRegion* region) {
    const hsa_region_t region_handle = {
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(region))
    };
    return region_handle;
  }

  static __forceinline const hsa_region_t Convert(const MemoryRegion* region) {
    const hsa_region_t region_handle = {
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(region))
    };
    return region_handle;
  }

  static __forceinline MemoryRegion* Convert(hsa_region_t region) {
    return reinterpret_cast<MemoryRegion*>(region.handle);
  }

  hsa_status_t Allocate(size_t& size, AllocateFlags alloc_flags,
                        void** address) const override;
  hsa_status_t Free(void *address, size_t size) const override;
  hsa_status_t IPCFragmentExport(void* address) const override;
  hsa_status_t GetInfo(hsa_region_info_t attribute, void* value) const override;
  hsa_status_t AssignAgent(void* ptr, size_t size, const core::Agent& agent,
                           hsa_access_permission_t access) const override;

  hsa_status_t GetPoolInfo(hsa_amd_memory_pool_info_t attribute,
                           void* value) const override;
  hsa_status_t GetAgentPoolInfo(const core::Agent& agent,
                                hsa_amd_agent_memory_pool_info_t attribute,
                                void* value) const override;

  hsa_status_t AllowAccess(uint32_t num_agents, const hsa_agent_t* agents,
                           const void* ptr, size_t size) const override;
  hsa_status_t CanMigrate(const MemoryRegion& dst, bool& result) const override;
  hsa_status_t Migrate(uint32_t flag, const void* ptr) const override;
  hsa_status_t Lock(uint32_t num_agents, const hsa_agent_t* agents,
                    void* host_ptr, size_t size, void** agent_ptr) const override;
  hsa_status_t Unlock(void* host_ptr) const override;

  uint64_t GetBaseAddress() const override;
  uint64_t GetPhysicalSize() const override;
  uint64_t GetVirtualSize() const override;
  uint64_t GetCacheSize() const override { return 0; }

  __forceinline bool IsLocalMemory() const override;
  __forceinline bool IsPublic() const override { return false; }
  __forceinline bool IsSystem() const override { return false; }
  __forceinline bool IsLDS() const override { return false; }
  __forceinline bool IsGDS() const override { return false; }
  __forceinline bool IsScratch() const override { return false; }

  static constexpr size_t air_page_size_{4096};

 private:
  hsa_status_t AllocateAirMemory(size_t& size, void** address) const;
  hsa_status_t FreeAirMemory(void* address) const;

  class BlockAllocator {
   public:
    explicit BlockAllocator(AirMemoryRegion& region) : region_(region) {}
    void* alloc(size_t request_size, size_t& allocated_size) const;
    void free(void* ptr, size_t length) const { region_.Free(ptr, length); }
    size_t block_size() const { return block_size_; }

   private:
    AirMemoryRegion& region_;
    static const size_t block_size_ = 8 * 1024 * 1024;
  };

  mutable SimpleHeap<BlockAllocator> fragment_allocator_;
  core::MemProperties mprops_;
  mutable bool fragment_heap_allocated_{false};
};

} // namespace AMD
} // namespace rocr

#endif  // header guard
