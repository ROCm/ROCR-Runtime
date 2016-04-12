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
  GpuAgentInt(uint32_t node_id)
      : core::Agent(node_id, core::Agent::DeviceType::kAmdGpuDevice) {}

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
  virtual hsa_status_t VisitRegion(bool include_peer,
                                   hsa_status_t (*callback)(hsa_region_t region,
                                                            void* data),
                                   void* data) const = 0;

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

  // @brief Query if agent represent Kaveri GPU.
  //
  // @retval true if agent is Kaveri GPU.
  virtual bool is_kv_device() const = 0;

  // @brief Query the agent HSA profile.
  //
  // @retval HSA profile.
  virtual hsa_profile_t profile() const = 0;

  // @brief Query the agent memory bus width in bit.
  //
  // @retval Bus width in bit.
  virtual uint32_t memory_bus_width() const = 0;

  // @brief Query the agent memory maximum frequency in MHz.
  //
  // @retval Bus width in MHz.
  virtual uint32_t memory_max_frequency() const = 0;
};

class GpuAgent : public GpuAgentInt {
 public:
  // @brief GPU agent constructor.
  //
  // @param [in] node Node id. Each CPU in different socket will get distinct
  // id.
  // @param [in] node_props Node property.
  GpuAgent(HSAuint32 node, const HsaNodeProperties& node_props);

  // @brief GPU agent destructor.
  ~GpuAgent();

  // @brief Initialize DMA queue.
  //
  // @retval HSA_STATUS_SUCCESS DMA queue initialization is successful.
  hsa_status_t InitDma();

  uint16_t GetMicrocodeVersion() const;

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

  // @brief Override from core::Agent.
  hsa_status_t DmaCopy(void* dst, const void* src, size_t size) override;

  // @brief Override from core::Agent.
  hsa_status_t DmaCopy(void* dst, core::Agent& dst_agent, const void* src,
                       core::Agent& src_agent, size_t size,
                       std::vector<core::Signal*>& dep_signals,
                       core::Signal& out_signal) override;

  // @brief Override from core::Agent.
  hsa_status_t DmaFill(void* ptr, uint32_t value, size_t count) override;

  // @brief Override from core::Agent.
  hsa_status_t GetInfo(hsa_agent_info_t attribute, void* value) const override;

  // @brief Override from core::Agent.
  hsa_status_t QueueCreate(size_t size, hsa_queue_type_t queue_type,
                           core::HsaEventCallback event_callback, void* data,
                           uint32_t private_segment_size,
                           uint32_t group_segment_size,
                           core::Queue** queue) override;

  // @brief Override from amd::GpuAgentInt.
  void AcquireQueueScratch(ScratchInfo& scratch) override;

  // @brief Override from amd::GpuAgentInt.
  void ReleaseQueueScratch(void* base) override;

  // @brief Override from amd::GpuAgentInt.
  void TranslateTime(core::Signal* signal,
                     hsa_amd_profiling_dispatch_time_t& time) override;

  // @brief Override from amd::GpuAgentInt.
  uint64_t TranslateTime(uint64_t tick) override;

  // @brief Override from amd::GpuAgentInt.
  bool current_coherency_type(hsa_amd_coherency_type_t type) override;

  // @brief Override from amd::GpuAgentInt.
  hsa_amd_coherency_type_t current_coherency_type() const override {
    return current_coherency_type_;
  }

  // Getter & setters.

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

  // @brief Override from core::Agent.
  const std::vector<const core::MemoryRegion*>& regions() const override {
    return regions_;
  }

  // @brief OVerride from core::Agent.
  const core::Isa* isa() const override { return isa_; }

  // @brief Override from amd::GpuAgentInt.
  __forceinline bool is_kv_device() const override { return is_kv_device_; }

  // @brief Override from amd::GpuAgentInt.
  __forceinline hsa_profile_t profile() const override { return profile_; }

  // @brief Override from amd::GpuAgentInt.
  __forceinline uint32_t memory_bus_width() const override {
    return memory_bus_width_;
  }

  // @brief Override from amd::GpuAgentInt.
  __forceinline uint32_t memory_max_frequency() const override {
    return memory_max_frequency_;
  }

 protected:
  static const uint32_t minAqlSize_ = 0x1000;   // 4KB min
  static const uint32_t maxAqlSize_ = 0x20000;  // 8MB max

  // @brief Create SDMA blit object.
  //
  // @retval NULL if SDMA blit creation and initialization failed.
  core::Blit* CreateBlitSdma();

  // @brief Create Kernel blit object.
  //
  // @retval NULL if Kernel blit creation and initialization failed.
  core::Blit* CreateBlitKernel();

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

  // @brief Blit object to handle memory copy from system to device memory.
  core::Blit* blit_h2d_;

  // @brief Blit object to handle memory copy from device to system, device to
  // device, and memory fill.
  core::Blit* blit_d2h_;

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

  core::Isa* isa_;

  // @brief HSA profile.
  hsa_profile_t profile_;

  bool is_kv_device_;

  void* trap_code_buf_;

  size_t trap_code_buf_size_;

  // @brief The GPU memory bus width in bit.
  uint32_t memory_bus_width_;

  // @brief The GPU memory maximum frequency in MHz.
  uint32_t memory_max_frequency_;

 private:
  // @brief Query the driver to get the region list owned by this agent.
  void InitRegionList();

  // @brief Reserve memory for scratch pool to be used by AQL queue of this
  // agent.
  void InitScratchPool();

  // @brief Query the driver to get the cache properties.
  void InitCacheList();

  // @brief Alternative aperture base address. Only on KV.
  uintptr_t ape1_base_;

  // @brief Alternative aperture size. Only on KV.
  size_t ape1_size_;

  DISALLOW_COPY_AND_ASSIGN(GpuAgent);
};

}  // namespace

#endif  // header guard
