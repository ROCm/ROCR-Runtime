#include <iostream>
#include <iomanip>

#include "aql_profile.h"
#include "cmdwriter.h"
#include "amd_aql_pm4_ib_packet.h"
#include "core/inc/amd_gpu_pm4.h"

namespace aql_profile {

typedef uint16_t aql_packet_header_t;

void * legacyAqlAcquire(const packet_t* aql_packet, void * data) {
  hsa_barrier_and_packet_t * aql_barrier =
    reinterpret_cast<hsa_barrier_and_packet_t*>(data);
  memset(aql_barrier, 0 , sizeof(hsa_barrier_and_packet_t));
  const aql_packet_header_t aql_header_type =
    HSA_PACKET_TYPE_BARRIER_AND << HSA_PACKET_HEADER_TYPE;
  const aql_packet_header_t aql_header_barrier =
    1ul << HSA_PACKET_HEADER_BARRIER;
  aql_barrier->header |= aql_header_type;
  aql_barrier->header |= aql_header_barrier;
  return data + sizeof(hsa_barrier_and_packet_t);
}

void * legacyAqlRelease(const packet_t* aql_packet, void * data) {
  hsa_barrier_and_packet_t * aql_barrier =
    reinterpret_cast<hsa_barrier_and_packet_t*>(data);
  memset(aql_barrier, 0 , sizeof(hsa_barrier_and_packet_t));
  const aql_packet_header_t aql_header_type =
    HSA_PACKET_TYPE_BARRIER_AND << HSA_PACKET_HEADER_TYPE;
  const aql_packet_header_t aql_header_barrier =
    1ul << HSA_PACKET_HEADER_BARRIER;
  const aql_packet_header_t aql_header_release =
    HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_SCRELEASE_FENCE_SCOPE;
  aql_barrier->header |= aql_header_type;
  aql_barrier->header |= aql_header_barrier;
  aql_barrier->header |= aql_header_release;
  aql_barrier->completion_signal = aql_packet->completion_signal;
  return data + sizeof(hsa_barrier_and_packet_t);
}

void * legacyPm4(const packet_t* aql_packet, void *data) {
  constexpr uint32_t major_version = 8;
  constexpr uint32_t slot_size_b = 0x40;
  constexpr uint32_t ib_jump_size_dw = 4;
  constexpr uint32_t slot_size_dw = uint32_t(slot_size_b / sizeof(uint32_t));

  const amd_aql_pm4_ib_packet_t* aql_pm4_ib =
    reinterpret_cast<const amd_aql_pm4_ib_packet_t*>(aql_packet);
  uint32_t * slot_data = (uint32_t*)data;
  // Construct a set of PM4 to fit inside the AQL packet slot.
  uint32_t slot_dw_idx = 0;

  // Construct a no-op command to pad the queue slot.
  constexpr uint32_t rel_mem_size_dw = 7;
  constexpr uint32_t nop_pad_size_dw = slot_size_dw - (ib_jump_size_dw + rel_mem_size_dw);

  uint32_t* nop_pad = &slot_data[slot_dw_idx];
  slot_dw_idx += nop_pad_size_dw;

  nop_pad[0] = PM4_HDR(PM4_HDR_IT_OPCODE_NOP, nop_pad_size_dw, major_version);

  for (int i = 1; i < nop_pad_size_dw; ++i) {
    nop_pad[i] = 0;
  }

  // Copy in command to execute the IB.
  assert(slot_dw_idx + ib_jump_size_dw <= slot_size_dw && "PM4 exceeded queue slot size");
  uint32_t* ib_jump = &slot_data[slot_dw_idx];
  slot_dw_idx += ib_jump_size_dw;

  memcpy(ib_jump, aql_pm4_ib->pm4_ib_command, sizeof(aql_pm4_ib->pm4_ib_command));

  // Construct a command to advance the read index and invalidate the packet
  // header. This must be the last command since this releases the queue slot
  // for writing.
  assert(slot_dw_idx + rel_mem_size_dw <= slot_size_dw && "PM4 exceeded queue slot size");
  uint32_t* rel_mem = &slot_data[slot_dw_idx];

  rel_mem[0] =
      PM4_HDR(PM4_HDR_IT_OPCODE_RELEASE_MEM, rel_mem_size_dw, major_version);
  rel_mem[1] = PM4_RELEASE_MEM_DW1_EVENT_INDEX(PM4_RELEASE_MEM_EVENT_INDEX_AQL);
  rel_mem[2] = 0;
  rel_mem[3] = 0;
  rel_mem[4] = 0;
  rel_mem[5] = 0;
  rel_mem[6] = 0;

  return data + slot_size_b;
}

void populateAql(uint32_t* ib_packet, packet_t* aql_packet) {
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

  uint32_t* words = (uint32_t*)aql_packet;
  std::clog << std::setw(40) << std::left << "AQL 'IB' size(16)"
            << ":";
  for (int idx = 0; idx < 16; idx++) {
    std::clog << " " << std::hex << std::setw(8) << std::setfill('0') << words[idx];
  }
  std::clog << std::setfill(' ') << std::endl;
}

void populateAql(void* cmd_buffer, uint32_t cmd_size,
                 pm4_profile::CommandWriter* cmd_writer, packet_t* aql_packet) {
  pm4_profile::DefaultCmdBuf ib_buffer;
  cmd_writer->BuildIndirectBufferCmd(&ib_buffer, cmd_buffer, (size_t)cmd_size);
  uint32_t* ib_packet = (uint32_t*)ib_buffer.Base();
  populateAql(ib_packet, aql_packet);
}
}
