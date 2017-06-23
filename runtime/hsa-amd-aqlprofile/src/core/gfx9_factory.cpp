#include "pm4_factory.h"
// Commandwriter includes
#include "gfx9_cmdwriter.h"
// PMC includes
#include "gfx9_perf_counter.h"
// SQTT includes
#include "gfx9_thread_trace.h"
// Block info
#include "gfx9_block_info.h"

namespace aql_profile {

// GFX9 block ID mapping table
uint32_t Gfx9Factory::block_id_table[HSA_VEN_AMD_AQLPROFILE_BLOCKS_NUMBER] = {
    pm4_profile::kHsaAiCounterBlockIdCb0,
    kBadBlockId /*CPF*/,
    pm4_profile::kHsaAiCounterBlockIdDb0,
    pm4_profile::kHsaAiCounterBlockIdGrbm,
    pm4_profile::kHsaAiCounterBlockIdGrbmSe,
    pm4_profile::kHsaAiCounterBlockIdPaSu,
    pm4_profile::kHsaAiCounterBlockIdPaSc,
    pm4_profile::kHsaAiCounterBlockIdSpi,
    pm4_profile::kHsaAiCounterBlockIdSq,
    kBadBlockId /*GFX8:SQES*/,
    pm4_profile::kHsaAiCounterBlockIdSqGs,
    pm4_profile::kHsaAiCounterBlockIdSqVs,
    pm4_profile::kHsaAiCounterBlockIdSqPs,
    kBadBlockId /*GFX8:SQLS*/,
    pm4_profile::kHsaAiCounterBlockIdSqHs,
    pm4_profile::kHsaAiCounterBlockIdSqCs,
    pm4_profile::kHsaAiCounterBlockIdSx,
    pm4_profile::kHsaAiCounterBlockIdTa0,
    pm4_profile::kHsaAiCounterBlockIdTca0,
    pm4_profile::kHsaAiCounterBlockIdTcc0,
    pm4_profile::kHsaAiCounterBlockIdTd0,
    pm4_profile::kHsaAiCounterBlockIdTcp0,
    pm4_profile::kHsaAiCounterBlockIdGds,
    pm4_profile::kHsaAiCounterBlockIdVgt,
    pm4_profile::kHsaAiCounterBlockIdIa,
    pm4_profile::kHsaAiCounterBlockIdMc,
    kBadBlockId /*SRBM*/,
    pm4_profile::kHsaAiCounterBlockIdTcs,
    pm4_profile::kHsaAiCounterBlockIdWd,
    kBadBlockId /*CPG*/,
    pm4_profile::kHsaAiCounterBlockIdCpc};

pm4_profile::CommandWriter* Gfx9Factory::getCommandWriter() {
  auto p = new pm4_profile::gfx9::Gfx9CmdWriter(false, true);
  if (p == NULL) throw aql_profile_exc_msg("CommandWriter allocation failed");
  return p;
}

pm4_profile::PerfCounter* Gfx9Factory::getPmcMgr() {
  auto p = new pm4_profile::Gfx9PerfCounter();
  if (p == NULL) throw aql_profile_exc_msg("PerfCounter mgr allocation failed");
  return p;
}

pm4_profile::ThreadTrace* Gfx9Factory::getSqttMgr() {
  auto p = new pm4_profile::Gfx9ThreadTrace();
  if (p == NULL) throw aql_profile_exc_msg("ThreadTrace mgr allocation failed");
  return p;
}

}  // aql_profile
