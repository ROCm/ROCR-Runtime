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
#include <utility>

namespace core {

bool Wavefront::GetInfo(
    const hsa_wavefront_info_t &attribute,
    void *value) const {
  if (!value) {
    return false;
  }

  switch (attribute) {
    case HSA_WAVEFRONT_INFO_SIZE: {
      *((uint32_t*)value) = 64;
      return true;
    }
    default: {
      return false;
    }
  }
}

std::string Isa::GetFullName() const {
  std::stringstream full_name;
  full_name << GetArchitecture() << "-" << GetVendor() << "-" << GetOS() << "-"
            << GetEnvironment() << "-gfx" << GetMajorVersion()
            << GetMinorVersion() << GetStepping();

  if (xnackEnabled_)
    full_name << "+xnack";

  if (sramEcc_)
    full_name << "+sram-ecc";

  return full_name.str();
}

bool Isa::GetInfo(const hsa_isa_info_t &attribute, void *value) const {
  if (!value) {
    return false;
  }

  switch (attribute) {
    case HSA_ISA_INFO_NAME_LENGTH: {
      std::string full_name = GetFullName();
      *((uint32_t*)value) = static_cast<uint32_t>(full_name.size() + 1);
      return true;
    }
    case HSA_ISA_INFO_NAME: {
      std::string full_name = GetFullName();
      memset(value, 0x0, full_name.size() + 1);
      memcpy(value, full_name.c_str(), full_name.size());
      return true;
    }
    // deprecated.
    case HSA_ISA_INFO_CALL_CONVENTION_COUNT: {
      *((uint32_t*)value) = 1;
      return true;
    }
    // deprecated.
    case HSA_ISA_INFO_CALL_CONVENTION_INFO_WAVEFRONT_SIZE: {
      *((uint32_t*)value) = 64;
      return true;
    }
    // deprecated.
    case HSA_ISA_INFO_CALL_CONVENTION_INFO_WAVEFRONTS_PER_COMPUTE_UNIT: {
      *((uint32_t*)value) = 40;
      return true;
    }
    case HSA_ISA_INFO_MACHINE_MODELS: {
      const bool machine_models[2] = {false, true};
      memcpy(value, machine_models, sizeof(machine_models));
      return true;
    }
    case HSA_ISA_INFO_PROFILES: {
      bool profiles[2] = {true, false};
      if (this->version() == Version(7, 0, 0) ||
          this->version() == Version(8, 0, 1)) {
        profiles[1] = true;
      }
      memcpy(value, profiles, sizeof(profiles));
      return true;
    }
    case HSA_ISA_INFO_DEFAULT_FLOAT_ROUNDING_MODES: {
      const bool rounding_modes[3] = {false, false, true};
      memcpy(value, rounding_modes, sizeof(rounding_modes));
      return true;
    }
    case HSA_ISA_INFO_BASE_PROFILE_DEFAULT_FLOAT_ROUNDING_MODES: {
      const bool rounding_modes[3] = {false, false, true};
      memcpy(value, rounding_modes, sizeof(rounding_modes));
      return true;
    }
    case HSA_ISA_INFO_FAST_F16_OPERATION: {
      if (this->GetMajorVersion() >= 8) {
        *((bool*)value) = true;
      } else {
        *((bool*)value) = false;
      }
      return true;
    }
    case HSA_ISA_INFO_WORKGROUP_MAX_DIM: {
      const uint16_t workgroup_max_dim[3] = {1024, 1024, 1024};
      memcpy(value, workgroup_max_dim, sizeof(workgroup_max_dim));
      return true;
    }
    case HSA_ISA_INFO_WORKGROUP_MAX_SIZE: {
      *((uint32_t*)value) = 1024;
      return true;
    }
    case HSA_ISA_INFO_GRID_MAX_DIM: {
      const hsa_dim3_t grid_max_dim = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
      memcpy(value, &grid_max_dim, sizeof(grid_max_dim));
      return true;
    }
    case HSA_ISA_INFO_GRID_MAX_SIZE: {
      *((uint64_t*)value) = UINT64_MAX;
      return true;
    }
    case HSA_ISA_INFO_FBARRIER_MAX_SIZE: {
      *((uint32_t*)value) = 32;
      return true;
    }
    default: {
      return false;
    }
  }
}

hsa_round_method_t Isa::GetRoundMethod(
    hsa_fp_type_t fp_type,
    hsa_flush_mode_t flush_mode) const {
  return HSA_ROUND_METHOD_SINGLE;
}

const Isa *IsaRegistry::GetIsa(const std::string &full_name) {
  auto isareg_iter = supported_isas_.find(full_name);
  return isareg_iter == supported_isas_.end() ? nullptr : &isareg_iter->second;
}

const Isa *IsaRegistry::GetIsa(const Isa::Version &version, bool xnack, bool ecc) {
  auto isareg_iter = supported_isas_.find(Isa(version, xnack, ecc).GetFullName());
  return isareg_iter == supported_isas_.end() ? nullptr : &isareg_iter->second;
}

const IsaRegistry::IsaMap IsaRegistry::supported_isas_ =
  IsaRegistry::GetSupportedIsas();

const IsaRegistry::IsaMap IsaRegistry::GetSupportedIsas() {
#define ISAREG_ENTRY_GEN(maj, min, stp, xnack, ecc)                                 \
  Isa amd_amdgpu_##maj##min##stp##xnack##ecc;                                       \
  amd_amdgpu_##maj##min##stp##xnack##ecc.version_ = Isa::Version(maj, min, stp);    \
  amd_amdgpu_##maj##min##stp##xnack##ecc.xnackEnabled_ = xnack;                     \
  amd_amdgpu_##maj##min##stp##xnack##ecc.sramEcc_ = ecc;                            \
  supported_isas.insert(std::make_pair(                                             \
      amd_amdgpu_##maj##min##stp##xnack##ecc.GetFullName(),                         \
      amd_amdgpu_##maj##min##stp##xnack##ecc));                                     \

  IsaMap supported_isas;

  ISAREG_ENTRY_GEN(7, 0, 0, false, false)
  ISAREG_ENTRY_GEN(7, 0, 1, false, false)
  ISAREG_ENTRY_GEN(7, 0, 2, false, false)
  ISAREG_ENTRY_GEN(8, 0, 1, true,  false)
  ISAREG_ENTRY_GEN(8, 0, 2, false, false)
  ISAREG_ENTRY_GEN(8, 0, 3, false, false)
  ISAREG_ENTRY_GEN(8, 1, 0, true,  false)
  ISAREG_ENTRY_GEN(9, 0, 0, false, false)
  ISAREG_ENTRY_GEN(9, 0, 2, true,  false)
  ISAREG_ENTRY_GEN(9, 0, 4, false, false)
  ISAREG_ENTRY_GEN(9, 0, 6, false, false)
  ISAREG_ENTRY_GEN(9, 0, 6, false, true )

  return supported_isas;
}

} // namespace core
