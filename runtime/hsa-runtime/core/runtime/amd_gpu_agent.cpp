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
#include <vector>

#include "core/inc/amd_aql_queue.h"
#include "core/inc/amd_blit_kernel.h"
#include "core/inc/amd_blit_sdma.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/interrupt_signal.h"
#include "core/inc/isa.h"
#include "core/inc/runtime.h"

#include "utils/sp3/sp3.h"

#include "hsa_ext_image.h"

// Size of scratch (private) segment pre-allocated per thread, in bytes.
#define DEFAULT_SCRATCH_BYTES_PER_THREAD 2048

namespace amd {
GpuAgent::GpuAgent(HSAuint32 node, const HsaNodeProperties& node_props)
    : GpuAgentInt(node),
      properties_(node_props),
      current_coherency_type_(HSA_AMD_COHERENCY_TYPE_COHERENT),
      blit_h2d_(NULL),
      blit_d2h_(NULL),
      is_kv_device_(false),
      trap_code_buf_(NULL),
      trap_code_buf_size_(0),
      ape1_base_(0),
      ape1_size_(0) {
  const bool is_apu_node = (properties_.NumCPUCores > 0);
  profile_ = (is_apu_node) ? HSA_PROFILE_FULL : HSA_PROFILE_BASE;

  HSAKMT_STATUS err = hsaKmtGetClockCounters(node_id(), &t0_);
  t1_ = t0_;
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaGetClockCounters error");

  // Set instruction set architecture via node property, only on GPU device.
  isa_ = (core::Isa*)core::IsaRegistry::GetIsa(core::Isa::Version(
      node_props.EngineId.ui32.Major, node_props.EngineId.ui32.Minor,
      node_props.EngineId.ui32.Stepping));
  // Check if the device is Kaveri, only on GPU device.
  if (isa_->GetMajorVersion() == 7 && isa_->GetMinorVersion() == 0 &&
      isa_->GetStepping() == 0) {
    is_kv_device_ = true;
  }

  current_coherency_type((profile_ == HSA_PROFILE_FULL)
                             ? HSA_AMD_COHERENCY_TYPE_COHERENT
                             : HSA_AMD_COHERENCY_TYPE_NONCOHERENT);

  max_queues_ =
      static_cast<uint32_t>(atoi(os::GetEnvVar("HSA_MAX_QUEUES").c_str()));
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

  // Reserve memory for scratch.
  InitScratchPool();

  // Populate cache list.
  InitCacheList();

  // Bind the second-level trap handler to this node.
  BindTrapHandler();
}

GpuAgent::~GpuAgent() {
  if (blit_h2d_ != NULL) {
    hsa_status_t status = blit_h2d_->Destroy();
    assert(status == HSA_STATUS_SUCCESS);

    delete blit_h2d_;
    blit_h2d_ = NULL;
  }

  if (blit_d2h_ != NULL) {
    hsa_status_t status = blit_d2h_->Destroy();
    assert(status == HSA_STATUS_SUCCESS);

    delete blit_d2h_;
    blit_d2h_ = NULL;
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
                              void*& code_buf, size_t& code_buf_size) {
#ifdef __linux__  // No VS builds of libsp3 available right now
  // Assemble source string with libsp3.
  sp3_context* sp3 = sp3_new();

  switch (isa_->GetMajorVersion()) {
    case 7:
      sp3_setasic(sp3, "CI");
      break;
    case 8:
      sp3_setasic(sp3, "VI");
      break;
    default:
      assert(false && "SP3 assembly not supported on this agent");
  }

  sp3_parse_string(sp3, src_sp3);
  sp3_shader* code_sp3_meta = sp3_compile(sp3, func_name);

  // Allocate a GPU-visible buffer for the trap shader.
  HsaMemFlags code_buf_flags = {0};
  code_buf_flags.ui32.HostAccess = 1;
  code_buf_flags.ui32.ExecuteAccess = 1;
  code_buf_flags.ui32.NoSubstitute = 1;

  size_t code_size = code_sp3_meta->size * sizeof(uint32_t);
  code_buf_size = AlignUp(code_size, 0x1000);

  HSAKMT_STATUS err =
      hsaKmtAllocMemory(node_id(), code_buf_size, code_buf_flags, &code_buf);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaKmtAllocMemory(Trap) failed");

  err = hsaKmtMapMemoryToGPU(code_buf, code_buf_size, NULL);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaKmtMapMemoryToGPU(Trap) failed");

  // Copy trap handler code into the GPU-visible buffer.
  memset(code_buf, 0, code_buf_size);
  memcpy(code_buf, code_sp3_meta->data, code_size);

  // Release SP3 resources.
  sp3_free_shader(code_sp3_meta);
  sp3_close(sp3);
#endif
}

void GpuAgent::ReleaseShader(void* code_buf, size_t code_buf_size) {
  hsaKmtUnmapMemoryToGPU(code_buf);
  hsaKmtFreeMemory(code_buf, code_buf_size);
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
        case HSA_HEAPTYPE_GPU_LDS:
        case HSA_HEAPTYPE_GPU_SCRATCH:
        case HSA_HEAPTYPE_DEVICE_SVM: {
          MemoryRegion* region =
              new MemoryRegion(false, false, this, mem_props[mem_idx]);

          regions_.push_back(region);
          break;
        }
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

  scratch_per_thread_ = atoi(os::GetEnvVar("HSA_SCRATCH_MEM").c_str());
  if (scratch_per_thread_ == 0)
    scratch_per_thread_ = DEFAULT_SCRATCH_BYTES_PER_THREAD;

  // Scratch length is: waves/CU * threads/wave * queues * #CUs *
  // scratch/thread
  const uint32_t num_cu =
      properties_.NumFComputeCores / properties_.NumSIMDPerCU;
  queue_scratch_len_ = 0;
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
    new (&scratch_pool_) SmallHeap(NULL, 0);
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
}

hsa_status_t GpuAgent::IterateRegion(
    hsa_status_t (*callback)(hsa_region_t region, void* data),
    void* data) const {
  return VisitRegion(true, callback, data);
}

hsa_status_t GpuAgent::VisitRegion(bool include_peer,
                                   hsa_status_t (*callback)(hsa_region_t region,
                                                            void* data),
                                   void* data) const {
  if (include_peer) {
    // Only expose system, local, and LDS memory of the blit agent.
    if (this->node_id() ==
        core::Runtime::runtime_singleton_->blit_agent()->node_id()) {
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
  for (const core::MemoryRegion* region : regions) {
    const amd::MemoryRegion* amd_region =
        reinterpret_cast<const amd::MemoryRegion*>(region);

    // Only expose system, local, and LDS memory.
    if (amd_region->IsSystem() || amd_region->IsLocalMemory() ||
        amd_region->IsLDS()) {
      hsa_region_t region_handle = core::MemoryRegion::Convert(region);
      hsa_status_t status = callback(region_handle, data);
      if (status != HSA_STATUS_SUCCESS) {
        return status;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

core::Blit* GpuAgent::CreateBlitSdma() {
  BlitSdma* sdma = new BlitSdma();

  if (sdma->Initialize(*this) != HSA_STATUS_SUCCESS) {
    sdma->Destroy();
    delete sdma;
    sdma = NULL;
  }

  return sdma;
}

core::Blit* GpuAgent::CreateBlitKernel() {
  BlitKernel* kernl = new BlitKernel();

  if (kernl->Initialize(*this) != HSA_STATUS_SUCCESS) {
    kernl->Destroy();
    delete kernl;
    kernl = NULL;
  }

  return kernl;
}

hsa_status_t GpuAgent::InitDma() {
  // Try create SDMA blit first.
  std::string sdma_enable = os::GetEnvVar("HSA_ENABLE_SDMA");

  if (sdma_enable != "0" && isa_->GetMajorVersion() == 8 &&
      isa_->GetMinorVersion() == 0 && isa_->GetStepping() == 3) {
    blit_h2d_ = CreateBlitSdma();
    blit_d2h_ = CreateBlitSdma();

    if (blit_h2d_ != NULL && blit_d2h_ != NULL) {
      return HSA_STATUS_SUCCESS;
    }
  }

  // Fall back to blit kernel if SDMA is unavailable.
  assert(blit_h2d_ == NULL || blit_d2h_ == NULL);

  if (blit_h2d_ == NULL) {
    blit_h2d_ = CreateBlitKernel();
  }

  if (blit_d2h_ == NULL) {
    blit_d2h_ = CreateBlitKernel();
  }

  return (blit_h2d_ != NULL && blit_d2h_ != NULL)
             ? HSA_STATUS_SUCCESS
             : HSA_STATUS_ERROR_OUT_OF_RESOURCES;
}

hsa_status_t GpuAgent::DmaCopy(void* dst, const void* src, size_t size) {
  if (blit_d2h_ == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return blit_d2h_->SubmitLinearCopyCommand(dst, src, size);
}

hsa_status_t GpuAgent::DmaCopy(void* dst, core::Agent& dst_agent,
                               const void* src, core::Agent& src_agent,
                               size_t size,
                               std::vector<core::Signal*>& dep_signals,
                               core::Signal& out_signal) {
  core::Blit* blit = (src_agent.device_type() == core::Agent::kAmdCpuDevice &&
                      dst_agent.device_type() == core::Agent::kAmdGpuDevice)
                         ? blit_h2d_
                         : blit_d2h_;

  if (blit == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // TODO(bwicakso): temporarily disable wait on thunk event if the out_signal
  // is an interrupt signal object. Remove this when SDMA handle interrupt
  // packet properly.
  if (out_signal.EopEvent() != NULL) {
    static_cast<core::InterruptSignal&>(out_signal).DisableWaitEvent();
  }

  return blit->SubmitLinearCopyCommand(dst, src, size, dep_signals, out_signal);
}

hsa_status_t GpuAgent::DmaFill(void* ptr, uint32_t value, size_t count) {
  if (blit_d2h_ == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return blit_d2h_->SubmitLinearFillCommand(ptr, value, count);
}

hsa_status_t GpuAgent::GetInfo(hsa_agent_info_t attribute, void* value) const {
  const size_t kNameSize = 64;  // agent, and vendor name size limit

  const core::ExtensionEntryPoints& extensions =
      core::Runtime::runtime_singleton_->extensions_;

  hsa_agent_t agent = core::Agent::Convert(this);

  const size_t attribute_u = static_cast<size_t>(attribute);
  switch (attribute_u) {
    case HSA_AGENT_INFO_NAME:
      // TODO(bwicakso): hardcode for now.
      std::memset(value, 0, kNameSize);
      if (isa_->GetMajorVersion() == 7) {
        std::memcpy(value, "Kaveri", sizeof("Kaveri"));
      } else if (isa_->GetMajorVersion() == 8) {
        if (isa_->GetMinorVersion() == 0 && isa_->GetStepping() == 2) {
          std::memcpy(value, "Tonga", sizeof("Tonga"));
        } else if (isa_->GetMinorVersion() == 0 && isa_->GetStepping() == 3) {
          std::memcpy(value, "Fiji", sizeof("Fiji"));
        } else {
          std::memcpy(value, "Carrizo", sizeof("Carrizo"));
        }
      } else {
        std::memcpy(value, "Unknown", sizeof("Unknown"));
      }
      break;
    case HSA_AGENT_INFO_VENDOR_NAME:
      std::memset(value, 0, kNameSize);
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
      *((hsa_queue_type_t*)value) = HSA_QUEUE_TYPE_MULTI;
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
    case HSA_AGENT_INFO_EXTENSIONS:
      memset(value, 0, sizeof(uint8_t) * 128);

      if (extensions.table.hsa_ext_program_finalize_fn != NULL) {
        *((uint8_t*)value) = 1 << HSA_EXTENSION_FINALIZER;
      }

      if (profile_ == HSA_PROFILE_FULL &&
          extensions.table.hsa_ext_image_create_fn != NULL) {
        // TODO(bwicakso): only APU supports images currently.
        *((uint8_t*)value) |= 1 << HSA_EXTENSION_IMAGES;
      }

      *((uint8_t*)value) |= 1 << HSA_EXTENSION_AMD_PROFILER;

      break;
    case HSA_AGENT_INFO_VERSION_MAJOR:
      *((uint16_t*)value) = 1;
      break;
    case HSA_AGENT_INFO_VERSION_MINOR:
      *((uint16_t*)value) = 0;
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
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
      break;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t GpuAgent::QueueCreate(size_t size, hsa_queue_type_t queue_type,
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
#if defined(HSA_LARGE_MODEL) && defined(__linux__)
  if (core::g_use_interrupt_wait) {
    if (private_segment_size == UINT_MAX) {
      private_segment_size =
          (profile_ == HSA_PROFILE_BASE) ? 0 : scratch_per_thread_;
    }

    if (private_segment_size > 262128) {
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }

    scratch.size_per_thread = AlignUp(private_segment_size, 16);
    if (scratch.size_per_thread > 262128) {
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }

    const uint32_t num_cu =
        properties_.NumFComputeCores / properties_.NumSIMDPerCU;
    scratch.size = scratch.size_per_thread * 32 * 64 * num_cu;
  } else {
    scratch.size = queue_scratch_len_;
    scratch.size_per_thread = scratch_per_thread_;
  }
#else
  scratch.size = queue_scratch_len_;
  scratch.size_per_thread = scratch_per_thread_;
#endif
  scratch.queue_base = NULL;
  if (scratch.size != 0) {
    AcquireQueueScratch(scratch);
    if (scratch.queue_base == NULL) {
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  }

  // Create an HW AQL queue
  AqlQueue* hw_queue = new AqlQueue(this, size, node_id(), scratch,
                                    event_callback, data, is_kv_device_);
  if (hw_queue && hw_queue->IsValid()) {
    // return queue
    *queue = hw_queue;
    return HSA_STATUS_SUCCESS;
  }
  // If reached here its always an ERROR.
  delete hw_queue;
  ReleaseQueueScratch(scratch.queue_base);
  return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
}

void GpuAgent::AcquireQueueScratch(ScratchInfo& scratch) {
  if (scratch.size == 0) {
    scratch.size = queue_scratch_len_;
    scratch.size_per_thread = scratch_per_thread_;
  }

  ScopedAcquire<KernelMutex> lock(&scratch_lock_);
  scratch.queue_base = scratch_pool_.alloc(scratch.size);
  scratch.queue_process_offset =
      uintptr_t(scratch.queue_base) - uintptr_t(scratch_pool_.base());

  if ((scratch.queue_base != NULL) && (profile_ == HSA_PROFILE_BASE)) {
    HSAuint64 alternate_va;
    if (HSAKMT_STATUS_SUCCESS !=
        hsaKmtMapMemoryToGPU(scratch.queue_base, scratch.size, &alternate_va)) {
      assert(false && "Map scratch subrange failed!");
      scratch_pool_.free(scratch.queue_base);
      scratch.queue_base = NULL;
    }
  }
}

void GpuAgent::ReleaseQueueScratch(void* base) {
  if (base == NULL) {
    return;
  }

  ScopedAcquire<KernelMutex> lock(&scratch_lock_);
  if (profile_ == HSA_PROFILE_BASE) {
    if (HSAKMT_STATUS_SUCCESS != hsaKmtUnmapMemoryToGPU(base)) {
      assert(false && "Unmap scratch subrange failed!");
    }
  }
  scratch_pool_.free(base);
}

void GpuAgent::TranslateTime(core::Signal* signal,
                             hsa_amd_profiling_dispatch_time_t& time) {
  // Ensure interpolation
  ScopedAcquire<KernelMutex> lock(&t1_lock_);
  if (t1_.GPUClockCounter < signal->signal_.end_ts) {
    SyncClocks();
  }

  time.start = uint64_t(
      (double(int64_t(t0_.SystemClockCounter - t1_.SystemClockCounter)) /
       double(int64_t(t0_.GPUClockCounter - t1_.GPUClockCounter))) *
          double(int64_t(signal->signal_.start_ts - t1_.GPUClockCounter)) +
      double(t1_.SystemClockCounter));
  time.end = uint64_t(
      (double(int64_t(t0_.SystemClockCounter - t1_.SystemClockCounter)) /
       double(int64_t(t0_.GPUClockCounter - t1_.GPUClockCounter))) *
          double(int64_t(signal->signal_.end_ts - t1_.GPUClockCounter)) +
      double(t1_.SystemClockCounter));
}

uint64_t GpuAgent::TranslateTime(uint64_t tick) {
  ScopedAcquire<KernelMutex> lock(&t1_lock_);
  SyncClocks();

  uint64_t system_tick = 0;
  system_tick = uint64_t(
      (double(int64_t(t0_.SystemClockCounter - t1_.SystemClockCounter)) /
       double(int64_t(t0_.GPUClockCounter - t1_.GPUClockCounter))) *
          double(int64_t(tick - t1_.GPUClockCounter)) +
      double(t1_.SystemClockCounter));
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

void GpuAgent::SyncClocks() {
  HSAKMT_STATUS err = hsaKmtGetClockCounters(node_id(), &t1_);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaGetClockCounters error");
}

void GpuAgent::BindTrapHandler() {
#ifdef __linux__  // No raw string literal support in VS builds right now
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
  AssembleShader(src_sp3, "TrapHandler", trap_code_buf_, trap_code_buf_size_);

  // Bind the trap handler to this node.
  HSAKMT_STATUS err = hsaKmtSetTrapHandler(node_id(), trap_code_buf_,
                                           trap_code_buf_size_, NULL, 0);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaKmtSetTrapHandler() failed");
#endif
}

}  // namespace
