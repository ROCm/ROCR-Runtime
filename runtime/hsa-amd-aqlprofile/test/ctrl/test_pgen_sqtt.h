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

#ifndef _TEST_PGEN_SQTT_H_
#define _TEST_PGEN_SQTT_H_

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>

#include "test_assert.h"
#include "test_pgen.h"

hsa_status_t TestPGenSQTT_Callback(hsa_ven_amd_aqlprofile_info_type_t info_type,
                                   hsa_ven_amd_aqlprofile_info_data_t* info_data,
                                   void* callback_data) {
  hsa_status_t status = HSA_STATUS_SUCCESS;
  typedef std::vector<hsa_ven_amd_aqlprofile_info_data_t> passed_data_t;
  reinterpret_cast<passed_data_t*>(callback_data)->push_back(*info_data);
  return status;
}

// SimpleConvolution: Class implements OpenCL SimpleConvolution sample
class TestPGenSQTT : public TestPGen {
  const static uint32_t buffer_alignment = 0x1000;  // 4K
  const static uint32_t buffer_size = 0x2000000;    // 32M

  hsa_agent_t agent;
  hsa_ven_amd_aqlprofile_profile_t profile;

  bool buildPackets() { return true; }

  bool dumpData() {
    std::cout << "TestPGenSQTT::dumpData :" << std::endl;

    typedef std::vector<hsa_ven_amd_aqlprofile_info_data_t> callback_data_t;

    callback_data_t data;
    api.hsa_ven_amd_aqlprofile_iterate_data(&profile, TestPGenSQTT_Callback, &data);
    for (callback_data_t::iterator it = data.begin(); it != data.end(); ++it) {
      std::cout << "> sample(" << dec << it->sample_id << ") ptr(" << hex << it->sqtt_data.ptr
                << ") size(" << dec << it->sqtt_data.size << ")" << std::endl;

      void* sys_buf = getRsrcFactory()->AllocateSysMemory(getAgentInfo(), it->sqtt_data.size);
      test_assert(sys_buf != NULL);
      if (sys_buf == NULL) return HSA_STATUS_ERROR;

      hsa_status_t status = hsa_memory_copy(sys_buf, it->sqtt_data.ptr, it->sqtt_data.size);
      test_assert(status == HSA_STATUS_SUCCESS);
      if (status != HSA_STATUS_SUCCESS) return status;

      std::string file_name;
      file_name.append("sqtt_dump_");
      file_name.append(std::to_string(it->sample_id));
      file_name.append(".txt");
      std::ofstream out_file;
      out_file.open(file_name);

      // Write the buffer in terms of shorts (16 bits)
      short* sqtt_data = (short*)sys_buf;
      for (int i = 0; i < (it->sqtt_data.size / sizeof(short)); ++i) {
        out_file << std::setw(4) << std::setfill('0') << std::hex << sqtt_data[i] << "\n";
      }

      out_file.close();
    }

    return true;
  }

 public:
  explicit TestPGenSQTT(TestAql* t) : TestPGen(t) { std::cout << "Test: PGen SQTT" << std::endl; }

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
    // Set the parameters
    // parameters = ....;

    // Initialization the profile
    memset(&profile, 0, sizeof(profile));
    profile.agent = agent;
    profile.type = HSA_VEN_AMD_AQLPROFILE_EVENT_TYPE_SQTT;

    // set parameters
    // profile.parameters = &event;
    // profile.parameter_count = 1;

    // Profile buffers attributes
    command_buffer_alignment = buffer_alignment;
    status = api.hsa_ven_amd_aqlprofile_get_info(
        &profile, HSA_VEN_AMD_AQLPROFILE_INFO_COMMAND_BUFFER_SIZE, &command_buffer_size);
    test_assert(status == HSA_STATUS_SUCCESS);

    output_buffer_alignment = buffer_alignment;
    output_buffer_size = buffer_size;

    // Application is allocating the command buffer
    // AllocateSystem(command_buffer_alignment, command_buffer_size,
    //                MODE_HOST_ACC|MODE_DEV_ACC|MODE_EXEC_DATA)
    profile.command_buffer.ptr =
        getRsrcFactory()->AllocateSysMemory(getAgentInfo(), command_buffer_size);
    profile.command_buffer.size = command_buffer_size;

    // Application is allocating the output buffer
    // AllocateLocal(output_buffer_alignment, output_buffer_size,
    //               MODE_DEV_ACC)
    profile.output_buffer.ptr =
        getRsrcFactory()->AllocateLocalMemory(getAgentInfo(), output_buffer_size);
    profile.output_buffer.size = output_buffer_size;

    // Populating the AQL start packet
    status = api.hsa_ven_amd_aqlprofile_start(&profile, PrePacket());
    test_assert(status == HSA_STATUS_SUCCESS);
    if (status != HSA_STATUS_SUCCESS) return false;

    // Populating the AQL stop packet
    status = api.hsa_ven_amd_aqlprofile_stop(&profile, PostPacket());
    test_assert(status == HSA_STATUS_SUCCESS);

    return (status == HSA_STATUS_SUCCESS);
  }
};

#endif  // _TEST_PGEN_SQTT_H_
