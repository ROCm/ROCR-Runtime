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

#ifndef HSA_RUNTIME_CORE_ISA_H_
#define HSA_RUNTIME_CORE_ISA_H_

#include <cassert>
#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>
#include "core/inc/amd_hsa_code.hpp"

namespace core {

/// @class Isa.
/// @brief Instruction Set Architecture.
class Isa final: public amd::hsa::common::Signed<0xB13594F2BD8F212D> {
 public:
  /// @brief Isa's version type.
  typedef std::tuple<int32_t, int32_t, int32_t> Version;

  /// @brief Default destructor.
  ~Isa() {}

  /// @returns Handle equivalent of @p isa_object.
  static hsa_isa_t Handle(const Isa *isa_object) {
    hsa_isa_t isa_handle = { reinterpret_cast<uint64_t>(isa_object) };
    return isa_handle;
  }
  /// @returns Object equivalend of @p isa_handle.
  static Isa *Object(const hsa_isa_t &isa_handle) {
    Isa *isa_object = amd::hsa::common::ObjectAt<Isa>(isa_handle.handle);
    return isa_object;
  }

  /// @returns This Isa's version.
  const Version &version() const {
    return version_;
  }

  /// @returns This Isa's vendor.
  std::string GetVendor() const {
    return "AMD";
  }
  /// @returns This Isa's architecture.
  std::string GetArchitecture() const {
    return "AMDGPU";
  }
  /// @returns This Isa's major version.
  int32_t GetMajorVersion() const {
    return std::get<0>(version_);
  }
  /// @returns This Isa's minor version.
  int32_t GetMinorVersion() const {
    return std::get<1>(version_);
  }
  /// @returns This Isa's stepping.
  int32_t GetStepping() const {
    return std::get<2>(version_);
  }

  /// @returns True if this Isa is compatible with @p isa_object, false
  /// otherwise.
  bool IsCompatible(const Isa *isa_object) const {
    assert(isa_object);
    return version_ == isa_object->version_;
  }
  /// @returns True if this Isa is compatible with @p isa_handle, false
  /// otherwise.
  bool IsCompatible(const hsa_isa_t &isa_handle) const {
    assert(isa_handle.handle);
    return IsCompatible(Object(isa_handle));
  }
  /// @brief Isa is always in valid state.
  bool IsValid() const {
    return true;
  }

  /// @returns This Isa's full name.
  std::string GetFullName() const;

  /// @brief Query value of requested @p attribute and record it in @p value.
  bool GetInfo(const hsa_isa_info_t &attribute, void *value) const;

 private:
  /// @brief Default constructor.
  Isa(): version_(Version(-1, -1, -1)) {}

  /// @brief Construct from @p version.
  Isa(const Version &version): version_(version) {}

  /// @brief Isa's version.
  Version version_;

  /// @brief Isa's friends.
  friend class IsaRegistry;
}; // class Isa

/// @class IsaRegistry.
/// @brief Instruction Set Architecture Registry.
class IsaRegistry final {
 public:
  /// @returns Isa for requested @p full_name, null pointer if not supported.
  static const Isa *GetIsa(const std::string &full_name);
  /// @returns Isa for requested @p version, null pointer if not supported.
  static const Isa *GetIsa(const Isa::Version &version);

 private:
  /// @brief IsaRegistry's map type.
  typedef std::unordered_map<std::string, Isa> IsaMap;

  /// @brief Supported instruction set architectures.
  static const IsaMap supported_isas_;

  /// @brief Default constructor - not available.
  IsaRegistry();
  /// @brief Default destructor - not available.
  ~IsaRegistry();

  /// @returns Supported instruction set architectures.
  static const IsaMap GetSupportedIsas();
}; // class IsaRegistry

} // namespace core

#endif // HSA_RUNTIME_CORE_ISA_HPP_
