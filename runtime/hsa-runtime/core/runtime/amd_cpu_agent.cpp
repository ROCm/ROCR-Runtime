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

#include "core/inc/amd_cpu_agent.h"

#include <algorithm>
#include <cstring>

#include "core/inc/amd_memory_region.h"
#include "core/inc/host_queue.h"

#include "hsa_ext_image.h"

namespace amd {
CpuAgent::CpuAgent(HSAuint32 node, const HsaNodeProperties& node_props)
    : core::Agent(node, kAmdCpuDevice), properties_(node_props) {
  InitRegionList();

  InitCacheList();
}

CpuAgent::~CpuAgent() {
  std::for_each(regions_.begin(), regions_.end(), DeleteObject());
  regions_.clear();
}

void CpuAgent::InitRegionList() {
  const bool is_apu_node = (properties_.NumFComputeCores > 0);

  std::vector<HsaMemoryProperties> mem_props(properties_.NumMemoryBanks);
  if (HSAKMT_STATUS_SUCCESS ==
      hsaKmtGetNodeMemoryProperties(node_id(), properties_.NumMemoryBanks,
                                    &mem_props[0])) {
    std::vector<HsaMemoryProperties>::iterator system_prop =
        std::find_if(mem_props.begin(), mem_props.end(),
                     [](HsaMemoryProperties prop) -> bool {
          return (prop.SizeInBytes > 0 && prop.HeapType == HSA_HEAPTYPE_SYSTEM);
        });

    if (system_prop != mem_props.end()) {
      MemoryRegion* system_region_fine =
          new MemoryRegion(true, is_apu_node, this, *system_prop);

      regions_.push_back(system_region_fine);

      if (!is_apu_node) {
        MemoryRegion* system_region_coarse =
            new MemoryRegion(false, is_apu_node, this, *system_prop);

        regions_.push_back(system_region_coarse);
      }
    } else {
      HsaMemoryProperties system_props;
      std::memset(&system_props, 0, sizeof(HsaMemoryProperties));

      const uintptr_t system_base = os::GetUserModeVirtualMemoryBase();
      const size_t system_physical_size = os::GetUsablePhysicalHostMemorySize();
      assert(system_physical_size != 0);

      system_props.HeapType = HSA_HEAPTYPE_SYSTEM;
      system_props.SizeInBytes = (HSAuint64)system_physical_size;
      system_props.VirtualBaseAddress = (HSAuint64)(system_base);

      MemoryRegion* system_region =
          new MemoryRegion(true, is_apu_node, this, system_props);

      regions_.push_back(system_region);
    }
  }
}

void CpuAgent::InitCacheList() {
  // Get CPU cache information.
  cache_props_.resize(properties_.NumCaches);
  if (HSAKMT_STATUS_SUCCESS !=
      hsaKmtGetNodeCacheProperties(node_id(), properties_.CComputeIdLo,
                                   properties_.NumCaches, &cache_props_[0])) {
    cache_props_.clear();
  } else {
    // Only store CPU D-cache.
    for (size_t cache_id = 0; cache_id < cache_props_.size(); ++cache_id) {
      const HsaCacheType type = cache_props_[cache_id].CacheType;
      if (type.ui32.CPU != 1 || type.ui32.Instruction == 1) {
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

hsa_status_t CpuAgent::VisitRegion(bool include_peer,
                                   hsa_status_t (*callback)(hsa_region_t region,
                                                            void* data),
                                   void* data) const {
  if (!include_peer) {
    return VisitRegion(regions_, callback, data);
  }

  // Expose all system regions in the system.
  hsa_status_t stat = VisitRegion(
      core::Runtime::runtime_singleton_->system_regions_fine(), callback, data);
  if (stat != HSA_STATUS_SUCCESS) {
    return stat;
  }

  return VisitRegion(core::Runtime::runtime_singleton_->system_regions_coarse(),
                     callback, data);
}

hsa_status_t CpuAgent::VisitRegion(
    const std::vector<const core::MemoryRegion*>& regions,
    hsa_status_t (*callback)(hsa_region_t region, void* data),
    void* data) const {
  for (const core::MemoryRegion* region : regions) {
    hsa_region_t region_handle = core::MemoryRegion::Convert(region);
    hsa_status_t status = callback(region_handle, data);
    if (status != HSA_STATUS_SUCCESS) {
      return status;
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t CpuAgent::IterateRegion(
    hsa_status_t (*callback)(hsa_region_t region, void* data),
    void* data) const {
  return VisitRegion(true, callback, data);
}

hsa_status_t CpuAgent::IterateCache(hsa_status_t (*callback)(hsa_cache_t cache, void* data),
                                    void* data) const {
  for (size_t i = 0; i < caches_.size(); i++) {
    hsa_status_t stat = callback(core::Cache::Convert(caches_[i].get()), data);
    if (stat != HSA_STATUS_SUCCESS) return stat;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t CpuAgent::GetInfo(hsa_agent_info_t attribute, void* value) const {
  
  // agent, and vendor name size limit
  const size_t attribute_u = static_cast<size_t>(attribute);
  
  switch (attribute_u) {
    
    // The code copies HsaNodeProperties.MarketingName a Unicode string
    // which is encoded in UTF-16 as a 7-bit ASCII string. The value of
    // HsaNodeProperties.MarketingName is obtained from the "model name"
    // property of /proc/cpuinfo file
    case HSA_AGENT_INFO_NAME:
    case HSA_AMD_AGENT_INFO_PRODUCT_NAME: {
      std::memset(value, 0, HSA_PUBLIC_NAME_SIZE);
      char* temp = reinterpret_cast<char*>(value);
      for (uint32_t idx = 0;
           properties_.MarketingName[idx] != 0 && idx < HSA_PUBLIC_NAME_SIZE - 1; idx++) {
        temp[idx] = (uint8_t)properties_.MarketingName[idx];
      }
      break;
    }
    case HSA_AGENT_INFO_VENDOR_NAME:
      // TODO: hardcode for now, wait until SWDEV-88894 implemented
      std::memset(value, 0, HSA_PUBLIC_NAME_SIZE);
      std::memcpy(value, "CPU", sizeof("CPU"));
      break;
    case HSA_AGENT_INFO_FEATURE:
      *((hsa_agent_feature_t*)value) = static_cast<hsa_agent_feature_t>(0);
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
      // TODO: validate if this is true.
      *((hsa_default_float_rounding_mode_t*)value) =
          HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR;
      break;
    case HSA_AGENT_INFO_FAST_F16_OPERATION:
      // TODO: validate if this is true.
      *((bool*)value) = false;
      break;
    case HSA_AGENT_INFO_PROFILE:
      *((hsa_profile_t*)value) = HSA_PROFILE_FULL;
      break;
    case HSA_AGENT_INFO_WAVEFRONT_SIZE:
      *((uint32_t*)value) = 0;
      break;
    case HSA_AGENT_INFO_WORKGROUP_MAX_DIM:
      std::memset(value, 0, sizeof(uint16_t) * 3);
      break;
    case HSA_AGENT_INFO_WORKGROUP_MAX_SIZE:
      *((uint32_t*)value) = 0;
      break;
    case HSA_AGENT_INFO_GRID_MAX_DIM:
      std::memset(value, 0, sizeof(hsa_dim3_t));
      break;
    case HSA_AGENT_INFO_GRID_MAX_SIZE:
      *((uint32_t*)value) = 0;
      break;
    case HSA_AGENT_INFO_FBARRIER_MAX_SIZE:
      // TODO: ?
      *((uint32_t*)value) = 0;
      break;
    case HSA_AGENT_INFO_QUEUES_MAX:
      *((uint32_t*)value) = 0;
      break;
    case HSA_AGENT_INFO_QUEUE_MIN_SIZE:
      *((uint32_t*)value) = 0;
      break;
    case HSA_AGENT_INFO_QUEUE_MAX_SIZE:
      *((uint32_t*)value) = 0;
      break;
    case HSA_AGENT_INFO_QUEUE_TYPE:
      *((hsa_queue_type32_t*)value) = HSA_QUEUE_TYPE_MULTI;
      break;
    case HSA_AGENT_INFO_NODE:
      // TODO: associate with OS NUMA support (numactl / GetNumaProcessorNode).
      *((uint32_t*)value) = node_id();
      break;
    case HSA_AGENT_INFO_DEVICE:
      *((hsa_device_type_t*)value) = HSA_DEVICE_TYPE_CPU;
      break;
    case HSA_AGENT_INFO_CACHE_SIZE: {
      std::memset(value, 0, sizeof(uint32_t) * 4);

      assert(cache_props_.size() > 0 && "CPU cache info missing.");
      const size_t num_cache = cache_props_.size();
      for (size_t i = 0; i < num_cache; ++i) {
        const uint32_t line_level = cache_props_[i].CacheLevel;
        ((uint32_t*)value)[line_level - 1] = cache_props_[i].CacheSize * 1024;
      }
    } break;
    case HSA_AGENT_INFO_ISA:
      ((hsa_isa_t*)value)->handle = 0;
      break;
    case HSA_AGENT_INFO_EXTENSIONS:
      memset(value, 0, sizeof(uint8_t) * 128);
      break;
    case HSA_AGENT_INFO_VERSION_MAJOR:
      *((uint16_t*)value) = 1;
      break;
    case HSA_AGENT_INFO_VERSION_MINOR:
      *((uint16_t*)value) = 1;
      break;
    case HSA_EXT_AGENT_INFO_IMAGE_1D_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_1DA_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_1DB_MAX_ELEMENTS:
      *((uint32_t*)value) = 0;
      break;
    case HSA_EXT_AGENT_INFO_IMAGE_2D_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_2DA_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_2DDEPTH_MAX_ELEMENTS:
    case HSA_EXT_AGENT_INFO_IMAGE_2DADEPTH_MAX_ELEMENTS:
      memset(value, 0, sizeof(uint32_t) * 2);
      break;
    case HSA_EXT_AGENT_INFO_IMAGE_3D_MAX_ELEMENTS:
      memset(value, 0, sizeof(uint32_t) * 3);
      break;
    case HSA_EXT_AGENT_INFO_IMAGE_ARRAY_MAX_LAYERS:
      *((uint32_t*)value) = 0;
      break;
    case HSA_EXT_AGENT_INFO_MAX_IMAGE_RD_HANDLES:
    case HSA_EXT_AGENT_INFO_MAX_IMAGE_RORW_HANDLES:
    case HSA_EXT_AGENT_INFO_MAX_SAMPLER_HANDLERS:
      *((uint32_t*)value) = 0;
      break;
    case HSA_AMD_AGENT_INFO_CHIP_ID:
      *((uint32_t*)value) = properties_.DeviceId;
      break;
    case HSA_AMD_AGENT_INFO_CACHELINE_SIZE:
      // TODO: hardcode for now.
      *((uint32_t*)value) = 64;
      break;
    case HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT:
      *((uint32_t*)value) = properties_.NumCPUCores;
      break;
    case HSA_AMD_AGENT_INFO_MAX_CLOCK_FREQUENCY:
      *((uint32_t*)value) = properties_.MaxEngineClockMhzCCompute;
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
      *((hsa_amd_hdp_flush_t*)value) = {nullptr, nullptr};
      break;
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
      break;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t CpuAgent::QueueCreate(size_t size, hsa_queue_type32_t queue_type,
                                   core::HsaEventCallback event_callback,
                                   void* data, uint32_t private_segment_size,
                                   uint32_t group_segment_size,
                                   core::Queue** queue) {
  // No HW AQL packet processor on CPU device.
  return HSA_STATUS_ERROR;
}

}  // namespace amd
