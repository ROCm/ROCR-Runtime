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

#ifndef HSA_RUNTIME_CORE_ISA_HPP_
#define HSA_RUNTIME_CORE_ISA_HPP_

#include <cstdint>
#include <ostream>
#include <string>
#include "core/runtime/compute_capability.hpp"
#include "core/inc/agent.h"
#include "core/inc/hsa_internal.h"
#include "core/inc/amd_hsa_code.hpp"

#define ISA_NAME_AMD_VENDOR "AMD"
#define ISA_NAME_AMD_DEVICE "AMDGPU"
#define ISA_NAME_AMD_TOKEN_COUNT 5

namespace core {

//===----------------------------------------------------------------------===//
// Isa.                                                                       //
//===----------------------------------------------------------------------===//

class Isa final: public amd::hsa::common::Signed<0xB13594F2BD8F212D> {
 public:
  const std::string &full_name() const { return full_name_; }
  const std::string &vendor() const { return vendor_; }
  const std::string &device() const { return device_; }
  const ComputeCapability &compute_capability() const {
    return compute_capability_;
  }

  static hsa_status_t Create(const hsa_agent_t &in_agent,
                             hsa_isa_t *out_isa_handle);

  static hsa_status_t Create(const char *in_isa_name,
                             hsa_isa_t *out_isa_handle);

  static hsa_isa_t Handle(const Isa *in_isa_object);

  static Isa *Object(const hsa_isa_t &in_isa_handle);

  Isa() : full_name_(""), vendor_(""), device_("") {}

  ~Isa() {}

  hsa_status_t Initialize(const hsa_agent_t &in_agent);

  hsa_status_t Initialize(const char *in_isa_name);

  void Reset();

  hsa_status_t GetInfo(const hsa_isa_info_t &in_isa_attribute,
                       const uint32_t &in_call_convention_index,
                       void *out_value) const;

  hsa_status_t IsCompatible(const Isa &in_isa_object, bool *out_result) const;

  bool IsValid();

  friend std::ostream &operator<<(std::ostream &out_stream, const Isa &in_isa);

 private:
  std::string full_name_;
  std::string vendor_;
  std::string device_;
  ComputeCapability compute_capability_;
};  // class Isa

}  // namespace core

#endif  // HSA_RUNTIME_CORE_ISA_HPP_
