////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2022-2023, Advanced Micro Devices, Inc. All rights reserved.
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

// AMD specific HSA backend.

#ifndef HSA_RUNTIME_CORE_INC_AMD_AIE_AGENT_H_
#define HSA_RUNTIME_CORE_INC_AMD_AIE_AGENT_H_

#include "core/inc/runtime.h"
#include "core/inc/agent.h"

namespace rocr {
namespace AMD {

class AieAgent : public core::Agent {
 public:
  /// @brief AIE agent constructor.
  /// @param [in] node Node id.
  AieAgent(HSAuint32 node);

  // @brief AIE agent destructor.
  ~AieAgent();

  hsa_status_t VisitRegion(bool include_peer, hsa_status_t (*callback)(hsa_region_t region,
                                                                       void* data),
                           void* data) const;
  hsa_status_t IterateRegion(hsa_status_t (*callback)(hsa_region_t region,
                                                      void* data),
                             void* data) const override;

  hsa_status_t IterateCache(hsa_status_t (*callback)(hsa_cache_t cache,
                                                     void* data),
                            void* value) const override;

  hsa_status_t GetInfo(hsa_agent_info_t attribute,
                       void* value) const override;

  hsa_status_t QueueCreate(size_t size,
                           hsa_queue_type32_t queue_type,
                           core::HsaEventCallback event_callback,
                           void* data,
                           uint32_t private_segment_size,
                           uint32_t group_segment_size,
                           core::Queue** queue) override;

  const core::Isa* isa() const override { return nullptr; }

  const std::vector <const core::MemoryRegion*>& regions() const override {
    return regions_;
  }

 uintptr_t GetDeviceHeapAddr() const;

 private:
  // @brief Query the driver to get the region list owned by this agent.
  void InitRegionList();
  void InitDeviceAllocator();

  std::vector<const core::MemoryRegion*> regions_;

  const hsa_profile_t profile_ = HSA_PROFILE_BASE;
  static const uint32_t maxQueues_ = 6;
  static const uint32_t minAqlSize_ = 0x40;
  static const uint32_t maxAqlSize_ = 0x40;
  uint32_t max_queues_;
  uintptr_t device_heap_vaddr_ = 0;
};

} // namespace amd
} // namespace rocr

#endif // header guard
