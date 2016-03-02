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

#ifndef HSA_RUNTIME_CORE_INC_AMD_GPU_AGENT_H_
#define HSA_RUNTIME_CORE_INC_AMD_GPU_AGENT_H_

#include <vector>

#include "hsakmt.h"

#include "core/inc/runtime.h"
#include "core/inc/agent.h"
#include "core/inc/blit.h"
#include "core/inc/signal.h"
#include "core/util/small_heap.h"
#include "core/util/locks.h"

namespace amd {

struct ScratchInfo {
  void* queue_base;
  size_t size;
  size_t size_per_thread;
  ptrdiff_t queue_process_offset;
};

class GpuAgentInt : public core::Agent {
 public:
  GpuAgentInt() : core::Agent(core::Agent::DeviceType::kAmdGpuDevice) {}

  virtual hsa_status_t VisitRegion(bool include_peer,
                                   hsa_status_t (*callback)(hsa_region_t region,
                                                            void* data),
                                   void* data) const = 0;

  virtual void TranslateTime(core::Signal* signal,
                             hsa_amd_profiling_dispatch_time_t& time) = 0;

  virtual uint64_t TranslateTime(uint64_t tick) = 0;

  virtual bool current_coherency_type(hsa_amd_coherency_type_t type) = 0;

  virtual hsa_amd_coherency_type_t current_coherency_type() const = 0;

  virtual HSAuint32 node_id() const = 0;

  virtual bool is_kv_device() const = 0;

  virtual hsa_profile_t profile() const = 0;
};

class GpuAgent : public GpuAgentInt {
 public:
  GpuAgent(HSAuint32 node, const HsaNodeProperties& node_props,
           const std::vector<HsaCacheProperties>& cache_props,
           hsa_profile_t profile);

  ~GpuAgent();

  hsa_status_t InitDma();

  void RegisterMemoryProperties(core::MemoryRegion& region);

  hsa_status_t VisitRegion(bool include_peer,
                           hsa_status_t (*callback)(hsa_region_t region,
                                                    void* data),
                           void* data) const override;

  hsa_status_t IterateRegion(hsa_status_t (*callback)(hsa_region_t region,
                                                      void* data),
                             void* data) const override;

  hsa_status_t DmaCopy(void* dst, const void* src, size_t size) override;

  hsa_status_t DmaCopy(void* dst, const void* src, size_t size,
                       std::vector<core::Signal*>& dep_signals,
                       core::Signal& out_signal) override;

  hsa_status_t DmaFill(void* ptr, uint32_t value, size_t count) override;

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

  void AcquireQueueScratch(ScratchInfo& scratch) {
    if (scratch.size == 0) {
      scratch.size = queue_scratch_len_;
      scratch.size_per_thread = scratch_per_thread_;
    }
    ScopedAcquire<KernelMutex> lock(&sclock_);
    scratch.queue_base = scratch_pool_.alloc(scratch.size);
    scratch.queue_process_offset =
        uintptr_t(scratch.queue_base) - uintptr_t(scratch_pool_.base());

    if ((scratch.queue_base != NULL) && (profile_ == HSA_PROFILE_BASE)) {
      HSAuint64 alternate_va;
      if (HSAKMT_STATUS_SUCCESS != hsaKmtMapMemoryToGPU(scratch.queue_base,
                                                        scratch.size,
                                                        &alternate_va)) {
        assert(false && "Map scratch subrange failed!");
        scratch_pool_.free(scratch.queue_base);
        scratch.queue_base = NULL;
      }
    }
  }

  void ReleaseQueueScratch(void* base) {
    if (base == NULL) {
      return;
    }

    ScopedAcquire<KernelMutex> lock(&sclock_);
    if (profile_ == HSA_PROFILE_BASE) {
      if (HSAKMT_STATUS_SUCCESS != hsaKmtUnmapMemoryToGPU(base)) {
        assert(false && "Unmap scratch subrange failed!");
      }
    }
    scratch_pool_.free(base);
  }

  void TranslateTime(core::Signal* signal,
                     hsa_amd_profiling_dispatch_time_t& time) override;

  uint64_t TranslateTime(uint64_t tick) override;

  uint16_t GetMicrocodeVersion() const;

  bool current_coherency_type(hsa_amd_coherency_type_t type) override;

  hsa_amd_coherency_type_t current_coherency_type() const override {
    return current_coherency_type_;
  }

  __forceinline HSAuint32 node_id() const override { return node_id_; }

  __forceinline const HsaNodeProperties& properties() const {
    return properties_;
  }

  __forceinline size_t num_cache() const { return cache_props_.size(); }

  __forceinline const HsaCacheProperties& cache_prop(int idx) const {
    return cache_props_[idx];
  }

  __forceinline bool is_kv_device() const override { return is_kv_device_; }

  __forceinline hsa_profile_t profile() const override { return profile_; }

  const std::vector<const core::MemoryRegion*>& regions() const override {
    return regions_;
  }

 protected:
  hsa_status_t VisitRegion(
      const std::vector<const core::MemoryRegion*>& regions,
      hsa_status_t (*callback)(hsa_region_t region, void* data),
      void* data) const;

  static const uint32_t minAqlSize_ = 0x1000;   // 4KB min
  static const uint32_t maxAqlSize_ = 0x20000;  // 8MB max

  void SyncClocks();

  const HSAuint32 node_id_;

  const HsaNodeProperties properties_;

  hsa_amd_coherency_type_t current_coherency_type_;

  uint32_t max_queues_;

  SmallHeap scratch_pool_;

  size_t queue_scratch_len_;

  size_t scratch_per_thread_;

  core::Blit* blit_;

  KernelMutex lock_, sclock_, t1_lock_;

  HsaClockCounters t0_, t1_;

  std::vector<HsaCacheProperties> cache_props_;

  std::vector<const core::MemoryRegion*> regions_;

  std::vector<const core::MemoryRegion*> peer_regions_;

  bool is_kv_device_;

  hsa_profile_t profile_;

 private:
  uintptr_t ape1_base_;

  size_t ape1_size_;

  DISALLOW_COPY_AND_ASSIGN(GpuAgent);
};

}  // namespace

#endif  // header guard
