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

#include "core/inc/amd_gpu_agent.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <climits>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <utility>

#include "core/inc/amd_aql_queue.h"
#include "core/inc/amd_blit_kernel.h"
#include "core/inc/amd_blit_sdma.h"
#include "core/inc/amd_gpu_pm4.h"
#include "core/inc/amd_gpu_shaders.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/interrupt_signal.h"
#include "core/inc/isa.h"
#include "core/inc/runtime.h"
#include "core/util/os.h"
#include "inc/hsa_ext_image.h"
#include "inc/hsa_ven_amd_aqlprofile.h"

// Size of scratch (private) segment pre-allocated per thread, in bytes.
#define DEFAULT_SCRATCH_BYTES_PER_THREAD 2048
#define MAX_WAVE_SCRATCH 8387584  // See COMPUTE_TMPRING_SIZE.WAVESIZE

extern core::HsaApiTable hsa_internal_api_table_;

namespace amd {
GpuAgent::GpuAgent(HSAuint32 node, const HsaNodeProperties& node_props)
    : GpuAgentInt(node),
      properties_(node_props),
      current_coherency_type_(HSA_AMD_COHERENCY_TYPE_COHERENT),
      blits_(),
      queues_(),
      local_region_(NULL),
      is_kv_device_(false),
      trap_code_buf_(NULL),
      trap_code_buf_size_(0),
      memory_bus_width_(0),
      memory_max_frequency_(0),
      ape1_base_(0),
      ape1_size_(0),
      end_ts_pool_size_(0),
      end_ts_pool_counter_(0),
      end_ts_base_addr_(NULL) {
  const bool is_apu_node = (properties_.NumCPUCores > 0);
  profile_ = (is_apu_node) ? HSA_PROFILE_FULL : HSA_PROFILE_BASE;

  HSAKMT_STATUS err = hsaKmtGetClockCounters(node_id(), &t0_);
  t1_ = t0_;
  historical_clock_ratio_ = 0.0;
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaGetClockCounters error");

  // Set instruction set architecture via node property, only on GPU device.
  isa_ = (core::Isa*)core::IsaRegistry::GetIsa(
      core::Isa::Version(node_props.EngineId.ui32.Major, node_props.EngineId.ui32.Minor,
                         node_props.EngineId.ui32.Stepping),
      profile_ == HSA_PROFILE_FULL, false);
  //Disable SRAM_ECC reporting until HCC is fixed.
  //profile_ == HSA_PROFILE_FULL, node_props.Capability.ui32.SRAM_EDCSupport == 1);

  // Check if the device is Kaveri, only on GPU device.
  if (isa_->GetMajorVersion() == 7 && isa_->GetMinorVersion() == 0 &&
      isa_->GetStepping() == 0) {
    is_kv_device_ = true;
  }

  current_coherency_type((profile_ == HSA_PROFILE_FULL)
                             ? HSA_AMD_COHERENCY_TYPE_COHERENT
                             : HSA_AMD_COHERENCY_TYPE_NONCOHERENT);

  max_queues_ = core::Runtime::runtime_singleton_->flag().max_queues();
#if !defined(HSA_LARGE_MODEL) || !defined(__linux__)
  if (max_queues_ == 0) {
    max_queues_ = 10;
  }
  max_queues_ = std::min(10U, max_queues_);
#else
  if (max_queues_ == 0) {
    max_queues_ = 128;
  }
  max_queues_ = std::min(128U, max_queues_);
#endif

  // Populate region list.
  InitRegionList();

  // Populate cache list.
  InitCacheList();
}

GpuAgent::~GpuAgent() {
  for (int i = 0; i < BlitCount; ++i) {
    if (blits_[i] != nullptr) {
      hsa_status_t status = blits_[i]->Destroy(*this);
      assert(status == HSA_STATUS_SUCCESS);
    }
  }

  if (end_ts_base_addr_ != NULL) {
    core::Runtime::runtime_singleton_->FreeMemory(end_ts_base_addr_);
  }

  if (ape1_base_ != 0) {
    _aligned_free(reinterpret_cast<void*>(ape1_base_));
  }

  if (scratch_pool_.base() != NULL) {
    hsaKmtFreeMemory(scratch_pool_.base(), scratch_pool_.size());
  }

  if (trap_code_buf_ != NULL) {
    ReleaseShader(trap_code_buf_, trap_code_buf_size_);
  }

  std::for_each(regions_.begin(), regions_.end(), DeleteObject());
  regions_.clear();
}

void GpuAgent::AssembleShader(const char* src_sp3, const char* func_name,
                              AssembleTarget assemble_target, void*& code_buf,
                              size_t& code_buf_size) const {
  // Select precompiled shader implementation from name/target.
  struct ASICShader {
    const void* code;
    size_t size;
    int num_sgprs;
    int num_vgprs;
  };

  struct CompiledShader {
    ASICShader compute_7;
    ASICShader compute_8;
    ASICShader compute_9;
  };

  std::map<std::string, CompiledShader> compiled_shaders = {
      {"TrapHandler",
       {
           {NULL, 0, 0, 0},
           {kCodeTrapHandler8, sizeof(kCodeTrapHandler8), 2, 4},
           {kCodeTrapHandler9, sizeof(kCodeTrapHandler9), 2, 4},
       }},
      {"CopyAligned",
       {
           {kCodeCopyAligned7, sizeof(kCodeCopyAligned7), 32, 12},
           {kCodeCopyAligned8, sizeof(kCodeCopyAligned8), 32, 12},
           {kCodeCopyAligned8, sizeof(kCodeCopyAligned8), 32, 12},
       }},
      {"CopyMisaligned",
       {
           {kCodeCopyMisaligned7, sizeof(kCodeCopyMisaligned7), 23, 10},
           {kCodeCopyMisaligned8, sizeof(kCodeCopyMisaligned8), 23, 10},
           {kCodeCopyMisaligned8, sizeof(kCodeCopyMisaligned8), 23, 10},
       }},
      {"Fill",
       {
           {kCodeFill7, sizeof(kCodeFill7), 19, 8},
           {kCodeFill8, sizeof(kCodeFill8), 19, 8},
           {kCodeFill8, sizeof(kCodeFill8), 19, 8},
       }}};

  auto compiled_shader_it = compiled_shaders.find(func_name);
  assert(compiled_shader_it != compiled_shaders.end() &&
         "Precompiled shader unavailable");

  ASICShader* asic_shader = NULL;

  switch (isa_->GetMajorVersion()) {
    case 7:
      asic_shader = &compiled_shader_it->second.compute_7;
      break;
    case 8:
      asic_shader = &compiled_shader_it->second.compute_8;
      break;
    case 9:
      asic_shader = &compiled_shader_it->second.compute_9;
      break;
    default:
      assert(false && "Precompiled shader unavailable for target");
  }

  // Allocate a GPU-visible buffer for the shader.
  size_t header_size =
      (assemble_target == AssembleTarget::AQL ? sizeof(amd_kernel_code_t) : 0);
  code_buf_size = AlignUp(header_size + asic_shader->size, 0x1000);

  code_buf = core::Runtime::runtime_singleton_->system_allocator()(
      code_buf_size, 0x1000, core::MemoryRegion::AllocateExecutable);
  assert(code_buf != NULL && "Code buffer allocation failed");

  memset(code_buf, 0, code_buf_size);

  // Populate optional code object header.
  if (assemble_target == AssembleTarget::AQL) {
    amd_kernel_code_t* header = reinterpret_cast<amd_kernel_code_t*>(code_buf);

    int gran_sgprs = std::max(0, (int(asic_shader->num_sgprs) - 1) / 8);
    int gran_vgprs = std::max(0, (int(asic_shader->num_vgprs) - 1) / 4);

    header->kernel_code_entry_byte_offset = sizeof(amd_kernel_code_t);
    AMD_HSA_BITS_SET(header->kernel_code_properties,
                     AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_KERNARG_SEGMENT_PTR,
                     1);
    AMD_HSA_BITS_SET(header->compute_pgm_rsrc1,
                     AMD_COMPUTE_PGM_RSRC_ONE_GRANULATED_WAVEFRONT_SGPR_COUNT,
                     gran_sgprs);
    AMD_HSA_BITS_SET(header->compute_pgm_rsrc1,
                     AMD_COMPUTE_PGM_RSRC_ONE_GRANULATED_WORKITEM_VGPR_COUNT,
                     gran_vgprs);
    AMD_HSA_BITS_SET(header->compute_pgm_rsrc1,
                     AMD_COMPUTE_PGM_RSRC_ONE_FLOAT_DENORM_MODE_16_64, 3);
    AMD_HSA_BITS_SET(header->compute_pgm_rsrc1,
                     AMD_COMPUTE_PGM_RSRC_ONE_ENABLE_IEEE_MODE, 1);
    AMD_HSA_BITS_SET(header->compute_pgm_rsrc2,
                     AMD_COMPUTE_PGM_RSRC_TWO_USER_SGPR_COUNT, 2);
    AMD_HSA_BITS_SET(header->compute_pgm_rsrc2,
                     AMD_COMPUTE_PGM_RSRC_TWO_ENABLE_SGPR_WORKGROUP_ID_X, 1);
  }

  // Copy shader code into the GPU-visible buffer.
  memcpy((void*)(uintptr_t(code_buf) + header_size), asic_shader->code,
         asic_shader->size);
}

void GpuAgent::ReleaseShader(void* code_buf, size_t code_buf_size) const {
  core::Runtime::runtime_singleton_->system_deallocator()(code_buf);
}

void GpuAgent::InitRegionList() {
  const bool is_apu_node = (properties_.NumCPUCores > 0);

  std::vector<HsaMemoryProperties> mem_props(properties_.NumMemoryBanks);
  if (HSAKMT_STATUS_SUCCESS ==
      hsaKmtGetNodeMemoryProperties(node_id(), properties_.NumMemoryBanks,
                                    &mem_props[0])) {
    for (uint32_t mem_idx = 0; mem_idx < properties_.NumMemoryBanks;
         ++mem_idx) {
      // Ignore the one(s) with unknown size.
      if (mem_props[mem_idx].SizeInBytes == 0) {
        continue;
      }

      switch (mem_props[mem_idx].HeapType) {
        case HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE:
        case HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC:
          if (!is_apu_node) {
            mem_props[mem_idx].VirtualBaseAddress = 0;
          }

          memory_bus_width_ = mem_props[mem_idx].Width;
          memory_max_frequency_ = mem_props[mem_idx].MemoryClockMax;
        case HSA_HEAPTYPE_GPU_LDS:
        case HSA_HEAPTYPE_GPU_SCRATCH: {
          MemoryRegion* region =
              new MemoryRegion(false, false, this, mem_props[mem_idx]);

          regions_.push_back(region);

          if (region->IsLocalMemory()) {
            local_region_ = region;
            // Expose VRAM as uncached/fine grain over PCIe (if enabled) or XGMI.
            if (core::Runtime::runtime_singleton_->flag().fine_grain_pcie())
              regions_.push_back(new MemoryRegion(true, false, this, mem_props[mem_idx]));
          }
          break;
        }
        case HSA_HEAPTYPE_SYSTEM:
          if (is_apu_node) {
            memory_bus_width_ = mem_props[mem_idx].Width;
            memory_max_frequency_ = mem_props[mem_idx].MemoryClockMax;
          }
          break;
        case HSA_HEAPTYPE_MMIO_REMAP:
          if (core::Runtime::runtime_singleton_->flag().fine_grain_pcie()) {
            // Remap offsets defined in kfd_ioctl.h
            HDP_flush_.HDP_MEM_FLUSH_CNTL = (uint32_t*)mem_props[mem_idx].VirtualBaseAddress;
            HDP_flush_.HDP_REG_FLUSH_CNTL = HDP_flush_.HDP_MEM_FLUSH_CNTL + 1;
          }
          break;
        default:
          continue;
      }
    }
  }
}

void GpuAgent::InitScratchPool() {
  HsaMemFlags flags;
  flags.Value = 0;
  flags.ui32.Scratch = 1;
  flags.ui32.HostAccess = 1;

  scratch_per_thread_ =
      core::Runtime::runtime_singleton_->flag().scratch_mem_size();
  if (scratch_per_thread_ == 0)
    scratch_per_thread_ = DEFAULT_SCRATCH_BYTES_PER_THREAD;

  // Scratch length is: waves/CU * threads/wave * queues * #CUs *
  // scratch/thread
  const uint32_t num_cu =
      properties_.NumFComputeCores / properties_.NumSIMDPerCU;
  queue_scratch_len_ = AlignUp(32 * 64 * num_cu * scratch_per_thread_, 65536);
  size_t max_scratch_len = queue_scratch_len_ * max_queues_;

#if defined(HSA_LARGE_MODEL) && defined(__linux__)
  // For 64-bit linux use max queues unless otherwise specified
  if ((max_scratch_len == 0) || (max_scratch_len > 4294967296)) {
    max_scratch_len = 4294967296;  // 4GB apeture max
  }
#endif

  void* scratch_base;
  HSAKMT_STATUS err =
      hsaKmtAllocMemory(node_id(), max_scratch_len, flags, &scratch_base);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaKmtAllocMemory(Scratch) failed");
  assert(IsMultipleOf(scratch_base, 0x1000) &&
         "Scratch base is not page aligned!");

  scratch_pool_. ~SmallHeap();
  if (HSAKMT_STATUS_SUCCESS == err) {
    new (&scratch_pool_) SmallHeap(scratch_base, max_scratch_len);
  } else {
    new (&scratch_pool_) SmallHeap();
  }
}

void GpuAgent::InitCacheList() {
  // Get GPU cache information.
  // Similar to getting CPU cache but here we use FComputeIdLo.
  cache_props_.resize(properties_.NumCaches);
  if (HSAKMT_STATUS_SUCCESS !=
      hsaKmtGetNodeCacheProperties(node_id(), properties_.FComputeIdLo,
                                   properties_.NumCaches, &cache_props_[0])) {
    cache_props_.clear();
  } else {
    // Only store GPU D-cache.
    for (size_t cache_id = 0; cache_id < cache_props_.size(); ++cache_id) {
      const HsaCacheType type = cache_props_[cache_id].CacheType;
      if (type.ui32.HSACU != 1 || type.ui32.Instruction == 1) {
        cache_props_.erase(cache_props_.begin() + cache_id);
        --cache_id;
      }
    }
  }

  // Update cache objects
  caches_.clear();
  caches_.resize(cache_props_.size());
  char name[64];
  GetInfo(HSA_AGENT_INFO_NAME, name);
  std::string deviceName = name;
  for (size_t i = 0; i < caches_.size(); i++)
    caches_[i].reset(new core::Cache(deviceName + " L" + std::to_string(cache_props_[i].CacheLevel),
                                     cache_props_[i].CacheLevel, cache_props_[i].CacheSize));
}

bool GpuAgent::InitEndTsPool() {
  if (HSA_PROFILE_FULL == profile_) {
    return true;
  }

  if (end_ts_base_addr_.load(std::memory_order_acquire) != NULL) {
    return true;
  }

  ScopedAcquire<KernelMutex> lock(&blit_lock_);

  if (end_ts_base_addr_.load(std::memory_order_relaxed) != NULL) {
    return true;
  }

  end_ts_pool_size_ =
      static_cast<uint32_t>((BlitSdmaBase::kQueueSize + BlitSdmaBase::kCopyPacketSize - 1) /
                            (BlitSdmaBase::kCopyPacketSize));

  // Allocate end timestamp object for both h2d and d2h DMA.
  const size_t alloc_size = 2 * end_ts_pool_size_ * kTsSize;

  core::Runtime* runtime = core::Runtime::runtime_singleton_;

  uint64_t* buff = NULL;
  if (HSA_STATUS_SUCCESS !=
      runtime->AllocateMemory(local_region_, alloc_size,
                              MemoryRegion::AllocateRestrict,
                              reinterpret_cast<void**>(&buff))) {
    return false;
  }

  end_ts_base_addr_.store(buff, std::memory_order_release);

  return true;
}

uint64_t* GpuAgent::ObtainEndTsObject() {
  if (end_ts_base_addr_ == NULL) {
    return NULL;
  }

  const uint32_t end_ts_index =
      end_ts_pool_counter_.fetch_add(1U, std::memory_order_acq_rel) %
      end_ts_pool_size_;
  const static size_t kNumU64 = kTsSize / sizeof(uint64_t);
  uint64_t* end_ts_addr = &end_ts_base_addr_[end_ts_index * kNumU64];
  assert(IsMultipleOf(end_ts_addr, kTsSize));

  return end_ts_addr;
}

hsa_status_t GpuAgent::IterateRegion(
    hsa_status_t (*callback)(hsa_region_t region, void* data),
    void* data) const {
  return VisitRegion(true, callback, data);
}

hsa_status_t GpuAgent::IterateCache(hsa_status_t (*callback)(hsa_cache_t cache, void* data),
                                    void* data) const {
  AMD::callback_t<decltype(callback)> call(callback);
  for (size_t i = 0; i < caches_.size(); i++) {
    hsa_status_t stat = call(core::Cache::Convert(caches_[i].get()), data);
    if (stat != HSA_STATUS_SUCCESS) return stat;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t GpuAgent::VisitRegion(bool include_peer,
                                   hsa_status_t (*callback)(hsa_region_t region,
                                                            void* data),
                                   void* data) const {
  if (include_peer) {
    // Only expose system, local, and LDS memory of the blit agent.
    if (this->node_id() == core::Runtime::runtime_singleton_->region_gpu()->node_id()) {
      hsa_status_t stat = VisitRegion(regions_, callback, data);
      if (stat != HSA_STATUS_SUCCESS) {
        return stat;
      }
    }

    // Also expose system regions accessible by this agent.
    hsa_status_t stat =
        VisitRegion(core::Runtime::runtime_singleton_->system_regions_fine(),
                    callback, data);
    if (stat != HSA_STATUS_SUCCESS) {
      return stat;
    }

    return VisitRegion(
        core::Runtime::runtime_singleton_->system_regions_coarse(), callback,
        data);
  }

  // Only expose system, local, and LDS memory of this agent.
  return VisitRegion(regions_, callback, data);
}

hsa_status_t GpuAgent::VisitRegion(
    const std::vector<const core::MemoryRegion*>& regions,
    hsa_status_t (*callback)(hsa_region_t region, void* data),
    void* data) const {
  AMD::callback_t<decltype(callback)> call(callback);
  for (const core::MemoryRegion* region : regions) {
    const amd::MemoryRegion* amd_region =
        reinterpret_cast<const amd::MemoryRegion*>(region);

    // Only expose system, local, and LDS memory.
    if (amd_region->IsSystem() || amd_region->IsLocalMemory() ||
        amd_region->IsLDS()) {
      hsa_region_t region_handle = core::MemoryRegion::Convert(region);
      hsa_status_t status = call(region_handle, data);
      if (status != HSA_STATUS_SUCCESS) {
        return status;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

core::Queue* GpuAgent::CreateInterceptibleQueue() {
  // Disabled intercept of internal queues pending tools updates.
  core::Queue* queue = nullptr;
  QueueCreate(minAqlSize_, HSA_QUEUE_TYPE_MULTI, NULL, NULL, 0, 0, &queue);
  if (queue != nullptr)
    core::Runtime::runtime_singleton_->InternalQueueCreateNotify(core::Queue::Convert(queue),
                                                                 this->public_handle());
  return queue;
}

core::Blit* GpuAgent::CreateBlitSdma(bool h2d) {
  core::Blit* sdma;

  if (isa_->GetMajorVersion() <= 8) {
    sdma = new BlitSdmaV2V3(h2d);
  } else {
    sdma = new BlitSdmaV4(h2d);
  }

  if (sdma->Initialize(*this) != HSA_STATUS_SUCCESS) {
    sdma->Destroy(*this);
    delete sdma;
    sdma = NULL;
  }

  return sdma;
}

core::Blit* GpuAgent::CreateBlitKernel(core::Queue* queue) {
  BlitKernel* kernl = new BlitKernel(queue);

  if (kernl->Initialize(*this) != HSA_STATUS_SUCCESS) {
    kernl->Destroy(*this);
    delete kernl;
    kernl = NULL;
  }

  return kernl;
}

void GpuAgent::InitDma() {
  // Setup lazy init pointers on queues and blits.
  auto queue_lambda = [this]() {
    auto ret = CreateInterceptibleQueue();
    if (ret == nullptr)
      throw AMD::hsa_exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES,
                               "Internal queue creation failed.");
    return ret;
  };
  // Dedicated compute queue for host-to-device blits.
  queues_[QueueBlitOnly].reset(queue_lambda);
  // Share utility queue with device-to-host blits.
  queues_[QueueUtility].reset(queue_lambda);

  // Decide which engine to use for blits.
  auto blit_lambda = [this](bool h2d, lazy_ptr<core::Queue>& queue) {
    const std::string& sdma_override = core::Runtime::runtime_singleton_->flag().enable_sdma();

    bool use_sdma = (isa_->GetMajorVersion() != 8);
    if (sdma_override.size() != 0) use_sdma = (sdma_override == "1");

    if (use_sdma && (HSA_PROFILE_BASE == profile_)) {
      auto ret = CreateBlitSdma(h2d);
      if (ret != nullptr) return ret;
    }

    auto ret = CreateBlitKernel((*queue).get());
    if (ret == nullptr)
      throw AMD::hsa_exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES, "Blit creation failed.");
    return ret;
  };

  blits_[BlitHostToDev].reset([blit_lambda, this]() { return blit_lambda(true, queues_[QueueBlitOnly]); });
  blits_[BlitDevToHost].reset([blit_lambda, this]() { return blit_lambda(false, queues_[QueueUtility]); });
  blits_[BlitDevToDev].reset([this]() {
    auto ret = CreateBlitKernel((*queues_[QueueUtility]).get());
    if (ret == nullptr)
      throw AMD::hsa_exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES, "Blit creation failed.");
    return ret;
  });
}

void GpuAgent::PreloadBlits() {
  blits_[BlitHostToDev].touch();
  blits_[BlitDevToHost].touch();
  blits_[BlitDevToDev].touch();
}

hsa_status_t GpuAgent::PostToolsInit() {
  // Defer memory allocation until agents have been discovered.
  InitScratchPool();
  BindTrapHandler();
  InitDma();

  return HSA_STATUS_SUCCESS;
}

hsa_status_t GpuAgent::DmaCopy(void* dst, const void* src, size_t size) {
  return blits_[BlitDevToDev]->SubmitLinearCopyCommand(dst, src, size);
}

hsa_status_t GpuAgent::DmaCopy(void* dst, core::Agent& dst_agent,
                               const void* src, core::Agent& src_agent,
                               size_t size,
                               std::vector<core::Signal*>& dep_signals,
                               core::Signal& out_signal) {
  lazy_ptr<core::Blit>& blit =
    (src_agent.device_type() == core::Agent::kAmdCpuDevice &&
     dst_agent.device_type() == core::Agent::kAmdGpuDevice)
       ? blits_[BlitHostToDev]
       : (src_agent.device_type() == core::Agent::kAmdGpuDevice &&
          dst_agent.device_type() == core::Agent::kAmdCpuDevice)
            ? blits_[BlitDevToHost]
            : (src_agent.node_id() == dst_agent.node_id())
              ? blits_[BlitDevToDev] : blits_[BlitDevToHost];

  if (profiling_enabled()) {
    // Track the agent so we could translate the resulting timestamp to system
    // domain correctly.
    out_signal.async_copy_agent(core::Agent::Convert(this->public_handle()));
  }

  hsa_status_t stat = blit->SubmitLinearCopyCommand(dst, src, size, dep_signals, out_signal);

  return stat;
}

hsa_status_t GpuAgent::DmaCopyRect(const hsa_pitched_ptr_t* dst, const hsa_dim3_t* dst_offset,
                                   const hsa_pitched_ptr_t* src, const hsa_dim3_t* src_offset,
                                   const hsa_dim3_t* range, hsa_amd_copy_direction_t dir,
                                   std::vector<core::Signal*>& dep_signals,
                                   core::Signal& out_signal) {
  if (isa_->GetMajorVersion() < 9) return HSA_STATUS_ERROR_INVALID_AGENT;

  lazy_ptr<core::Blit>& blit =
      (dir == hsaHostToDevice) ? blits_[BlitHostToDev] : blits_[BlitDevToHost];

  if (!blit->isSDMA()) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;

  if (profiling_enabled()) {
    // Track the agent so we could translate the resulting timestamp to system
    // domain correctly.
    out_signal.async_copy_agent(core::Agent::Convert(this->public_handle()));
  }

  BlitSdmaBase* sdmaBlit = static_cast<BlitSdmaBase*>((*blit).get());
  hsa_status_t stat = sdmaBlit->SubmitCopyRectCommand(dst, dst_offset, src, src_offset, range,
                                                      dep_signals, out_signal);

  return stat;
}

hsa_status_t GpuAgent::DmaFill(void* ptr, uint32_t value, size_t count) {
  return blits_[BlitDevToDev]->SubmitLinearFillCommand(ptr, value, count);
}

hsa_status_t GpuAgent::EnableDmaProfiling(bool enable) {
  if (enable && !InitEndTsPool()) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  for (int i = 0; i < BlitCount; ++i) {
    if (blits_[i].created()) {
      const hsa_status_t stat = blits_[i]->EnableProfiling(enable);
      if (stat != HSA_STATUS_SUCCESS) {
        return stat;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t GpuAgent::GetInfo(hsa_agent_info_t attribute, void* value) const {
  
  // agent, and vendor name size limit
  const size_t attribute_u = static_cast<size_t>(attribute);
  
  switch (attribute_u) {
    
    // Build agent name by concatenating the Major, Minor and Stepping Ids
    // of devices compute capability with a prefix of "gfx"
    case HSA_AGENT_INFO_NAME: {
      std::stringstream name;
      std::memset(value, 0, HSA_PUBLIC_NAME_SIZE);
      char* temp = reinterpret_cast<char*>(value);
      name << "gfx" << isa_->GetMajorVersion() << isa_->GetMinorVersion() << isa_->GetStepping();
      std::strcpy(temp, name.str().c_str());
      break;
    }
    case HSA_AGENT_INFO_VENDOR_NAME:
      std::memset(value, 0, HSA_PUBLIC_NAME_SIZE);
      std::memcpy(value, "AMD", sizeof("AMD"));
      break;
    case HSA_AGENT_INFO_FEATURE:
      *((hsa_agent_feature_t*)value) = HSA_AGENT_FEATURE_KERNEL_DISPATCH;
      break;
    case HSA_AGENT_INFO_MACHINE_MODEL:
#if defined(HSA_LARGE_MODEL)
      *((hsa_machine_model_t*)value) = HSA_MACHINE_MODEL_LARGE;
#else
      *((hsa_machine_model_t*)value) = HSA_MACHINE_MODEL_SMALL;
#endif
      break;
    case HSA_AGENT_INFO_BASE_PROFILE_DEFAULT_FLOAT_ROUNDING_MODES:
    case HSA_AGENT_INFO_DEFAULT_FLOAT_ROUNDING_MODE:
      *((hsa_default_float_rounding_mode_t*)value) =
          HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR;
      break;
    case HSA_AGENT_INFO_FAST_F16_OPERATION:
      *((bool*)value) = false;
      break;
    case HSA_AGENT_INFO_PROFILE:
      *((hsa_profile_t*)value) = profile_;
      break;
    case HSA_AGENT_INFO_WAVEFRONT_SIZE:
      *((uint32_t*)value) = properties_.WaveFrontSize;
      break;
    case HSA_AGENT_INFO_WORKGROUP_MAX_DIM: {
      // TODO: must be per-device
      const uint16_t group_size[3] = {1024, 1024, 1024};
      std::memcpy(value, group_size, sizeof(group_size));
    } break;
    case HSA_AGENT_INFO_WORKGROUP_MAX_SIZE:
      // TODO: must be per-device
      *((uint32_t*)value) = 1024;
      break;
    case HSA_AGENT_INFO_GRID_MAX_DIM: {
      const hsa_dim3_t grid_size = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
      std::memcpy(value, &grid_size, sizeof(hsa_dim3_t));
    } break;
    case HSA_AGENT_INFO_GRID_MAX_SIZE:
      *((uint32_t*)value) = UINT32_MAX;
      break;
    case HSA_AGENT_INFO_FBARRIER_MAX_SIZE:
      // TODO: to confirm
      *((uint32_t*)value) = 32;
      break;
    case HSA_AGENT_INFO_QUEUES_MAX:
      *((uint32_t*)value) = max_queues_;
      break;
    case HSA_AGENT_INFO_QUEUE_MIN_SIZE:
      *((uint32_t*)value) = minAqlSize_;
      break;
    case HSA_AGENT_INFO_QUEUE_MAX_SIZE:
      *((uint32_t*)value) = maxAqlSize_;
      break;
    case HSA_AGENT_INFO_QUEUE_TYPE:
      *((hsa_queue_type32_t*)value) = HSA_QUEUE_TYPE_MULTI;
      break;
    case HSA_AGENT_INFO_NODE:
      // TODO: associate with OS NUMA support (numactl / GetNumaProcessorNode).
      *((uint32_t*)value) = node_id();
      break;
    case HSA_AGENT_INFO_DEVICE:
      *((hsa_device_type_t*)value) = HSA_DEVICE_TYPE_GPU;
      break;
    case HSA_AGENT_INFO_CACHE_SIZE:
      std::memset(value, 0, sizeof(uint32_t) * 4);
      // TODO: no GPU cache info from KFD. Hardcode for now.
      // GCN whitepaper: L1 data cache is 16KB.
      ((uint32_t*)value)[0] = 16 * 1024;
      break;
    case HSA_AGENT_INFO_ISA:
      *((hsa_isa_t*)value) = core::Isa::Handle(isa_);
      break;
    case HSA_AGENT_INFO_EXTENSIONS: {
      memset(value, 0, sizeof(uint8_t) * 128);

      auto setFlag = [&](uint32_t bit) {
        assert(bit < 128 * 8 && "Extension value exceeds extension bitmask");
        uint index = bit / 8;
        uint subBit = bit % 8;
        ((uint8_t*)value)[index] |= 1 << subBit;
      };

      if (core::hsa_internal_api_table_.finalizer_api.hsa_ext_program_finalize_fn != NULL) {
        setFlag(HSA_EXTENSION_FINALIZER);
      }

      if (core::hsa_internal_api_table_.image_api.hsa_ext_image_create_fn != NULL) {
        setFlag(HSA_EXTENSION_IMAGES);
      }

      if (os::LibHandle lib = os::LoadLib(kAqlProfileLib)) {
        os::CloseLib(lib);
        setFlag(HSA_EXTENSION_AMD_AQLPROFILE);
      }

      setFlag(HSA_EXTENSION_AMD_PROFILER);

      break;
    }
    case HSA_AGENT_INFO_VERSION_MAJOR:
      *((uint16_t*)value) = 1;
      break;
    case HSA_AGENT_INFO_VERSION_MINOR:
      *((uint16_t*)value) = 1;
      break;
    case HSA_EXT_AGENT_INFO_IMAGE_1D_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_1DA_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_1DB_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_2D_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_2DA_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_2DDEPTH_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_2DADEPTH_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_3D_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_ARRAY_MAX_LAYERS:
      return hsa_amd_image_get_info_max_dim(public_handle(), attribute, value);
    case HSA_EXT_AGENT_INFO_MAX_IMAGE_RD_HANDLES:
      // TODO: hardcode based on OCL constants.
      *((uint32_t*)value) = 128;
      break;
    case HSA_EXT_AGENT_INFO_MAX_IMAGE_RORW_HANDLES:
      // TODO: hardcode based on OCL constants.
      *((uint32_t*)value) = 64;
      break;
    case HSA_EXT_AGENT_INFO_MAX_SAMPLER_HANDLERS:
      // TODO: hardcode based on OCL constants.
      *((uint32_t*)value) = 16;
    case HSA_AMD_AGENT_INFO_CHIP_ID:
      *((uint32_t*)value) = properties_.DeviceId;
      break;
    case HSA_AMD_AGENT_INFO_CACHELINE_SIZE:
      // TODO: hardcode for now.
      // GCN whitepaper: cache line size is 64 byte long.
      *((uint32_t*)value) = 64;
      break;
    case HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT:
      *((uint32_t*)value) =
          (properties_.NumFComputeCores / properties_.NumSIMDPerCU);
      break;
    case HSA_AMD_AGENT_INFO_MAX_CLOCK_FREQUENCY:
      *((uint32_t*)value) = properties_.MaxEngineClockMhzFCompute;
      break;
    case HSA_AMD_AGENT_INFO_DRIVER_NODE_ID:
      *((uint32_t*)value) = node_id();
      break;
    case HSA_AMD_AGENT_INFO_MAX_ADDRESS_WATCH_POINTS:
      *((uint32_t*)value) = static_cast<uint32_t>(
          1 << properties_.Capability.ui32.WatchPointsTotalBits);
      break;
    case HSA_AMD_AGENT_INFO_BDFID:
      *((uint32_t*)value) = static_cast<uint32_t>(properties_.LocationId);
      break;
    case HSA_AMD_AGENT_INFO_MEMORY_WIDTH:
      *((uint32_t*)value) = memory_bus_width_;
      break;
    case HSA_AMD_AGENT_INFO_MEMORY_MAX_FREQUENCY:
      *((uint32_t*)value) = memory_max_frequency_;
      break;
    
    // The code copies HsaNodeProperties.MarketingName a Unicode string
    // which is encoded in UTF-16 as a 7-bit ASCII string
    case HSA_AMD_AGENT_INFO_PRODUCT_NAME: {
      std::memset(value, 0, HSA_PUBLIC_NAME_SIZE);
      char* temp = reinterpret_cast<char*>(value);
      for (uint32_t idx = 0;
           properties_.MarketingName[idx] != 0 && idx < HSA_PUBLIC_NAME_SIZE - 1; idx++) {
        temp[idx] = (uint8_t)properties_.MarketingName[idx];
      }
      break;
    }
    case HSA_AMD_AGENT_INFO_MAX_WAVES_PER_CU:
      *((uint32_t*)value) = static_cast<uint32_t>(
          properties_.NumSIMDPerCU * properties_.MaxWavesPerSIMD);
      break;
    case HSA_AMD_AGENT_INFO_NUM_SIMDS_PER_CU:
      *((uint32_t*)value) = properties_.NumSIMDPerCU;
      break;
    case HSA_AMD_AGENT_INFO_NUM_SHADER_ENGINES:
      *((uint32_t*)value) = properties_.NumShaderBanks;
      break;
    case HSA_AMD_AGENT_INFO_NUM_SHADER_ARRAYS_PER_SE:
      *((uint32_t*)value) = properties_.NumArrays;
      break;
    case HSA_AMD_AGENT_INFO_HDP_FLUSH:
      *((hsa_amd_hdp_flush_t*)value) = HDP_flush_;
      break;
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
      break;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t GpuAgent::QueueCreate(size_t size, hsa_queue_type32_t queue_type,
                                   core::HsaEventCallback event_callback,
                                   void* data, uint32_t private_segment_size,
                                   uint32_t group_segment_size,
                                   core::Queue** queue) {
  // AQL queues must be a power of two in length.
  if (!IsPowerOfTwo(size)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  // Enforce max size
  if (size > maxAqlSize_) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // Allocate scratch memory
  ScratchInfo scratch;
  if (private_segment_size == UINT_MAX) {
    private_segment_size = 0;
  }
  scratch.size_per_thread = private_segment_size;

  const uint32_t num_cu = properties_.NumFComputeCores / properties_.NumSIMDPerCU;
  scratch.size =
      scratch.size_per_thread * properties_.MaxSlotsScratchCU * properties_.WaveFrontSize * num_cu;
  scratch.queue_base = nullptr;
  scratch.queue_process_offset = 0;

  MAKE_NAMED_SCOPE_GUARD(scratchGuard, [&]() { ReleaseQueueScratch(scratch); });

  if (scratch.size != 0) {
    AcquireQueueScratch(scratch);
    if (scratch.queue_base == nullptr) {
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  }

  // Ensure utility queue has been created.
  // Deferring longer risks exhausting queue count before ISA upload and invalidation capability is
  // ensured.
  queues_[QueueUtility].touch();

  // Create an HW AQL queue
  *queue = new AqlQueue(this, size, node_id(), scratch, event_callback, data, is_kv_device_);
  scratchGuard.Dismiss();
  return HSA_STATUS_SUCCESS;
}

void GpuAgent::AcquireQueueScratch(ScratchInfo& scratch) {
  assert(scratch.queue_base == nullptr && "AcquireQueueScratch called while holding scratch.");
  bool need_queue_scratch_base = (isa_->GetMajorVersion() > 8);

  if (scratch.size == 0) {
    scratch.size = queue_scratch_len_;
    scratch.size_per_thread = scratch_per_thread_;
  }
  scratch.retry = false;

  // Fail scratch allocation if per wave limits are exceeded.
  uint64_t size_per_wave = AlignUp(scratch.size_per_thread * properties_.WaveFrontSize, 1024);
  if (size_per_wave > MAX_WAVE_SCRATCH) return;

  ScopedAcquire<KernelMutex> lock(&scratch_lock_);
  // Limit to 1/8th of scratch pool for small scratch and 1/4 of that for a single queue.
  size_t small_limit = scratch_pool_.size() >> 3;
  size_t single_limit = small_limit >> 2;
  bool large = (scratch.size > single_limit) ||
      (scratch_pool_.size() - scratch_pool_.remaining() + scratch.size > small_limit);
  large = (isa_->GetMajorVersion() < 8) ? false : large;
  if (large)
    scratch.queue_base = scratch_pool_.alloc_high(scratch.size);
  else
    scratch.queue_base = scratch_pool_.alloc(scratch.size);
  large |= scratch.queue_base > scratch_pool_.high_split();
  scratch.large = large;

  scratch.queue_process_offset =
      (need_queue_scratch_base)
          ? uintptr_t(scratch.queue_base)
          : uintptr_t(scratch.queue_base) - uintptr_t(scratch_pool_.base());

  if (scratch.queue_base != nullptr) {
    if (profile_ == HSA_PROFILE_FULL) return;
    if (profile_ == HSA_PROFILE_BASE) {
      HSAuint64 alternate_va;
      if (hsaKmtMapMemoryToGPU(scratch.queue_base, scratch.size, &alternate_va) ==
          HSAKMT_STATUS_SUCCESS) {
        if (large) scratch_used_large_ += scratch.size;
        return;
      }
    }
  }

  // Scratch request failed allocation or mapping.
  scratch_pool_.free(scratch.queue_base);
  scratch.queue_base = nullptr;

  // Retry if large may yield needed space.
  if (scratch_used_large_ != 0) {
    scratch.retry = true;
    return;
  }

  // Attempt to trim the maximum number of concurrent waves to allow scratch to fit.
  if (core::Runtime::runtime_singleton_->flag().enable_queue_fault_message())
    debug_print("Failed to map requested scratch - reducing queue occupancy.\n");
  uint64_t num_cus = properties_.NumFComputeCores / properties_.NumSIMDPerCU;
  uint64_t total_waves = scratch.size / size_per_wave;
  uint64_t waves_per_cu = total_waves / num_cus;
  while (waves_per_cu != 0) {
    size_t size = waves_per_cu * num_cus * size_per_wave;
    void* base = scratch_pool_.alloc(size);
    HSAuint64 alternate_va;
    if ((base != nullptr) &&
        ((profile_ == HSA_PROFILE_FULL) ||
         (hsaKmtMapMemoryToGPU(base, size, &alternate_va) == HSAKMT_STATUS_SUCCESS))) {
      // Scratch allocated and either full profile or map succeeded.
      scratch.queue_base = base;
      scratch.size = size;
      scratch.queue_process_offset =
          (need_queue_scratch_base)
              ? uintptr_t(scratch.queue_base)
              : uintptr_t(scratch.queue_base) - uintptr_t(scratch_pool_.base());
      scratch.large = true;
      scratch_used_large_ += scratch.size;
      return;
    }
    scratch_pool_.free(base);
    waves_per_cu--;
  }

  // Failed to allocate minimal scratch
  assert(scratch.queue_base == nullptr && "bad scratch data");
  if (core::Runtime::runtime_singleton_->flag().enable_queue_fault_message())
    debug_print("Could not allocate scratch for one wave per CU.\n");
}

void GpuAgent::ReleaseQueueScratch(ScratchInfo& scratch) {
  if (scratch.queue_base == nullptr) {
    return;
  }

  ScopedAcquire<KernelMutex> lock(&scratch_lock_);
  if (profile_ == HSA_PROFILE_BASE) {
    if (HSAKMT_STATUS_SUCCESS != hsaKmtUnmapMemoryToGPU(scratch.queue_base)) {
      assert(false && "Unmap scratch subrange failed!");
    }
  }
  scratch_pool_.free(scratch.queue_base);
  scratch.queue_base = nullptr;

  if (scratch.large) scratch_used_large_ -= scratch.size;

  // Notify waiters that additional scratch may be available.
  for (auto notifier : scratch_notifiers_)
    HSA::hsa_signal_or_relaxed(notifier.first, notifier.second);
}

void GpuAgent::TranslateTime(core::Signal* signal,
                             hsa_amd_profiling_dispatch_time_t& time) {
  // Order is important, we want to translate the end time first to ensure that packet duration is
  // not impacted by clock measurement latency jitter.
  time.end = TranslateTime(signal->signal_.end_ts);
  time.start = TranslateTime(signal->signal_.start_ts);

  if ((signal->signal_.start_ts == 0) || (signal->signal_.end_ts == 0) ||
      (signal->signal_.start_ts > t1_.GPUClockCounter) ||
      (signal->signal_.end_ts > t1_.GPUClockCounter) ||
      (signal->signal_.start_ts < t0_.GPUClockCounter) ||
      (signal->signal_.end_ts < t0_.GPUClockCounter))
    debug_print("Signal %p time stamps may be invalid.", &signal->signal_);
}

/*
Times during program execution are interpolated to adjust for relative clock drift.
Interval timing may appear as ticks well before process start, leading to large errors due to
frequency adjustment (ie the profiling with NTP problem).  This is fixed by using a fixed frequency
for early times.
Intervals larger than t0_ will be frequency adjusted.  This admits a numerical error of not more
than twice the frequency stability (~10^-5).
*/
uint64_t GpuAgent::TranslateTime(uint64_t tick) {
  // Ensure interpolation for times during program execution.
  ScopedAcquire<KernelMutex> lock(&t1_lock_);
  if ((t1_.GPUClockCounter < tick) || (t1_.GPUClockCounter == t0_.GPUClockCounter)) SyncClocks();

  // Good for ~300 yrs
  // uint64_t sysdelta = t1_.SystemClockCounter - t0_.SystemClockCounter;
  // uint64_t gpudelta = t1_.GPUClockCounter - t0_.GPUClockCounter;
  // int64_t offtick = int64_t(tick - t1_.GPUClockCounter);
  //__int128 num = __int128(sysdelta)*__int128(offtick) +
  //__int128(gpudelta)*__int128(t1_.SystemClockCounter);
  //__int128 sysLarge = num / __int128(gpudelta);
  // return sysLarge;

  // Good for ~3.5 months.
  uint64_t system_tick = 0;
  double ratio = double(t1_.SystemClockCounter - t0_.SystemClockCounter) /
      double(t1_.GPUClockCounter - t0_.GPUClockCounter);
  system_tick = uint64_t(ratio * double(int64_t(tick - t1_.GPUClockCounter))) + t1_.SystemClockCounter;

  // tick predates HSA startup - extrapolate with fixed clock ratio
  if (tick < t0_.GPUClockCounter) {
    if (historical_clock_ratio_ == 0.0) historical_clock_ratio_ = ratio;
    system_tick = uint64_t(historical_clock_ratio_ * double(int64_t(tick - t0_.GPUClockCounter))) +
        t0_.SystemClockCounter;
  }

  return system_tick;
}

bool GpuAgent::current_coherency_type(hsa_amd_coherency_type_t type) {
  if (!is_kv_device_) {
    current_coherency_type_ = type;
    return true;
  }

  ScopedAcquire<KernelMutex> Lock(&coherency_lock_);

  if (ape1_base_ == 0 && ape1_size_ == 0) {
    static const size_t kApe1Alignment = 64 * 1024;
    ape1_size_ = kApe1Alignment;
    ape1_base_ = reinterpret_cast<uintptr_t>(
        _aligned_malloc(ape1_size_, kApe1Alignment));
    assert((ape1_base_ != 0) && ("APE1 allocation failed"));
  } else if (type == current_coherency_type_) {
    return true;
  }

  HSA_CACHING_TYPE type0, type1;
  if (type == HSA_AMD_COHERENCY_TYPE_COHERENT) {
    type0 = HSA_CACHING_CACHED;
    type1 = HSA_CACHING_NONCACHED;
  } else {
    type0 = HSA_CACHING_NONCACHED;
    type1 = HSA_CACHING_CACHED;
  }

  if (hsaKmtSetMemoryPolicy(node_id(), type0, type1,
                            reinterpret_cast<void*>(ape1_base_),
                            ape1_size_) != HSAKMT_STATUS_SUCCESS) {
    return false;
  }
  current_coherency_type_ = type;
  return true;
}

uint16_t GpuAgent::GetMicrocodeVersion() const {
  return (properties_.EngineId.ui32.uCode);
}

uint16_t GpuAgent::GetSdmaMicrocodeVersion() const {
  return (properties_.uCodeEngineVersions.uCodeSDMA);
}

void GpuAgent::SyncClocks() {
  HSAKMT_STATUS err = hsaKmtGetClockCounters(node_id(), &t1_);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaGetClockCounters error");
}

void GpuAgent::BindTrapHandler() {
  const char* src_sp3 = R"(
    var s_trap_info_lo = ttmp0
    var s_trap_info_hi = ttmp1
    var s_tmp0         = ttmp2
    var s_tmp1         = ttmp3
    var s_tmp2         = ttmp4
    var s_tmp3         = ttmp5

    shader TrapHandler
      type(CS)

      // Retrieve the queue inactive signal.
      s_load_dwordx2       [s_tmp0, s_tmp1], s[0:1], 0xC0
      s_waitcnt            lgkmcnt(0)

      // Mask all but one lane of the wavefront.
      s_mov_b64            exec, 0x1

      // Set queue signal value to unhandled exception error.
      s_add_u32            s_tmp0, s_tmp0, 0x8
      s_addc_u32           s_tmp1, s_tmp1, 0x0
      v_mov_b32            v0, s_tmp0
      v_mov_b32            v1, s_tmp1
      v_mov_b32            v2, 0x80000000
      v_mov_b32            v3, 0x0
      flat_atomic_swap_x2  v[0:1], v[0:1], v[2:3]
      s_waitcnt            vmcnt(0)

      // Skip event if the signal was already set to unhandled exception.
      v_cmp_eq_u64         vcc, v[0:1], v[2:3]
      s_cbranch_vccnz      L_SIGNAL_DONE

      // Check for a non-NULL signal event mailbox.
      s_load_dwordx2       [s_tmp2, s_tmp3], [s_tmp0, s_tmp1], 0x8
      s_waitcnt            lgkmcnt(0)
      s_and_b64            [s_tmp2, s_tmp3], [s_tmp2, s_tmp3], [s_tmp2, s_tmp3]
      s_cbranch_scc0       L_SIGNAL_DONE

      // Load the signal event value.
      s_add_u32            s_tmp0, s_tmp0, 0x10
      s_addc_u32           s_tmp1, s_tmp1, 0x0
      s_load_dword         s_tmp0, [s_tmp0, s_tmp1], 0x0
      s_waitcnt            lgkmcnt(0)

      // Write the signal event value to the mailbox.
      v_mov_b32            v0, s_tmp2
      v_mov_b32            v1, s_tmp3
      v_mov_b32            v2, s_tmp0
      flat_store_dword     v[0:1], v2
      s_waitcnt            vmcnt(0)

      // Send an interrupt to trigger event notification.
      s_sendmsg            sendmsg(MSG_INTERRUPT)

    L_SIGNAL_DONE:
      // Halt wavefront and exit trap.
      s_sethalt            1
      s_rfe_b64            [s_trap_info_lo, s_trap_info_hi]
    end
  )";

  if (isa_->GetMajorVersion() == 7) {
    // No trap handler support on Gfx7, soft error.
    return;
  }

  // Disable trap handler on Carrizo until KFD is fixed.
  if (profile_ == HSA_PROFILE_FULL) {
    return;
  }

  // Assemble the trap handler source code.
  AssembleShader(src_sp3, "TrapHandler", AssembleTarget::ISA, trap_code_buf_,
                 trap_code_buf_size_);

  // Bind the trap handler to this node.
  HSAKMT_STATUS err = hsaKmtSetTrapHandler(node_id(), trap_code_buf_,
                                           trap_code_buf_size_, NULL, 0);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaKmtSetTrapHandler() failed");
}

void GpuAgent::InvalidateCodeCaches() {
  // Check for microcode cache invalidation support.
  // This is deprecated in later microcode builds.
  if (isa_->GetMajorVersion() == 7) {
    if (properties_.EngineId.ui32.uCode < 420) {
      // Microcode is handling code cache invalidation.
      return;
    }
  } else if (isa_->GetMajorVersion() == 8 && isa_->GetMinorVersion() == 0) {
    if (properties_.EngineId.ui32.uCode < 685) {
      // Microcode is handling code cache invalidation.
      return;
    }
  } else if (isa_->GetMajorVersion() > 9) {
    assert(false && "Code cache invalidation not implemented for this agent");
  }

  // Invalidate caches which may hold lines of code object allocation.
  constexpr uint32_t cache_inv_size_dw = 7;
  uint32_t cache_inv[cache_inv_size_dw];

  cache_inv[0] = PM4_HDR(PM4_HDR_IT_OPCODE_ACQUIRE_MEM, cache_inv_size_dw,
                         isa_->GetMajorVersion());
  cache_inv[1] = PM4_ACQUIRE_MEM_DW1_COHER_CNTL(
      PM4_ACQUIRE_MEM_COHER_CNTL_SH_ICACHE_ACTION_ENA |
      PM4_ACQUIRE_MEM_COHER_CNTL_SH_KCACHE_ACTION_ENA |
      PM4_ACQUIRE_MEM_COHER_CNTL_TC_ACTION_ENA |
      PM4_ACQUIRE_MEM_COHER_CNTL_TC_WB_ACTION_ENA);
  cache_inv[2] = PM4_ACQUIRE_MEM_DW2_COHER_SIZE(0xFFFFFFFF);
  cache_inv[3] = PM4_ACQUIRE_MEM_DW3_COHER_SIZE_HI(0xFF);
  cache_inv[4] = 0;
  cache_inv[5] = 0;
  cache_inv[6] = 0;

  // Submit the command to the utility queue and wait for it to complete.
  queues_[QueueUtility]->ExecutePM4(cache_inv, sizeof(cache_inv));
}

}  // namespace
