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

#ifndef _TEST_SMGR_H_
#define _TEST_SMGR_H_

#include <atomic>

#include "test_aql.h"

// SimpleConvolution: Class implements OpenCL SimpleConvolution sample
class TestPMgr : public TestAql {
 public:
  typedef hsa_ext_amd_aql_pm4_packet_t packet_t;
  TestPMgr(TestAql* t);
  bool run();

 protected:
  packet_t prePacket;
  packet_t postPacket;
  hsa_signal_t dummySignal;
  hsa_signal_t postSignal;

  virtual bool buildPackets() { return false; }
  virtual bool dumpData() { return false; }
  virtual bool initialize(int argc, char** argv);

 private:
  enum {
    SLOT_PM4_SIZE_DW = HSA_EXT_AQL_PROFILE_LEGACY_PM4_PACKET_SIZE / sizeof(uint32_t),
    SLOT_PM4_SIZE_AQLP = HSA_EXT_AQL_PROFILE_LEGACY_PM4_PACKET_SIZE / sizeof(packet_t)
  };
  struct slot_pm4_s {
    uint32_t words[SLOT_PM4_SIZE_DW];
  };
  typedef std::atomic<slot_pm4_s> slot_pm4_t;

  bool addPacket(const packet_t* packet);
  bool addPacketGfx8(const packet_t* packet);
  bool addPacketGfx9(const packet_t* packet);
};

#endif  // _TEST_SMGR_H_
