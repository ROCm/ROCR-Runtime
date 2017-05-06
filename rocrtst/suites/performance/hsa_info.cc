/*
 * =============================================================================
 *   ROC Runtime Conformance Release License
 * =============================================================================
 * The University of Illinois/NCSA
 * Open Source License (NCSA)
 *
 * Copyright (c) 2017, Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Developed by:
 *
 *                 AMD Research and AMD ROC Software Development
 *
 *                 Advanced Micro Devices, Inc.
 *
 *                 www.amd.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimers in
 *    the documentation and/or other materials provided with the distribution.
 *  - Neither the names of <Name of Development Group, Name of Institution>,
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this Software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "gtest/gtest.h"
#include "hsa_info.h"

static hsa_status_t get_agent_info(hsa_agent_t, void*);

static hsa_status_t get_pool_info(hsa_amd_memory_pool_t, void*);

static int agent_number = 0;
static bool output_amd = false;

//@Brief: Map to store the peak FLOPS for different agent
std::map<std::string, double> flops_table = { {"Kaveri CPU", 118.4}, {
    "S    pectre", 737.0
  }, {"Carrizo CPU", 67.2}, {"Carrizo GPU", 819.2}
};

//@Brief: Vector to store the agent_names
std::vector<std::string> agent_names = {"Kaveri CPU", "Spectre",
                                        "Carri    zo CPU", "Carrizo GPU"
                                       };

HsaInfo::HsaInfo() :
  BaseRocR() {
}

HsaInfo::~HsaInfo() {
}

void HsaInfo::SetUp() {
  // Get Env Var to determine if output AMD specific info
  char* EnvVar = rocrtst::GetEnv("HSA_VENDOR_AMD");

  if (NULL != EnvVar) {
    output_amd = ('1' == *EnvVar);
  }

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }
}

void HsaInfo::Run() {
  hsa_status_t err;
  // Get the system info first
  // Get version info
  uint16_t major, minor;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  err = hsa_system_get_info(HSA_SYSTEM_INFO_VERSION_MAJOR, &major);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  err = hsa_system_get_info(HSA_SYSTEM_INFO_VERSION_MINOR, &minor);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Get timestamp frequency
  uint64_t timestamp_frequency = 0;
  err = hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY,
                            &timestamp_frequency);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Get maximum duration of a signal wait operation
  uint64_t max_wait = 0;
  err = hsa_system_get_info(HSA_SYSTEM_INFO_SIGNAL_MAX_WAIT, &max_wait);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Get Endianness of the system
  hsa_endianness_t endianness;
  err = hsa_system_get_info(HSA_SYSTEM_INFO_ENDIANNESS, &endianness);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Get machine model info
  hsa_machine_model_t machine_model;
  err = hsa_system_get_info(HSA_SYSTEM_INFO_MACHINE_MODEL, &machine_model);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Print out the results
  std::cout << "HSA System Info:" << std::endl;
  std::cout << "Runtime Version:				" << major <<
                                                     "." << minor << std::endl;
  std::cout << "System Timestamp Frequency: 			" <<
                               timestamp_frequency / 1e6 << "MHz" << std::endl;

  std::cout << "Signal Max Wait Duration:                        " << max_wait
            << "(number of timestamp)" << std::endl;
  std::cout << "Machine Model:					";

  if (HSA_MACHINE_MODEL_SMALL == machine_model) {
    std::cout << "SMALL" << std::endl;
  }
  else if (HSA_MACHINE_MODEL_LARGE == machine_model) {
    std::cout << "LARGE" << std::endl;
  }

  std::cout << "System Endianness:				";

  if (HSA_ENDIANNESS_LITTLE == endianness) {
    std::cout << "LITTLE" << std::endl;
  }
  else if (HSA_ENDIANNESS_BIG == endianness) {
    std::cout << "BIG" << std::endl;
  }

  std::cout << std::endl;

  // Iterate every agent and get their info
  err = hsa_iterate_agents(get_agent_info, NULL);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  return;

}

#define RET_IF_HSA_INFO_ERR(err) { \
  if ((err) != HSA_STATUS_SUCCESS) { \
    std::cout << "hsa api call failure at line " << __LINE__ << ", file: " << \
              __FILE__ << std::endl; \
    return (err); \
  } \
}

static hsa_status_t get_agent_info(hsa_agent_t agent, void* data) {
  int pool_number = 0;
  hsa_status_t err;
  {
    // Increase the number of agent
    agent_number++;

    // Get agent name and vendor
    char name[64];
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, name);
    RET_IF_HSA_INFO_ERR(err)
    char vendor_name[64];
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_VENDOR_NAME, &vendor_name);
    RET_IF_HSA_INFO_ERR(err)

    // Get agent feature
    hsa_agent_feature_t agent_feature;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_FEATURE, &agent_feature);
    RET_IF_HSA_INFO_ERR(err)

    // Get profile supported by the agent
    hsa_profile_t agent_profile;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_PROFILE, &agent_profile);
    RET_IF_HSA_INFO_ERR(err)

    // Get floating-point rounding mode
    hsa_default_float_rounding_mode_t float_rounding_mode;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEFAULT_FLOAT_ROUNDING_MODE,
                             &float_rounding_mode);
    RET_IF_HSA_INFO_ERR(err)

    // Get max number of queue
    uint32_t max_queue = 0;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUES_MAX, &max_queue);
    RET_IF_HSA_INFO_ERR(err)

    // Get queue min size
    uint32_t queue_min_size = 0;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MIN_SIZE,
                             &queue_min_size);
    RET_IF_HSA_INFO_ERR(err)

    // Get queue max size
    uint32_t queue_max_size = 0;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE,
                             &queue_max_size);
    RET_IF_HSA_INFO_ERR(err)

    // Get queue type
    hsa_queue_type_t queue_type;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_TYPE, &queue_type);
    RET_IF_HSA_INFO_ERR(err)

    // Get agent node
    uint32_t node;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_NODE, &node);
    RET_IF_HSA_INFO_ERR(err)

    // Get device type
    hsa_device_type_t device_type;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
    RET_IF_HSA_INFO_ERR(err)

    // Get cache size
    uint32_t cache_size[4];
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_CACHE_SIZE, cache_size);
    RET_IF_HSA_INFO_ERR(err)

    // Get chip id
    uint32_t chip_id = 0;
    err = hsa_agent_get_info(agent,
                             (hsa_agent_info_t) HSA_AMD_AGENT_INFO_CHIP_ID,
                                                                     &chip_id);
    RET_IF_HSA_INFO_ERR(err)

    // Get cacheline size
    uint32_t cacheline_size = 0;
    err = hsa_agent_get_info(agent,
                         (hsa_agent_info_t) HSA_AMD_AGENT_INFO_CACHELINE_SIZE,
                                                              &cacheline_size);
    RET_IF_HSA_INFO_ERR(err)

    // Get Max clock frequency
    uint32_t max_clock_freq = 0;
    err = hsa_agent_get_info(agent,
                    (hsa_agent_info_t) HSA_AMD_AGENT_INFO_MAX_CLOCK_FREQUENCY,
                                                              &max_clock_freq);
    RET_IF_HSA_INFO_ERR(err)

    // Get Agent BDFID
    uint16_t bdf_id = 1;
    err = hsa_agent_get_info(agent, (hsa_agent_info_t) HSA_AMD_AGENT_INFO_BDFID,
                             &bdf_id);
    RET_IF_HSA_INFO_ERR(err)

    // Get number of Compute Unit
    uint32_t compute_unit = 0;
    err = hsa_agent_get_info(agent,
                     (hsa_agent_info_t) HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT,
                                                                &compute_unit);
    RET_IF_HSA_INFO_ERR(err)

    // Print out the common results
    std::cout << std::endl;
    std::cout << "Agent #" << agent_number << ":" << std::endl;
    std::cout << "Agent Name:					" << name <<
                                                                     std::endl;
    std::cout << "Agent Vendor Name:				" <<
                                                      vendor_name << std::endl;

    if (agent_feature & HSA_AGENT_FEATURE_KERNEL_DISPATCH
        && agent_feature & HSA_AGENT_FEATURE_AGENT_DISPATCH)
      std::cout << "Agent Feature:					KERNEL_DISPATCH & AGENT_DISPATCH"
                << std::endl;
    else if (agent_feature & HSA_AGENT_FEATURE_KERNEL_DISPATCH) {
      std::cout << "Agent Feature:					KERNEL_DISPATCH" << std::endl;
    }
    else if (agent_feature & HSA_AGENT_FEATURE_AGENT_DISPATCH) {
      std::cout << "Agent Feature:					AGENT_DISPATCH" << std::endl;
    }
    else {
      std::cout << "Agent Feature:					Not Supported" << std::endl;
    }

    if (HSA_PROFILE_BASE == agent_profile) {
      std::cout << "Agent Profile:					BASE_PROFILE" << std::endl;
    }
    else if (HSA_PROFILE_FULL == agent_profile) {
      std::cout << "Agent Profile:					FULL_PROFILE" << std::endl;
    }
    else {
      std::cout << "Agent Profile:					Not Supported" << std::endl;
    }

    if (HSA_DEFAULT_FLOAT_ROUNDING_MODE_ZERO == float_rounding_mode) {
      std::cout << "Agent Floating Rounding Mode:			ZERO" << std::endl;
    }
    else if (HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR == float_rounding_mode) {
      std::cout << "Agent Floating Rounding Mode:			NEAR" << std::endl;
    }
    else {
      std::cout << "Agent Floating Rounding Mode:			Not Supported" << std::endl;
    }

    std::cout << "Agent Max Queue Number:				" << max_queue << std::endl;
    std::cout << "Agent Queue Min Size:				" << queue_min_size << std::endl;
    std::cout << "Agent Queue Max Size:				" << queue_max_size << std::endl;

    if (HSA_QUEUE_TYPE_MULTI == queue_type) {
      std::cout << "Agent Queue Type:				MULTI" << std::endl;
    }
    else if (HSA_QUEUE_TYPE_SINGLE == queue_type) {
      std::cout << "Agent Queue Type:				SINGLE" << std::endl;
    }
    else {
      std::cout << "Agent Queue Type:				Not Supported" << std::endl;
    }

    std::cout << "Agent Node:					" << node << std::endl;

    if (HSA_DEVICE_TYPE_CPU == device_type) {
      std::cout << "Agent Device Type:				CPU" << std::endl;
    }
    else if (HSA_DEVICE_TYPE_GPU == device_type) {
      std::cout << "Agent Device Type:				GPU" << std::endl;
      // Get ISA info
      hsa_isa_t agent_isa;
      err = hsa_agent_get_info(agent, HSA_AGENT_INFO_ISA, &agent_isa);
      RET_IF_HSA_INFO_ERR(err)
    }
    else {
      std::cout << "Agent Device Type:				DSP" << std::endl;
    }

    std::cout << "Agent Cache Info:" << std::endl;

    for (int i = 0; i < 4; i++) {
      if (cache_size[i]) {
        std::cout << "  $L" << i + 1 << ":						" << cache_size[i] / 1024
                  << "KB" << std::endl;
      }
    }

    std::cout << "Agent Chip ID:					" << chip_id << std::endl;
    std::cout << "Agent Cacheline Size:				" << cacheline_size << std::endl;
    std::cout << "Agent Max Clock Frequency:			" << max_clock_freq << "MHz"
              << std::endl;
    std::cout << "Agent BDFID:					" << bdf_id << std::endl;
    std::cout << "Agent Compute Unit:				" << compute_unit << std::endl;

    // Output Peak FLOPS and Peak Bandwidth if Env var is set
    // TODO: Fan, need to add BW
    if (output_amd) {
      std::string agent_name = name;

      for (size_t i = 0; i < agent_names.size(); i++) {
        if (agent_name.compare(agent_names[i]) == 0)
          std::cout << "Agent Peak GFLOPS:				" << flops_table[agent_name]
                    << std::endl;
      }
    }

    // Check if the agent is kernel agent
    if (agent_feature & HSA_AGENT_FEATURE_KERNEL_DISPATCH) {

      // Get flaf of fast_f16 operation
      bool fast_f16;
      err = hsa_agent_get_info(agent, HSA_AGENT_INFO_FAST_F16_OPERATION,
                               &fast_f16);
      RET_IF_HSA_INFO_ERR(err)

      // Get wavefront size
      uint32_t wavefront_size = 0;
      err = hsa_agent_get_info(agent, HSA_AGENT_INFO_WAVEFRONT_SIZE,
                               &wavefront_size);
      RET_IF_HSA_INFO_ERR(err)

      // Get max total number of work-items in a workgroup
      uint32_t workgroup_max_size = 0;
      err = hsa_agent_get_info(agent, HSA_AGENT_INFO_WORKGROUP_MAX_SIZE,
                               &workgroup_max_size);
      RET_IF_HSA_INFO_ERR(err)

      // Get max number of work-items of each dimension of a work-group
      uint16_t workgroup_max_dim[3];
      err = hsa_agent_get_info(agent, HSA_AGENT_INFO_WORKGROUP_MAX_DIM,
                               &workgroup_max_dim);
      RET_IF_HSA_INFO_ERR(err)

      // Get max number of a grid per dimension
      hsa_dim3_t grid_max_dim;
      err = hsa_agent_get_info(agent, HSA_AGENT_INFO_GRID_MAX_DIM,
                               &grid_max_dim);
      RET_IF_HSA_INFO_ERR(err)

      // Get max total number of work-items in a grid
      uint32_t grid_max_size = 0;
      err = hsa_agent_get_info(agent, HSA_AGENT_INFO_GRID_MAX_SIZE,
                               &grid_max_size);
      RET_IF_HSA_INFO_ERR(err)

      // Get max number of fbarriers per work group
      uint32_t fbarrier_max_size = 0;
      err = hsa_agent_get_info(agent, HSA_AGENT_INFO_FBARRIER_MAX_SIZE,
                               &fbarrier_max_size);
      RET_IF_HSA_INFO_ERR(err)

      // Print info for kernel agent
      if (true == fast_f16) {
        std::cout << "Agent Fast F16 Operation:			TRUE" <<
                                                                    std::endl;
      }

      std::cout << "Agent Wavefront Size:				" <<
                                                  wavefront_size << std::endl;
      std::cout << "Agent Workgroup Max Size:			" <<
                                              workgroup_max_size << std::endl;
      std::cout <<
               "Agent Workgroup Max Size Per Dimension:			" <<
                                                                    std::endl;

      for (int i = 0; i < 3; i++) {
        std::cout << "  Dim[" << i <<
            "]:					" << workgroup_max_dim[i] <<
                                                                    std::endl;
      }

      std::cout << "Agent Grid Max Size:				" <<
                                                   grid_max_size << std::endl;

      // Stop using the above kmt functions as per SWDEV-97044
      //
      uint32_t waves_per_cu = 0;
      err = hsa_agent_get_info(agent,
                        (hsa_agent_info_t)HSA_AMD_AGENT_INFO_MAX_WAVES_PER_CU,
                                                                &waves_per_cu);
      RET_IF_HSA_INFO_ERR(err)
      std::cout << "Agent Waves Per CU:				" <<
                                                     waves_per_cu << std::endl;
      std::cout << "Agent Max Work-item Per CU:			"
                << wavefront_size* waves_per_cu << std::endl;

      std::cout << "Agent Grid Max Size per Dimension:" << std::endl;

      for (int i = 0; i < 3; i++) {
        std::cout << "  Dim[" << i <<
                                     "]					"
                 << reinterpret_cast<uint32_t*>(&grid_max_dim)[i] << std::endl;
      }

      std::cout << "Agent Max number Of fbarriers Per Workgroup:	"
                << fbarrier_max_size << std::endl;
    }
  }

  // Get pool info
  std::cout << "Agent Pool Info:" << std::endl;
  err = hsa_amd_agent_iterate_memory_pools(agent, get_pool_info, &pool_number);
  RET_IF_HSA_INFO_ERR(err)

  return HSA_STATUS_SUCCESS;
}

// Implement region iteration function
hsa_status_t get_pool_info(hsa_amd_memory_pool_t pool, void* data) {
  hsa_status_t err;
  int* p_int = reinterpret_cast<int*>(data);
  (*p_int)++;

  std::cout << "  Pool #" << *p_int << ":" << std::endl;

  err = rocrtst::DumpMemoryPoolInfo(pool, 4);
  RET_IF_HSA_INFO_ERR(err)

  return err;
}

#undef RET_IF_HSA_INFO_ERR

void HsaInfo::DisplayResults() const {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  return;
}

void HsaInfo::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  return;
}

