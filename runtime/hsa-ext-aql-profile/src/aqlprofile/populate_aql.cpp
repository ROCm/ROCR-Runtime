#include <iostream>
#include <sstream>
#include <iomanip>
#include <assert.h>

#include "aql_profile.h"
#include "cmdwriter.h"
#include "amd_aql_pm4_ib_packet.h"

namespace aql_profile {

void populateAql(const uint32_t* ib_packet, packet_t* aql_packet) {
  // Populate relevant fields of Aql pkt
  // Size of IB pkt is four DWords
  // Header and completion sinal are not set
  amd_aql_pm4_ib_packet_t* aql_pm4_ib = reinterpret_cast<amd_aql_pm4_ib_packet_t*>(aql_packet);
  aql_pm4_ib->pm4_ib_format = AMD_AQL_PM4_IB_FORMAT;
  aql_pm4_ib->pm4_ib_command[0] = ib_packet[0];
  aql_pm4_ib->pm4_ib_command[1] = ib_packet[1];
  aql_pm4_ib->pm4_ib_command[2] = ib_packet[2];
  aql_pm4_ib->pm4_ib_command[3] = ib_packet[3];
  aql_pm4_ib->dw_count_remain = AMD_AQL_PM4_IB_DW_COUNT_REMAIN;
  for (int i = 0; i < AMD_AQL_PM4_IB_RESERVED_COUNT; ++i) {
    aql_pm4_ib->reserved[i] = 0;
  }

#if !defined(NDEBUG)
  const uint32_t* dwords = (uint32_t*)aql_packet;
  const uint32_t dword_count = sizeof(*aql_packet) / sizeof(uint32_t);
  std::ostringstream oss;
  oss << "AQL 'IB' size(" << dword_count << ")";
  std::clog << std::setw(40) << std::left << "AQL 'IB' size(16)"
            << ":";
  for (int idx = 0; idx < dword_count; idx++) {
    std::clog << " " << std::hex << std::setw(8) << std::setfill('0') << dwords[idx];
  }
  std::clog << std::setfill(' ') << std::endl;
#endif
}

void populateAql(const void* cmd_buffer, uint32_t cmd_size, pm4_profile::CommandWriter* cmd_writer,
                 packet_t* aql_packet) {
  pm4_profile::DefaultCmdBuf ib_buffer;
  cmd_writer->BuildIndirectBufferCmd(&ib_buffer, cmd_buffer, (size_t)cmd_size);
  populateAql((const uint32_t*)ib_buffer.Base(), aql_packet);
}

}  // aql_profile
