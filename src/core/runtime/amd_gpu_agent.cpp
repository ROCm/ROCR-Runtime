////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2023, Advanced Micro Devices, Inc. All rights reserved.
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
#include <iomanip>

#include "core/inc/amd_aql_queue.h"
#include "core/inc/amd_blit_kernel.h"
#include "core/inc/amd_blit_sdma.h"
#include "core/inc/amd_gpu_pm4.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/default_signal.h"
#include "core/inc/interrupt_signal.h"
#include "core/inc/isa.h"
#include "core/inc/runtime.h"
#include "core/util/os.h"
#include "inc/hsa_ext_image.h"
#include "inc/hsa_ven_amd_aqlprofile.h"

#include "core/inc/amd_trap_handler_v1.h"
#include "core/inc/amd_blit_shaders.h"
// Generated header
#include "amd_trap_handler_v2.h"
#include "amd_blit_shaders_v2.h"

#if defined(__linux__)
// libdrm headers
#include <xf86drm.h>
#include <amdgpu.h>
#endif


// Size of scratch (private) segment pre-allocated per thread, in bytes.
#define DEFAULT_SCRATCH_BYTES_PER_THREAD 2048
#define MAX_WAVE_SCRATCH 8387584  // See COMPUTE_TMPRING_SIZE.WAVESIZE
#define MAX_NUM_DOORBELLS 0x400
#define MAX_SCRATCH_APERTURE_PER_XCC 4294967296
#define DEFAULT_SCRATCH_SINGLE_LIMIT_ASYNC_PER_XCC (1 << 30)  // 1 GB

namespace rocr {
namespace core {
extern HsaApiTable hsa_internal_api_table_;
} // namespace core

namespace AMD {
GpuAgent::GpuAgent(HSAuint32 node, const HsaNodeProperties& node_props, bool xnack_mode,
                   uint32_t index)
    : GpuAgentInt(node),
      properties_(node_props),
      current_coherency_type_(HSA_AMD_COHERENCY_TYPE_COHERENT),
      scratch_used_large_(0),
      queues_(),
      is_kv_device_(false),
      trap_code_buf_(NULL),
      trap_code_buf_size_(0),
      doorbell_queue_map_(NULL),
      memory_bus_width_(0),
      memory_max_frequency_(0),
      enum_index_(index),
      ape1_base_(0),
      ape1_size_(0),
      pending_copy_req_ref_(0),
      pending_copy_stat_check_ref_(0),
      sdma_blit_used_mask_(0),
      scratch_limit_async_threshold_(0),
      scratch_cache_(
          [this](void* base, size_t size, bool large) { ReleaseScratch(base, size, large); }) {
  const bool is_apu_node = (properties_.NumCPUCores > 0);
  profile_ = (is_apu_node) ? HSA_PROFILE_FULL : HSA_PROFILE_BASE;

  HSAKMT_STATUS err = hsaKmtGetClockCounters(node_id(), &t0_);
  t1_ = t0_;
  historical_clock_ratio_ = 0.0;
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaGetClockCounters error");

  const core::Isa *isa_base = core::IsaRegistry::GetIsa(
      core::Isa::Version(node_props.EngineId.ui32.Major,
                         node_props.EngineId.ui32.Minor,
                         node_props.EngineId.ui32.Stepping));
  if (!isa_base) {
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ISA, "Agent creation failed.\nThe GPU node has an unrecognized id.\n");
  }

  rocr::core::IsaFeature sramecc = rocr::core::IsaFeature::Unsupported;
  if (isa_base->IsSrameccSupported()) {
    switch (core::Runtime::runtime_singleton_->flag().sramecc_enable()) {
      case Flag::SRAMECC_DISABLED:
        sramecc = core::IsaFeature::Disabled;
        break;
      case Flag::SRAMECC_ENABLED:
        sramecc = core::IsaFeature::Enabled;
        break;
      case Flag::SRAMECC_DEFAULT:
        sramecc = node_props.Capability.ui32.SRAM_EDCSupport == 1 ? core::IsaFeature::Enabled
                                                                  : core::IsaFeature::Disabled;
        break;
    }
  }

  rocr::core::IsaFeature xnack = rocr::core::IsaFeature::Unsupported;
  if (isa_base->IsXnackSupported()) {
    // TODO: This needs to be obtained form KFD once HMM implemented.
    xnack = xnack_mode ? core::IsaFeature::Enabled
                      : core::IsaFeature::Disabled;
  }

  // Set instruction set architecture via node property, only on GPU device.
  isa_ = (core::Isa*)core::IsaRegistry::GetIsa(
      core::Isa::Version(node_props.EngineId.ui32.Major, node_props.EngineId.ui32.Minor,
                         node_props.EngineId.ui32.Stepping), sramecc, xnack);

  assert(isa_ != nullptr && "ISA registry inconsistency.");

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

  // Initialize libdrm device handle
  InitLibDrm();

#if !defined(__linux__)
  wallclock_frequency_ = 0;
#else
  // Get wallclock freq from libdrm.
  amdgpu_gpu_info info;
  if (amdgpu_query_gpu_info(ldrm_dev_, &info) < 0)
    throw AMD::hsa_exception(HSA_STATUS_ERROR, "Agent creation failed.\nlibdrm query failed.\n");

  // Reported by libdrm in KHz.
  wallclock_frequency_ = uint64_t(info.gpu_counter_freq) * 1000ull;
#endif

  // Populate region list.
  InitRegionList();

  // Populate cache list.
  InitCacheList();

  // Initialize thresholds for async-scratch handling
  InitAsyncScratchThresholds();
}

GpuAgent::~GpuAgent() {
  if (!(this)->Enabled()) return;

  for (auto& blit : blits_) {
    if (!blit.empty()) {
      hsa_status_t status = blit->Destroy(*this);
      assert(status == HSA_STATUS_SUCCESS);
    }
  }

  if (ape1_base_ != 0) {
    _aligned_free(reinterpret_cast<void*>(ape1_base_));
  }

  scratch_cache_.trim(true);
  scratch_cache_.free_reserve();

  if (scratch_pool_.base() != NULL) {
    hsaKmtFreeMemory(scratch_pool_.base(), scratch_pool_.size());
  }

  system_deallocator()(doorbell_queue_map_);

  if (trap_code_buf_ != NULL) {
    ReleaseShader(trap_code_buf_, trap_code_buf_size_);
  }

  std::for_each(regions_.begin(), regions_.end(), DeleteObject());
  regions_.clear();
}

void GpuAgent::AssembleShader(const char* func_name, AssembleTarget assemble_target,
                              void*& code_buf, size_t& code_buf_size) const {
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
    ASICShader compute_90a;
    ASICShader compute_940;
    ASICShader compute_942;
    ASICShader compute_1010;
    ASICShader compute_10;
    ASICShader compute_11;
  };

  std::map<std::string, CompiledShader> compiled_shaders = {
      {"TrapHandler",
       {
           {NULL, 0, 0, 0},                                             // gfx7
           {kCodeTrapHandler8, sizeof(kCodeTrapHandler8), 2, 4},        // gfx8
           {kCodeTrapHandler9, sizeof(kCodeTrapHandler9), 2, 4},        // gfx9
           {kCodeTrapHandler90a, sizeof(kCodeTrapHandler90a), 2, 4},    // gfx90a
           {NULL, 0, 0, 0},                                             // gfx940
           {NULL, 0, 0, 0},                                             // gfx942
           {kCodeTrapHandler1010, sizeof(kCodeTrapHandler1010), 2, 4},  // gfx1010
           {kCodeTrapHandler10, sizeof(kCodeTrapHandler10), 2, 4},      // gfx10
           {NULL, 0, 0, 0},                                             // gfx11
       }},
      {"TrapHandlerKfdExceptions",
       {
           {NULL, 0, 0, 0},                                                   // gfx7
           {kCodeTrapHandler8, sizeof(kCodeTrapHandler8), 2, 4},              // gfx8
           {kCodeTrapHandlerV2_9, sizeof(kCodeTrapHandlerV2_9), 2, 4},        // gfx9
           {kCodeTrapHandlerV2_9, sizeof(kCodeTrapHandlerV2_9), 2, 4},        // gfx90a
           {kCodeTrapHandlerV2_940, sizeof(kCodeTrapHandlerV2_940), 2, 4},    // gfx940
           {kCodeTrapHandlerV2_940, sizeof(kCodeTrapHandlerV2_940), 2, 4},    // gfx942
           {kCodeTrapHandlerV2_1010, sizeof(kCodeTrapHandlerV2_1010), 2, 4},  // gfx1010
           {kCodeTrapHandlerV2_10, sizeof(kCodeTrapHandlerV2_10), 2, 4},      // gfx10
           {kCodeTrapHandlerV2_11, sizeof(kCodeTrapHandlerV2_11), 2, 4},      // gfx11
       }},
      {"CopyAligned",
       {
           {kCodeCopyAligned7, sizeof(kCodeCopyAligned7), 32, 12},      // gfx7
           {kCodeCopyAligned8, sizeof(kCodeCopyAligned8), 32, 12},      // gfx8
           {kCodeCopyAligned9, sizeof(kCodeCopyAligned9), 32, 12},      // gfx9
           {kCodeCopyAligned9, sizeof(kCodeCopyAligned9), 32, 12},      // gfx90a
           {kCodeCopyAligned940, sizeof(kCodeCopyAligned940), 32, 12},  // gfx940
           {kCodeCopyAligned9, sizeof(kCodeCopyAligned9), 32, 12},      // gfx942
           {kCodeCopyAligned10, sizeof(kCodeCopyAligned10), 32, 12},    // gfx1010
           {kCodeCopyAligned10, sizeof(kCodeCopyAligned10), 32, 12},    // gfx10
           {kCodeCopyAligned11, sizeof(kCodeCopyAligned11), 32, 12},    // gfx11
       }},
      {"CopyMisaligned",
       {
           {kCodeCopyMisaligned7, sizeof(kCodeCopyMisaligned7), 23, 10},      // gfx7
           {kCodeCopyMisaligned8, sizeof(kCodeCopyMisaligned8), 23, 10},      // gfx8
           {kCodeCopyMisaligned9, sizeof(kCodeCopyMisaligned9), 23, 10},      // gfx9
           {kCodeCopyMisaligned9, sizeof(kCodeCopyMisaligned9), 23, 10},      // gfx90a
           {kCodeCopyMisaligned940, sizeof(kCodeCopyMisaligned940), 23, 10},  // gfx940
           {kCodeCopyMisaligned9, sizeof(kCodeCopyMisaligned9), 23, 10},      // gfx942
           {kCodeCopyMisaligned10, sizeof(kCodeCopyMisaligned10), 23, 10},    // gfx1010
           {kCodeCopyMisaligned10, sizeof(kCodeCopyMisaligned10), 23, 10},    // gfx10
           {kCodeCopyMisaligned11, sizeof(kCodeCopyMisaligned11), 23, 10},    // gfx11
       }},
      {"Fill",
       {
           {kCodeFill7, sizeof(kCodeFill7), 19, 8},      // gfx7
           {kCodeFill8, sizeof(kCodeFill8), 19, 8},      // gfx8
           {kCodeFill9, sizeof(kCodeFill9), 19, 8},      // gfx9
           {kCodeFill9, sizeof(kCodeFill9), 19, 8},      // gfx90a
           {kCodeFill940, sizeof(kCodeFill940), 19, 8},  // gfx940
           {kCodeFill9, sizeof(kCodeFill9), 19, 8},      // gfx942
           {kCodeFill10, sizeof(kCodeFill10), 19, 8},    // gfx1010
           {kCodeFill10, sizeof(kCodeFill10), 19, 8},    // gfx10
           {kCodeFill11, sizeof(kCodeFill11), 19, 8},    // gfx11
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
      if((isa_->GetMinorVersion() == 0) && (isa_->GetStepping() == 10)) {
        asic_shader = &compiled_shader_it->second.compute_90a;
      } else if(isa_->GetMinorVersion() == 4) {
        switch(isa_->GetStepping()) {
          case 0:
          case 1:
            asic_shader = &compiled_shader_it->second.compute_940;
            break;
          case 2:
          default:
            asic_shader = &compiled_shader_it->second.compute_942;
            break;
        }
      } else {
        asic_shader = &compiled_shader_it->second.compute_9;
      }
      break;
    case 10:
      if(isa_->GetMinorVersion() == 1)
        asic_shader = &compiled_shader_it->second.compute_1010;
      else
        asic_shader = &compiled_shader_it->second.compute_10;
      break;
    case 11:
        asic_shader = &compiled_shader_it->second.compute_11;
      break;
    default:
      assert(false && "Precompiled shader unavailable for target");
  }

  // Allocate a GPU-visible buffer for the shader.
  size_t header_size =
      (assemble_target == AssembleTarget::AQL ? sizeof(amd_kernel_code_t) : 0);
  code_buf_size = AlignUp(header_size + asic_shader->size, 0x1000);

  code_buf = system_allocator()(code_buf_size, 0x1000, core::MemoryRegion::AllocateExecutable);
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

    // gfx90a, gfx940, gfx941, gfx942
    if ((isa_->GetMajorVersion() == 9) &&
        (((isa_->GetMinorVersion() == 0) && (isa_->GetStepping() == 10)) ||
        (isa_->GetMinorVersion() == 4))) {
      // Program COMPUTE_PGM_RSRC3.ACCUM_OFFSET for 0 ACC VGPRs on gfx90a.
      // FIXME: Assemble code objects from source at build time
      int gran_accvgprs = ((gran_vgprs + 1) * 8) / 4 - 1;
      header->max_scratch_backing_memory_byte_size = uint64_t(gran_accvgprs) << 32;
    }
  }

  // Copy shader code into the GPU-visible buffer.
  memcpy((void*)(uintptr_t(code_buf) + header_size), asic_shader->code,
         asic_shader->size);
}

void GpuAgent::ReleaseShader(void* code_buf, size_t code_buf_size) const {
  system_deallocator()(code_buf);
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
              new MemoryRegion(false, false, false, false, this, mem_props[mem_idx]);

          regions_.push_back(region);

          if (region->IsLocalMemory()) {
            regions_.push_back(
                new MemoryRegion(false, false, false, true, this, mem_props[mem_idx]));
            // Expose VRAM as uncached/fine grain over PCIe (if enabled) or XGMI.
            if ((properties_.HiveID != 0) ||
                (core::Runtime::runtime_singleton_->flag().fine_grain_pcie())) {
              regions_.push_back(
                  new MemoryRegion(true, false, false, false, this, mem_props[mem_idx]));
            }
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
          // Remap offsets defined in kfd_ioctl.h
          HDP_flush_.HDP_MEM_FLUSH_CNTL = (uint32_t*)mem_props[mem_idx].VirtualBaseAddress;
          HDP_flush_.HDP_REG_FLUSH_CNTL = HDP_flush_.HDP_MEM_FLUSH_CNTL + 1;
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
  const size_t max_scratch_device = properties_.NumXcc * MAX_SCRATCH_APERTURE_PER_XCC;
  // For 64-bit linux use max queues unless otherwise specified
  if ((max_scratch_len == 0) || (max_scratch_len > max_scratch_device)) {
    max_scratch_len = max_scratch_device;  // 4GB per XCC aperture max
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

void GpuAgent::InitAsyncScratchThresholds() {
  scratch_limit_async_threshold_ =
      core::Runtime::runtime_singleton_->flag().scratch_single_limit_async();

  if (!scratch_limit_async_threshold_)
    scratch_limit_async_threshold_ =
        DEFAULT_SCRATCH_SINGLE_LIMIT_ASYNC_PER_XCC * properties().NumXcc;
}

void GpuAgent::ReserveScratch()
{
  size_t reserved_sz = core::Runtime::runtime_singleton_->flag().scratch_single_limit();
  size_t available;
  HSAKMT_STATUS err = hsaKmtAvailableMemory(node_id(), &available);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaKmtAvailableMemory failed");
  ScopedAcquire<KernelMutex> lock(&scratch_lock_);
  if (!scratch_cache_.reserved_bytes() && reserved_sz && available > 8 * reserved_sz) {
    HSAuint64 alt_va;
    void* reserved_base = scratch_pool_.alloc(reserved_sz);
    assert(reserved_base && "Could not allocate reserved memory");

    if (hsaKmtMapMemoryToGPU(reserved_base, reserved_sz, &alt_va) == HSAKMT_STATUS_SUCCESS)
      scratch_cache_.reserve(reserved_sz, reserved_base);
    else
      throw AMD::hsa_exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES, "Reserve scratch memory failed.");
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

void GpuAgent::InitLibDrm() {
  HSAKMT_STATUS status;

  HsaAMDGPUDeviceHandle device_handle;
  status = hsaKmtGetAMDGPUDeviceHandle(node_id(), &device_handle);
  if (status != HSAKMT_STATUS_SUCCESS)
    throw AMD::hsa_exception(HSA_STATUS_ERROR,
                             "Agent creation failed.\nlibdrm get device handle failed.\n");

  ldrm_dev_ = (amdgpu_device_handle)device_handle;
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
    const AMD::MemoryRegion* amd_region =
        reinterpret_cast<const AMD::MemoryRegion*>(region);

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

core::Queue* GpuAgent::CreateInterceptibleQueue(void (*callback)(hsa_status_t status,
                                                                 hsa_queue_t* source, void* data),
                                                void* data) {
  // Disabled intercept of internal queues pending tools updates.
  core::Queue* queue = nullptr;
  QueueCreate(minAqlSize_, HSA_QUEUE_TYPE_MULTI, callback, data, 0, 0, &queue);
  if (queue != nullptr)
    core::Runtime::runtime_singleton_->InternalQueueCreateNotify(core::Queue::Convert(queue),
                                                                 this->public_handle());
  return queue;
}

core::Blit* GpuAgent::CreateBlitSdma(bool use_xgmi) {
  AMD::BlitSdmaBase* sdma;
  size_t copy_size_override = 0;
  const size_t copy_size_overrides[2] = {0x3fffff, 0x3fffffff};

  switch (isa_->GetMajorVersion()) {
    case 7:
    case 8:
      sdma = new BlitSdmaV2V3();
      break;
    case 9:
      sdma = new BlitSdmaV4();
      copy_size_override = (isa_->GetMinorVersion() == 0 && isa_->GetStepping() == 10) ||
                            isa_->GetMinorVersion() > 0 ? copy_size_overrides[1] :
                                                          copy_size_overrides[0];
      break;
    case 10:
      sdma = new BlitSdmaV5();
      copy_size_override = isa_->GetMinorVersion() < 3 ? copy_size_overrides[0] :
                                                         copy_size_overrides[1];
      break;
    case 11:
      sdma = new BlitSdmaV5();
      copy_size_override = copy_size_overrides[1];
      break;
    default:
      assert(false && "Unexpected device major version.");
      return nullptr;
  }

  Flag::SDMA_OVERRIDE copy_size_override_setting =
    core::Runtime::runtime_singleton_->flag().enable_sdma_copy_size_override();
  if (copy_size_override_setting == Flag::SDMA_DISABLE) copy_size_override = 0;

  if (sdma->Initialize(*this, use_xgmi, copy_size_override) != HSA_STATUS_SUCCESS) {
    sdma->Destroy(*this);
    delete sdma;
    sdma = nullptr;
  }

  return sdma;
}

core::Blit* GpuAgent::CreateBlitKernel(core::Queue* queue) {
  AMD::BlitKernel* kernl = new AMD::BlitKernel(queue);

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
  auto blit_lambda = [this](bool use_xgmi, lazy_ptr<core::Queue>& queue, bool isHostToDev) {
    Flag::SDMA_OVERRIDE sdma_override = core::Runtime::runtime_singleton_->flag().enable_sdma();

    // User SDMA queues are unstable on gfx8 and unsupported on gfx1013.
    bool use_sdma =
        ((isa_->GetMajorVersion() != 8) && (isa_->GetVersion() != std::make_tuple(10, 1, 3)));
    if (sdma_override != Flag::SDMA_DEFAULT) use_sdma = (sdma_override == Flag::SDMA_ENABLE);

    if (use_sdma && (HSA_PROFILE_BASE == profile_)) {
      // On gfx90a ensure that HostToDevice queue is created first and so is placed on SDMA0.
      if ((!use_xgmi) && (!isHostToDev) && (isa_->GetMajorVersion() == 9) &&
          (isa_->GetMinorVersion() == 0) && (isa_->GetStepping() == 10)) {
        GetBlitObject(BlitHostToDev);
        *blits_[BlitHostToDev];
      }

      auto ret = CreateBlitSdma(use_xgmi);
      if (ret != nullptr) return ret;
    }

    // pending_copy_stat_check_ref_ will prevent unnecessary compute queue creation
    // since there is no graceful way to handle lazy loading when the caller needs to know
    // the status of available SDMA HW resources without a fallback.
    // Call to isSDMA should be used as a proxy error check if !blit_copy_fallback.
    auto ret = pending_copy_stat_check_ref_ ? new AMD::BlitKernel(NULL) :
                                              CreateBlitKernel((*queue).get());
    if (ret == nullptr)
      throw AMD::hsa_exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES, "Blit creation failed.");
    return ret;
  };

  // Determine and instantiate the number of blit objects to
  // engage. The total number is sum of three plus number of
  // sdma-xgmi engines
  uint32_t blit_cnt_ = DefaultBlitCount + properties_.NumSdmaXgmiEngines;
  blits_.resize(blit_cnt_);

  // Initialize blit objects used for D2D, H2D, D2H, and
  // P2P copy operations.
  // -- Blit at index BlitDevToDev(0) deals with copies within
  //    local framebuffer and always engages a Blit Kernel
  // -- Blit at index BlitHostToDev(1) deals with copies from
  //    Host to Device (H2D) and could engage either a Blit
  //    Kernel or sDMA
  // -- Blit at index BlitDevToHost(2) deals with copies from
  //    Device to Host (D2H) and Peer to Peer (P2P) over PCIe.
  //    It could engage either a Blit Kernel or sDMA
  // -- Blit at index DefaultBlitCount(3) and beyond deal
  //    exclusively P2P over xGMI links
  blits_[BlitDevToDev].reset([this]() {
    auto ret = CreateBlitKernel((*queues_[QueueUtility]).get());
    if (ret == nullptr)
      throw AMD::hsa_exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES, "Blit creation failed.");
    return ret;
  });
  blits_[BlitHostToDev].reset(
      [blit_lambda, this]() { return blit_lambda(false, queues_[QueueBlitOnly], true); });
  blits_[BlitDevToHost].reset(
      [blit_lambda, this]() { return blit_lambda(false, queues_[QueueUtility], false); });

  // XGMI engines.
  for (uint32_t idx = DefaultBlitCount; idx < blit_cnt_; idx++) {
    blits_[idx].reset(
        [blit_lambda, this]() { return blit_lambda(true, queues_[QueueUtility], false); });
  }

  // GWS queues.
  InitGWS();
}

void GpuAgent::InitGWS() {
  gws_queue_.queue_.reset([this]() {
    if (properties_.NumGws == 0) return (core::Queue*)nullptr;
    std::unique_ptr<core::Queue> queue(CreateInterceptibleQueue());
    if (queue == nullptr)
      throw AMD::hsa_exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES,
                               "Internal queue creation failed.");

    auto err = static_cast<AqlQueue*>(queue.get())->EnableGWS(1);
    if (err != HSA_STATUS_SUCCESS) throw AMD::hsa_exception(err, "GWS allocation failed.");

    gws_queue_.ref_ct_ = 0;
    return queue.release();
  });
}

void GpuAgent::GWSRelease() {
  ScopedAcquire<KernelMutex> lock(&gws_queue_.lock_);
  gws_queue_.ref_ct_--;
  if (gws_queue_.ref_ct_ != 0) return;
  InitGWS();
}

void GpuAgent::PreloadBlits() {
  for (auto& blit : blits_) {
    blit.touch();
  }
}

hsa_status_t GpuAgent::PostToolsInit() {
  // Defer memory allocation until agents have been discovered.
  InitNumaAllocator();
  InitScratchPool();
  BindTrapHandler();
  InitDma();

  return HSA_STATUS_SUCCESS;
}

hsa_status_t GpuAgent::DmaCopy(void* dst, const void* src, size_t size) {
  return blits_[BlitDevToDev]->SubmitLinearCopyCommand(dst, src, size);
}

void GpuAgent::SetCopyRequestRefCount(bool set) {
  ScopedAcquire<KernelMutex> lock(&blit_lock_);
  while (pending_copy_stat_check_ref_) {
    blit_lock_.Release();
    os::YieldThread();
    blit_lock_.Acquire();
  }
  if (!set && pending_copy_req_ref_) pending_copy_req_ref_--;
  else pending_copy_req_ref_++;
}

void GpuAgent::SetCopyStatusCheckRefCount(bool set) {
  ScopedAcquire<KernelMutex> lock(&blit_lock_);
  while (pending_copy_req_ref_) {
    blit_lock_.Release();
    os::YieldThread();
    blit_lock_.Acquire();
  }
  if (!set && pending_copy_stat_check_ref_) pending_copy_stat_check_ref_--;
  else pending_copy_stat_check_ref_++;
}

// Assign direct peer gang factor to GPU
void GpuAgent::RegisterGangPeer(core::Agent& peer, unsigned int max_bandwidth_factor) {
  gang_peers_info_[peer.public_handle().handle] = max_bandwidth_factor;
}

// Destroy gang signal
static bool GangCopyCompleteHandler(hsa_signal_value_t, void *arg ) {
  core::Signal *gang_signal = reinterpret_cast<core::Signal*>(arg);
  if (gang_signal->IsValid()) {
    gang_signal->DestroySignal();
  }
  return true;
}

hsa_status_t GpuAgent::DmaCopy(void* dst, core::Agent& dst_agent,
                               const void* src, core::Agent& src_agent,
                               size_t size,
                               std::vector<core::Signal*>& dep_signals,
                               core::Signal& out_signal) {
  if (profiling_enabled()) {
    // Track the agent so we could translate the resulting timestamp to system
    // domain correctly.
    out_signal.async_copy_agent(core::Agent::Convert(this->public_handle()));
  }

  // Calculate the number of gang items
  unsigned int gang_factor = 1;
  if (core::Runtime::runtime_singleton_->flag().enable_sdma_gang() != Flag::SDMA_DISABLE &&
      size >= 4096 && dst_agent.device_type() == core::Agent::kAmdGpuDevice)
    gang_factor = gang_peers_info_[dst_agent.public_handle().handle];
  // Use non-D2D (auxillary) SDMA engines in the event of xGMI D2D support
  // when xGMI SDMA context is not available.
  bool has_aux_gang = gang_factor > 1 &&
                      gang_factor >= properties_.NumSdmaEngines &&
                      !!!properties_.NumSdmaXgmiEngines;
  if (gang_factor > 1) {
    gang_factor = has_aux_gang ?
                      std::min(gang_factor, properties_.NumSdmaEngines) :
                      std::min(gang_factor, properties_.NumSdmaXgmiEngines);
  }

  ScopedAcquire<KernelMutex> lock(&sdma_gang_lock_);
  if (gang_factor == 1) sdma_gang_lock_.Release();
  // Manage internal gang signals
  std::vector<core::Signal*> gang_signals;
  if (gang_factor > 1) {
    for (int i = 0; i < gang_factor - 1; i++) {
      core::Signal *gang_signal;

      // Initial value is 2 where 1 is for gang-leader to ack and
      // 1 for non-leader gang item to decrement
      gang_signal = new core::DefaultSignal(2);

      // Fall back to non-gang copy
      if (!gang_signal->IsValid()) {
        for (int j = 0; j < gang_signals.size(); j++) gang_signals[j]->DestroySignal();
        gang_factor = 1;
        break;
      }

      core::Runtime::runtime_singleton_->SetAsyncSignalHandler(
                                         core::Signal::Convert(gang_signal),
                                         HSA_SIGNAL_CONDITION_EQ, 0, GangCopyCompleteHandler,
                                         reinterpret_cast<void*>(gang_signal));
      gang_signals.push_back(gang_signal);
    }
  }

  // Bind the Blit object that will drive this copy operation
  size_t offset = 0, remainder_size = size;
  int gang_sig_count = 0;
  for (int i = 0; i < gang_factor; i++) {
    // Set leader and gang status to blit
    SetCopyRequestRefCount(true);
    MAKE_SCOPE_GUARD([&]() { SetCopyRequestRefCount(false); });
    lazy_ptr<core::Blit>& blit = gang_factor > 1 ?
                                 (has_aux_gang ? blits_[i + 1] : blits_[i + DefaultBlitCount]) :
                                 GetBlitObject(dst_agent, src_agent, size);
    blit->GangLeader(gang_factor > 1 && !i);

    hsa_status_t stat;
    size_t chunk = std::min(remainder_size, (size + gang_factor - 1)/gang_factor);
    if (!blit->GangLeader() && !gang_signals.empty()) {
      stat = blit->SubmitLinearCopyCommand(reinterpret_cast<uint8_t*>(dst) + offset,
                                           reinterpret_cast<const uint8_t*>(src) + offset,
                                           chunk, dep_signals,
                                           *gang_signals[gang_sig_count], gang_signals);
      gang_sig_count++;
    } else {
      stat = blit->SubmitLinearCopyCommand(reinterpret_cast<uint8_t*>(dst) + offset,
                                           reinterpret_cast<const uint8_t*>(src) + offset,
                                           chunk, dep_signals,
                                           out_signal, gang_signals);
    }

    if (stat)
      return stat;

    offset += chunk;
    remainder_size -= chunk;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t GpuAgent::DmaCopyOnEngine(void* dst, core::Agent& dst_agent,
                               const void* src, core::Agent& src_agent,
                               size_t size,
                               std::vector<core::Signal*>& dep_signals,
                               core::Signal& out_signal,
                               int engine_offset,
                               bool force_copy_on_sdma) {
  // At this point it is guaranteed that one of
  // the two devices is a GPU, potentially both
  assert(((src_agent.device_type() == core::Agent::kAmdGpuDevice) ||
          (dst_agent.device_type() == core::Agent::kAmdGpuDevice)) &&
         ("Both devices are CPU agents which is not expected"));

  if (engine_offset > properties_.NumSdmaEngines + properties_.NumSdmaXgmiEngines) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  // check if dst and src are the same gpu or over xGMI.
  bool is_same_gpu = (src_agent.public_handle().handle == dst_agent.public_handle().handle) &&
                     (dst_agent.public_handle().handle == public_handle_.handle);

  bool is_p2p = !is_same_gpu && src_agent.device_type() == core::Agent::kAmdGpuDevice &&
                                dst_agent.device_type() == core::Agent::kAmdGpuDevice;

  if ((is_p2p &&
      core::Runtime::runtime_singleton_->flag().enable_peer_sdma() == Flag::SDMA_DISABLE) ||
      core::Runtime::runtime_singleton_->flag().enable_sdma() == Flag::SDMA_DISABLE) {
    // Note  that VDI/HIP will call DmaCopy instead of DmaCopyOnEngine for P2P copies, but
    // we still want to handle force Blit Kernels in this function in case other libraries
    // decide to use DmaCopyOnEngine for P2P copies

    engine_offset = BlitDevToDev;
  } else {
    bool is_xgmi = is_p2p && dst_agent.HiveId() && src_agent.HiveId() == dst_agent.HiveId() &&
                         properties_.NumSdmaXgmiEngines;

    // Due to a RAS issue, GFX90a can only support H2D copies on SDMA0
    bool is_h2d_blit = (src_agent.device_type() == core::Agent::kAmdCpuDevice &&
      dst_agent.device_type() == core::Agent::kAmdGpuDevice);
    bool limit_h2d_blit = isa_->GetVersion() == core::Isa::Version(9, 0, 10);

    // Ensure engine selection is within proper range based on transfer type
    if ((is_xgmi && engine_offset <= properties_.NumSdmaEngines) ||
        (!is_xgmi && engine_offset > (properties_.NumSdmaEngines +
                                      properties_.NumSdmaXgmiEngines)) ||
          (!is_h2d_blit && !is_same_gpu && limit_h2d_blit &&
            engine_offset == BlitHostToDev)) {
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }

    engine_offset = is_same_gpu && !force_copy_on_sdma ? BlitDevToDev : engine_offset;
  }

  SetCopyRequestRefCount(true);
  MAKE_SCOPE_GUARD([&]() { SetCopyRequestRefCount(false); });
  lazy_ptr<core::Blit>& blit = GetBlitObject(engine_offset);

  if (profiling_enabled()) {
    // Track the agent so we could translate the resulting timestamp to system
    // domain correctly.
    out_signal.async_copy_agent(core::Agent::Convert(this->public_handle()));
  }

  std::vector<core::Signal*> gang_signals(0);

  hsa_status_t stat = blit->SubmitLinearCopyCommand(dst, src, size, dep_signals, out_signal,
                                                    gang_signals);

  return stat;
}

bool GpuAgent::DmaEngineIsFree(uint32_t engine_offset) {
  SetCopyStatusCheckRefCount(true);
  MAKE_SCOPE_GUARD([&]() { SetCopyStatusCheckRefCount(false); });
  bool is_free = !!!(sdma_blit_used_mask_ & (1 << engine_offset)) ||
                    (blits_[engine_offset]->isSDMA() &&
                     !!!blits_[engine_offset]->PendingBytes());
  return is_free;
}

hsa_status_t GpuAgent::DmaCopyStatus(core::Agent& dst_agent, core::Agent& src_agent,
                                     uint32_t *engine_ids_mask) {
  assert(((src_agent.device_type() == core::Agent::kAmdGpuDevice) ||
          (dst_agent.device_type() == core::Agent::kAmdGpuDevice)) &&
         ("Both devices are CPU agents which is not expected"));

  *engine_ids_mask = 0;
  if (src_agent.device_type() == core::Agent::kAmdGpuDevice &&
                   dst_agent.device_type() == core::Agent::kAmdGpuDevice &&
                     dst_agent.HiveId() && src_agent.HiveId() == dst_agent.HiveId() &&
                       properties_.NumSdmaXgmiEngines) {
    //Find a free xGMI SDMA engine
    for (int i = 0; i < properties_.NumSdmaXgmiEngines; i++) {
      if (DmaEngineIsFree(DefaultBlitCount + i)) {
        *engine_ids_mask |= (HSA_AMD_SDMA_ENGINE_2 << i);
      }
    }
  } else {
    bool is_h2d_blit = (src_agent.device_type() == core::Agent::kAmdCpuDevice &&
      dst_agent.device_type() == core::Agent::kAmdGpuDevice);
    // Due to a RAS issue, GFX90a can only support H2D copies on SDMA0
    bool limit_h2d_blit = isa_->GetVersion() == core::Isa::Version(9, 0, 10);

    // Check if H2D is free
    if (DmaEngineIsFree(BlitHostToDev)) {
      if (is_h2d_blit || !limit_h2d_blit) {
        *engine_ids_mask |= HSA_AMD_SDMA_ENGINE_0;
      }
    }

    // Check is D2H is free
    if (DmaEngineIsFree(BlitDevToHost)) {
      *engine_ids_mask |= properties_.NumSdmaEngines > 1 ?
                          HSA_AMD_SDMA_ENGINE_1 :
                          HSA_AMD_SDMA_ENGINE_0;
    }
    // Find a free xGMI SDMA engine for H2D/D2H though it may be lower bandwidth
    for (int i = 0; i < properties_.NumSdmaXgmiEngines; i++) {
      if (DmaEngineIsFree(DefaultBlitCount + i)) {
         *engine_ids_mask |= (HSA_AMD_SDMA_ENGINE_2 << i);
      }
    }
  }

  return !!(*engine_ids_mask) ? HSA_STATUS_SUCCESS : HSA_STATUS_ERROR_OUT_OF_RESOURCES;
}

hsa_status_t GpuAgent::DmaCopyRect(const hsa_pitched_ptr_t* dst, const hsa_dim3_t* dst_offset,
                                   const hsa_pitched_ptr_t* src, const hsa_dim3_t* src_offset,
                                   const hsa_dim3_t* range, hsa_amd_copy_direction_t dir,
                                   std::vector<core::Signal*>& dep_signals,
                                   core::Signal& out_signal) {
  if (isa_->GetMajorVersion() < 9) return HSA_STATUS_ERROR_INVALID_AGENT;

  SetCopyRequestRefCount(true);
  MAKE_SCOPE_GUARD([&]() { SetCopyRequestRefCount(false); });
  lazy_ptr<core::Blit>& blit = GetBlitObject((dir == hsaHostToDevice) ? BlitHostToDev :
                                                                        BlitDevToHost);

  if (!blit->isSDMA()) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

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
  for (auto& blit : blits_) {
    if (!blit.empty()) {
      const hsa_status_t stat = blit->EnableProfiling(enable);
      if (stat != HSA_STATUS_SUCCESS) {
        return stat;
      }
    }
  }

  if (enable) CheckClockTicks();

  return HSA_STATUS_SUCCESS;
}

void GpuAgent::GetInfoMemoryProperties(uint8_t value[8]) const {
  auto setFlag = [&](uint32_t bit) {
    assert(bit < 8 * 8 && "Flag value exceeds input parameter size");

    uint index = bit / 8;
    uint subBit = bit % 8;
    ((uint8_t*)value)[index] |= 1 << subBit;
  };

  // Fill the HSA_AMD_MEMORY_PROPERTY_AGENT_IS_APU flag
  switch (properties_.DeviceId) {
    case 0x15DD: /* gfx902 - Raven Ridge */
    case 0x15D8: /* gfx909 - Raven Ridge 2 */
    case 0x1636: /* gfx90c - Renoir */
    case 0x74A0: /* gfx940 and gfx942-APU */
      setFlag(HSA_AMD_MEMORY_PROPERTY_AGENT_IS_APU);
      break;
    default:
      break;
  }
}

hsa_status_t GpuAgent::GetInfo(hsa_agent_info_t attribute, void* value) const {
  // agent, and vendor name size limit
  const size_t attribute_u = static_cast<size_t>(attribute);
  // agent, and vendor name length limit excluding terminating nul character.
  constexpr size_t hsa_name_size = 63;

  const bool isa_has_image_support =
      (isa_->GetMajorVersion() == 9 && isa_->GetMinorVersion() == 4) ? false : true;

  switch (attribute_u) {
    case HSA_AGENT_INFO_NAME: {
      std::string name = isa_->GetProcessorName();
      assert(name.size() <= hsa_name_size);
      std::memset(value, 0, hsa_name_size);
      char* temp = reinterpret_cast<char*>(value);
      std::strcpy(temp, name.c_str());
      break;
    }
    case HSA_AGENT_INFO_VENDOR_NAME:
      std::memset(value, 0, hsa_name_size);
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
      if (isa_->GetMajorVersion() >= 8) {
        *((bool*)value) = true;
      } else {
        *((bool*)value) = false;
      }
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
    case HSA_AGENT_INFO_CACHE_SIZE: {
      std::memset(value, 0, sizeof(uint32_t) * 4);
      assert(cache_props_.size() > 0 && "GPU cache info missing.");
      const size_t num_cache = cache_props_.size();
      for (size_t i = 0; i < num_cache; ++i) {
        const uint32_t line_level = cache_props_[i].CacheLevel;
        if (reinterpret_cast<uint32_t*>(value)[line_level - 1] == 0)
          reinterpret_cast<uint32_t*>(value)[line_level - 1] = cache_props_[i].CacheSize * 1024;
      }
    } break;
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
      if (!isa_has_image_support)
        *((uint32_t*)value) = 0;
      else
        return hsa_amd_image_get_info_max_dim(public_handle(), attribute, value);
      break;
    case HSA_EXT_AGENT_INFO_MAX_IMAGE_RD_HANDLES:
      // TODO: hardcode based on OCL constants.
      *((uint32_t*)value) = isa_has_image_support ? 128 : 0;
      break;
    case HSA_EXT_AGENT_INFO_MAX_IMAGE_RORW_HANDLES:
      *((uint32_t*)value) = isa_has_image_support ? 64 : 0;
      break;
    case HSA_EXT_AGENT_INFO_MAX_SAMPLER_HANDLERS:
      *((uint32_t*)value) = isa_has_image_support ? 16 : 0;
      break;
    case HSA_AMD_AGENT_INFO_CHIP_ID:
      *((uint32_t*)value) = properties_.DeviceId;
      break;
    case HSA_AMD_AGENT_INFO_CACHELINE_SIZE:
      for (auto& cache : cache_props_) {
        if ((cache.CacheLevel == 2) && (cache.CacheLineSize != 0)) {
          *((uint32_t*)value) = cache.CacheLineSize;
          return HSA_STATUS_SUCCESS;
        }
      }
      // Fallback for when KFD is returning zero.
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
    case HSA_AMD_AGENT_INFO_DOMAIN:
      *((uint32_t*)value) = static_cast<uint32_t>(properties_.Domain);
      break;
    case HSA_AMD_AGENT_INFO_COOPERATIVE_QUEUES:
      *((bool*)value) = properties_.NumGws != 0;
      break;
    case HSA_AMD_AGENT_INFO_UUID: {
      uint64_t uuid_value = static_cast<uint64_t>(properties_.UniqueID);

      // Either device does not support UUID e.g. a Gfx8 device,
      // or runtime is using an older thunk library that does not
      // support UUID's
      if (uuid_value == 0) {
        char uuid_tmp[] = "GPU-XX";
        snprintf((char*)value, sizeof(uuid_tmp), "%s", uuid_tmp);
        break;
      }

      // Device supports UUID, build UUID string to return
      std::stringstream ss;
      ss << "GPU-" << std::setfill('0') << std::setw(sizeof(uint64_t) * 2) << std::hex
         << uuid_value;
      snprintf((char*)value, (ss.str().length() + 1), "%s", (char*)ss.str().c_str());
      break;
    }
    case HSA_AMD_AGENT_INFO_ASIC_REVISION:
      *((uint32_t*)value) = static_cast<uint32_t>(properties_.Capability.ui32.ASICRevision);
      break;
    case HSA_AMD_AGENT_INFO_SVM_DIRECT_HOST_ACCESS:
      assert(regions_.size() != 0 && "No device local memory found!");
      *((bool*)value) = properties_.Capability.ui32.CoherentHostAccess == 1;
      break;
    case HSA_AMD_AGENT_INFO_COOPERATIVE_COMPUTE_UNIT_COUNT:
      if (core::Runtime::runtime_singleton_->flag().coop_cu_count() &&
          !(core::Runtime::runtime_singleton_->flag().cu_mask(enum_index_).empty())) {
        debug_warning(false && "Cooperative launch and CU masking are currently incompatible!");
        *((uint32_t*)value) = 0;
        break;
      }

      if (core::Runtime::runtime_singleton_->flag().coop_cu_count() &&
          (isa_->GetMajorVersion() == 9) && (isa_->GetMinorVersion() == 0) &&
          (isa_->GetStepping() == 10)) {
        uint32_t count = 0;
        hsa_status_t err = GetInfo((hsa_agent_info_t)HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT, &count);
        assert(err == HSA_STATUS_SUCCESS && "CU count query failed.");
        *((uint32_t*)value) = (count & 0xFFFFFFF8) - 8;  // value = floor(count/8)*8-8
        break;
      }
      return GetInfo((hsa_agent_info_t)HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT, value);
    case HSA_AMD_AGENT_INFO_MEMORY_AVAIL: {
      HSAuint64 availableBytes;
      HSAKMT_STATUS status;

      status = hsaKmtAvailableMemory(node_id(), &availableBytes);

      if (status != HSAKMT_STATUS_SUCCESS) return HSA_STATUS_ERROR_INVALID_ARGUMENT;

      for (auto r : regions()) availableBytes += ((AMD::MemoryRegion*)r)->GetCacheSize();

      availableBytes += scratch_cache_.free_bytes() - scratch_cache_.reserved_bytes();

      *((uint64_t*)value) = availableBytes;
      break;
    }
    case HSA_AMD_AGENT_INFO_TIMESTAMP_FREQUENCY:
      *((uint64_t*)value) = wallclock_frequency_;
      break;
    case HSA_AMD_AGENT_INFO_ASIC_FAMILY_ID:
      *((uint32_t*)value) = static_cast<uint32_t>(properties_.FamilyID);
      break;
    case HSA_AMD_AGENT_INFO_UCODE_VERSION:
      *((uint32_t*)value) = static_cast<uint32_t>(properties_.EngineId.ui32.uCode);
      break;
    case HSA_AMD_AGENT_INFO_SDMA_UCODE_VERSION:
      *((uint32_t*)value) = static_cast<uint32_t>(properties_.uCodeEngineVersions.uCodeSDMA);
      break;
    case HSA_AMD_AGENT_INFO_NUM_SDMA_ENG:
      *((uint32_t*)value) = static_cast<uint32_t>(properties_.NumSdmaEngines);
      break;
    case HSA_AMD_AGENT_INFO_NUM_SDMA_XGMI_ENG:
      *((uint32_t*)value) = static_cast<uint32_t>(properties_.NumSdmaXgmiEngines);
      break;
    case HSA_AMD_AGENT_INFO_IOMMU_SUPPORT:
      if (properties_.Capability.ui32.HSAMMUPresent)
        *((hsa_amd_iommu_version_t*)value) = HSA_IOMMU_SUPPORT_V2;
      else
        *((hsa_amd_iommu_version_t*)value) = HSA_IOMMU_SUPPORT_NONE;
      break;
    case HSA_AMD_AGENT_INFO_NUM_XCC:
      *((uint32_t*)value) = static_cast<uint32_t>(properties_.NumXcc);
      break;
    case HSA_AMD_AGENT_INFO_DRIVER_UID:
      *((uint32_t*)value) = KfdGpuID();
      break;
    case HSA_AMD_AGENT_INFO_NEAREST_CPU:
      *((hsa_agent_t*)value) = GetNearestCpuAgent()->public_handle();
      break;
    case HSA_AMD_AGENT_INFO_MEMORY_PROPERTIES:
      memset(value, 0, sizeof(uint8_t) * 8);
      GetInfoMemoryProperties((uint8_t*)value);
      break;
    case HSA_AMD_AGENT_INFO_AQL_EXTENSIONS:
      memset(value, 0, sizeof(uint8_t) * 8);
      /* Not yet implemented */
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
  // Handle GWS queues.
  if (queue_type == HSA_QUEUE_TYPE_COOPERATIVE) {
    ScopedAcquire<KernelMutex> lock(&gws_queue_.lock_);
    auto ret = (*gws_queue_.queue_).get();
    if (ret != nullptr) {
      gws_queue_.ref_ct_++;
      *queue = ret;
      return HSA_STATUS_SUCCESS;
    }
    return HSA_STATUS_ERROR_INVALID_QUEUE_CREATION;
  }

  // AQL queues must be a power of two in length.
  if (!IsPowerOfTwo(size)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  // Enforce max size
  if (size > maxAqlSize_) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // Enforce min size
  if (size < minAqlSize_) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  // Allocate scratch memory
  ScratchInfo scratch = {0};
  if (private_segment_size == UINT_MAX) {
    private_segment_size = (profile_ == HSA_PROFILE_BASE) ? 0 : scratch_per_thread_;
  }

  if (private_segment_size > 262128) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // Asynchronous reclaim flag bit is set by CP FW on queue-connect, we will update this when
  // we get the first scratch request.
  scratch.async_reclaim = false;

  scratch.main_lanes_per_wave = 64;
  scratch.main_size_per_thread = AlignUp(private_segment_size, 1024 / scratch.main_lanes_per_wave);
  if (scratch.main_size_per_thread > 262128) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }
  scratch.main_size_per_thread = private_segment_size;

  const uint32_t num_cu = properties_.NumFComputeCores / properties_.NumSIMDPerCU;
  scratch.main_size = scratch.main_size_per_thread * properties_.MaxSlotsScratchCU *
      scratch.main_lanes_per_wave * num_cu;
  scratch.main_queue_base = nullptr;
  scratch.main_queue_process_offset = 0;

  MAKE_NAMED_SCOPE_GUARD(scratchGuard, [&]() { ReleaseQueueMainScratch(scratch); });

  if (scratch.main_size != 0) {
    AcquireQueueMainScratch(scratch);
    if (scratch.main_queue_base == nullptr) {
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  }

  // Ensure utility queue has been created.
  // Deferring longer risks exhausting queue count before ISA upload and invalidation capability is
  // ensured.
  queues_[QueueUtility].touch();

  // Create an HW AQL queue
  auto aql_queue =
      new AqlQueue(this, size, node_id(), scratch, event_callback, data, is_kv_device_);
  *queue = aql_queue;
  aql_queues_.push_back(aql_queue);

  if (doorbell_queue_map_) {
    // Calculate index of the queue doorbell within the doorbell aperture.
    auto doorbell_addr = uintptr_t(aql_queue->signal_.hardware_doorbell_ptr);
    auto doorbell_idx = (doorbell_addr >> 3) & (MAX_NUM_DOORBELLS - 1);
    doorbell_queue_map_[doorbell_idx] = &aql_queue->amd_queue_;
  }

  scratchGuard.Dismiss();
  return HSA_STATUS_SUCCESS;
}

void GpuAgent::AcquireQueueMainScratch(ScratchInfo& scratch) {
  assert(scratch.main_queue_base == nullptr &&
         "AcquireQueueMainScratch called while holding scratch.");
  bool need_queue_scratch_base = (isa_->GetMajorVersion() > 8);

  if (scratch.main_size == 0) {
    scratch.main_size = queue_scratch_len_;
    scratch.main_size_per_thread = scratch_per_thread_;
  }
  scratch.retry = false;

  // Fail scratch allocation if per wave limits are exceeded.
  uint64_t size_per_wave = AlignUp(scratch.main_size_per_thread * properties_.WaveFrontSize, 1024);
  if (size_per_wave > MAX_WAVE_SCRATCH) return;

  /*
  Determine size class needed.

  Scratch allocations come in two flavors based on how it is retired.  Small allocations may be
  kept bound to a queue and reused by firmware.  This memory can not be reclaimed by the runtime
  on demand so must be kept small to avoid egregious OOM conditions.  Other allocations, aka large,
  may be used by firmware only for one dispatch and are then surrendered to the runtime.  This has
  significant latency so we don't want to make all scratch allocations large (ie single use).

  Note that the designation "large" is for contrast with "small", which must really be small
  amounts of memory, and does not always imply a large quantity of memory is needed.  Other
  properties of the allocation may require single use and so qualify the allocation or use as
  "large".

  Here we decide on the boundaries for small scratch allocations.  Both the largest small single
  allocation and the maximum amount of memory bound by small allocations are limited.  Additionally
  some legacy devices do not support large scratch.

  For small scratch we must allocate enough memory for every physical scratch slot.
  For large scratch compute the minimum memory needed to run the dispatch without limiting
  occupancy.
  Limit total bound small scratch allocations to 1/8th of scratch pool and 1/4 of that for a single
  allocation.
  */
  bool large;

  ScopedAcquire<KernelMutex> lock(&scratch_lock_);
  const size_t small_limit = scratch_pool_.size() >> 3;
  bool use_reclaim = true;

  large = (scratch.main_size > scratch.use_once_limit) ||
      ((scratch_pool_.size() - scratch_pool_.remaining() - scratch_cache_.free_bytes() +
        scratch.main_size) > small_limit);

  if ((isa_->GetMajorVersion() < 8) ||
      core::Runtime::runtime_singleton_->flag().no_scratch_reclaim()) {
    large = false;
    use_reclaim = false;
  }

  // If large is selected then the scratch will not be retained.
  // In that case allocate the minimum necessary for the dispatch since we don't need all slots.
  if (large) scratch.main_size = scratch.dispatch_size;

  // Ensure mapping will be in whole pages.
  scratch.main_size = AlignUp(scratch.main_size, 4096);

  /*
  Sequence of attempts is:
    check cache
    attempt a new allocation
    trim unused blocks from cache
    attempt a new allocation
    check cache for sufficient used block, steal and wait (not implemented)
    trim used blocks from cache, evaluate retry
    reduce occupancy
  */

  // Lambda called in place.
  // Used to allow exit from nested loops.
  [&]() {
    // Check scratch cache
    scratch.large = large;
    if (scratch_cache_.allocMain(scratch)) return;

    // Attempt new allocation.
    for (int i = 0; i < 3; i++) {
      if (large)
        scratch.main_queue_base = scratch_pool_.alloc_high(scratch.main_size);
      else
        scratch.main_queue_base = scratch_pool_.alloc(scratch.main_size);

      scratch.large = large | (scratch.main_queue_base > scratch_pool_.high_split());
      assert(((!scratch.large) | use_reclaim) && "Large scratch used with reclaim disabled.");

      if (scratch.main_queue_base != nullptr) {
        HSAuint64 alternate_va;
        if ((profile_ == HSA_PROFILE_FULL) ||
            (hsaKmtMapMemoryToGPU(scratch.main_queue_base, scratch.main_size, &alternate_va) ==
             HSAKMT_STATUS_SUCCESS)) {
          if (scratch.large) scratch_used_large_ += scratch.main_size;
          scratch_cache_.insertMain(scratch);
          return;
        }
      }

      // Scratch request failed allocation or mapping.
      scratch_pool_.free(scratch.main_queue_base);
      scratch.main_queue_base = nullptr;

      // Release cached scratch and retry.
      // First iteration trims unused blocks, second trims all. 3rd uses reserved memory
      switch (i) {
        case 0:
          scratch_cache_.trim(false);
          break;
        case 1:
          scratch_cache_.trim(true);
          break;
        case 2:
          if (scratch_cache_.use_reserved(scratch)) return;
      }
    }

    // Retry if large may yield needed space.
    if (scratch_used_large_ != 0) {
      if (AddScratchNotifier(scratch.queue_retry, 0x8000000000000000ull)) scratch.retry = true;
      return;
    }

    // Fail scratch allocation if reducing occupancy is disabled.
    if (scratch.cooperative || (!use_reclaim) ||
        core::Runtime::runtime_singleton_->flag().no_scratch_thread_limiter())
      return;

    // Attempt to trim the maximum number of concurrent waves to allow scratch to fit.
    if (core::Runtime::runtime_singleton_->flag().enable_queue_fault_message())
      debug_print("Failed to map requested scratch (%ld) - reducing queue occupancy.\n",
                  scratch.main_size);
    const uint64_t num_cus = properties_.NumFComputeCores / properties_.NumSIMDPerCU;
    const uint64_t se_per_xcc = properties_.NumShaderBanks / properties_.NumXcc;

    const uint64_t total_waves = scratch.main_size / size_per_wave;
    uint64_t waves_per_cu = AlignUp(total_waves / num_cus, scratch.main_waves_per_group);

    while (waves_per_cu != 0) {
      size_t size = waves_per_cu * num_cus * size_per_wave;
      void* base = scratch_pool_.alloc_high(size);
      HSAuint64 alternate_va;
      if ((base != nullptr) &&
          ((profile_ == HSA_PROFILE_FULL) ||
           (hsaKmtMapMemoryToGPU(base, size, &alternate_va) == HSAKMT_STATUS_SUCCESS))) {
        // Scratch allocated and either full profile or map succeeded.
        scratch.main_queue_base = base;
        scratch.main_size = size;
        scratch.large = true;
        scratch_used_large_ += scratch.main_size;
        scratch_cache_.insertMain(scratch);
        if (core::Runtime::runtime_singleton_->flag().enable_queue_fault_message())
          debug_print("  %ld scratch mapped, %.2f%% occupancy.\n", scratch.main_size,
                      float(waves_per_cu * num_cus) / scratch.dispatch_slots * 100.0f);
        return;
      }
      scratch_pool_.free(base);

      // Wave count must be divisible by #SEs in an XCC. If occupancy must be reduced
      // such that waves_per_cu < waves_per_group, continue reducing by #SEs per XCC
      // (only allowed if waves_per_group is a multiple #SEs per XCC).
      waves_per_cu -= (waves_per_cu <= scratch.main_waves_per_group &&
                       se_per_xcc < scratch.main_waves_per_group &&
                       scratch.main_waves_per_group % se_per_xcc == 0)
                       ? se_per_xcc
                       : scratch.main_waves_per_group;
    }

    // Failed to allocate minimal scratch
    assert(scratch.main_queue_base == nullptr && "bad scratch data");
    if (core::Runtime::runtime_singleton_->flag().enable_queue_fault_message())
      debug_print("  Could not allocate scratch for one wave per CU.\n");
    return;
  }();

  scratch.main_queue_process_offset = need_queue_scratch_base
      ? uintptr_t(scratch.main_queue_base)
      : uintptr_t(scratch.main_queue_base) - uintptr_t(scratch_pool_.base());
}

void GpuAgent::ReleaseQueueMainScratch(ScratchInfo& scratch) {
  ScopedAcquire<KernelMutex> lock(&scratch_lock_);
  if (scratch.main_queue_base == nullptr) return;

  scratch_cache_.freeMain(scratch);
  scratch.main_queue_base = nullptr;
}

void GpuAgent::AcquireQueueAltScratch(ScratchInfo& scratch) {
  assert(scratch.async_reclaim && "Acquire Alt Scratch when FW does not support it");
  assert(scratch.alt_queue_base == nullptr &&
         "AcquireQueueAltScratch called while holding alt scratch.");

  // Fail scratch allocation if per wave limits are exceeded.
  uint64_t size_per_wave = AlignUp(scratch.alt_size_per_thread * properties_.WaveFrontSize, 1024);
  if (size_per_wave > MAX_WAVE_SCRATCH) return;

  ScopedAcquire<KernelMutex> lock(&scratch_lock_);

  // Ensure mapping will be in whole pages.
  scratch.alt_size = AlignUp(scratch.alt_size, 4096);

  /*
  Sequence of attempts is:
    check cache
    attempt a new allocation
    trim unused blocks from cache
    attempt a new allocation
    check cache for sufficient used block, steal and wait (not implemented)
    trim used blocks from cache, evaluate retry
  */

  // Lambda called in place.
  // Used to allow exit from nested loops.
  [&]() {
    // Check scratch cache
    if (scratch_cache_.allocAlt(scratch)) return;

    // Attempt new allocation.
    for (int i = 0; i < 2; i++) {
      scratch.alt_queue_base = scratch_pool_.alloc(scratch.alt_size);
      if (scratch.alt_queue_base != nullptr) {
        HSAuint64 alternate_va;
        if ((profile_ == HSA_PROFILE_FULL) ||
            (hsaKmtMapMemoryToGPU(scratch.alt_queue_base, scratch.alt_size, &alternate_va) ==
             HSAKMT_STATUS_SUCCESS)) {
          scratch_cache_.insertAlt(scratch);
          return;
        }
      }

      // Scratch request failed allocation or mapping.
      scratch_pool_.free(scratch.alt_queue_base);
      scratch.alt_queue_base = nullptr;

      // Release cached scratch and retry.
      // First iteration trims unused blocks, second trims all. 3rd uses reserved memory
      switch (i) {
        case 0:
          scratch_cache_.trim(false);
          break;
        case 1:
          scratch_cache_.trim(true);
          break;
      }
    }

    if (core::Runtime::runtime_singleton_->flag().enable_queue_fault_message())
      debug_print("  Could not allocate alt scratch.\n");
    return;
  }();

  scratch.alt_queue_process_offset = uintptr_t(scratch.alt_queue_base);
}

void GpuAgent::ReleaseQueueAltScratch(ScratchInfo& scratch) {
  ScopedAcquire<KernelMutex> lock(&scratch_lock_);
  if (scratch.alt_queue_base == nullptr) return;

  scratch_cache_.freeAlt(scratch);
  scratch.alt_queue_base = nullptr;
}

void GpuAgent::ReleaseScratch(void* base, size_t size, bool large) {
  if (profile_ == HSA_PROFILE_BASE) {
    if (HSAKMT_STATUS_SUCCESS != hsaKmtUnmapMemoryToGPU(base)) {
      assert(false && "Unmap scratch subrange failed!");
    }
  }
  scratch_pool_.free(base);

  if (large) scratch_used_large_ -= size;

  // Notify waiters that additional scratch may be available.
  for (auto notifier : scratch_notifiers_) {
    HSA::hsa_signal_or_relaxed(notifier.first, notifier.second);
  }
  ClearScratchNotifiers();
}

// Go through all the AQL queues and try to release scratch memory
void GpuAgent::AsyncReclaimScratchQueues() {
  for (auto iter : aql_queues_) {
    auto aqlQueue = static_cast<AqlQueue*>(iter);
    aqlQueue->AsyncReclaimMainScratch();
    aqlQueue->AsyncReclaimAltScratch();
  }
}

hsa_status_t GpuAgent::SetAsyncScratchThresholds(size_t use_once_limit) {
  if (use_once_limit > properties_.NumXcc * MAX_SCRATCH_APERTURE_PER_XCC)
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;

  scratch_limit_async_threshold_ = use_once_limit;

  for (auto iter : aql_queues_) {
    auto aqlQueue = static_cast<AqlQueue*>(iter);
    aqlQueue->CheckScratchLimits();
  }

  return HSA_STATUS_SUCCESS;
}

void GpuAgent::TranslateTime(core::Signal* signal, hsa_amd_profiling_dispatch_time_t& time) {
  uint64_t start, end;
  signal->GetRawTs(false, start, end);
  // Order is important, we want to translate the end time first to ensure that packet duration is
  // not impacted by clock measurement latency jitter.
  time.end = TranslateTime(end);
  time.start = TranslateTime(start);

  if ((start == 0) || (end == 0) || (start < t0_.GPUClockCounter) || (end < t0_.GPUClockCounter))
    debug_print("Signal %p time stamps may be invalid.\n", &signal->signal_);
}

void GpuAgent::TranslateTime(core::Signal* signal, hsa_amd_profiling_async_copy_time_t& time) {
  uint64_t start, end;
  signal->GetRawTs(true, start, end);
  // Order is important, we want to translate the end time first to ensure that packet duration is
  // not impacted by clock measurement latency jitter.
  time.end = TranslateTime(end);
  time.start = TranslateTime(start);

  if ((start == 0) || (end == 0) || (start < t0_.GPUClockCounter) || (end < t0_.GPUClockCounter))
    debug_print("Signal %p time stamps may be invalid.\n", &signal->signal_);
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
  // Only allow short (error bounded) extrapolation for times during program execution.
  // Limit errors due to relative frequency drift to ~0.5us.  Sync clocks at 16Hz.
  const int64_t max_extrapolation = core::Runtime::runtime_singleton_->sys_clock_freq() >> 4;

  ScopedAcquire<KernelMutex> lock(&t1_lock_);
  // Limit errors due to correlated pair certainty to ~0.5us.
  // extrapolated time < (0.5us / half clock read certainty) * delay between clock measures
  // clock read certainty is <4us.
  if (((t1_.GPUClockCounter - t0_.GPUClockCounter) >> 2) + t1_.GPUClockCounter < tick) SyncClocks();

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
  int64_t elapsed = 0;
  double ratio;

  // Valid ticks only need at most one SyncClocks.
  for (int i = 0; i < 2; i++) {
    ratio = double(t1_.SystemClockCounter - t0_.SystemClockCounter) /
        double(t1_.GPUClockCounter - t0_.GPUClockCounter);
    elapsed = int64_t(ratio * double(int64_t(tick - t1_.GPUClockCounter)));

    // Skip clock sync if under the extrapolation limit.
    if (elapsed < max_extrapolation) break;
    SyncClocks();
  }

  system_tick = uint64_t(elapsed) + t1_.SystemClockCounter;

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
  if (isa_->GetMajorVersion() == 7) {
    // No trap handler support on Gfx7, soft error.
    return;
  }

  // Assemble the trap handler source code.
  void* tma_addr = nullptr;
  uint64_t tma_size = 0;

  if (core::Runtime::runtime_singleton_->KfdVersion().supports_exception_debugging) {
    AssembleShader("TrapHandlerKfdExceptions", AssembleTarget::ISA, trap_code_buf_,
                   trap_code_buf_size_);
  } else {
    if (isa_->GetMajorVersion() >= 11 ||
       (isa_->GetMajorVersion() == 9 && isa_->GetMinorVersion() == 4)) {
      // No trap handler support without exception handling, soft error.
      return;
    }

    AssembleShader("TrapHandler", AssembleTarget::ISA, trap_code_buf_, trap_code_buf_size_);

    // Make an empty map from doorbell index to queue.
    // The trap handler uses this to retrieve a wave's amd_queue_t*.
    auto doorbell_queue_map_size = MAX_NUM_DOORBELLS * sizeof(amd_queue_t*);

    doorbell_queue_map_ = (amd_queue_t**)system_allocator()(doorbell_queue_map_size, 0x1000, 0);
    assert(doorbell_queue_map_ != NULL && "Doorbell queue map allocation failed");

    memset(doorbell_queue_map_, 0, doorbell_queue_map_size);

    tma_addr = doorbell_queue_map_;
    tma_size = doorbell_queue_map_size;
  }

  // Bind the trap handler to this node.
  HSAKMT_STATUS err = hsaKmtSetTrapHandler(node_id(), trap_code_buf_, trap_code_buf_size_,
                                           tma_addr, tma_size);
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
  } else if (isa_->GetMajorVersion() > 11) {
    assert(false && "Code cache invalidation not implemented for this agent");
  }

  // Invalidate caches which may hold lines of code object allocation.
  uint32_t cache_inv[8] = {0};
  uint32_t cache_inv_size_dw;

  if (isa_->GetMajorVersion() < 10) {
      cache_inv[1] = PM4_ACQUIRE_MEM_DW1_COHER_CNTL(
          PM4_ACQUIRE_MEM_COHER_CNTL_SH_ICACHE_ACTION_ENA |
          PM4_ACQUIRE_MEM_COHER_CNTL_SH_KCACHE_ACTION_ENA |
          PM4_ACQUIRE_MEM_COHER_CNTL_TC_ACTION_ENA |
          PM4_ACQUIRE_MEM_COHER_CNTL_TC_WB_ACTION_ENA);

      cache_inv_size_dw = 7;
  } else {
      cache_inv[7] = PM4_ACQUIRE_MEM_DW7_GCR_CNTL(
          PM4_ACQUIRE_MEM_GCR_CNTL_GLI_INV(1) |
          PM4_ACQUIRE_MEM_GCR_CNTL_GLK_INV |
          PM4_ACQUIRE_MEM_GCR_CNTL_GLV_INV |
          PM4_ACQUIRE_MEM_GCR_CNTL_GL1_INV |
          PM4_ACQUIRE_MEM_GCR_CNTL_GL2_INV);

      cache_inv_size_dw = 8;
  }

  cache_inv[0] = PM4_HDR(PM4_HDR_IT_OPCODE_ACQUIRE_MEM, cache_inv_size_dw,
             isa_->GetMajorVersion());
  cache_inv[2] = PM4_ACQUIRE_MEM_DW2_COHER_SIZE(0xFFFFFFFF);
  cache_inv[3] = PM4_ACQUIRE_MEM_DW3_COHER_SIZE_HI(0xFF);

  // Submit the command to the utility queue and wait for it to complete.
  queues_[QueueUtility]->ExecutePM4(cache_inv, cache_inv_size_dw * sizeof(uint32_t));
}

lazy_ptr<core::Blit>& GpuAgent::GetBlitObject(uint32_t engine_offset) {
  sdma_blit_used_mask_ |= 1 << engine_offset;
  return blits_[engine_offset];
}

lazy_ptr<core::Blit>& GpuAgent::GetXgmiBlit(const core::Agent& dst_agent) {
  // Determine if destination is a member xgmi peers list
  uint32_t xgmi_engine_cnt = properties_.NumSdmaXgmiEngines;
  assert((xgmi_engine_cnt > 0) && ("Illegal condition, should not happen"));

  ScopedAcquire<KernelMutex> lock(&xgmi_peer_list_lock_);

  for (uint32_t idx = 0; idx < xgmi_peer_list_.size(); idx++) {
    uint64_t dst_handle = dst_agent.public_handle().handle;
    uint64_t peer_handle = xgmi_peer_list_[idx]->public_handle().handle;
    if (peer_handle == dst_handle) {
      return blits_[(idx % xgmi_engine_cnt) + DefaultBlitCount];
    }
  }

  // Add agent to the xGMI neighbours list
  xgmi_peer_list_.push_back(&dst_agent);
  return GetBlitObject(((xgmi_peer_list_.size() - 1) % xgmi_engine_cnt) + DefaultBlitCount);
}

lazy_ptr<core::Blit>& GpuAgent::GetPcieBlit(const core::Agent& dst_agent,
                                            const core::Agent& src_agent) {
  bool is_h2d = (src_agent.device_type() == core::Agent::kAmdCpuDevice &&
                 dst_agent.device_type() == core::Agent::kAmdGpuDevice);

  lazy_ptr<core::Blit>& blit = GetBlitObject(is_h2d ? BlitHostToDev : BlitDevToHost);
  return blit;
}

lazy_ptr<core::Blit>& GpuAgent::GetBlitObject(const core::Agent& dst_agent,
                                              const core::Agent& src_agent,
                                              const size_t size) {
  // At this point it is guaranteed that one of
  // the two devices is a GPU, potentially both
  assert(((src_agent.device_type() == core::Agent::kAmdGpuDevice) ||
          (dst_agent.device_type() == core::Agent::kAmdGpuDevice)) &&
         ("Both devices are CPU agents which is not expected"));

  // Determine if Src and Dst devices are same and are the copying device
  // Such a copy is in the device local memory, which can only be saturated by a blit kernel.
  if ((src_agent.public_handle().handle) == (dst_agent.public_handle().handle) &&
      (dst_agent.public_handle().handle == public_handle_.handle)) {
    // If the copy is very small then cache flush overheads can dominate.
    // Choose a (potentially) SDMA enabled engine to avoid cache flushing.
    if (size < core::Runtime::runtime_singleton_->flag().force_sdma_size()) {
      return GetBlitObject(BlitDevToHost);
    }
    return blits_[BlitDevToDev];
  }

  if (core::Runtime::runtime_singleton_->flag().enable_peer_sdma() == Flag::SDMA_DISABLE
      && src_agent.device_type() == core::Agent::kAmdGpuDevice
      && dst_agent.device_type() == core::Agent::kAmdGpuDevice) {
      return blits_[BlitDevToDev];
  }

  // Acquire Hive Id of Src and Dst devices - ignore hive id for CPU devices.
  // CPU-GPU connections should always use the host (aka pcie) facing SDMA engines, even if the
  // connection is XGMI.
  uint64_t src_hive_id =
      (src_agent.device_type() == core::Agent::kAmdGpuDevice) ? src_agent.HiveId() : 0;
  uint64_t dst_hive_id =
      (dst_agent.device_type() == core::Agent::kAmdGpuDevice) ? dst_agent.HiveId() : 0;

  // Bind to a PCIe facing Blit object if the two
  // devices have different Hive Ids. This can occur
  // for following scenarios:
  //
  //  Neither device claims membership in a Hive
  //   srcId = 0 <-> dstId = 0;
  //
  //  Src device claims membership in a Hive
  //   srcId = 0x1926 <-> dstId = 0;
  //
  //  Dst device claims membership in a Hive
  //   srcId = 0 <-> dstId = 0x1123;
  //
  //  Both device claims membership in a Hive
  //  and the  Hives are different
  //   srcId = 0x1926 <-> dstId = 0x1123;
  //
  if ((dst_hive_id != src_hive_id) || (dst_hive_id == 0)) {
    return GetPcieBlit(dst_agent, src_agent);
  }

  // Accommodates platforms where devices have xGMI
  // links but without sdmaXgmiEngines e.g. Vega 20
  if (properties_.NumSdmaXgmiEngines == 0) {
    return GetPcieBlit(dst_agent, src_agent);
  }

  return GetXgmiBlit(dst_agent);
}

void GpuAgent::Trim() {
  Agent::Trim();
  AsyncReclaimScratchQueues();
  ScopedAcquire<KernelMutex> lock(&scratch_lock_);
  scratch_cache_.trim(false);
}

void GpuAgent::InitNumaAllocator() {
  for (auto pool : GetNearestCpuAgent()->regions()) {
    if (pool->kernarg()) {
      system_allocator_ = [pool](size_t size, size_t alignment,
                                 MemoryRegion::AllocateFlags alloc_flags) -> void* {
        assert(alignment <= 4096);
        void* ptr = nullptr;
        return (HSA_STATUS_SUCCESS ==
                core::Runtime::runtime_singleton_->AllocateMemory(pool, size, alloc_flags, &ptr))
            ? ptr
            : nullptr;
      };

      system_deallocator_ = [](void* ptr) { core::Runtime::runtime_singleton_->FreeMemory(ptr); };

      return;
    }
  }
  assert(false && "Nearest NUMA node did not have a kernarg pool.");
}

core::Agent* GpuAgent::GetNearestCpuAgent() const {
  core::Agent* nearCpu = nullptr;
  uint32_t dist = -1u;
  for (auto cpu : core::Runtime::runtime_singleton_->cpu_agents()) {
    const core::Runtime::LinkInfo link_info =
        core::Runtime::runtime_singleton_->GetLinkInfo(node_id(), cpu->node_id());
    if (link_info.info.numa_distance < dist) {
      dist = link_info.info.numa_distance;
      nearCpu = cpu;
    }
  }
  return nearCpu;
}

}  // namespace amd
}  // namespace rocr
