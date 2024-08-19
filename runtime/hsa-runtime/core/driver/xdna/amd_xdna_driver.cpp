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

#include <memory>
#include <string>

#include "core/inc/amd_memory_region.h"
#include "core/inc/runtime.h"
#include "uapi/amdxdna_accel.h"

namespace rocr {
namespace AMD {

XdnaDriver::XdnaDriver(std::string devnode_name)
    : core::Driver(core::DriverType::XDNA, devnode_name) {}

hsa_status_t XdnaDriver::DiscoverDriver() {
  const int max_minor_num(64);
  const std::string devnode_prefix("/dev/accel/accel");

  for (int i = 0; i < max_minor_num; ++i) {
    std::unique_ptr<Driver> xdna_drv(
        new XdnaDriver(devnode_prefix + std::to_string(i)));
    if (xdna_drv->Open() == HSA_STATUS_SUCCESS) {
      if (xdna_drv->QueryKernelModeDriver(
              core::DriverQuery::GET_DRIVER_VERSION) == HSA_STATUS_SUCCESS) {
        core::Runtime::runtime_singleton_->RegisterDriver(xdna_drv);
        return HSA_STATUS_SUCCESS;
      } else {
        xdna_drv->Close();
      }
    }
  }

  return HSA_STATUS_ERROR;
}

hsa_status_t XdnaDriver::QueryKernelModeDriver(core::DriverQuery query) {
  switch (query) {
  case core::DriverQuery::GET_DRIVER_VERSION:
    return QueryDriverVersion();
  default:
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
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
  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::FreeMemory(void *mem, size_t size) {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::CreateQueue(core::Queue &queue) {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t XdnaDriver::DestroyQueue(core::Queue &queue) const {
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

} // namespace AMD
} // namespace rocr
