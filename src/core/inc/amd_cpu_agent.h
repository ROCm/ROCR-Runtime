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

// AMD specific HSA backend.

#ifndef HSA_RUNTIME_CORE_INC_AMD_CPU_AGENT_H_
#define HSA_RUNTIME_CORE_INC_AMD_CPU_AGENT_H_

#include <vector>

#include "hsakmt.h"

#include "core/inc/runtime.h"
#include "core/inc/agent.h"
#include "core/inc/queue.h"

namespace amd {
class CpuAgent : public core::Agent {
 public:
  CpuAgent(HSAuint32 node, const HsaNodeProperties& node_props,
           const std::vector<HsaCacheProperties>& cache_props);

  ~CpuAgent();

  hsa_status_t VisitRegion(bool include_peer,
                           hsa_status_t (*callback)(hsa_region_t region,
                                                    void* data),
                           void* data) const;

  void RegisterMemoryProperties(core::MemoryRegion& region);

  hsa_status_t IterateRegion(hsa_status_t (*callback)(hsa_region_t region,
                                                      void* data),
                             void* data) const override;

  hsa_status_t GetInfo(hsa_agent_info_t attribute, void* value) const override;

  /// @brief Api to create an Aql queue
  ///
  /// @param size Size of Queue in terms of Aql packet size
  ///
  /// @param type of Queue Single Writer or Multiple Writer
  ///
  /// @param callback Callback function to register in case Quee
  /// encounters an error
  ///
  /// @param data Application data that is passed to @p callback on every
  /// iteration.May be NULL.
  ///
  /// @param private_segment_size Hint indicating the maximum
  /// expected private segment usage per work - item, in bytes.There may
  /// be performance degradation if the application places a Kernel
  /// Dispatch packet in the queue and the corresponding private segment
  /// usage exceeds @p private_segment_size.If the application does not
  /// want to specify any particular value for this argument, @p
  /// private_segment_size must be UINT32_MAX.If the queue does not
  /// support Kernel Dispatch packets, this argument is ignored.
  ///
  /// @param group_segment_size Hint indicating the maximum expected
  /// group segment usage per work - group, in bytes.There may be
  /// performance degradation if the application places a Kernel Dispatch
  /// packet in the queue and the corresponding group segment usage
  /// exceeds @p group_segment_size.If the application does not want to
  /// specify any particular value for this argument, @p
  /// group_segment_size must be UINT32_MAX.If the queue does not
  /// support Kernel Dispatch packets, this argument is ignored.
  ///
  /// @parm queue Output parameter updated with a pointer to the
  /// queue being created
  ///
  /// @return hsa_status
  hsa_status_t QueueCreate(size_t size, hsa_queue_type_t queue_type,
                           core::HsaEventCallback event_callback, void* data,
                           uint32_t private_segment_size,
                           uint32_t group_segment_size,
                           core::Queue** queue) override;

  __forceinline HSAuint32 node_id() const { return node_id_; }

  __forceinline size_t num_cache() const { return cache_props_.size(); }

  __forceinline const HsaCacheProperties& cache_prop(int idx) const {
    return cache_props_[idx];
  }

  const std::vector<const core::MemoryRegion*>& regions() const override {
    return regions_;
  }

 private:
  hsa_status_t VisitRegion(
      const std::vector<const core::MemoryRegion*>& regions,
      hsa_status_t (*callback)(hsa_region_t region, void* data),
      void* data) const;

  const HSAuint32 node_id_;

  const HsaNodeProperties properties_;

  std::vector<HsaCacheProperties> cache_props_;

  std::vector<const core::MemoryRegion*> regions_;

  std::vector<const core::MemoryRegion*> peer_regions_;

  DISALLOW_COPY_AND_ASSIGN(CpuAgent);
};

}  // namespace

#endif  // header guard
