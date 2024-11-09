////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2024, Advanced Micro Devices, Inc. All rights reserved.
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

#include "core/inc/amd_kfd_driver.h"

#include <sys/ioctl.h>

#include <memory>
#include <string>

#include "hsakmt/hsakmt.h"

#include "core/inc/amd_cpu_agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/exceptions.h"
#include "core/inc/runtime.h"

namespace rocr {
namespace AMD {

KfdDriver::KfdDriver(std::string devnode_name)
    : core::Driver(core::DriverType::KFD, devnode_name) {}

hsa_status_t KfdDriver::Init() { return HSA_STATUS_SUCCESS; }

hsa_status_t KfdDriver::DiscoverDriver() {
  std::unique_ptr<Driver> kfd_drv(new KfdDriver("/dev/kfd"));

  if (kfd_drv->Open() == HSA_STATUS_SUCCESS) {
    core::Runtime::runtime_singleton_->RegisterDriver(kfd_drv);
    return HSA_STATUS_SUCCESS;
  }
  return HSA_STATUS_ERROR;
}

hsa_status_t KfdDriver::QueryKernelModeDriver(core::DriverQuery query) {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t KfdDriver::Open() {
  return hsaKmtOpenKFD() == HSAKMT_STATUS_SUCCESS ? HSA_STATUS_SUCCESS
                                                  : HSA_STATUS_ERROR;
}

hsa_status_t KfdDriver::Close() {
  return hsaKmtCloseKFD() == HSAKMT_STATUS_SUCCESS ? HSA_STATUS_SUCCESS
                                                   : HSA_STATUS_ERROR;
}

hsa_status_t KfdDriver::GetAgentProperties(core::Agent &agent) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t
KfdDriver::GetMemoryProperties(uint32_t node_id,
                               core::MemoryRegion &mem_region) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t
KfdDriver::AllocateMemory(const core::MemoryRegion &mem_region,
                          core::MemoryRegion::AllocateFlags alloc_flags,
                          void **mem, size_t size, uint32_t agent_node_id) {
  const MemoryRegion &m_region(static_cast<const MemoryRegion &>(mem_region));
  HsaMemFlags kmt_alloc_flags(m_region.mem_flags());

  kmt_alloc_flags.ui32.ExecuteAccess =
      (alloc_flags & core::MemoryRegion::AllocateExecutable ? 1 : 0);
  kmt_alloc_flags.ui32.AQLQueueMemory =
      (alloc_flags & core::MemoryRegion::AllocateDoubleMap ? 1 : 0);

  if (m_region.IsSystem() &&
      (alloc_flags & core::MemoryRegion::AllocateNonPaged)) {
    kmt_alloc_flags.ui32.NonPaged = 1;
  }

  if (!m_region.IsLocalMemory() &&
      (alloc_flags & core::MemoryRegion::AllocateMemoryOnly)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  // Allocating a memory handle for virtual memory
  kmt_alloc_flags.ui32.NoAddress =
      !!(alloc_flags & core::MemoryRegion::AllocateMemoryOnly);

  // Allocate pseudo fine grain memory
  kmt_alloc_flags.ui32.CoarseGrain =
      (alloc_flags & core::MemoryRegion::AllocatePCIeRW
           ? 0
           : kmt_alloc_flags.ui32.CoarseGrain);

  kmt_alloc_flags.ui32.NoSubstitute =
      (alloc_flags & core::MemoryRegion::AllocatePinned
           ? 1
           : kmt_alloc_flags.ui32.NoSubstitute);

  kmt_alloc_flags.ui32.GTTAccess =
      (alloc_flags & core::MemoryRegion::AllocateGTTAccess
           ? 1
           : kmt_alloc_flags.ui32.GTTAccess);

  kmt_alloc_flags.ui32.Uncached =
      (alloc_flags & core::MemoryRegion::AllocateUncached
            ? 1
            : kmt_alloc_flags.ui32.Uncached);

  if (m_region.IsLocalMemory()) {
    // Allocate physically contiguous memory. AllocateKfdMemory function call
    // will fail if this flag is not supported in KFD.
    kmt_alloc_flags.ui32.Contiguous =
        (alloc_flags & core::MemoryRegion::AllocateContiguous
             ? 1
             : kmt_alloc_flags.ui32.Contiguous);
  }

  //// Only allow using the suballocator for ordinary VRAM.
  if (m_region.IsLocalMemory() && !kmt_alloc_flags.ui32.NoAddress) {
    bool subAllocEnabled =
        !core::Runtime::runtime_singleton_->flag().disable_fragment_alloc();
    // Avoid modifying executable or queue allocations.
    bool useSubAlloc = subAllocEnabled;
    useSubAlloc &=
        ((alloc_flags & (~core::MemoryRegion::AllocateRestrict)) == 0);

    if (useSubAlloc) {
      *mem = m_region.fragment_alloc(size);

      if ((alloc_flags & core::MemoryRegion::AllocateAsan) &&
          hsaKmtReplaceAsanHeaderPage(*mem) != HSAKMT_STATUS_SUCCESS) {
        m_region.fragment_free(*mem);
        *mem = nullptr;
        return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
      }

      return HSA_STATUS_SUCCESS;
    }
  }

  const uint32_t node_id =
      (alloc_flags & core::MemoryRegion::AllocateGTTAccess)
          ? agent_node_id
          : m_region.owner()->node_id();

  //// Allocate memory.
  //// If it fails attempt to release memory from the block allocator and retry.
  *mem = AllocateKfdMemory(kmt_alloc_flags, node_id, size);
  if (*mem == nullptr) {
    m_region.owner()->Trim();
    *mem = AllocateKfdMemory(kmt_alloc_flags, node_id, size);
  }

  if (*mem != nullptr) {
    if (kmt_alloc_flags.ui32.NoAddress)
      return HSA_STATUS_SUCCESS;

    // Commit the memory.
    // For system memory, on non-restricted allocation, map it to all GPUs. On
    // restricted allocation, only CPU is allowed to access by default, so
    // no need to map
    // For local memory, only map it to the owning GPU. Mapping to other GPU,
    // if the access is allowed, is performed on AllowAccess.
    HsaMemMapFlags map_flag = m_region.map_flags();
    size_t map_node_count = 1;
    const uint32_t owner_node_id = m_region.owner()->node_id();
    const uint32_t *map_node_id = &owner_node_id;

    if (m_region.IsSystem()) {
      if ((alloc_flags & core::MemoryRegion::AllocateRestrict) == 0) {
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
        map_node_count, map_node_id, *mem, size, &alternate_va, map_flag);

    const bool require_pinning =
        (!m_region.full_profile() || m_region.IsLocalMemory() ||
         m_region.IsScratch());

    if (require_pinning && !is_resident) {
      FreeKfdMemory(*mem, size);
      *mem = nullptr;
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }

    if ((alloc_flags & core::MemoryRegion::AllocateAsan) &&
        hsaKmtReplaceAsanHeaderPage(*mem) != HSAKMT_STATUS_SUCCESS) {
      FreeKfdMemory(*mem, size);
      *mem = nullptr;
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
    return HSA_STATUS_SUCCESS;
  }

  return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
}

hsa_status_t KfdDriver::FreeMemory(void *mem, size_t size) {
  MakeKfdMemoryUnresident(mem);
  return FreeKfdMemory(mem, size) ? HSA_STATUS_SUCCESS : HSA_STATUS_ERROR;
}

hsa_status_t KfdDriver::CreateQueue(core::Queue &queue) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t KfdDriver::DestroyQueue(core::Queue &queue) const {
  return HSA_STATUS_SUCCESS;
}

void *KfdDriver::AllocateKfdMemory(const HsaMemFlags &flags, uint32_t node_id,
                                   size_t size) {
  void *mem = nullptr;
  const HSAKMT_STATUS status = hsaKmtAllocMemory(node_id, size, flags, &mem);
  return (status == HSAKMT_STATUS_SUCCESS) ? mem : nullptr;
}

bool KfdDriver::FreeKfdMemory(void *mem, size_t size) {
  if (mem == nullptr || size == 0) {
    debug_print("Invalid free ptr:%p size:%lu\n", mem, size);
    return false;
  }

  if (hsaKmtFreeMemory(mem, size) != HSAKMT_STATUS_SUCCESS) {
    debug_print("Failed to free ptr:%p size:%lu\n", mem, size);
    return false;
  }
  return true;
}

bool KfdDriver::MakeKfdMemoryResident(size_t num_node, const uint32_t *nodes,
                                      const void *mem, size_t size,
                                      uint64_t *alternate_va,
                                      HsaMemMapFlags map_flag) {
  assert(num_node > 0);
  assert(nodes);

  *alternate_va = 0;

  HSAKMT_STATUS kmt_status(hsaKmtMapMemoryToGPUNodes(
      const_cast<void *>(mem), size, alternate_va, map_flag, num_node,
      const_cast<uint32_t *>(nodes)));

  return (kmt_status == HSAKMT_STATUS_SUCCESS);
}

void KfdDriver::MakeKfdMemoryUnresident(const void *mem) {
  hsaKmtUnmapMemoryToGPU(const_cast<void *>(mem));
}

} // namespace AMD
} // namespace rocr
