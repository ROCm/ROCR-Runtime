////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
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

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <utility>

namespace rocr {
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

/* static */
bool Isa::IsCompatible(const Isa &code_object_isa,
                       const Isa &agent_isa) {
  if (code_object_isa.GetVersion() != agent_isa.GetVersion())
    return false;

  assert(code_object_isa.IsSrameccSupported() == agent_isa.IsSrameccSupported()  && agent_isa.GetSramecc() != IsaFeature::Any);
  if ((code_object_isa.GetSramecc() == IsaFeature::Enabled ||
        code_object_isa.GetSramecc() == IsaFeature::Disabled) &&
      code_object_isa.GetSramecc() != agent_isa.GetSramecc())
    return false;

  assert(code_object_isa.IsXnackSupported() == agent_isa.IsXnackSupported() && agent_isa.GetXnack() != IsaFeature::Any);
  if ((code_object_isa.GetXnack() == IsaFeature::Enabled ||
        code_object_isa.GetXnack() == IsaFeature::Disabled) &&
      code_object_isa.GetXnack() != agent_isa.GetXnack())
    return false;

  return true;
}

std::string Isa::GetProcessorName() const {
  std::string processor(targetid_);
  return processor.substr(0, processor.find(':'));
}

std::string Isa::GetIsaName() const {
  constexpr char hsa_isa_name_prefix[] = "amdgcn-amd-amdhsa--";
  return std::string(hsa_isa_name_prefix) + targetid_;
}

bool Isa::GetInfo(const hsa_isa_info_t &attribute, void *value) const {
  if (!value) {
    return false;
  }

  switch (attribute) {
    case HSA_ISA_INFO_NAME_LENGTH: {
      std::string isa_name = GetIsaName();
      *((uint32_t*)value) = static_cast<uint32_t>(isa_name.size() + 1);
      return true;
    }
    case HSA_ISA_INFO_NAME: {
      std::string isa_name = GetIsaName();
      memset(value, 0x0, isa_name.size() + 1);
      memcpy(value, isa_name.c_str(), isa_name.size());
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
      if (this->GetVersion() == Version(7, 0, 0) ||
          this->GetVersion() == Version(8, 0, 1)) {
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

const Isa *IsaRegistry::GetIsa(const Isa::Version &version, IsaFeature sramecc, IsaFeature xnack) {
  auto isareg_iter = std::find_if(supported_isas_.begin(), supported_isas_.end(),
                                  [&](const IsaMap::value_type& isareg) {
                                    return isareg.second.GetVersion() == version &&
                                        (isareg.second.GetSramecc() == IsaFeature::Unsupported ||
                                         isareg.second.GetSramecc() == sramecc) &&
                                        (isareg.second.GetXnack() == IsaFeature::Unsupported ||
                                         isareg.second.GetXnack() == xnack);
                                  });
  return isareg_iter == supported_isas_.end() ? nullptr : &isareg_iter->second;
}

const IsaRegistry::IsaMap IsaRegistry::supported_isas_ =
  IsaRegistry::GetSupportedIsas();

const IsaRegistry::IsaMap IsaRegistry::GetSupportedIsas() {

// agent, and vendor name length limit excluding terminating nul character.
constexpr size_t hsa_name_size = 63;

// FIXME: Use static_assert when C++17 used.
#define ISAREG_ENTRY_GEN(name, maj, min, stp, sramecc, xnack)                                            \
  assert(std::char_traits<char>::length(name) <= hsa_name_size);                                         \
  Isa amd_amdgpu_##maj##min##stp##_SRAMECC_##sramecc##_XNACK_##xnack;                                    \
  amd_amdgpu_##maj##min##stp##_SRAMECC_##sramecc##_XNACK_##xnack.targetid_ = name;                       \
  amd_amdgpu_##maj##min##stp##_SRAMECC_##sramecc##_XNACK_##xnack.version_ = Isa::Version(maj, min, stp); \
  amd_amdgpu_##maj##min##stp##_SRAMECC_##sramecc##_XNACK_##xnack.sramecc_ = sramecc;                     \
  amd_amdgpu_##maj##min##stp##_SRAMECC_##sramecc##_XNACK_##xnack.xnack_ = xnack;                         \
  supported_isas.insert(std::make_pair(                                                                  \
      amd_amdgpu_##maj##min##stp##_SRAMECC_##sramecc##_XNACK_##xnack.GetIsaName(),                       \
      amd_amdgpu_##maj##min##stp##_SRAMECC_##sramecc##_XNACK_##xnack));                                  \

  IsaMap supported_isas;
  IsaFeature unsupported = IsaFeature::Unsupported;
  IsaFeature any = IsaFeature::Any;
  IsaFeature disabled = IsaFeature::Disabled;
  IsaFeature enabled = IsaFeature::Enabled;

  //               Target ID                 Version   SRAMECC      XNACK
  ISAREG_ENTRY_GEN("gfx700",                 7, 0, 0,  unsupported, unsupported)
  ISAREG_ENTRY_GEN("gfx701",                 7, 0, 1,  unsupported, unsupported)
  ISAREG_ENTRY_GEN("gfx702",                 7, 0, 2,  unsupported, unsupported)
  ISAREG_ENTRY_GEN("gfx801",                 8, 0, 1,  unsupported, any)
  ISAREG_ENTRY_GEN("gfx801:xnack-",          8, 0, 1,  unsupported, disabled)
  ISAREG_ENTRY_GEN("gfx801:xnack+",          8, 0, 1,  unsupported, enabled)
  ISAREG_ENTRY_GEN("gfx802",                 8, 0, 2,  unsupported, unsupported)
  ISAREG_ENTRY_GEN("gfx803",                 8, 0, 3,  unsupported, unsupported)
  ISAREG_ENTRY_GEN("gfx805",                 8, 0, 5,  unsupported, unsupported)
  ISAREG_ENTRY_GEN("gfx810",                 8, 1, 0,  unsupported, any)
  ISAREG_ENTRY_GEN("gfx810:xnack-",          8, 1, 0,  unsupported, disabled)
  ISAREG_ENTRY_GEN("gfx810:xnack+",          8, 1, 0,  unsupported, enabled)
  ISAREG_ENTRY_GEN("gfx900",                 9, 0, 0,  unsupported, any)
  ISAREG_ENTRY_GEN("gfx900:xnack-",          9, 0, 0,  unsupported, disabled)
  ISAREG_ENTRY_GEN("gfx900:xnack+",          9, 0, 0,  unsupported, enabled)
  ISAREG_ENTRY_GEN("gfx902",                 9, 0, 2,  unsupported, any)
  ISAREG_ENTRY_GEN("gfx902:xnack-",          9, 0, 2,  unsupported, disabled)
  ISAREG_ENTRY_GEN("gfx902:xnack+",          9, 0, 2,  unsupported, enabled)
  ISAREG_ENTRY_GEN("gfx904",                 9, 0, 4,  unsupported, any)
  ISAREG_ENTRY_GEN("gfx904:xnack-",          9, 0, 4,  unsupported, disabled)
  ISAREG_ENTRY_GEN("gfx904:xnack+",          9, 0, 4,  unsupported, enabled)
  ISAREG_ENTRY_GEN("gfx906",                 9, 0, 6,  any,         any)
  ISAREG_ENTRY_GEN("gfx906:xnack-",          9, 0, 6,  any,         disabled)
  ISAREG_ENTRY_GEN("gfx906:xnack+",          9, 0, 6,  any,         enabled)
  ISAREG_ENTRY_GEN("gfx906:sramecc-",        9, 0, 6,  disabled,    any)
  ISAREG_ENTRY_GEN("gfx906:sramecc+",        9, 0, 6,  enabled,     any)
  ISAREG_ENTRY_GEN("gfx906:sramecc-:xnack-", 9, 0, 6,  disabled,    disabled)
  ISAREG_ENTRY_GEN("gfx906:sramecc-:xnack+", 9, 0, 6,  disabled,    enabled)
  ISAREG_ENTRY_GEN("gfx906:sramecc+:xnack-", 9, 0, 6,  enabled,     disabled)
  ISAREG_ENTRY_GEN("gfx906:sramecc+:xnack+", 9, 0, 6,  enabled,     enabled)
  ISAREG_ENTRY_GEN("gfx908",                 9, 0, 8,  any,         any)
  ISAREG_ENTRY_GEN("gfx908:xnack-",          9, 0, 8,  any,         disabled)
  ISAREG_ENTRY_GEN("gfx908:xnack+",          9, 0, 8,  any,         enabled)
  ISAREG_ENTRY_GEN("gfx908:sramecc-",        9, 0, 8,  disabled,    any)
  ISAREG_ENTRY_GEN("gfx908:sramecc+",        9, 0, 8,  enabled,     any)
  ISAREG_ENTRY_GEN("gfx908:sramecc-:xnack-", 9, 0, 8,  disabled,    disabled)
  ISAREG_ENTRY_GEN("gfx908:sramecc-:xnack+", 9, 0, 8,  disabled,    enabled)
  ISAREG_ENTRY_GEN("gfx908:sramecc+:xnack-", 9, 0, 8,  enabled,     disabled)
  ISAREG_ENTRY_GEN("gfx908:sramecc+:xnack+", 9, 0, 8,  enabled,     enabled)
  ISAREG_ENTRY_GEN("gfx1010",                10, 1, 0, unsupported, any)
  ISAREG_ENTRY_GEN("gfx1010:xnack-",         10, 1, 0, unsupported, disabled)
  ISAREG_ENTRY_GEN("gfx1010:xnack+",         10, 1, 0, unsupported, enabled)
  ISAREG_ENTRY_GEN("gfx1011",                10, 1, 1, unsupported, any)
  ISAREG_ENTRY_GEN("gfx1011:xnack-",         10, 1, 1, unsupported, disabled)
  ISAREG_ENTRY_GEN("gfx1011:xnack+",         10, 1, 1, unsupported, enabled)
  ISAREG_ENTRY_GEN("gfx1012",                10, 1, 2, unsupported, any)
  ISAREG_ENTRY_GEN("gfx1012:xnack-",         10, 1, 2, unsupported, disabled)
  ISAREG_ENTRY_GEN("gfx1012:xnack+",         10, 1, 2, unsupported, enabled)
  ISAREG_ENTRY_GEN("gfx1030",                10, 3, 0, unsupported, unsupported)
  ISAREG_ENTRY_GEN("gfx1031",                10, 3, 1, unsupported, unsupported)
  ISAREG_ENTRY_GEN("gfx1032",                10, 3, 2, unsupported, unsupported)
  ISAREG_ENTRY_GEN("gfx1033",                10, 3, 3, unsupported, unsupported)
#undef ISAREG_ENTRY_GEN
  return supported_isas;
}

} // namespace core
} // namespace rocr
