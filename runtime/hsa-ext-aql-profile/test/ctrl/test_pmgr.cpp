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

#include <atomic>
#include "test_assert.h"

#include "test_pmgr.h"

bool TestPMgr::addPacketGfx9(const packet_t* packet) {
  packet_t aql_packet = *packet;

  // Compute the write index of queue and copy Aql packet into it
  uint64_t que_idx = hsa_queue_load_write_index_relaxed(getQueue());
  const uint32_t mask = getQueue()->size - 1;

  // Disable packet so that submission to HW is complete
  const auto header = HSA_PACKET_TYPE_VENDOR_SPECIFIC << HSA_PACKET_HEADER_TYPE;
  aql_packet.header &= (~((1 << HSA_PACKET_HEADER_WIDTH_TYPE) - 1)) << HSA_PACKET_HEADER_TYPE;
  aql_packet.header |= HSA_PACKET_TYPE_INVALID << HSA_PACKET_HEADER_TYPE;

  // Copy Aql packet into queue buffer
  ((packet_t*)(getQueue()->base_address))[que_idx & mask] = aql_packet;

  // After AQL packet is fully copied into queue buffer
  // update packet header from invalid state to valid state
  std::atomic_thread_fence(std::memory_order_release);
  ((packet_t*)(getQueue()->base_address))[que_idx & mask].header = header;

  // Increment the write index and ring the doorbell to dispatch the kernel.
  hsa_queue_store_write_index_relaxed(getQueue(), (que_idx + 1));
  hsa_signal_store_relaxed(getQueue()->doorbell_signal, que_idx);

  return true;
}

bool TestPMgr::addPacketGfx8(const packet_t* packet) {
  // Create legacy devices PM4 data
  const hsa_ext_amd_aql_pm4_packet_t* aql_packet = (const hsa_ext_amd_aql_pm4_packet_t*)packet;
  slot_pm4_s data;
  hsa_ext_amd_aql_profile_legacy_get_pm4(aql_packet, (void*)data.words);

  // Compute the write index of queue and copy Aql packet into it
  uint64_t que_idx = hsa_queue_load_write_index_relaxed(getQueue());
  const uint32_t mask = getQueue()->size - 1;

  // Copy Aql packet into queue buffer
  packet_t* ptr = ((packet_t*)(getQueue()->base_address)) + (que_idx & mask);
  slot_pm4_t* slot_pm4 = (slot_pm4_t*)ptr;
  slot_pm4->store(data, std::memory_order_relaxed);

  // Increment the write index and ring the doorbell to dispatch the kernel.
  hsa_queue_store_write_index_relaxed(getQueue(), (que_idx + SLOT_PM4_SIZE_AQLP));
  hsa_signal_store_relaxed(getQueue()->doorbell_signal, que_idx + SLOT_PM4_SIZE_AQLP - 1);

  return true;
}

bool TestPMgr::addPacket(const packet_t* packet) {
  const char* agent_name = getAgentInfo()->name;
  return (strncmp(agent_name, "gfx8", 4) == 0) ? addPacketGfx8(packet) : addPacketGfx9(packet);
}

bool TestPMgr::run() {
  // Build Aql Pkts
  const bool active = buildPackets();
  if (active) {
    // Submit Pre-Dispatch Aql packet
    addPacket(&prePacket);
  }

  testAql()->run();

  if (active) {
    // Set post packet completion signal
    postPacket.completion_signal = postSignal;

    // Submit Post-Dispatch Aql packet
    addPacket(&postPacket);

    // Wait for Post-Dispatch packet to complete
    hsa_signal_wait_acquire(postSignal, HSA_SIGNAL_CONDITION_LT, 1, (uint64_t)-1,
                            HSA_WAIT_STATE_BLOCKED);

    // Dumping profiling data
    dumpData();
  }

  return true;
}

bool TestPMgr::initialize(int argc, char** argv) {
  TestAql::initialize(argc, argv);
  hsa_status_t status = hsa_signal_create(1, 0, NULL, &postSignal);
  test_assert(status == HSA_STATUS_SUCCESS);
  return (status == HSA_STATUS_SUCCESS);
}

TestPMgr::TestPMgr(TestAql* t) : TestAql(t) {
  dummySignal.handle = 0;
  postSignal = dummySignal;
}
