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
#include <vector>
#include <cstring>
#include <climits>

#include "core/inc/amd_blit_kernel.h"
#include "core/inc/amd_blit_sdma.h"
#include "core/inc/runtime.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/amd_hw_aql_command_processor.h"
#include "core/inc/interrupt_signal.h"
#include "core/runtime/isa.hpp"

#include "inc/hsa_ext_image.h"

// Size of scratch (private) segment pre-allocated per thread, in bytes.
#define DEFAULT_SCRATCH_BYTES_PER_THREAD 2048

namespace amd {
GpuAgent::GpuAgent(HSAuint32 node, const HsaNodeProperties& node_props,
                   const std::vector<HsaCacheProperties>& cache_props,
                   hsa_profile_t profile)
    : node_id_(node),
      properties_(node_props),
      current_coherency_type_(HSA_AMD_COHERENCY_TYPE_COHERENT),
      blit_(NULL),
      cache_props_(cache_props),
      is_kv_device_(false),
      profile_(profile),
      ape1_base_(0),
      ape1_size_(0) {
  HSAKMT_STATUS err = hsaKmtGetClockCounters(node_id_, &t0_);
  t1_ = t0_;
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaGetClockCounters error");

  // Set compute_capability_ via node property, only on GPU device.
  compute_capability_.Initialize(node_props.EngineId.ui32.Major,
                                 node_props.EngineId.ui32.Minor,
                                 node_props.EngineId.ui32.Stepping);
  // Check if the device is Kaveri, only on GPU device.
  if (compute_capability_.version_major() == 7 &&
      compute_capability_.version_minor() == 0 &&
      compute_capability_.version_stepping() == 0) {
    is_kv_device_ = true;
  }

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
}

GpuAgent::~GpuAgent() {
  if (blit_ != NULL) {
    hsa_status_t status = blit_->Destroy();
    assert(status == HSA_STATUS_SUCCESS);

    delete blit_;
    blit_ = NULL;
  }

  if (ape1_base_ != 0) {
    _aligned_free(reinterpret_cast<void*>(ape1_base_));
  }

  if (scratch_pool_.base() != NULL) {
    hsaKmtFreeMemory(scratch_pool_.base(), scratch_pool_.size());
  }

  regions_.clear();
}

void GpuAgent::RegisterMemoryProperties(core::MemoryRegion& region) {
  MemoryRegion* amd_region = reinterpret_cast<MemoryRegion*>(&region);

  assert((!amd_region->IsGDS()) &&
         ("Memory region should only be global, group or scratch"));

  regions_.push_back(amd_region);

  if (amd_region->IsScratch()) {
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
    size_t scratchLen = queue_scratch_len_ * max_queues_;

#if defined(HSA_LARGE_MODEL) && defined(__linux__)
    // For 64-bit linux use max queues unless otherwise specified
    if ((scratchLen == 0) || (scratchLen > 4294967296))
      scratchLen = 4294967296;  // 4GB apeture max
#endif

    void* scratchBase;
    HSAKMT_STATUS err =
        hsaKmtAllocMemory(node_id_, scratchLen, flags, &scratchBase);
    assert(err == HSAKMT_STATUS_SUCCESS && "hsaKmtAllocMemory(Scratch) failed");
    assert(IsMultipleOf(scratchBase, 0x1000) &&
           "Scratch base is not page aligned!");

    scratch_pool_. ~SmallHeap();
    new (&scratch_pool_) SmallHeap(scratchBase, scratchLen);
  }
}

hsa_status_t GpuAgent::IterateRegion(
    hsa_status_t (*callback)(hsa_region_t region, void* data),
    void* data) const {
  const size_t num_mems = regions().size();

  for (size_t j = 0; j < num_mems; ++j) {
    const MemoryRegion* amd_region =
        reinterpret_cast<const MemoryRegion*>(regions()[j]);

    // Skip memory regions other than host, gpu local, or LDS.
    if (amd_region->IsSystem() || amd_region->IsLocalMemory() ||
        amd_region->IsLDS()) {
      hsa_region_t hsa_region = core::MemoryRegion::Convert(amd_region);
      hsa_status_t status = callback(hsa_region, data);
      if (status != HSA_STATUS_SUCCESS) {
        return status;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t GpuAgent::InitDma() {
  std::string sdma_enable = os::GetEnvVar("HSA_ENABLE_SDMA");

  if (sdma_enable != "0" && compute_capability_.version_major() == 8 &&
      compute_capability_.version_minor() == 0 &&
      compute_capability_.version_stepping() == 3) {
    blit_ = new BlitSdma();
  } else {
    blit_ = new BlitKernel();
  }

  assert(blit_ != NULL);

  if (blit_->Initialize(*this) != HSA_STATUS_SUCCESS) {
    blit_->Destroy();
    delete blit_;
    blit_ = NULL;

    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t GpuAgent::DmaCopy(void* dst, const void* src, size_t size) {
  if (blit_ == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return blit_->SubmitLinearCopyCommand(dst, src, size);
}

hsa_status_t GpuAgent::DmaCopy(void* dst, const void* src, size_t size,
                               std::vector<core::Signal*>& dep_signals,
                               core::Signal& out_signal) {
  if (blit_ == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // TODO(bwicakso): temporarily disable wait on thunk event if the out_signal
  // is an interrupt signal object. Remove this when SDMA handle interrupt
  // packet properly.
  if (out_signal.EopEvent() != NULL) {
    reinterpret_cast<core::InterruptSignal&>(out_signal).DisableWaitEvent();
  }

  return blit_->SubmitLinearCopyCommand(dst, src, size, dep_signals,
                                        out_signal);
}

hsa_status_t GpuAgent::DmaFill(void* ptr, uint32_t value, size_t count) {
  if (blit_ == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return blit_->SubmitLinearFillCommand(ptr, value, count);
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
      if (compute_capability_.version_major() == 7) {
        std::memcpy(value, "Kaveri", sizeof("Kaveri"));
      } else if (compute_capability_.version_major() == 8) {
        if (compute_capability_.version_minor() == 0 &&
            compute_capability_.version_stepping() == 2) {
          std::memcpy(value, "Tonga", sizeof("Tonga"));
        } else if (compute_capability_.version_minor() == 0 &&
                   compute_capability_.version_stepping() == 3) {
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
      *((uint32_t*)value) = node_id_;
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
      return core::Isa::Create(agent, (hsa_isa_t*)value);
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
      *((uint32_t*)value) = node_id_;
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
  if (!IsPowerOfTwo(size)) return HSA_STATUS_ERROR_INVALID_ARGUMENT;

  // Enforce max size
  if (size > maxAqlSize_) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;

  // Allocate scratch memory
  ScratchInfo scratch;
#if defined(HSA_LARGE_MODEL) && defined(__linux__)
  if (core::g_use_interrupt_wait) {
    if (private_segment_size == UINT_MAX)
      private_segment_size =
          (profile_ == HSA_PROFILE_BASE) ? 0 : scratch_per_thread_;
    if (private_segment_size > 262128) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    scratch.size_per_thread = AlignUp(private_segment_size, 16);
    if (scratch.size_per_thread > 262128)
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    uint32_t CUs = properties_.NumFComputeCores / properties_.NumSIMDPerCU;
    // TODO: Replace constants with proper topology data.
    scratch.size = scratch.size_per_thread * 32 * 64 * CUs;
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
    if (scratch.queue_base == NULL) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // Create an HW AQL queue
  HwAqlCommandProcessor* hw_queue = new HwAqlCommandProcessor(
      this, size, node_id_, scratch, event_callback, data, is_kv_device_);
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

void GpuAgent::TranslateTime(core::Signal* signal,
                             hsa_amd_profiling_dispatch_time_t& time) {
  // Ensure interpolation
  ScopedAcquire<KernelMutex> lock(&t1_lock_);
  if (t1_.GPUClockCounter < signal->signal_.end_ts) SyncClocks();

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
  ScopedAcquire<KernelMutex> Lock(&lock_);

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

  if (hsaKmtSetMemoryPolicy(node_id_, type0, type1,
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
  HSAKMT_STATUS err = hsaKmtGetClockCounters(node_id_, &t1_);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaGetClockCounters error");
}
}  // namespace
