#include "pm4_factory.h"
// Commandwriter includes
#include "gfx8_cmdwriter.h"
#include "gfx9_cmdwriter.h"
// PMC includes
#include "vi_pmu.h"
#include "ai_pmu.h"
// SQTT includes
#include "gfx8_thread_trace.h"
#include "gfx9_thread_trace.h"

namespace aql_profile {

// GFX8 block ID mapping table
uint32_t gfx8_block_id_table[HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER] = {
    pm4_profile::kHsaViCounterBlockIdCb0,    pm4_profile::kHsaViCounterBlockIdCpf,
    pm4_profile::kHsaViCounterBlockIdDb0,    pm4_profile::kHsaViCounterBlockIdGrbm,
    pm4_profile::kHsaViCounterBlockIdGrbmSe, pm4_profile::kHsaViCounterBlockIdPaSu,
    pm4_profile::kHsaViCounterBlockIdPaSc,   pm4_profile::kHsaViCounterBlockIdSpi,
    pm4_profile::kHsaViCounterBlockIdSq,     pm4_profile::kHsaViCounterBlockIdSqGs,
    pm4_profile::kHsaViCounterBlockIdSqVs,   pm4_profile::kHsaViCounterBlockIdSqPs,
    pm4_profile::kHsaViCounterBlockIdSqHs,   pm4_profile::kHsaViCounterBlockIdSqCs,
    pm4_profile::kHsaViCounterBlockIdSx,     pm4_profile::kHsaViCounterBlockIdTa0,
    pm4_profile::kHsaViCounterBlockIdTca0,   pm4_profile::kHsaViCounterBlockIdTcc0,
    pm4_profile::kHsaViCounterBlockIdTd0,    pm4_profile::kHsaViCounterBlockIdTcp0,
    pm4_profile::kHsaViCounterBlockIdGds,    pm4_profile::kHsaViCounterBlockIdVgt,
    pm4_profile::kHsaViCounterBlockIdIa,     pm4_profile::kHsaViCounterBlockIdMc,
    pm4_profile::kHsaViCounterBlockIdTcs,    pm4_profile::kHsaViCounterBlockIdWd};

// GFX9 block ID mapping table
uint32_t gfx9_block_id_table[HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER] = {
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

pm4_profile::CommandWriter * Pm4Factory::getCommandWriter() {
  return (is_gfx9 == true) ?
    new pm4_profile::gfx9::Gfx9CmdWriter(false, true) :
    new pm4_profile::gfx8::Gfx8CmdWriter(false, true);
}

pm4_profile::Pmu * Pm4Factory::getPmcMgr() {
  return (is_gfx9 == true) ?
    new pm4_profile::AiPmu() :
    new pm4_profile::ViPmu();
}

pm4_profile::ThreadTrace * Pm4Factory::getSqttMgr() {
  return (is_gfx9 == true) ?
    new pm4_profile::Gfx9ThreadTrace() :
    new pm4_profile::Gfx8ThreadTrace();
}

uint32_t Pm4Factory::getBlockId(const event_t* event) {
  return (is_gfx9 == true) ?
    gfx9_block_id_table[event->block_name] + event->block_index :
    gfx8_block_id_table[event->block_name] + event->block_index :
}

} // aql_profile
