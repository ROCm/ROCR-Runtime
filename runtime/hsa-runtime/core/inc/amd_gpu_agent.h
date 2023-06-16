////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
// 
// Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
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
#include <map>

#include "hsakmt/hsakmt.h"

#include "core/inc/runtime.h"
#include "core/inc/agent.h"
#include "core/inc/blit.h"
#include "core/inc/signal.h"
#include "core/inc/cache.h"
#include "core/inc/scratch_cache.h"
#include "core/util/small_heap.h"
#include "core/util/locks.h"
#include "core/util/lazy_ptr.h"

namespace rocr {
namespace AMD {
class MemoryRegion;

typedef ScratchCache::ScratchInfo ScratchInfo;

// @brief Interface to represent a GPU agent.
class GpuAgentInt : public core::Agent {
 public:
  // @brief Constructor
  GpuAgentInt(uint32_t node_id)
      : core::Agent(node_id,core::Agent::DeviceType::kAmdGpuDevice) {}

  // @brief Ensure blits are ready (performance hint).
  virtual void PreloadBlits() {}

  // @brief Initialization hook invoked after tools library has loaded,
  // to allow tools interception of interface functions.
  //
  // @retval HSA_STATUS_SUCCESS if initialization is successful.
  virtual hsa_status_t PostToolsInit() = 0;

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
  // @param [in/out] scratch Structure to be populated with the carved memory
  // information.
  virtual void AcquireQueueScratch(ScratchInfo& scratch) = 0;

  // @brief Release scratch memory back to scratch pool.
  //
  // @param [in/out] scratch Scratch memory previously acquired with call to
  // ::AcquireQueueScratch.
  virtual void ReleaseQueueScratch(ScratchInfo& base) = 0;

  // @brief Translate the kernel start and end dispatch timestamp from agent
  // domain to host domain.
  //
  // @param [in] signal Pointer to signal that provides the dispatch timing.
  // @param [out] time Structure to be populated with the host domain value.
  virtual void TranslateTime(core::Signal* signal,
                             hsa_amd_profiling_dispatch_time_t& time) = 0;

  // @brief Translate the async copy start and end timestamp from agent
  // domain to host domain.
  //
  // @param [in] signal Pointer to signal that provides the async copy timing.
  // @param [out] time Structure to be populated with the host domain value.
  virtual void TranslateTime(core::Signal* signal, hsa_amd_profiling_async_copy_time_t& time) = 0;

  // @brief Translate timestamp agent domain to host domain.
  //
  // @param [out] time Timestamp in agent domain.
  virtual uint64_t TranslateTime(uint64_t tick) = 0;

  // @brief Invalidate caches on the agent which may hold code object data.
  virtual void InvalidateCodeCaches() = 0;

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
  // @param [in] xnack_mode XNACK mode of device.
  GpuAgent(HSAuint32 node, const HsaNodeProperties& node_props, bool xnack_mode, uint32_t index);

  // @brief GPU agent destructor.
  ~GpuAgent();

  // @brief Ensure blits are ready (performance hint).
  void PreloadBlits() override;

  // @brief Override from core::Agent.
  hsa_status_t PostToolsInit() override;

  uint16_t GetMicrocodeVersion() const;

  uint16_t GetSdmaMicrocodeVersion() const;

  // @brief Assembles SP3 shader source into ISA or AQL code object.
  //
  // @param [in] src_sp3 SP3 shader source text representation.
  // @param [in] func_name Name of the SP3 function to assemble.
  // @param [in] assemble_target ISA or AQL assembly target.
  // @param [out] code_buf Code object buffer.
  // @param [out] code_buf_size Size of code object buffer in bytes.
  enum class AssembleTarget { ISA, AQL };

  void AssembleShader(const char* func_name, AssembleTarget assemble_target, void*& code_buf,
                      size_t& code_buf_size) const;

  // @brief Frees code object created by AssembleShader.
  //
  // @param [in] code_buf Code object buffer.
  // @param [in] code_buf_size Size of code object buffer in bytes.
  void ReleaseShader(void* code_buf, size_t code_buf_size) const;

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
  hsa_status_t IterateCache(hsa_status_t (*callback)(hsa_cache_t cache, void* data),
                            void* value) const override;

  // @brief Override from core::Agent.
  hsa_status_t DmaCopy(void* dst, const void* src, size_t size) override;

  // @brief Override from core::Agent.
  hsa_status_t DmaCopy(void* dst, core::Agent& dst_agent, const void* src,
                       core::Agent& src_agent, size_t size,
                       std::vector<core::Signal*>& dep_signals,
                       core::Signal& out_signal) override;

  // @brief Override from core::Agent.
  hsa_status_t DmaCopyOnEngine(void* dst, core::Agent& dst_agent, const void* src,
                       core::Agent& src_agent, size_t size,
                       std::vector<core::Signal*>& dep_signals,
                       core::Signal& out_signal, int engine_offset,
                       bool force_copy_on_sdma) override;

  // @brief Override from core::Agent.
  hsa_status_t DmaCopyStatus(core::Agent& dst_agent, core::Agent& src_agent,
                             uint32_t *engine_ids_mask) override;

  // @brief Override from core::Agent.
  hsa_status_t DmaCopyRect(const hsa_pitched_ptr_t* dst, const hsa_dim3_t* dst_offset,
                           const hsa_pitched_ptr_t* src, const hsa_dim3_t* src_offset,
                           const hsa_dim3_t* range, hsa_amd_copy_direction_t dir,
                           std::vector<core::Signal*>& dep_signals, core::Signal& out_signal);

  // @brief Override from core::Agent.
  hsa_status_t DmaFill(void* ptr, uint32_t value, size_t count) override;

  // @brief Override from core::Agent.
  hsa_status_t GetInfo(hsa_agent_info_t attribute, void* value) const override;

  // @brief Override from core::Agent.
  hsa_status_t QueueCreate(size_t size, hsa_queue_type32_t queue_type,
                           core::HsaEventCallback event_callback, void* data,
                           uint32_t private_segment_size,
                           uint32_t group_segment_size,
                           core::Queue** queue) override;

  // @brief Decrement GWS ref count.
  void GWSRelease();

  // @brief Override from AMD::GpuAgentInt.
  void AcquireQueueScratch(ScratchInfo& scratch) override;

  // @brief Override from AMD::GpuAgentInt.
  void ReleaseQueueScratch(ScratchInfo& scratch) override;

  // @brief Override from AMD::GpuAgentInt.
  void TranslateTime(core::Signal* signal, hsa_amd_profiling_dispatch_time_t& time) override;

  // @brief Override from AMD::GpuAgentInt.
  void TranslateTime(core::Signal* signal, hsa_amd_profiling_async_copy_time_t& time) override;

  // @brief Override from AMD::GpuAgentInt.
  uint64_t TranslateTime(uint64_t tick) override;

  // @brief Override from AMD::GpuAgentInt.
  void InvalidateCodeCaches() override;

  // @brief Override from AMD::GpuAgentInt.
  bool current_coherency_type(hsa_amd_coherency_type_t type) override;

  hsa_amd_coherency_type_t current_coherency_type() const override {
    return current_coherency_type_;
  }

  // Getter & setters.

  // @brief Returns Hive ID
  __forceinline uint64_t HiveId() const override { return  properties_.HiveID; }

  // @brief Returns KFD's GPU id which is a hash used internally.
  __forceinline uint64_t KfdGpuID() const { return properties_.KFDGpuID; }

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

  // @brief Override from core::Agent.
  const core::Isa* isa() const override { return isa_; }

  // @brief Override from AMD::GpuAgentInt.
  __forceinline bool is_kv_device() const override { return is_kv_device_; }

  // @brief Override from AMD::GpuAgentInt.
  __forceinline hsa_profile_t profile() const override { return profile_; }

  // @brief Override from AMD::GpuAgentInt.
  __forceinline uint32_t memory_bus_width() const override {
    return memory_bus_width_;
  }

  // @brief Override from AMD::GpuAgentInt.
  __forceinline uint32_t memory_max_frequency() const override {
    return memory_max_frequency_;
  }

  // @brief Order the device is surfaced in hsa_iterate_agents counting only
  // GPU devices.
  __forceinline uint32_t enumeration_index() const { return enum_index_; }

  // @brief returns true if agent uses MES scheduler
  __forceinline const bool isMES() const { return (isa_->GetMajorVersion() >= 11) ? true : false; };

  void ReserveScratch();

  void Trim() override;

  const std::function<void*(size_t size, size_t align, core::MemoryRegion::AllocateFlags flags)>&
  system_allocator() const {
    return system_allocator_;
  }

  const std::function<void(void*)>& system_deallocator() const { return system_deallocator_; }

 protected:
  // Sizes are in packets.
  static const uint32_t minAqlSize_ = 0x40;     // 4KB min
  static const uint32_t maxAqlSize_ = 0x20000;  // 8MB max

  // @brief Create an internal queue allowing tools to be notified.
  core::Queue* CreateInterceptibleQueue() {
    return CreateInterceptibleQueue(core::Queue::DefaultErrorHandler, nullptr);
  }

  // @brief Create an internal queue, with a custom error handler, allowing tools to be
  // notified.
  core::Queue* CreateInterceptibleQueue(void (*callback)(hsa_status_t status, hsa_queue_t* source,
                                                         void* data),
                                        void* data);

  // @brief Create SDMA blit object.
  //
  // @retval NULL if SDMA blit creation and initialization failed.
  core::Blit* CreateBlitSdma(bool use_xgmi);

  // @brief Create Kernel blit object using provided compute queue.
  //
  // @retval NULL if Kernel blit creation and initialization failed.
  core::Blit* CreateBlitKernel(core::Queue* queue);

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

  // @brief Override from core::Agent.
  hsa_status_t EnableDmaProfiling(bool enable) override;

  // @brief Node properties.
  const HsaNodeProperties properties_;

  // @brief Current coherency type.
  hsa_amd_coherency_type_t current_coherency_type_;

  // @brief Maximum number of queues that can be created.
  uint32_t max_queues_;

  // @brief Object to manage scratch memory.
  SmallHeap scratch_pool_;

  // @brief Current short duration scratch memory size.
  size_t scratch_used_large_;

  // @brief Notifications for scratch release.
  std::map<hsa_signal_t, hsa_signal_value_t> scratch_notifiers_;

  // @brief Default scratch size per queue.
  size_t queue_scratch_len_;

  // @brief Default scratch size per work item.
  size_t scratch_per_thread_;

  // @brief Blit interfaces for each data path.
  enum BlitEnum { BlitDevToDev, BlitHostToDev, BlitDevToHost, DefaultBlitCount };

  // Blit objects managed by an instance of GpuAgent
  std::vector<lazy_ptr<core::Blit>> blits_;

  // List of agents connected via xGMI
  std::vector<const core::Agent*> xgmi_peer_list_;

  // Protects xgmi_peer_list_
  KernelMutex xgmi_peer_list_lock_;

  // @brief AQL queues for cache management and blit compute usage.
  enum QueueEnum {
    QueueUtility,   // Cache management and device to {host,device} blit compute
    QueueBlitOnly,  // Host to device blit
    QueueCount
  };

  lazy_ptr<core::Queue> queues_[QueueCount];

  // @brief Mutex to protect the update to coherency type.
  KernelMutex coherency_lock_;

  // @brief Mutex to protect access to scratch pool.
  KernelMutex scratch_lock_;

  // @brief Mutex to protect access to ::t1_.
  KernelMutex t1_lock_;

  // @brief Mutex to protect access to blit objects.
  KernelMutex blit_lock_;

  // @brief GPU tick on initialization.
  HsaClockCounters t0_;

  HsaClockCounters t1_;

  double historical_clock_ratio_;

  // @brief s_memrealtime nominal clock frequency
  uint64_t wallclock_frequency_;

  // @brief Array of GPU cache property.
  std::vector<HsaCacheProperties> cache_props_;

  // @brief Array of HSA cache objects.
  std::vector<std::unique_ptr<core::Cache>> caches_;

  // @brief Array of regions owned by this agent.
  std::vector<const core::MemoryRegion*> regions_;

  core::Isa* isa_;

  // @brief HSA profile.
  hsa_profile_t profile_;

  bool is_kv_device_;

  void* trap_code_buf_;

  size_t trap_code_buf_size_;

  // @brief Mappings from doorbell index to queue, for trap handler.
  // Correlates with output of s_sendmsg(MSG_GET_DOORBELL) for queue identification.
  amd_queue_t** doorbell_queue_map_;

  // @brief The GPU memory bus width in bit.
  uint32_t memory_bus_width_;

  // @brief The GPU memory maximum frequency in MHz.
  uint32_t memory_max_frequency_;

  // @brief Enumeration index
  uint32_t enum_index_;

  // @brief HDP flush registers
  hsa_amd_hdp_flush_t HDP_flush_ = {nullptr, nullptr};

 private:
  // @brief Query the driver to get the region list owned by this agent.
  void InitRegionList();

  // @brief Reserve memory for scratch pool to be used by AQL queue of this
  // agent.
  void InitScratchPool();

  // @brief Query the driver to get the cache properties.
  void InitCacheList();

  // @brief Create internal queues and blits.
  void InitDma();

  // @brief Setup GWS accessing queue.
  void InitGWS();

  // @brief Setup NUMA aware system memory allocator.
  void InitNumaAllocator();

  // @brief Register signal for notification when scratch may become available.
  // @p signal is notified by OR'ing with @p value.
  bool AddScratchNotifier(hsa_signal_t signal, hsa_signal_value_t value) {
    if (signal.handle != 0) return false;
    scratch_notifiers_[signal] = value;
    return true;
  }

  // @brief Deregister scratch notification signals.
  void ClearScratchNotifiers() { scratch_notifiers_.clear(); }

  // @brief Releases scratch back to the driver.
  // caller must hold scratch_lock_.
  void ReleaseScratch(void* base, size_t size, bool large);

  // Bind index of peer device that is connected via xGMI links
  lazy_ptr<core::Blit>& GetXgmiBlit(const core::Agent& peer_agent);

  // Bind the Blit object that will drive the copy operation
  // across PCIe links (H2D or D2H) or is within same device D2D
  lazy_ptr<core::Blit>& GetPcieBlit(const core::Agent& dst_agent, const core::Agent& src_agent);

  // Bind the Blit object that will drive the copy operation
  lazy_ptr<core::Blit>& GetBlitObject(const core::Agent& dst_agent, const core::Agent& src_agent,
                                      const size_t size);

  // Bind the Blit object that will drive the copy operation by engine ID
  lazy_ptr<core::Blit>& GetBlitObject(uint32_t engine_id);

  // @brief Alternative aperture base address. Only on KV.
  uintptr_t ape1_base_;

  // @brief Alternative aperture size. Only on KV.
  size_t ape1_size_;

  // @brief Queue with GWS access.
  struct {
    lazy_ptr<core::Queue> queue_;
    int ref_ct_;
    KernelMutex lock_;
  } gws_queue_;

  // Sets and Tracks pending SDMA status check or request counts
  void SetCopyRequestRefCount(bool set);
  void SetCopyStatusCheckRefCount(bool set);
  int pending_copy_req_ref_;
  int pending_copy_stat_check_ref_;

  // Tracks what SDMA blits have been used since initialization.
  uint32_t sdma_blit_used_mask_;

  ScratchCache scratch_cache_;

  // System memory allocator in the nearest NUMA node.
  std::function<void*(size_t size, size_t align, core::MemoryRegion::AllocateFlags flags)>
      system_allocator_;

  std::function<void(void*)> system_deallocator_;

  DISALLOW_COPY_AND_ASSIGN(GpuAgent);

  // Check if SDMA engine by ID is free
  bool DmaEngineIsFree(uint32_t engine_id);
};

}  // namespace amd
}  // namespace rocr

#endif  // header guard
