#include "pm4_factory.h"
// Commandwriter includes
#include "gfx8_cmdwriter.h"
// PMC includes
#include "gfx8_perf_counter.h"
// SQTT includes
#include "gfx8_thread_trace.h"
// Block info
#include "gfx8_block_info.h"

namespace aql_profile {

// GFX9 block ID mapping table
uint32_t Gfx8Factory::block_id_table[HSA_EXT_AQL_PROFILE_BLOCKS_NUMBER] = {
    pm4_profile::kHsaViCounterBlockIdCb0,    pm4_profile::kHsaViCounterBlockIdCpf,
    pm4_profile::kHsaViCounterBlockIdDb0,    pm4_profile::kHsaViCounterBlockIdGrbm,
    pm4_profile::kHsaViCounterBlockIdGrbmSe, pm4_profile::kHsaViCounterBlockIdPaSu,
    pm4_profile::kHsaViCounterBlockIdPaSc,   pm4_profile::kHsaViCounterBlockIdSpi,
    pm4_profile::kHsaViCounterBlockIdSq,     pm4_profile::kHsaViCounterBlockIdSqEs,
    pm4_profile::kHsaViCounterBlockIdSqGs,   pm4_profile::kHsaViCounterBlockIdSqVs,
    pm4_profile::kHsaViCounterBlockIdSqPs,   pm4_profile::kHsaViCounterBlockIdSqLs,
    pm4_profile::kHsaViCounterBlockIdSqHs,   pm4_profile::kHsaViCounterBlockIdSqCs,
    pm4_profile::kHsaViCounterBlockIdSx,     pm4_profile::kHsaViCounterBlockIdTa0,
    pm4_profile::kHsaViCounterBlockIdTca0,   pm4_profile::kHsaViCounterBlockIdTcc0,
    pm4_profile::kHsaViCounterBlockIdTd0,    pm4_profile::kHsaViCounterBlockIdTcp0,
    pm4_profile::kHsaViCounterBlockIdGds,    pm4_profile::kHsaViCounterBlockIdVgt,
    pm4_profile::kHsaViCounterBlockIdIa,     pm4_profile::kHsaViCounterBlockIdMc,
    pm4_profile::kHsaViCounterBlockIdSrbm,   pm4_profile::kHsaViCounterBlockIdTcs,
    pm4_profile::kHsaViCounterBlockIdWd,     pm4_profile::kHsaViCounterBlockIdCpg,
    pm4_profile::kHsaViCounterBlockIdCpc};

pm4_profile::CommandWriter* Gfx8Factory::getCommandWriter() {
  return new pm4_profile::gfx8::Gfx8CmdWriter(false, true);
}

pm4_profile::Pmu* Gfx8Factory::getPmcMgr() { return new pm4_profile::Gfx8PerfCounter(); }

pm4_profile::ThreadTrace* Gfx8Factory::getSqttMgr() { return new pm4_profile::Gfx8ThreadTrace(); }

}  // aql_profile
