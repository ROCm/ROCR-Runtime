#include "pm4_factory.h"
// Commandwriter includes
#include "gfx9_cmdwriter.h"
// PMC includes
#include "ai_pmu.h"
// SQTT includes
#include "gfx9_thread_trace.h"

namespace aql_profile {

// GFX9 block ID mapping table
uint32_t Gfx9Factory::block_id_table[HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER] = {
    pm4_profile::kHsaAiCounterBlockIdCb0,    pm4_profile::kHsaAiCounterBlockIdCpf,
    pm4_profile::kHsaAiCounterBlockIdDb0,    pm4_profile::kHsaAiCounterBlockIdGrbm,
    pm4_profile::kHsaAiCounterBlockIdGrbmSe, pm4_profile::kHsaAiCounterBlockIdPaSu,
    pm4_profile::kHsaAiCounterBlockIdPaSc,   pm4_profile::kHsaAiCounterBlockIdSpi,
    pm4_profile::kHsaAiCounterBlockIdSq,     pm4_profile::kHsaAiCounterBlockIdSqGs,
    pm4_profile::kHsaAiCounterBlockIdSqVs,   pm4_profile::kHsaAiCounterBlockIdSqPs,
    pm4_profile::kHsaAiCounterBlockIdSqHs,   pm4_profile::kHsaAiCounterBlockIdSqCs,
    pm4_profile::kHsaAiCounterBlockIdSx,     pm4_profile::kHsaAiCounterBlockIdTa0,
    pm4_profile::kHsaAiCounterBlockIdTca0,   pm4_profile::kHsaAiCounterBlockIdTcc0,
    pm4_profile::kHsaAiCounterBlockIdTd0,    pm4_profile::kHsaAiCounterBlockIdTcp0,
    pm4_profile::kHsaAiCounterBlockIdGds,    pm4_profile::kHsaAiCounterBlockIdVgt,
    pm4_profile::kHsaAiCounterBlockIdIa,     pm4_profile::kHsaAiCounterBlockIdMc,
    pm4_profile::kHsaAiCounterBlockIdTcs,    pm4_profile::kHsaAiCounterBlockIdWd};

pm4_profile::CommandWriter * Gfx9Factory::getCommandWriter() {
  return new pm4_profile::gfx9::Gfx9CmdWriter(false, true);
}

pm4_profile::Pmu * Gfx9Factory::getPmcMgr() {
  return new pm4_profile::AiPmu();
}

pm4_profile::ThreadTrace * Gfx9Factory::getSqttMgr() {
  return new pm4_profile::Gfx9ThreadTrace();
}

uint32_t Gfx9Factory::getBlockId(const event_t* event) {
  return block_id_table[event->block_name] + event->block_index;
}

} // aql_profile
