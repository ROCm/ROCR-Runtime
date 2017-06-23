#include <string.h>
#include <assert.h>

#include <iostream>
#include <sstream>
#include <iomanip>

#include "aql_profile.h"
#include "amd_aql_pm4_ib_packet.h"
#include "gfxip/gfx8/si_pm4defs.h"
#include "gfxip/gfx8/si_ci_vi_merged_pm4_it_opcodes.h"
#include "gfxip/gfx8/si_ci_vi_merged_pm4cmds.h"

namespace aql_profile {

typedef uint16_t aql_packet_header_t;

void* legacyAqlAcquire(const packet_t* aql_packet, void* data) {
  hsa_barrier_and_packet_t* aql_barrier = reinterpret_cast<hsa_barrier_and_packet_t*>(data);
  memset(aql_barrier, 0, sizeof(hsa_barrier_and_packet_t));
  const aql_packet_header_t aql_header_type = HSA_PACKET_TYPE_BARRIER_AND << HSA_PACKET_HEADER_TYPE;
  const aql_packet_header_t aql_header_barrier = 1ul << HSA_PACKET_HEADER_BARRIER;
  const aql_packet_header_t aql_header_acquire = HSA_FENCE_SCOPE_SYSTEM
      << HSA_PACKET_HEADER_SCACQUIRE_FENCE_SCOPE;
  aql_barrier->header |= aql_header_type;
  aql_barrier->header |= aql_header_barrier;
  aql_barrier->header |= aql_header_acquire;
  return data + sizeof(hsa_barrier_and_packet_t);
}

void* legacyAqlRelease(const packet_t* aql_packet, void* data) {
  hsa_barrier_and_packet_t* aql_barrier = reinterpret_cast<hsa_barrier_and_packet_t*>(data);
  memset(aql_barrier, 0, sizeof(hsa_barrier_and_packet_t));
  const aql_packet_header_t aql_header_type = HSA_PACKET_TYPE_BARRIER_AND << HSA_PACKET_HEADER_TYPE;
  const aql_packet_header_t aql_header_barrier = 1ul << HSA_PACKET_HEADER_BARRIER;
  const aql_packet_header_t aql_header_release = HSA_FENCE_SCOPE_SYSTEM
      << HSA_PACKET_HEADER_SCRELEASE_FENCE_SCOPE;
  aql_barrier->header |= aql_header_type;
  aql_barrier->header |= aql_header_barrier;
  aql_barrier->header |= aql_header_release;
  aql_barrier->completion_signal = aql_packet->completion_signal;
  return data + sizeof(hsa_barrier_and_packet_t);
}

void* legacyPm4(const packet_t* aql_packet, void* data) {
  constexpr uint32_t major_version = 8;
  constexpr uint32_t slot_size_b = 0x40;
  constexpr uint32_t slot_size_dw = uint32_t(slot_size_b / sizeof(uint32_t));
  constexpr uint32_t ib_jump_size_dw = 4;
  constexpr uint32_t rel_mem_size_dw = 7;
  constexpr uint32_t nop_pad_size_dw = slot_size_dw - (ib_jump_size_dw + rel_mem_size_dw);

  // Construct a set of PM4 to fit inside the AQL packet slot.
  const amd_aql_pm4_ib_packet_t* aql_pm4_ib =
      reinterpret_cast<const amd_aql_pm4_ib_packet_t*>(aql_packet);
  uint32_t* const slot_data = (uint32_t*)data;
  uint32_t slot_dw_idx = 0;

  // Construct a no-op command to pad the queue slot.
  uint32_t* nop_pad = &slot_data[slot_dw_idx];
  slot_dw_idx += nop_pad_size_dw;
  nop_pad[0] = PM4_CMD(IT_NOP, nop_pad_size_dw);
  for (int i = 1; i < nop_pad_size_dw; ++i) {
    nop_pad[i] = 0;
  }

  // Copy in command to execute the IB.
  assert(slot_dw_idx + ib_jump_size_dw <= slot_size_dw);
  uint32_t* ib_jump = &slot_data[slot_dw_idx];
  slot_dw_idx += ib_jump_size_dw;
  assert(ib_jump_size_dw == sizeof(aql_pm4_ib->pm4_ib_command) / sizeof(uint32_t));
  memcpy(ib_jump, aql_pm4_ib->pm4_ib_command, sizeof(aql_pm4_ib->pm4_ib_command));

  // Construct a command to advance the read index and invalidate the packet
  // header. This must be the last command since this releases the queue slot
  // for writing.
  assert(slot_dw_idx + rel_mem_size_dw <= slot_size_dw);
  PM4CMDRELEASEMEM* rel_mem = reinterpret_cast<PM4CMDRELEASEMEM*>(&slot_data[slot_dw_idx]);
  assert(rel_mem_size_dw == sizeof(*rel_mem) / sizeof(uint32_t));
  memset(rel_mem, 0, sizeof(*rel_mem));
  rel_mem->ordinal1 = PM4_CMD(IT_RELEASE_MEM__CI__VI, rel_mem_size_dw);
  rel_mem->eventIndex = EVENT_WRITE_INDEX_CACHE_FLUSH_EVENT;

#if !defined(NDEBUG)
  std::ostringstream oss;
  oss << "AQL 'Legacy PM4' size(" << slot_size_dw << ")";
  std::clog << std::setw(40) << std::left << oss.str() << ":";
  for (int idx = 0; idx < 16; idx++) {
    std::clog << " " << std::hex << std::setw(8) << std::setfill('0') << slot_data[idx];
  }
  std::clog << std::setfill(' ') << std::endl;
#endif

  return data + slot_size_b;
}

}  // aql_profile
