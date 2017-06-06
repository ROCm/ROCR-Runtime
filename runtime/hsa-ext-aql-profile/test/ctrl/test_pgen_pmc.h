/******************************************************************************

Copyright ©2013 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#ifndef _TEST_PGEN_PMC_H_
#define _TEST_PGEN_PMC_H_

#include "test_pgen.h"

hsa_status_t TestPGenPMC_Callback(hsa_ext_amd_aql_profile_info_type_t info_type,
                                  hsa_ext_amd_aql_profile_info_data_t* info_data,
                                  void* callback_data) {
  hsa_status_t status = HSA_STATUS_SUCCESS;
  typedef std::vector<hsa_ext_amd_aql_profile_info_data_t> passed_data_t;
  reinterpret_cast<passed_data_t*>(callback_data)->push_back(*info_data);
  return status;
}

// SimpleConvolution: Class implements OpenCL SimpleConvolution sample
class TestPGenPMC : public TestPGen {
  const static uint32_t buffer_alignment = 0x1000;  // 4K

  hsa_agent_t agent;
  hsa_ext_amd_aql_profile_profile_t profile;
  hsa_ext_amd_aql_profile_event_t events[2];

  bool buildPackets() { return true; }

  bool dumpData() {
    std::cout << "TestPGenPMC::dumpData :" << std::endl;

    typedef std::vector<hsa_ext_amd_aql_profile_info_data_t> callback_data_t;

    callback_data_t data;
    hsa_ext_amd_aql_profile_iterate_data(&profile, TestPGenPMC_Callback, &data);
    for (callback_data_t::iterator it = data.begin(); it != data.end(); ++it) {
      std::cout << "> sample(" << dec << it->sample_id << ") block("
                << it->pmc_data.event.block_name << "_" << it->pmc_data.event.block_index
                << ") result(" << hex << it->pmc_data.result << ")" << std::endl;
    }

    return true;
  }

 public:
  TestPGenPMC(TestAql* t) : TestPGen(t) { std::cout << "Test: PGen PMC" << std::endl; }

  bool initialize(int arg_cnt, char** arg_list) {
    if (!TestPMgr::initialize(arg_cnt, arg_list)) return false;

    hsa_status_t status;
    hsa_agent_t agent;
    uint32_t command_buffer_alignment;
    uint32_t command_buffer_size;
    uint32_t output_buffer_alignment;
    uint32_t output_buffer_size;

    // GPU identificator
    agent = getAgentInfo()->dev_id;

    // Instantiation of the profile object
    // //////////////////////////////////////////////////////////////
    // Set the event fields
    events[0].block_name = HSA_EXT_AQL_PROFILE_BLOCK_SQ;
    events[0].block_index = 0;
    events[0].counter_id = 0x4;  // SQ_SQ_PERF_SEL_WAVES
    events[1].block_name = HSA_EXT_AQL_PROFILE_BLOCK_SQ;
    events[1].block_index = 0;
    events[1].counter_id = 0xe;  // SQ_SQ_PERF_SEL_ITEMS

    // Initialization the profile
    memset(&profile, 0, sizeof(profile));
    profile.agent = agent;
    profile.type = HSA_EXT_AQL_PROFILE_EVENT_PMC;

    // set enabled events list
    profile.events = events;
    profile.event_count = 2;

    // Profile buffers attributes
    command_buffer_alignment = buffer_alignment;
    status = hsa_ext_amd_aql_profile_get_info(
        &profile, HSA_EXT_AQL_PROFILE_INFO_COMMAND_BUFFER_SIZE, &command_buffer_size);
    assert(status == HSA_STATUS_SUCCESS);

    output_buffer_alignment = buffer_alignment;
    status = hsa_ext_amd_aql_profile_get_info(&profile, HSA_EXT_AQL_PROFILE_INFO_PMC_DATA_SIZE,
                                              &output_buffer_size);
    assert(status == HSA_STATUS_SUCCESS);

    // Application is allocating the command buffer
    // Allocate(command_buffer_alignment, command_buffer_size,
    //          MODE_HOST_ACC|MODE_DEV_ACC|MODE_EXEC_DATA)
    profile.command_buffer.ptr =
        getRsrcFactory()->AllocateSysMemory(getAgentInfo(), command_buffer_size);
    profile.command_buffer.size = command_buffer_size;

    // Application is allocating the output buffer
    // Allocate(output_buffer_alignment, output_buffer_size,
    //          MODE_HOST_ACC|MODE_DEV_ACC)
    profile.output_buffer.ptr =
        getRsrcFactory()->AllocateSysMemory(getAgentInfo(), output_buffer_size);
    profile.output_buffer.size = output_buffer_size;
    memset(profile.output_buffer.ptr, 0x77, output_buffer_size);

    // Populating the AQL start packet
    status = hsa_ext_amd_aql_profile_start(&profile, PrePacket());
    assert(status == HSA_STATUS_SUCCESS);
    if (status != HSA_STATUS_SUCCESS) return false;

    // Populating the AQL stop packet
    status = hsa_ext_amd_aql_profile_stop(&profile, PostPacket());
    assert(status == HSA_STATUS_SUCCESS);

    return (status == HSA_STATUS_SUCCESS);
  }
};

#endif  // _TEST_PGEN_PMC_H_
