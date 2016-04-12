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

#include "core/inc/amd_topology.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "hsakmt.h"

#include "core/inc/runtime.h"
#include "core/inc/amd_cpu_agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/util/utils.h"

namespace amd {
// Minimum acceptable KFD version numbers
static const uint kKfdVersionMajor = 0;
static const uint kKfdVersionMinor = 99;

CpuAgent* DiscoverCpu(HSAuint32 node_id, HsaNodeProperties& node_prop) {
  if (node_prop.NumCPUCores == 0) {
    return NULL;
  }

  CpuAgent* cpu = new CpuAgent(node_id, node_prop);
  core::Runtime::runtime_singleton_->RegisterAgent(cpu);

  return cpu;
}

GpuAgent* DiscoverGpu(HSAuint32 node_id, HsaNodeProperties& node_prop) {
  if (node_prop.NumFComputeCores == 0) {
    return NULL;
  }

  GpuAgent* gpu = new GpuAgent(node_id, node_prop);
  core::Runtime::runtime_singleton_->RegisterAgent(gpu);

  if (HSA_STATUS_SUCCESS != gpu->InitDma()) {
    assert(false && "Fail init blit");
    delete gpu;
    gpu = NULL;
  }

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

    if (io_link.Flags.ui32.Override == 1) {
      if (io_link.Flags.ui32.NoPeerToPeerDMA == 1) {
        // Ignore this link since peer to peer is not allowed.
        continue;
      }
      link_info.atomic_support_32bit = (io_link.Flags.ui32.NoAtomics32bit == 0);
      link_info.atomic_support_64bit = (io_link.Flags.ui32.NoAtomics64bit == 0);
      link_info.coherent_support = (io_link.Flags.ui32.NonCoherent == 0);
    } else {
      // TODO: decipher HSA_IOLINKTYPE to fill out the atomic
      // and coherent information.
    }

    switch (io_link.IoLinkType) {
      case HSA_IOLINKTYPE_HYPERTRANSPORT:
        link_info.link_type = HSA_AMD_LINK_INFO_TYPE_HYPERTRANSPORT;
        break;
      case HSA_IOLINKTYPE_PCIEXPRESS:
        link_info.link_type = HSA_AMD_LINK_INFO_TYPE_PCIE;
        break;
      case HSA_IOLINK_TYPE_QPI_1_1:
        link_info.link_type = HSA_AMD_LINK_INFO_TYPE_QPI;
        break;
      case HSA_IOLINK_TYPE_INFINIBAND:
        link_info.link_type = HSA_AMD_LINK_INFO_TYPE_INFINBAND;
        break;
      default:
        break;
    }

    link_info.max_bandwidth = io_link.MaximumBandwidth;
    link_info.max_latency = io_link.MaximumLatency;
    link_info.min_bandwidth = io_link.MinimumBandwidth;
    link_info.min_latency = io_link.MinimumLatency;

    core::Runtime::runtime_singleton_->RegisterLinkInfo(
        io_link.NodeFrom, io_link.NodeTo, io_link.Weight, link_info);
  }
}

/// @brief Calls Kfd thunk to get the snapshot of the topology of the system,
/// which includes associations between, node, devices, memory and caches.
void BuildTopology() {
  HsaVersionInfo info;
  if (hsaKmtGetVersion(&info) != HSAKMT_STATUS_SUCCESS) {
    return;
  }

  if (info.KernelInterfaceMajorVersion == kKfdVersionMajor &&
      info.KernelInterfaceMinorVersion < kKfdVersionMinor) {
    return;
  }

  // Disable KFD event support when using open source KFD
  if (info.KernelInterfaceMajorVersion == 1 &&
      info.KernelInterfaceMinorVersion == 0) {
    core::g_use_interrupt_wait = false;
  }

  HsaSystemProperties props;
  hsaKmtReleaseSystemProperties();

  if (hsaKmtAcquireSystemProperties(&props) != HSAKMT_STATUS_SUCCESS) {
    return;
  }

  core::Runtime::runtime_singleton_->SetLinkCount(props.NumNodes);

  // Discover agents on every node in the platform.
  for (HSAuint32 node_id = 0; node_id < props.NumNodes; node_id++) {
    HsaNodeProperties node_prop = {0};
    if (hsaKmtGetNodeProperties(node_id, &node_prop) != HSAKMT_STATUS_SUCCESS) {
      continue;
    }

    const CpuAgent* cpu = DiscoverCpu(node_id, node_prop);
    const GpuAgent* gpu = DiscoverGpu(node_id, node_prop);

    assert(!(cpu == NULL && gpu == NULL));

    RegisterLinkInfo(node_id, node_prop.NumIOLinks);
  }
}

bool Load() {
  // Open connection to kernel driver.
  if (hsaKmtOpenKFD() != HSAKMT_STATUS_SUCCESS) {
    return false;
  }

  // Build topology table.
  BuildTopology();

  return true;
}

bool Unload() {
  hsaKmtReleaseSystemProperties();

  // Close connection to kernel driver.
  hsaKmtCloseKFD();

  return true;
}
}  // namespace amd
