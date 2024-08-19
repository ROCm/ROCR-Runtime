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

#include "core/inc/runtime.h"

namespace rocr {
namespace AMD {

KfdDriver::KfdDriver(std::string devnode_name)
    : core::Driver(core::DriverType::KFD, devnode_name) {}

hsa_status_t KfdDriver::DiscoverDriver() {
  if (hsaKmtOpenKFD() == HSAKMT_STATUS_SUCCESS) {
    std::unique_ptr<Driver> kfd_drv(new KfdDriver("/dev/kfd"));
    core::Runtime::runtime_singleton_->RegisterDriver(kfd_drv);
    return HSA_STATUS_SUCCESS;
  }
  return HSA_STATUS_ERROR;
}

hsa_status_t KfdDriver::QueryKernelModeDriver(core::DriverQuery query) {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t KfdDriver::GetMemoryProperties(uint32_t node_id,
                                            core::MemProperties &mprops) const {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t KfdDriver::AllocateMemory(void **mem, size_t size,
                                       uint32_t node_id, core::MemFlags flags) {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t KfdDriver::FreeMemory(void *mem, uint32_t node_id) {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t KfdDriver::CreateQueue(core::Queue &queue) {
  return HSA_STATUS_SUCCESS;
}

hsa_status_t KfdDriver::DestroyQueue(core::Queue &queue) const {
  return HSA_STATUS_SUCCESS;
}

} // namespace AMD
} // namespace rocr
