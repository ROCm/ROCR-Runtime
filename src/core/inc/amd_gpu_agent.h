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
// @brief Contains scratch memory information.
struct ScratchInfo {
  void* queue_base;
  size_t size;
  size_t size_per_thread;
  ptrdiff_t queue_process_offset;
};

// @brief Interface to represent a GPU agent.
class GpuAgentInt : public core::Agent {
 public:
  // @brief Constructor
  GpuAgentInt() : core::Agent(core::Agent::DeviceType::kAmdGpuDevice) {}

<<<<<<< HEAD
  // @brief Invoke the user provided callback for each region accessible by
  // this agent.
  //
  // @param [in] include_peer If true, the callback will be also invoked on each
  // peer memory region accessible by this agent. If false, only invoke the
  // callback on memory region owned by this agent.
  // @param [in] callback User provided callback function.
  // @param [in] data User provided pointer as input for @p callback.
  //
  // @retval ::HSA_STATUS_SUCCESS if the callback function for each traversed
  // region returns ::HSA_STATUS_SUCCESS.
=======
>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f
  virtual hsa_status_t VisitRegion(bool include_peer,
                                   hsa_status_t (*callback)(hsa_region_t region,
                                                            void* data),
                                   void* data) const = 0;

<<<<<<< HEAD
  // @brief Carve scratch memory from scratch pool.
  //
  // @param [out] scratch Structure to be populated with the carved memory
  // information.
  virtual void AcquireQueueScratch(ScratchInfo& scratch) = 0;

  // @brief Release scratch memory back to scratch pool.
  //
  // @param [in] base Address of scratch memory previously acquired with
  // call to ::AcquireQueueScratch.
  virtual void ReleaseQueueScratch(void* base) = 0;

  // @brief Translate the kernel start and end dispatch timestamp from agent
  // domain to host domain.
  //
  // @param [in] signal Pointer to signal that provides the dispatch timing.
  // @param [out] time Structure to be populated with the host domain value.
  virtual void TranslateTime(core::Signal* signal,
                             hsa_amd_profiling_dispatch_time_t& time) = 0;

  // @brief Translate timestamp agent domain to host domain.
  //
  // @param [out] time Timestamp in agent domain.
  virtual uint64_t TranslateTime(uint64_t tick) = 0;

  // @brief Sets the coherency type of this agent.
  //
  // @param [in] type New coherency type.
  //
  // @retval true The new coherency type is set successfuly.
  virtual bool current_coherency_type(hsa_amd_coherency_type_t type) = 0;

  // @brief Returns the current coherency type of this agent.
  //
  // @retval Coherency type.
  virtual hsa_amd_coherency_type_t current_coherency_type() const = 0;

  // @brief Returns node id associated with this agent.
  virtual HSAuint32 node_id() const = 0;

  // @brief Query if agent represent Kaveri GPU.
  //
  // @retval true if agent is Kaveri GPU.
  virtual bool is_kv_device() const = 0;

  // @brief Query the agent HSA profile.
  //
  // @retval HSA profile.
=======
  virtual void TranslateTime(core::Signal* signal,
                             hsa_amd_profiling_dispatch_time_t& time) = 0;

  virtual uint64_t TranslateTime(uint64_t tick) = 0;

  virtual bool current_coherency_type(hsa_amd_coherency_type_t type) = 0;

  virtual hsa_amd_coherency_type_t current_coherency_type() const = 0;

  virtual HSAuint32 node_id() const = 0;

  virtual bool is_kv_device() const = 0;

>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f
  virtual hsa_profile_t profile() const = 0;
};

class GpuAgent : public GpuAgentInt {
 public:
  // @brief GPU agent constructor.
  //
  // @param [in] node Node id. Each CPU in different socket will get distinct
  // id.
  // @param [in] node_props Node property.
  // @param [in] cache_props Array of data cache properties. The array index
  // represent the cache level.
  // @param [in] HSA profile of the agent.
  GpuAgent(HSAuint32 node, const HsaNodeProperties& node_props,
           const std::vector<HsaCacheProperties>& cache_props,
           hsa_profile_t profile);

  // @brief GPU agent destructor.
  ~GpuAgent();

<<<<<<< HEAD
  // @brief Initialize DMA queue.
  //
  // @retval HSA_STATUS_SUCCESS DMA queue initialization is successful.
  hsa_status_t InitDma();

  // @brief Add a region object to ::regions_ if @p region is owned by this
  // agent. Add a region object to ::peer_regions_ if @p region is owned by
  // the peer agent.
  //
  // @param [in] region Region object to be added into the list.
  void RegisterMemoryProperties(core::MemoryRegion& region);

  uint16_t GetMicrocodeVersion() const;
=======
  hsa_status_t InitDma();

  void RegisterMemoryProperties(core::MemoryRegion& region);

  hsa_status_t VisitRegion(bool include_peer,
                           hsa_status_t (*callback)(hsa_region_t region,
                                                    void* data),
                           void* data) const override;
>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f

  // @brief Assembles SP3 shader source into executable code.
  //
  // @param [in] src_sp3 SP3 shader source text representation.
  // @param [in] func_name Name of the SP3 function to assemble.
  // @param [out] code_buf Executable code buffer.
  // @param [out] code_buf_size Size of executable code buffer in bytes.
  void AssembleShader(const char* src_sp3, const char* func_name,
                      void*& code_buf, size_t& code_buf_size);

  // @brief Frees executable code created by AssembleShader.
  //
  // @param [in] code_buf Executable code buffer.
  // @param [in] code_buf_size Size of executable code buffer in bytes.
  void ReleaseShader(void* code_buf, size_t code_buf_size);

  // @brief Override from core::Agent.
  hsa_status_t VisitRegion(bool include_peer,
                           hsa_status_t (*callback)(hsa_region_t region,
                                                    void* data),
                           void* data) const override;

  // @brief Override from core::Agent.
  hsa_status_t IterateRegion(hsa_status_t (*callback)(hsa_region_t region,
                                                      void* data),
                             void* data) const override;

<<<<<<< HEAD
  // @brief Override from core::Agent.
=======
>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f
  hsa_status_t DmaCopy(void* dst, const void* src, size_t size) override;

  // @brief Override from core::Agent.
  hsa_status_t DmaCopy(void* dst, const void* src, size_t size,
                       std::vector<core::Signal*>& dep_signals,
                       core::Signal& out_signal) override;

<<<<<<< HEAD
  // @brief Override from core::Agent.
  hsa_status_t DmaFill(void* ptr, uint32_t value, size_t count) override;

  // @brief Override from core::Agent.
  hsa_status_t GetInfo(hsa_agent_info_t attribute, void* value) const override;

  // @brief Override from core::Agent.
=======
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
>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f
  hsa_status_t QueueCreate(size_t size, hsa_queue_type_t queue_type,
                           core::HsaEventCallback event_callback, void* data,
                           uint32_t private_segment_size,
                           uint32_t group_segment_size,
                           core::Queue** queue) override;
<<<<<<< HEAD

  // @brief Override from amd::GpuAgentInt.
  void AcquireQueueScratch(ScratchInfo& scratch) override;

  // @brief Override from amd::GpuAgentInt.
  void ReleaseQueueScratch(void* base) override;

  // @brief Override from amd::GpuAgentInt.
  void TranslateTime(core::Signal* signal,
                     hsa_amd_profiling_dispatch_time_t& time) override;
=======

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
>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f

  // @brief Override from amd::GpuAgentInt.
  uint64_t TranslateTime(uint64_t tick) override;

<<<<<<< HEAD
  // @brief Override from amd::GpuAgentInt.
  bool current_coherency_type(hsa_amd_coherency_type_t type) override;

  // @brief Override from amd::GpuAgentInt.
=======
  bool current_coherency_type(hsa_amd_coherency_type_t type) override;

>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f
  hsa_amd_coherency_type_t current_coherency_type() const override {
    return current_coherency_type_;
  }

<<<<<<< HEAD
  // Getter & setters.
=======
  __forceinline HSAuint32 node_id() const override { return node_id_; }
>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f

  // @brief Returns node property.
  __forceinline const HsaNodeProperties& properties() const {
    return properties_;
  }

  // @brief Returns number of data caches.
  __forceinline size_t num_cache() const { return cache_props_.size(); }

  // @brief Returns data cache property.
  //
  // @param [in] idx Cache level.
  __forceinline const HsaCacheProperties& cache_prop(int idx) const {
    return cache_props_[idx];
  }

<<<<<<< HEAD
  // @brief Override from core::Agent.
=======
  __forceinline bool is_kv_device() const override { return is_kv_device_; }

  __forceinline hsa_profile_t profile() const override { return profile_; }

>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f
  const std::vector<const core::MemoryRegion*>& regions() const override {
    return regions_;
  }

  // @brief Override from amd::GpuAgentInt.
  __forceinline HSAuint32 node_id() const override { return node_id_; }

  // @brief Override from amd::GpuAgentInt.
  __forceinline bool is_kv_device() const override { return is_kv_device_; }

  // @brief Override from amd::GpuAgentInt.
  __forceinline hsa_profile_t profile() const override { return profile_; }

 protected:
  hsa_status_t VisitRegion(
      const std::vector<const core::MemoryRegion*>& regions,
      hsa_status_t (*callback)(hsa_region_t region, void* data),
      void* data) const;

  static const uint32_t minAqlSize_ = 0x1000;   // 4KB min
  static const uint32_t maxAqlSize_ = 0x20000;  // 8MB max

  // @brief Invoke the user provided callback for every region in @p regions.
  //
  // @param [in] regions Array of region object.
  // @param [in] callback User provided callback function.
  // @param [in] data User provided pointer as input for @p callback.
  //
  // @retval ::HSA_STATUS_SUCCESS if the callback function for each traversed
  // region returns ::HSA_STATUS_SUCCESS.
  hsa_status_t VisitRegion(
      const std::vector<const core::MemoryRegion*>& regions,
      hsa_status_t (*callback)(hsa_region_t region, void* data),
      void* data) const;

  // @brief Update ::t1_ tick count.
  void SyncClocks();

  // @brief Binds the second-level trap handler to this node.
  void BindTrapHandler();

  // @brief Distinct id for a given GPU node.
  const HSAuint32 node_id_;

  // @brief Node properties.
  const HsaNodeProperties properties_;

  // @brief Current coherency type.
  hsa_amd_coherency_type_t current_coherency_type_;

  // @brief Maximum number of queues that can be created.
  uint32_t max_queues_;

  // @brief Object to manage scratch memory.
  SmallHeap scratch_pool_;

  // @brief Default scratch size per queue.
  size_t queue_scratch_len_;

  // @brief Default scratch size per work item.
  size_t scratch_per_thread_;

  // @brief Blit object to handle memory copy/fill.
  core::Blit* blit_;

  // @brief Mutex to protect the update to coherency type.
  KernelMutex coherency_lock_;

  // @brief Mutex to protect access to scratch pool.
  KernelMutex scratch_lock_;
  
  // @brief Mutex to protect access to ::t1_.
  KernelMutex t1_lock_;

  // @brief GPU tick on initialization.
  HsaClockCounters t0_;

  HsaClockCounters t1_;

  // @brief Array of GPU cache property.
  std::vector<HsaCacheProperties> cache_props_;

  // @brief Array of regions owned by this agent.
  std::vector<const core::MemoryRegion*> regions_;

<<<<<<< HEAD
  // @brief Array of regions owned by peer agents and accessible by this agent.
  std::vector<const core::MemoryRegion*> peer_regions_;
=======
  std::vector<const core::MemoryRegion*> peer_regions_;

  bool is_kv_device_;
>>>>>>> 85ad07b87d1513e094d206ed8d5f49946f86991f

  // @brief HSA profile.
  hsa_profile_t profile_;

  bool is_kv_device_;

  void* trap_code_buf_;

  size_t trap_code_buf_size_;

 private:
   // @brief Alternative aperture base address. Only on KV.
  uintptr_t ape1_base_;

  // @brief Alternative aperture size. Only on KV.
  size_t ape1_size_;

  DISALLOW_COPY_AND_ASSIGN(GpuAgent);
};

}  // namespace

#endif  // header guard
