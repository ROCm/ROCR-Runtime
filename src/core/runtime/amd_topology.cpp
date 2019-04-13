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
#include <map>
#include <string>
#include <sstream>

#ifndef NDBEUG
#include <iostream>
#endif

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

#ifndef NDEBUG
static bool PrintUsrGpuMap(std::map<uint32_t, int32_t>& gpu_usr_map) {
  (void)PrintUsrGpuMap;  // Suppress unused symbol warning.
  std::map<uint32_t, int32_t>::iterator it;
  for (it = gpu_usr_map.begin(); it != gpu_usr_map.end(); it++) {
    int32_t usrIdx = it->second;
    uint32_t kfdIdx = it->first;
    std::cout << "KfdIdx: " << kfdIdx << " @ UsrIdx: " << usrIdx << std::endl;
  }
  return true;
}
#endif

/**
 * Determines if user has defined the env that indicates which
 * subset of Gpu's are desired to be surfaced. If defined the
 * set of Gpu's are captured into a map of Gpu index and
 *
 * @return true if env is not blank, false otherwise. It is
 * possible to have zero devices surfaced even when env is
 * not blank.
 */
static bool MapUsrGpuList(int32_t numNodes, std::map<uint32_t, int32_t>& gpu_usr_map) {
  const std::string& env_value = core::Runtime::runtime_singleton_->flag().visible_gpus();
  if (env_value.empty()) {
    return false;
  }

  // Capture the env value string as a parsable stream
  std::stringstream stream(env_value);

  // Read stream until there are no more tokens
  int32_t usrIdx = 0;
  int32_t token = 0x11231926;
  while (!stream.eof()) {
    // Read the option value
    stream >> token;
    if (stream.fail()) {
      return true;
    }

    // Stop processing input tokens if invalid index is seen
    // A value that is less than zero or greater than the
    // number of Numa nodes is considered invalid
    if ((token < 0) || (token >= numNodes)) {
      return true;
    }

    // Determine if current value has been seen before
    // @note: Currently we are interpreting a repeat as
    // an invalid index i.e. is equal to -1
    bool exists = gpu_usr_map.find(token) != gpu_usr_map.end();
    if (exists) {
      return true;
    }

    // Update Gpu User map table
    gpu_usr_map[token] = usrIdx++;

    // Ignore the delimiter
    if (stream.peek() == ',') {
      stream.ignore();
    } else {
      return true;
    }
  }

  return true;
}

CpuAgent* DiscoverCpu(HSAuint32 node_id, HsaNodeProperties& node_prop) {
  if (node_prop.NumCPUCores == 0) {
    return nullptr;
  }

  CpuAgent* cpu = new CpuAgent(node_id, node_prop);
  core::Runtime::runtime_singleton_->RegisterAgent(cpu);

  return cpu;
}

GpuAgent* DiscoverGpu(HSAuint32 node_id, HsaNodeProperties& node_prop) {
  if (node_prop.NumFComputeCores == 0) {
    return nullptr;
  }

  GpuAgent* gpu = new GpuAgent(node_id, node_prop);
  core::Runtime::runtime_singleton_->RegisterAgent(gpu);

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
static void SurfaceGpuList(std::vector<int32_t>& gpu_list) {
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
    const GpuAgent* gpu = DiscoverGpu(gpu_list[idx], node_prop);
    assert((node_prop.NumFComputeCores != 0) && (gpu != nullptr) && "GPU device failed discovery.");
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

  // Determine and process user's request to surface
  // a subset of Gpu devices
  int32_t invalidIdx = -1;
  std::vector<int32_t> gpu_usr_list;
  std::map<uint32_t, int32_t> gpu_usr_map;
  bool filter = MapUsrGpuList(props.NumNodes, gpu_usr_map);
  int32_t list_sz = gpu_usr_map.size();
  if (filter) {
    for (int32_t idx = 0; idx < list_sz; idx++) {
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

    // Instantiate a Cpu/Apu device
    const CpuAgent* cpu = DiscoverCpu(node_id, node_prop);
    assert(((node_prop.NumCPUCores == 0) || (cpu != nullptr)) && "CPU device failed discovery.");

    // Current node is either a dGpu or Apu and might belong
    // to user visible list. Process node if present in usr
    // visible list, continue if not found
    if (node_prop.NumFComputeCores != 0) {
      if (filter) {
        const auto& it = gpu_usr_map.find(kfdIdx);
        if (it != gpu_usr_map.end()) {
          gpu_usr_list[it->second] = node_id;
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

  SurfaceGpuList(gpu_usr_list);
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
