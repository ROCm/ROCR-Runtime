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

#include "core/inc/compute_capability.h"

#include <cstdint>
#include <ostream>

#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

using core::ComputeCapability;
using core::ComputeProperties;

//===----------------------------------------------------------------------===//
// CapabilityMapping Initialization.                                          //
//===----------------------------------------------------------------------===//

struct ComputeCapabilityComparison final {
  bool operator()(const ComputeCapability &in_lhs,
                  const ComputeCapability &in_rhs) const {
    return in_lhs.version_major() == in_rhs.version_major() &&
           in_lhs.version_minor() == in_rhs.version_minor() &&
           in_lhs.version_stepping() == in_rhs.version_stepping();
  }
};  // struct ComputeCapabilityComparison

struct ComputeCapabilityHash final {
  std::size_t operator()(const ComputeCapability &in_cc) const {
    // FIXME: better hashing function?
    std::ostringstream cc_stream;
    cc_stream << in_cc;
    return std::hash<std::string>()(cc_stream.str());
  }
};  // struct ComputeCapabilityHash

typedef std::unordered_map<ComputeCapability,           // Key.
                           ComputeProperties,           // Value.
                           ComputeCapabilityHash,       // Hash function.
                           ComputeCapabilityComparison  // Comparison function.
                           > CapabilityMap;

struct CapabilityMapping final {
  static CapabilityMap Initialize() {
    CapabilityMap contents;

    // NOTE: All compute capabilities and compute properties must be added below
    // this line.
    // -------------------------------------------------------------------------

    contents[ComputeCapability(7, 0, 0)] = ComputeProperties();
    contents[ComputeCapability(7, 0, 1)] = ComputeProperties();
    contents[ComputeCapability(8, 0, 0)] = ComputeProperties();
    contents[ComputeCapability(8, 0, 1)] = ComputeProperties();
    contents[ComputeCapability(8, 0, 2)] = ComputeProperties();
    contents[ComputeCapability(8, 0, 3)] = ComputeProperties();
    contents[ComputeCapability(8, 1, 0)] = ComputeProperties();
    contents[ComputeCapability(9, 0, 0)] = ComputeProperties();

    return contents;
  }

  static const CapabilityMap contents_;
};  // struct CapabilityMapping

const CapabilityMap CapabilityMapping::contents_ =
    CapabilityMapping::Initialize();

}  // namespace anonymous

namespace core {

//===----------------------------------------------------------------------===//
// ComputeProperties.                                                         //
//===----------------------------------------------------------------------===//

void ComputeProperties::Initialize() {
  is_initialized_ = true;
}  // ComputeProperties::Initialize

void ComputeProperties::Reset() {
  is_initialized_ = false;
}  // ComputeProperties::Reset

//===----------------------------------------------------------------------===//
// ComputeCapability.                                                         //
//===----------------------------------------------------------------------===//

ComputeCapability::ComputeCapability(const int32_t &in_version_major,
                                     const int32_t &in_version_minor,
                                     const int32_t &in_version_stepping)
    : version_major_(in_version_major),
      version_minor_(in_version_minor),
      version_stepping_(in_version_stepping) {
  auto compute_properties = CapabilityMapping::contents_.find(*this);
  if (compute_properties != CapabilityMapping::contents_.end()) {
    compute_properties_.Initialize();
  }
}

void ComputeCapability::Initialize(const int32_t &in_version_major,
                                   const int32_t &in_version_minor,
                                   const int32_t &in_version_stepping) {
  version_major_ = in_version_major;
  version_minor_ = in_version_minor;
  version_stepping_ = in_version_stepping;
  auto compute_properties = CapabilityMapping::contents_.find(*this);
  if (compute_properties != CapabilityMapping::contents_.end()) {
    compute_properties_.Initialize();
  }
}  // ComputeCapability::Initialize

/*void ComputeCapability::Initialize(const uint32_t &in_device_id) {
  auto compute_capability = DeviceMapping::contents_.find(in_device_id);
  if (compute_capability != DeviceMapping::contents_.end()) {
    version_major_ = compute_capability->second.version_major_;
    version_minor_ = compute_capability->second.version_minor_;
    version_stepping_ = compute_capability->second.version_stepping_;
  }
}  // ComputeCapability::Initialize */

void ComputeCapability::Reset() {
  version_major_ = COMPUTE_CAPABILITY_VERSION_MAJOR_UNDEFINED;
  version_minor_ = COMPUTE_CAPABILITY_VERSION_MINOR_UNDEFINED;
  version_stepping_ = COMPUTE_CAPABILITY_VERSION_STEPPING_UNDEFINED;
  compute_properties_.Reset();
}  // ComputeCapability::Reset

bool ComputeCapability::IsValid() {
  return compute_properties_.is_initialized();
}  // ComputeCapability::IsValid

std::ostream &operator<<(std::ostream &out_stream,
                         const ComputeCapability &in_compute_capability) {
  return out_stream << in_compute_capability.version_major_ << ":"
                    << in_compute_capability.version_minor_ << ":"
                    << in_compute_capability.version_stepping_;
}  // ostream<<ComputeCapability

}  // namespace core
