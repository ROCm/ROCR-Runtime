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

// HSA runtime C++ interface file.

#ifndef HSA_RUNTME_CORE_INC_AGENT_H_
#define HSA_RUNTME_CORE_INC_AGENT_H_

#include <assert.h>

#include <vector>

#include "core/inc/runtime.h"
#include "core/inc/checked.h"
#include "core/inc/queue.h"
#include "core/inc/memory_region.h"
#include "core/util/utils.h"
#include "core/runtime/compute_capability.hpp"

namespace core {
class Signal;

typedef void (*HsaEventCallback)(hsa_status_t status, hsa_queue_t* source,
                                 void* data);

class MemoryRegion;

/*
Agent is intended to be an pure interface class and may be wrapped or replaced
by tools.
All funtions other than Convert, device_type, and public_handle must be virtual.
*/
class Agent : public Checked<0xF6BC25EB17E6F917> {
 public:
  // Lightweight RTTI for vendor specific implementations.
  enum DeviceType { kAmdGpuDevice = 0, kAmdCpuDevice = 1, kUnknownDevice = 2 };

  explicit Agent(DeviceType type)
      : compute_capability_(), device_type_(uint32_t(type)) {
    public_handle_ = Convert(this);
  }
  explicit Agent(uint32_t type) : compute_capability_(), device_type_(type) {
    public_handle_ = Convert(this);
  }

  virtual ~Agent() {}

  // Convert this object into hsa_agent_t.
  static __forceinline hsa_agent_t Convert(Agent* agent) {
    const hsa_agent_t agent_handle = {
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(agent))};
    return agent_handle;
  }

  static __forceinline const hsa_agent_t Convert(const Agent* agent) {
    const hsa_agent_t agent_handle = {
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(agent))};
    return agent_handle;
  }

  // Convert hsa_agent_t into Agent *.
  static __forceinline Agent* Convert(hsa_agent_t agent) {
    return reinterpret_cast<Agent*>(agent.handle);
  }

  virtual hsa_status_t DmaCopy(void* dst, const void* src, size_t size) {
    return HSA_STATUS_ERROR;
  }

  virtual hsa_status_t DmaCopy(void* dst, const void* src, size_t size,
                               std::vector<core::Signal*>& dep_signals,
                               core::Signal& out_signal) {
    return HSA_STATUS_ERROR;
  }

  virtual hsa_status_t DmaFill(void* ptr, uint32_t value, size_t count) {
    return HSA_STATUS_ERROR;
  }

  virtual hsa_status_t IterateRegion(
      hsa_status_t (*callback)(hsa_region_t region, void* data),
      void* data) const = 0;

  virtual hsa_status_t QueueCreate(size_t size, hsa_queue_type_t queue_type,
                                   HsaEventCallback event_callback, void* data,
                                   uint32_t private_segment_size,
                                   uint32_t group_segment_size,
                                   Queue** queue) = 0;

  // Translate vendor specific agent properties into HSA agent attribute.
  virtual hsa_status_t GetInfo(hsa_agent_info_t attribute,
                               void* value) const = 0;

  virtual const std::vector<const core::MemoryRegion*>& regions() const = 0;

  // For lightweight RTTI
  __forceinline uint32_t device_type() const { return device_type_; }

  __forceinline hsa_agent_t public_handle() const { return public_handle_; }

  // Get agent's compute capability value
  __forceinline const ComputeCapability& compute_capability() const {
    return compute_capability_;
  }

 protected:
  // Intention here is to have a polymorphic update procedure for public_handle_
  // which is callable on any Agent* but only from some class dervied from
  // Agent*.  do_set_public_handle should remain protected or private in all
  // derived types.
  static __forceinline void set_public_handle(Agent* agent,
                                              hsa_agent_t handle) {
    agent->do_set_public_handle(handle);
  }
  virtual void do_set_public_handle(hsa_agent_t handle) {
    public_handle_ = handle;
  }
  hsa_agent_t public_handle_;
  ComputeCapability compute_capability_;

 private:
  // Forbid copying and moving of this object
  DISALLOW_COPY_AND_ASSIGN(Agent);

  const uint32_t device_type_;
};
}  // namespace core

#endif  // header guard
