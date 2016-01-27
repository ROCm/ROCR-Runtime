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

#ifndef HSA_RUNTIME_CORE_COMPUTE_CAPABILITY_HPP_
#define HSA_RUNTIME_CORE_COMPUTE_CAPABILITY_HPP_

#include <cstdint>
#include <ostream>

#define COMPUTE_CAPABILITY_VERSION_MAJOR_UNDEFINED -1
#define COMPUTE_CAPABILITY_VERSION_MINOR_UNDEFINED -1
#define COMPUTE_CAPABILITY_VERSION_STEPPING_UNDEFINED -1

namespace core {

//===----------------------------------------------------------------------===//
// ComputeProperties.                                                         //
//===----------------------------------------------------------------------===//

class ComputeProperties final {
 public:
  const bool &is_initialized() const { return is_initialized_; }

  ComputeProperties() : is_initialized_(false) {}

  ~ComputeProperties() {}

  void Initialize();

  void Reset();

 private:
  bool is_initialized_;
};  // class ComputeProperties

//===----------------------------------------------------------------------===//
// ComputeCapability.                                                         //
//===----------------------------------------------------------------------===//

class ComputeCapability final {
 public:
  const int32_t &version_major() const { return version_major_; }
  const int32_t &version_minor() const { return version_minor_; }
  const int32_t &version_stepping() const { return version_stepping_; }
  const ComputeProperties &compute_properties() const {
    return compute_properties_;
  }

  void set_version_major(const int32_t &in_version_major) {
    version_major_ = in_version_major;
  }
  void set_version_minor(const int32_t &in_version_minor) {
    version_minor_ = in_version_minor;
  }
  void set_version_stepping(const int32_t &in_version_stepping) {
    version_stepping_ = in_version_stepping;
  }

  ComputeCapability()
      : version_major_(COMPUTE_CAPABILITY_VERSION_MAJOR_UNDEFINED),
        version_minor_(COMPUTE_CAPABILITY_VERSION_MINOR_UNDEFINED),
        version_stepping_(COMPUTE_CAPABILITY_VERSION_STEPPING_UNDEFINED) {}

  ComputeCapability(const int32_t &in_version_major,
                    const int32_t &in_version_minor,
                    const int32_t &in_version_stepping);

  ~ComputeCapability() {}

  void Initialize(const int32_t &in_version_major,
                  const int32_t &in_version_minor,
                  const int32_t &in_version_stepping);

  // void Initialize(const uint32_t &in_device_id);

  void Reset();

  bool IsValid();

  friend std::ostream &operator<<(
      std::ostream &out_stream, const ComputeCapability &in_compute_capability);

 private:
  int32_t version_major_;
  int32_t version_minor_;
  int32_t version_stepping_;
  ComputeProperties compute_properties_;
};  // class ComputeCapability

}  // namespace core

#endif  // HSA_RUNTIME_CORE_COMPUTE_CAPABILITY_HPP_
