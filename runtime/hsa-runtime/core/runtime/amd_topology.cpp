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

#include "core/inc/amd_topology.h"
#include "core/inc/amd_filter_device.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <link.h>

#ifndef NDBEUG
#include <iostream>
#endif

#include "hsakmt/hsakmt.h"

#include "core/inc/runtime.h"
#include "core/inc/amd_cpu_agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/util/utils.h"

extern r_debug _amdgpu_r_debug;

namespace rocr {
namespace AMD {
// Minimum acceptable KFD version numbers
static const uint kKfdVersionMajor = 0;
static const uint kKfdVersionMinor = 99;

// Query for user preference and use that to determine Xnack mode of ROCm system.
// Return true if Xnack mode is ON or false if OFF. Xnack mode of a system is
// orthogonal to devices that do not support Xnack mode. It is legal for a
// system with Xnack ON to have devices that do not support Xnack functionality.
bool BindXnackMode() {
  // Get users' preference for Xnack mode of ROCm platform
  HSAint32 mode;
  mode = core::Runtime::runtime_singleton_->flag().xnack();
  bool config_xnack =
      (core::Runtime::runtime_singleton_->flag().xnack() != Flag::XNACK_REQUEST::XNACK_UNCHANGED);

  // Indicate to driver users' preference for Xnack mode
  // Call to driver can fail and is a supported feature
  HSAKMT_STATUS status = HSAKMT_STATUS_ERROR;
  if (config_xnack) {
    status = hsaKmtSetXNACKMode(mode);
    if (status == HSAKMT_STATUS_SUCCESS) {
      return mode;
    }
  }

  // Get Xnack mode of devices bound by driver. This could happen
  // when a call to SET Xnack mode fails or user has no particular
  // preference
  status = hsaKmtGetXNACKMode((HSAint32*)&mode);
  if(status != HSAKMT_STATUS_SUCCESS) {
    debug_print("KFD does not support xnack mode query.\nROCr must assume xnack is disabled.\n");
    return false;
  }
  return mode;
}

CpuAgent* DiscoverCpu(HSAuint32 node_id, HsaNodeProperties& node_prop) {
  if (node_prop.NumCPUCores == 0) {
    return nullptr;
  }

  CpuAgent* cpu = new CpuAgent(node_id, node_prop);
  cpu->Enable();
  core::Runtime::runtime_singleton_->RegisterAgent(cpu, true);

  return cpu;
}

GpuAgent* DiscoverGpu(HSAuint32 node_id, HsaNodeProperties& node_prop, bool xnack_mode,
                      bool enabled) {
  GpuAgent* gpu = nullptr;
  if (node_prop.NumFComputeCores == 0) {
      // Ignore non GPUs.
      return nullptr;
  }
  try {
    gpu = new GpuAgent(node_id, node_prop, xnack_mode,
                       core::Runtime::runtime_singleton_->gpu_agents().size());

    const HsaVersionInfo& kfd_version = core::Runtime::runtime_singleton_->KfdVersion().version;

    // Check for sramecc incompatibility due to sramecc not being reported correctly in kfd before
    // 1.4.
    if (gpu->isa()->IsSrameccSupported() && (kfd_version.KernelInterfaceMajorVersion <= 1 &&
                                             kfd_version.KernelInterfaceMinorVersion < 4)) {
      // gfx906 has both sramecc modes in use.  Suppress the device.
      if ((gpu->isa()->GetProcessorName() == "gfx906") &&
          core::Runtime::runtime_singleton_->flag().check_sramecc_validity()) {
        char name[64];
        gpu->GetInfo((hsa_agent_info_t)HSA_AMD_AGENT_INFO_PRODUCT_NAME, name);
        name[63] = '\0';
        fprintf(stderr,
                "HSA Error:  Incompatible kernel and userspace, %s disabled. Upgrade amdgpu.\n",
                name);
        delete gpu;
        return nullptr;
      }

      // gfx908 always has sramecc set to on in vbios.  Set mode bit to on and recreate the device.
      if (gpu->isa()->GetProcessorName() == "gfx908") {
        node_prop.Capability.ui32.SRAM_EDCSupport = 1;
        delete gpu;
        gpu = new GpuAgent(node_id, node_prop, xnack_mode,
                           core::Runtime::runtime_singleton_->gpu_agents().size());
      }
    }
  } catch (const hsa_exception& e) {
    if(e.error_code() == HSA_STATUS_ERROR_INVALID_ISA) {
      ifdebug {
        if (!strIsEmpty(e.what())) debug_print("Warning: %s\n", e.what());
      }
      // Ignore unrecognized GPUs.
      return nullptr;
    } else {
      // Rethrow remaining exceptions.
      throw;
    }
  }
  if (enabled) gpu->Enable();
  core::Runtime::runtime_singleton_->RegisterAgent(gpu, enabled);
  return gpu;
}

void RegisterLinkInfo(uint32_t node_id, uint32_t num_link) {
  // Register connectivity links for this agent to the runtime.
  if (num_link == 0) {
    return;
  }

  std::vector<HsaIoLinkProperties> links(num_link);
  if (HSAKMT_STATUS_SUCCESS !=
      hsaKmtGetNodeIoLinkProperties(node_id, num_link, &links[0])) {
    return;
  }

  for (HsaIoLinkProperties io_link : links) {
    // Populate link info with thunk property.
    hsa_amd_memory_pool_link_info_t link_info = {0};

    switch (io_link.IoLinkType) {
      case HSA_IOLINKTYPE_HYPERTRANSPORT:
        link_info.link_type = HSA_AMD_LINK_INFO_TYPE_HYPERTRANSPORT;
        link_info.atomic_support_32bit = true;
        link_info.atomic_support_64bit = true;
        link_info.coherent_support = true;
        break;
      case HSA_IOLINKTYPE_PCIEXPRESS:
        link_info.link_type = HSA_AMD_LINK_INFO_TYPE_PCIE;
        link_info.atomic_support_32bit = true;
        link_info.atomic_support_64bit = true;
        link_info.coherent_support = true;
        break;
      case HSA_IOLINK_TYPE_QPI_1_1:
        link_info.link_type = HSA_AMD_LINK_INFO_TYPE_QPI;
        link_info.atomic_support_32bit = true;
        link_info.atomic_support_64bit = true;
        link_info.coherent_support = true;
        break;
      case HSA_IOLINK_TYPE_INFINIBAND:
        link_info.link_type = HSA_AMD_LINK_INFO_TYPE_INFINBAND;
        debug_print("IOLINK is missing atomic and coherency defaults.\n");
        break;
      case HSA_IOLINK_TYPE_XGMI:
        link_info.link_type = HSA_AMD_LINK_INFO_TYPE_XGMI;
        link_info.atomic_support_32bit = true;
        link_info.atomic_support_64bit = true;
        link_info.coherent_support = true;
        break;
      default:
        debug_print("Unrecognized IOLINK type.\n");
        break;
    }

    // KFD is reporting wrong override status for XGMI.  Disallow override for bringup.
    if (io_link.Flags.ui32.Override == 1) {
      if (io_link.Flags.ui32.NoPeerToPeerDMA == 1) {
        // Ignore this link since peer to peer is not allowed.
        continue;
      }
      link_info.atomic_support_32bit = (io_link.Flags.ui32.NoAtomics32bit == 0);
      link_info.atomic_support_64bit = (io_link.Flags.ui32.NoAtomics64bit == 0);
      link_info.coherent_support = (io_link.Flags.ui32.NonCoherent == 0);
    }

    link_info.max_bandwidth = io_link.MaximumBandwidth;
    link_info.max_latency = io_link.MaximumLatency;
    link_info.min_bandwidth = io_link.MinimumBandwidth;
    link_info.min_latency = io_link.MinimumLatency;
    link_info.numa_distance = io_link.Weight;

    core::Runtime::runtime_singleton_->RegisterLinkInfo(
        io_link.NodeFrom, io_link.NodeTo, io_link.Weight, link_info);
  }
}

/**
 * Process the list of Gpus that are surfaced to user
 */
static void SurfaceGpuList(std::vector<int32_t>& gpu_list, bool xnack_mode, bool enabled) {
  // Process user visible Gpu devices
  int32_t invalidIdx = -1;
  int32_t list_sz = gpu_list.size();
  HsaNodeProperties node_prop = {0};
  for (int32_t idx = 0; idx < list_sz; idx++) {
    if (gpu_list[idx] == invalidIdx) {
      break;
    }

    // Obtain properties of the node
    HSAKMT_STATUS err_val = hsaKmtGetNodeProperties(gpu_list[idx], &node_prop);
    assert(err_val == HSAKMT_STATUS_SUCCESS && "Error in getting Node Properties");

    // Instantiate a Gpu device. The IO links
    // of this node have already been registered
    assert((node_prop.NumFComputeCores != 0) && "Improper node used for GPU device discovery.");
    DiscoverGpu(gpu_list[idx], node_prop, xnack_mode, enabled);
  }
}

/// @brief Calls Kfd thunk to get the snapshot of the topology of the system,
/// which includes associations between, node, devices, memory and caches.
void BuildTopology() {
  HsaVersionInfo kfd_version;
  if (hsaKmtGetVersion(&kfd_version) != HSAKMT_STATUS_SUCCESS) {
    return;
  }

  if (kfd_version.KernelInterfaceMajorVersion == kKfdVersionMajor &&
      kfd_version.KernelInterfaceMinorVersion < kKfdVersionMinor) {
    return;
  }

  // Disable KFD event support when using open source KFD
  if (kfd_version.KernelInterfaceMajorVersion == 1 &&
      kfd_version.KernelInterfaceMinorVersion == 0) {
    core::g_use_interrupt_wait = false;
  }

  core::Runtime::runtime_singleton_->KfdVersion(kfd_version);

  HsaSystemProperties props;
  hsaKmtReleaseSystemProperties();

  if (hsaKmtAcquireSystemProperties(&props) != HSAKMT_STATUS_SUCCESS) {
    return;
  }

  core::Runtime::runtime_singleton_->SetLinkCount(props.NumNodes);

  // Query if env ROCR_VISIBLE_DEVICES is defined. If defined
  // determine number and order of GPU devices to be surfaced
  RvdFilter rvdFilter;
  int32_t invalidIdx = -1;
  uint32_t visibleCnt = 0;
  std::vector<int32_t> gpu_usr_list;
  std::vector<int32_t> gpu_disabled;
  bool filter = RvdFilter::FilterDevices();
  if (filter) {
    rvdFilter.BuildRvdTokenList();
    rvdFilter.BuildDeviceUuidList(props.NumNodes);
    visibleCnt = rvdFilter.BuildUsrDeviceList();
    for (int32_t idx = 0; idx < visibleCnt; idx++) {
      gpu_usr_list.push_back(invalidIdx);
    }
  }

  // Discover agents on every node in the platform.
  int32_t kfdIdx = 0;
  for (HSAuint32 node_id = 0; node_id < props.NumNodes; node_id++) {
    HsaNodeProperties node_prop = {0};
    if (hsaKmtGetNodeProperties(node_id, &node_prop) != HSAKMT_STATUS_SUCCESS) {
      continue;
    }

    // Instantiate a Cpu device
    const CpuAgent* cpu = DiscoverCpu(node_id, node_prop);
    assert(((node_prop.NumCPUCores == 0) || (cpu != nullptr)) && "CPU device failed discovery.");

    // Current node is either a dGpu or Apu and might belong
    // to user visible list. Process node if present in usr
    // visible list, continue if not found
    if (node_prop.NumFComputeCores != 0) {
      if (filter) {
        int32_t devRank = rvdFilter.GetUsrDeviceRank(kfdIdx);
        if (devRank != (-1)) {
          gpu_usr_list[devRank] = node_id;
        } else {
          gpu_disabled.push_back(node_id);
        }
      } else {
        gpu_usr_list.push_back(node_id);
      }
      kfdIdx++;
    }

    // Register IO links of node without regard to
    // it being visible to user or not. It is not
    // possible to access links of nodes that are
    // not visible
    RegisterLinkInfo(node_id, node_prop.NumIOLinks);
  }

  // Determine the Xnack mode to be bound for system
  bool xnack_mode = BindXnackMode();

  // Instantiate ROCr objects to encapsulate Gpu devices
  SurfaceGpuList(gpu_usr_list, xnack_mode, true);
  SurfaceGpuList(gpu_disabled, xnack_mode, false);

  // Parse HSA_CU_MASK with GPU and CU count limits.
  uint32_t maxGpu = core::Runtime::runtime_singleton_->gpu_agents().size();
  uint32_t maxCu = 0;
  uint32_t cus;
  for (auto& gpu : core::Runtime::runtime_singleton_->gpu_agents()) {
    gpu->GetInfo((hsa_agent_info_t)HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT, &cus);
    maxCu = Max(maxCu, cus);
  }
  const_cast<Flag&>(core::Runtime::runtime_singleton_->flag()).parse_masks(maxGpu, maxCu);
}

bool Load() {
  // Open connection to kernel driver.
  if (hsaKmtOpenKFD() != HSAKMT_STATUS_SUCCESS) {
    return false;
  }
  MAKE_NAMED_SCOPE_GUARD(kfd, [&]() { hsaKmtCloseKFD(); });

  // Build topology table.
  BuildTopology();

  // Register runtime and optionally enable the debugger
  // BuildTopology calls hsaKmtAcquireSystemProperties() causes libhsakmt to cache topology
  // information. So we need to call hsaKmtRuntimeEnable() after calling BuildTopology() so that
  // Thunk can re-use it's cached copy instead of re-parsing whole system topology. Otherwise
  // BuildTopology will cause libhsakmt to destroyed cached copy because it calls
  // hsaKmtReleaseSystemProperties() at the beginning.

  HSAKMT_STATUS err =
      hsaKmtRuntimeEnable(&_amdgpu_r_debug, core::Runtime::runtime_singleton_->flag().debug());
  if ((err != HSAKMT_STATUS_SUCCESS) && (err != HSAKMT_STATUS_NOT_SUPPORTED)) return false;
  core::Runtime::runtime_singleton_->KfdVersion(err != HSAKMT_STATUS_NOT_SUPPORTED);

  kfd.Dismiss();
  return true;
}

bool Unload() {
  hsaKmtRuntimeDisable();

  hsaKmtReleaseSystemProperties();

  // Close connection to kernel driver.
  hsaKmtCloseKFD();

  return true;
}
}  // namespace amd
}  // namespace rocr
