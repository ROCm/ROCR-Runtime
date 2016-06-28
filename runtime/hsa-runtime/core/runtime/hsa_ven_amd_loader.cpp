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

#include "hsa_ven_amd_loader.h"

#include "core/inc/amd_hsa_loader.hpp"
#include "core/inc/runtime.h"

using namespace core;

hsa_status_t HSA_API hsa_ven_amd_loader_query_segment_descriptors(
  hsa_ven_amd_loader_segment_descriptor_t *segment_descriptors,
  size_t *num_segment_descriptors) {
  if (false == core::Runtime::runtime_singleton_->IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }

  // Arguments are checked by the loader.
  return Runtime::runtime_singleton_->loader()->QuerySegmentDescriptors(segment_descriptors, num_segment_descriptors);
}

hsa_status_t HSA_API hsa_ven_amd_loader_query_host_address(
  const void *device_address,
  const void **host_address) {
  if (false == core::Runtime::runtime_singleton_->IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }
  if (nullptr == device_address) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  if (nullptr == host_address) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  uint64_t udaddr = reinterpret_cast<uint64_t>(device_address);
  uint64_t uhaddr = Runtime::runtime_singleton_->loader()->FindHostAddress(udaddr);
  if (0 == uhaddr) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  *host_address = reinterpret_cast<void*>(uhaddr);
  return HSA_STATUS_SUCCESS;
}
