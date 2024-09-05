////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.
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

#include "core/inc/amd_aie_agent.h"

#include <functional>

#include "core/inc/amd_aie_aql_queue.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/amd_xdna_driver.h"
#include "core/inc/driver.h"
#include "core/inc/runtime.h"

namespace rocr {
namespace AMD {

AieAgent::AieAgent(uint32_t node)
    : core::Agent(core::DriverType::XDNA, node,
                  core::Agent::DeviceType::kAmdAieDevice) {
  InitRegionList();
  InitAllocators();
  GetAgentProperties();
}

AieAgent::~AieAgent() {
  std::for_each(regions_.begin(), regions_.end(), DeleteObject());
  regions_.clear();
}

hsa_status_t AieAgent::VisitRegion(bool include_peer,
                                   hsa_status_t (*callback)(hsa_region_t region,
                                                            void *data),
                                   void *data) const {
  AMD::callback_t<decltype(callback)> call(callback);
  for (const auto r : regions_) {
    hsa_region_t region_handle(core::MemoryRegion::Convert(r));
    call(region_handle, data);
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AieAgent::IterateRegion(
    hsa_status_t (*callback)(hsa_region_t region, void *data),
    void *data) const {
  return VisitRegion(false, callback, data);
}

hsa_status_t AieAgent::IterateCache(hsa_status_t (*callback)(hsa_cache_t cache,
                                                             void *data),
                                    void *data) const {
  // AIE has no caches.
  return HSA_STATUS_ERROR_INVALID_CACHE;
}

hsa_status_t AieAgent::GetInfo(hsa_agent_info_t attribute, void *value) const {
  const size_t attribute_ = static_cast<size_t>(attribute);

  switch (attribute_) {
  case HSA_AGENT_INFO_NAME: {
    std::string name_info_("aie2");
    std::strcpy(reinterpret_cast<char *>(value), name_info_.c_str());
    break;
  }
  case HSA_AGENT_INFO_VENDOR_NAME: {
    std::string vendor_name_info_("AMD");
    std::strcpy(reinterpret_cast<char *>(value), vendor_name_info_.c_str());
    break;
  }
  case HSA_AGENT_INFO_FEATURE:
    *((hsa_agent_feature_t *)value) = HSA_AGENT_FEATURE_AGENT_DISPATCH;
    break;
  case HSA_AGENT_INFO_MACHINE_MODEL:
    *reinterpret_cast<hsa_machine_model_t *>(value) = HSA_MACHINE_MODEL_LARGE;
    break;
  case HSA_AGENT_INFO_BASE_PROFILE_DEFAULT_FLOAT_ROUNDING_MODES:
  case HSA_AGENT_INFO_DEFAULT_FLOAT_ROUNDING_MODE:
    // TODO: validate if this is true.
    *reinterpret_cast<hsa_default_float_rounding_mode_t *>(value) =
        HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR;
    break;
   case HSA_AGENT_INFO_PROFILE:
    *reinterpret_cast<hsa_profile_t *>(value) = profile_;
    break;
  case HSA_AGENT_INFO_WAVEFRONT_SIZE:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AGENT_INFO_WORKGROUP_MAX_DIM:
    std::memset(value, 0, sizeof(uint16_t) * 3);
    break;
  case HSA_AGENT_INFO_WORKGROUP_MAX_SIZE:
    *reinterpret_cast<uint32_t*>(value) = 0;
    break;
  case HSA_AGENT_INFO_GRID_MAX_DIM:
    std::memset(value, 0, sizeof(hsa_dim3_t));
    break;
  case HSA_AGENT_INFO_GRID_MAX_SIZE:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AGENT_INFO_FBARRIER_MAX_SIZE:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AGENT_INFO_QUEUES_MAX:
    *reinterpret_cast<uint32_t *>(value) = max_queues_;
    break;
  case HSA_AGENT_INFO_QUEUE_MIN_SIZE:
    *reinterpret_cast<uint32_t *>(value) = min_aql_size_;
    break;
  case HSA_AGENT_INFO_QUEUE_MAX_SIZE:
    *reinterpret_cast<uint32_t *>(value) = max_aql_size_;
    break;
  case HSA_AGENT_INFO_QUEUE_TYPE:
    *reinterpret_cast<hsa_queue_type32_t *>(value) = HSA_QUEUE_TYPE_SINGLE;
    break;
  case HSA_AGENT_INFO_NODE:
    *reinterpret_cast<uint32_t *>(value) = node_id();
    break;
  case HSA_AGENT_INFO_DEVICE:
    *reinterpret_cast<hsa_device_type_t *>(value) = HSA_DEVICE_TYPE_AIE;
    break;
  case HSA_AGENT_INFO_CACHE_SIZE:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AGENT_INFO_VERSION_MAJOR:
    *reinterpret_cast<uint32_t *>(value) = 1;
    break;
  case HSA_AGENT_INFO_VERSION_MINOR:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_CHIP_ID:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_CACHELINE_SIZE:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_MAX_CLOCK_FREQUENCY:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_DRIVER_NODE_ID:
    *reinterpret_cast<uint32_t *>(value) = node_id();
    break;
  case HSA_AMD_AGENT_INFO_MAX_ADDRESS_WATCH_POINTS:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_BDFID:
   *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_NUM_SIMDS_PER_CU:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_NUM_SHADER_ENGINES:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_NUM_SHADER_ARRAYS_PER_SE:
    *reinterpret_cast<uint32_t *>(value) = 0;
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
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_PRODUCT_NAME: {
    std::string product_name_info_("AIE-ML");
    std::strcpy(reinterpret_cast<char *>(value), product_name_info_.c_str());
    break;
  }
  case HSA_AMD_AGENT_INFO_UUID: {
    // TODO: uuid_value needs to be obtained somehow
    uint64_t uuid_value = 0;

    if (uuid_value == 0) {
      static const char uuid_tmp[] = "AIE-XX";
      snprintf(static_cast<char *>(value), sizeof(uuid_tmp), "%s", uuid_tmp);
      break;
    }

    // Device supports UUID, build UUID string to return.
    constexpr std::size_t max_uuid_length = 36;
    static const char uuid_tmp[] = "AIE-";
    snprintf(static_cast<char *>(value), max_uuid_length + sizeof(uuid_tmp),
             "%s%036lX", uuid_tmp, uuid_value);
    break;
  }
  case HSA_AMD_AGENT_INFO_ASIC_REVISION:
    *reinterpret_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_SVM_DIRECT_HOST_ACCESS:
    // commented out until we populate AIE agent regions
    assert(regions_.size() != 0 && "No device local memory found!");
    *reinterpret_cast<bool *>(value) = true;
    break;
  case HSA_AMD_AGENT_INFO_TIMESTAMP_FREQUENCY:
    return core::Runtime::runtime_singleton_->GetSystemInfo(
        HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY, value);
    break;
  case HSA_AMD_AGENT_INFO_ASIC_FAMILY_ID:
    *static_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_UCODE_VERSION:
    *static_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_SDMA_UCODE_VERSION:
    *static_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_NUM_SDMA_ENG:
    *static_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_NUM_SDMA_XGMI_ENG:
    *static_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_IOMMU_SUPPORT:
    *static_cast<hsa_amd_iommu_version_t *>(value) = HSA_IOMMU_SUPPORT_NONE;
    break;
  case HSA_AMD_AGENT_INFO_NUM_XCC:
    *static_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_DRIVER_UID:
    *static_cast<uint32_t *>(value) = 0;
    break;
  case HSA_AMD_AGENT_INFO_NEAREST_CPU:
    static_cast<hsa_agent_t *>(value)->handle = 0;
    break;
  case HSA_AMD_AGENT_INFO_MEMORY_PROPERTIES:
    memset(value, 0, sizeof(uint8_t) * 8);
    break;
  case HSA_AMD_AGENT_INFO_AQL_EXTENSIONS:
    memset(value, 0, sizeof(uint8_t) * 8);
    break;
  default:
    *reinterpret_cast<uint32_t *>(value) = 0;
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AieAgent::QueueCreate(size_t size, hsa_queue_type32_t queue_type,
                                   core::HsaEventCallback event_callback,
                                   void *data, uint32_t private_segment_size,
                                   uint32_t group_segment_size,
                                   core::Queue **queue) {
  if (!IsPowerOfTwo(size)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (size < min_aql_size_ || size > max_aql_size_) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  auto aql_queue(new AieAqlQueue(this, size, node_id()));
  *queue = aql_queue;

  return HSA_STATUS_SUCCESS;
}

void AieAgent::InitRegionList() {
  /// TODO: Find a way to set the other memory properties in a reasonable way.
  ///       This should be easier once the ROCt source is incorporated into the
  ///       ROCr source. Since the AIE itself currently has no memory regions of
  ///       its own all memory is just the system DRAM.

  /// For allocating kernel arguments or other objects that only need
  /// system memory.
  HsaMemoryProperties sys_mem_props = {};
  sys_mem_props.HeapType = HSA_HEAPTYPE_SYSTEM;

  /// For allocating memory for programmable device image (PDI) files. These
  /// need to be mapped to the device so the hardware can access the PDIs.
  HsaMemoryProperties dev_mem_props = {};
  dev_mem_props.HeapType = HSA_HEAPTYPE_DEVICE_SVM,
  dev_mem_props.SizeInBytes = XdnaDriver::GetDevHeapByteSize();

  /// As of now the AIE devices support coarse-grain memory regions that require
  /// explicit sync operations.
  regions_.reserve(2);
  regions_.push_back(
      new MemoryRegion(false, true, false, false, true, this, sys_mem_props));
  regions_.push_back(
      new MemoryRegion(false, false, false, false, true, this, dev_mem_props));
}

void AieAgent::GetAgentProperties() {
  core::Runtime::runtime_singleton_->AgentDriver(driver_type)
      .GetAgentProperties(*this);
}

void AieAgent::InitAllocators() {
  for (const auto *region : regions()) {
    const MemoryRegion *amd_mem_region(
        static_cast<const MemoryRegion *>(region));
    if (amd_mem_region->kernarg()) {
      system_allocator_ =
          [region](size_t size, size_t align,
                   core::MemoryRegion::AllocateFlags alloc_flags) -> void * {
        void *mem(nullptr);
        return (core::Runtime::runtime_singleton_->AllocateMemory(
                    region, size, alloc_flags, &mem) == HSA_STATUS_SUCCESS)
                   ? mem
                   : nullptr;
      };
      break;
    }
  }
}

} // namespace AMD
} // namespace rocr
