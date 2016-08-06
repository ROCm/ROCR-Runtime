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

#include "core/inc/isa.h"

#include <cstring>
#include <sstream>

namespace core {

std::string Isa::GetFullName() const {
  std::stringstream full_name;
  full_name << GetVendor() << ":" << GetArchitecture() << ":"
            << GetMajorVersion() << ":" << GetMinorVersion() << ":"
            << GetStepping();
  return full_name.str();
}

bool Isa::GetInfo(const hsa_isa_info_t &attribute, void *value) const {
  if (!value) {
    return false;
  }

  switch (attribute) {
    case HSA_ISA_INFO_NAME_LENGTH: {
      std::string full_name = GetFullName();
      *((uint32_t *)value) = static_cast<uint32_t>(full_name.size());
      return true;
    }
    case HSA_ISA_INFO_NAME: {
      std::string full_name = GetFullName();
      memcpy(value, full_name.c_str(), full_name.size());
      return true;
    }
    // @todo: following case needs to be removed.
    case HSA_ISA_INFO_CALL_CONVENTION_COUNT: {
      *((uint32_t *)value) = 1;
      return true;
    }
    // @todo: following case needs to be removed.
    case HSA_ISA_INFO_CALL_CONVENTION_INFO_WAVEFRONT_SIZE: {
      *((uint32_t *)value) = 64;
      return true;
    }
    // @todo: following needs to be removed.
    case HSA_ISA_INFO_CALL_CONVENTION_INFO_WAVEFRONTS_PER_COMPUTE_UNIT: {
      *((uint32_t *)value) = 40;
      return true;
    }
    default: {
      return false;
    }
  }
}

const Isa *IsaRegistry::GetIsa(const std::string &full_name) {
  auto isareg_iter = supported_isas_.find(full_name);
  return isareg_iter == supported_isas_.end() ? nullptr : &isareg_iter->second;
}

const Isa *IsaRegistry::GetIsa(const Isa::Version &version) {
  auto isareg_iter = supported_isas_.find(Isa(version).GetFullName());
  return isareg_iter == supported_isas_.end() ? nullptr : &isareg_iter->second;
}

const IsaRegistry::IsaMap IsaRegistry::supported_isas_ =
  IsaRegistry::GetSupportedIsas();

const IsaRegistry::IsaMap IsaRegistry::GetSupportedIsas() {
#define ISAREG_ENTRY_GEN(maj, min, stp)                                        \
  Isa amd_amdgpu_##maj##min##stp;                                              \
  amd_amdgpu_##maj##min##stp.version_ = Isa::Version(maj, min, stp);           \
  supported_isas.insert(                                                       \
    std::make_pair(                                                            \
      amd_amdgpu_##maj##min##stp.GetFullName(), amd_amdgpu_##maj##min##stp));  \

  IsaMap supported_isas;

  ISAREG_ENTRY_GEN(7, 0, 0)
  ISAREG_ENTRY_GEN(7, 0, 1)
  ISAREG_ENTRY_GEN(8, 0, 0)
  ISAREG_ENTRY_GEN(8, 0, 1)
  ISAREG_ENTRY_GEN(8, 0, 2)
  ISAREG_ENTRY_GEN(8, 0, 3)
  ISAREG_ENTRY_GEN(8, 1, 0)
  ISAREG_ENTRY_GEN(9, 0, 0)

  return supported_isas;
}

} // namespace core
