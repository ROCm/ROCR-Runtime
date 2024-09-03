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

#include "core/inc/amd_xdna_driver.h"

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <memory>
#include <string>

#include "core/inc/amd_aie_aql_queue.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/runtime.h"
#include "core/util/utils.h"
#include "uapi/amdxdna_accel.h"

namespace rocr {
namespace AMD {

XdnaDriver::XdnaDriver(std::string devnode_name)
    : core::Driver(core::DriverType::XDNA, devnode_name) {}

XdnaDriver::~XdnaDriver() { FreeDeviceHeap(); }

hsa_status_t XdnaDriver::DiscoverDriver() {
  const int max_minor_num(64);
  const std::string devnode_prefix("/dev/accel/accel");

  for (int i = 0; i < max_minor_num; ++i) {
    std::unique_ptr<Driver> xdna_drv(
        new XdnaDriver(devnode_prefix + std::to_string(i)));
    if (xdna_drv->Open() == HSA_STATUS_SUCCESS) {
      if (xdna_drv->QueryKernelModeDriver(
              core::DriverQuery::GET_DRIVER_VERSION) == HSA_STATUS_SUCCESS) {
        static_cast<XdnaDriver *>(xdna_drv.get())->Init();
        core::Runtime::runtime_singleton_->RegisterDriver(xdna_drv);
        return HSA_STATUS_SUCCESS;
      } else {
        xdna_drv->Close();
      }
    }
  }

  return HSA_STATUS_ERROR;
}

uint64_t XdnaDriver::GetDevHeapByteSize() {
  return dev_heap_size;
}

hsa_status_t XdnaDriver::Init() { return InitDeviceHeap(); }

hsa_status_t XdnaDriver::QueryKernelModeDriver(core::DriverQuery query) {
  switch (query) {
  case core::DriverQuery::GET_DRIVER_VERSION:
    return QueryDriverVersion();
  default:
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::GetAgentProperties(core::Agent &agent) const {
  if (agent.device_type() != core::Agent::DeviceType::kAmdAieDevice) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  auto &aie_agent(static_cast<AieAgent &>(agent));

  amdxdna_drm_query_aie_metadata aie_metadata{0};
  amdxdna_drm_get_info get_info_args{
      .param = DRM_AMDXDNA_QUERY_AIE_METADATA,
      .buffer_size = sizeof(aie_metadata),
      .buffer = reinterpret_cast<uintptr_t>(&aie_metadata)};

  if (ioctl(fd_, DRM_IOCTL_AMDXDNA_GET_INFO, &get_info_args) < 0) {
    return HSA_STATUS_ERROR;
  }

  // Right now can only target N-1 columns
  aie_agent.SetNumCols(aie_metadata.cols - 1);
  aie_agent.SetNumCoreRows(aie_metadata.core.row_count);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t
XdnaDriver::GetMemoryProperties(uint32_t node_id,
                                core::MemoryRegion &mem_region) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t
XdnaDriver::AllocateMemory(const core::MemoryRegion &mem_region,
                           core::MemoryRegion::AllocateFlags alloc_flags,
                           void **mem, size_t size, uint32_t node_id) {
  const MemoryRegion &m_region(static_cast<const MemoryRegion &>(mem_region));

  amdxdna_drm_create_bo create_bo_args{0};
  create_bo_args.size = size;

  amdxdna_drm_get_bo_info get_bo_info_args{0};
  drm_gem_close close_bo_args{0};
  void *mapped_mem(nullptr);

  if (!m_region.IsSystem()) {
    return HSA_STATUS_ERROR_INVALID_REGION;
  }

  if (m_region.kernarg()) {
    create_bo_args.type = AMDXDNA_BO_CMD;
  } else {
    create_bo_args.type = AMDXDNA_BO_DEV;
  }

  if (ioctl(fd_, DRM_IOCTL_AMDXDNA_CREATE_BO, &create_bo_args) < 0) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  get_bo_info_args.handle = create_bo_args.handle;
  // In case we need to close this BO to avoid leaks due to some error after
  // creation.
  close_bo_args.handle = create_bo_args.handle;

  if (ioctl(fd_, DRM_IOCTL_AMDXDNA_GET_BO_INFO, &get_bo_info_args) < 0) {
    // Close the BO in the case we can't get info about it.
    ioctl(fd_, DRM_IOCTL_GEM_CLOSE, &close_bo_args);
    return HSA_STATUS_ERROR;
  }

  /// TODO: For now we always map the memory and keep a mapping from handles
  /// to VA memory addresses. Once we can support the separate VMEM call to
  /// map handles we can fix this.
  if (m_region.kernarg()) {
    mapped_mem = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_,
                      get_bo_info_args.map_offset);
    if (mapped_mem == MAP_FAILED) {
      // Close the BO in the case when a mapping fails and we got a BO handle.
      ioctl(fd_, DRM_IOCTL_GEM_CLOSE, &close_bo_args);
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  } else {
    mapped_mem = reinterpret_cast<void *>(get_bo_info_args.vaddr);
  }

  if (alloc_flags & core::MemoryRegion::AllocateMemoryOnly) {
    *mem = reinterpret_cast<void *>(create_bo_args.handle);
  } else {
    *mem = mapped_mem;
  }

  vmem_handle_mappings.emplace(create_bo_args.handle, mapped_mem);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::FreeMemory(void *mem, size_t size) {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::CreateQueue(core::Queue &queue) const {
  if (!AieAqlQueue::IsType(&queue)) {
    return HSA_STATUS_ERROR_INVALID_QUEUE;
  }

  auto &aie_queue(static_cast<AieAqlQueue &>(queue));
  auto &aie_agent(aie_queue.GetAgent());

  // Currently we do not leverage QoS information.
  amdxdna_qos_info qos_info{0};
  amdxdna_drm_create_hwctx create_hwctx_args{
      .ext = 0,
      .ext_flags = 0,
      .qos_p = reinterpret_cast<uintptr_t>(&qos_info),
      .umq_bo = 0,
      .log_buf_bo = 0,
      // TODO: Make this configurable.
      .max_opc = 0x800,
      // This field is for the number of core tiles.
      .num_tiles = aie_agent.GetNumCores(),
      .mem_size = 0,
      .umq_doorbell = 0};

  if (ioctl(fd_, DRM_IOCTL_AMDXDNA_CREATE_HWCTX, &create_hwctx_args) < 0) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  aie_queue.SetHwCtxHandle(create_hwctx_args.handle);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::DestroyQueue(core::Queue &queue) const {
  if (!AieAqlQueue::IsType(&queue)) {
    return HSA_STATUS_ERROR_INVALID_QUEUE;
  }

  auto &aie_queue(static_cast<AieAqlQueue &>(queue));
  amdxdna_drm_destroy_hwctx destroy_hwctx_args{.handle =
                                                   aie_queue.GetHwCtxHandle()};

  if (ioctl(fd_, DRM_IOCTL_AMDXDNA_DESTROY_HWCTX, &destroy_hwctx_args) < 0) {
    return HSA_STATUS_ERROR;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::QueryDriverVersion() {
  amdxdna_drm_query_aie_version aie_version{0, 0};
  amdxdna_drm_get_info args{DRM_AMDXDNA_QUERY_AIE_VERSION, sizeof(aie_version),
                            reinterpret_cast<uintptr_t>(&aie_version)};

  if (ioctl(fd_, DRM_IOCTL_AMDXDNA_GET_INFO, &args) < 0) {
    return HSA_STATUS_ERROR;
  }

  version_.major = aie_version.major;
  version_.minor = aie_version.minor;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::InitDeviceHeap() {
  amdxdna_drm_create_bo create_bo_args{
    .flags = 0,
    .type = AMDXDNA_BO_DEV_HEAP,
    ._pad = 0,
    .vaddr = reinterpret_cast<uintptr_t>(nullptr),
    .size = dev_heap_size,
    .handle = 0};

  amdxdna_drm_get_bo_info get_bo_info_args{0};
  drm_gem_close close_bo_args{0};

  if (ioctl(fd_, DRM_IOCTL_AMDXDNA_CREATE_BO, &create_bo_args) < 0) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  get_bo_info_args.handle = create_bo_args.handle;
  // In case we need to close this BO to avoid leaks due to some error after
  // creation.
  close_bo_args.handle = create_bo_args.handle;

  if (ioctl(fd_, DRM_IOCTL_AMDXDNA_GET_BO_INFO, &get_bo_info_args) < 0) {
    // Close the BO in the case we can't get info about it.
    ioctl(fd_, DRM_IOCTL_GEM_CLOSE, &close_bo_args);
    return HSA_STATUS_ERROR;
  }

  dev_heap_parent = mmap(0, dev_heap_align * 2 - 1, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (dev_heap_parent == MAP_FAILED) {
    // Close the BO in the case when a mapping fails and we got a BO handle.
    ioctl(fd_, DRM_IOCTL_GEM_CLOSE, &close_bo_args);
    dev_heap_parent = nullptr;
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  void *addr_aligned(reinterpret_cast<void *>(
      AlignUp(reinterpret_cast<uintptr_t>(dev_heap_parent), dev_heap_align)));

  dev_heap_aligned =
      mmap(addr_aligned, dev_heap_size, PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_FIXED, fd_, get_bo_info_args.map_offset);

  if (dev_heap_aligned == MAP_FAILED) {
    // Close the BO in the case when a mapping fails and we got a BO handle.
    ioctl(fd_, DRM_IOCTL_GEM_CLOSE, &close_bo_args);
    // Unmap the dev_heap_parent.
    dev_heap_aligned = nullptr;
    FreeDeviceHeap();
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::GetHandleMappings(std::unordered_map<uint32_t, void*> &vmem_handle_mappings) {
  vmem_handle_mappings = this->vmem_handle_mappings;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::GetFd(int &fd) {
  fd = fd_;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::FreeDeviceHeap() {
  if (dev_heap_parent) {
    munmap(dev_heap_parent, dev_heap_align * 2 - 1);
    dev_heap_parent = nullptr;
  }

  if (dev_heap_aligned) {
    munmap(dev_heap_aligned, dev_heap_size);
    dev_heap_aligned = nullptr;
  }

  return HSA_STATUS_SUCCESS;
}

} // namespace AMD
} // namespace rocr
