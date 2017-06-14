#include <assert.h>

#include "gfxip/gfx9/gfx9_registers.h"
#include "gfxip/gfx9/gfx9_typedef.h"
#include "gfxip/gfx9/gfx9_offset.h"
#include "gfxip/gfx9/gfx9_pm4defs.h"

#include "gfx9_perf_counter.h"
#include "gfx9_block_info.h"
#include "cmdwriter.h"

using namespace std;
using namespace pm4_profile;
using namespace pm4_profile::gfx9;

// A flag to indicate the current packet is for copy register value
#define MAX_REG_NUM 100
#define COPY_DATA_FLAG 0xFFFFFFFF

namespace pm4_profile {

static char errorString[][64] = {{"No error"},
                                 {"unknow countergroup id"},
                                 {"no countergroup id"},
                                 {"invalid operation"},
                                 {"counter is not available"},
                                 {"countegroup error state"},
                                 {"countegroup is not completed"}};

Gfx9PerfCounter::Gfx9PerfCounter() {
  // Initialize the number of shader engines
  num_se_ = 4;
  Init();
}

void Gfx9PerfCounter::Init() {
  error_code_ = 0;

  // Initialize the value to use in resetting GRBM
  regGRBM_GFX_INDEX grbm_gfx_index;
  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  reset_grbm_ = grbm_gfx_index.u32All;
}

void Gfx9PerfCounter::begin(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter,
                            const CountersMap& countersMap) {
  // Reset Grbm to its default state - broadcast
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, reset_grbm_);

  // Disable RLC Perfmon Clock Gating
  // On Vega this is needed to collect Perf Cntrs
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmRLC_PERFMON_CLK_CNTL, 1);

  // Iterate through the list of blocks to generate Pm4 commands to
  // program corresponding perf counters of each block
  for (CountersMap::const_iterator block_it = countersMap.begin(); block_it != countersMap.end();
       ++block_it) {
    const uint32_t block_id = block_it->first;
    const CountersVec& counters = block_it->second;
    const uint32_t counter_count = counters.size();

    // Iterate through each enabled perf counter and building
    // corresponding Pm4 commands to program the various control
    // registers involved

    for (uint32_t ind = 0; ind < counter_count; ++ind) {
      const uint32_t counter_id = counters[ind];

      // Build the list of control registers to program which
      // varies per perf counter block
      uint32_t reg_addr[MAX_REG_NUM], reg_val[MAX_REG_NUM];
      const uint32_t reg_num =
          BuildCounterSelRegister(ind, reg_addr, reg_val, block_id, counter_id);

      // Build the list of Pm4 commands that support control
      // register programming
      for (uint32_t n = 0; n < reg_num; ++n) {
        cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, reg_addr[n], reg_val[n]);
      }
    }
  }

  // Reset Grbm to its default state - broadcast
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, reset_grbm_);

  // Program Compute_Perfcount_Enable register to support perf counting
  regCOMPUTE_PERFCOUNT_ENABLE cp_perfcount_enable;
  cp_perfcount_enable.u32All = 0;
  cp_perfcount_enable.bits.PERFCOUNT_ENABLE = 1;
  cmdWriter->BuildWriteShRegPacket(cmdBuff, mmCOMPUTE_PERFCOUNT_ENABLE, cp_perfcount_enable.u32All);

  // Reset the counter list
  regCP_PERFMON_CNTL cp_perfmon_cntl;
  cp_perfmon_cntl.u32All = 0;
  cp_perfmon_cntl.bits.PERFMON_STATE = 0;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL, cp_perfmon_cntl.u32All);

  // Start the counter list
  cp_perfmon_cntl.bits.PERFMON_STATE = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL, cp_perfmon_cntl.u32All);

  // Issue barrier command to apply the commands to configure perfcounters
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);
}

uint32_t Gfx9PerfCounter::end(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter,
                              const CountersMap& countersMap, void* dataBuff) {
  // Issue barrier command to wait for dispatch to complete
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);

  // Build PM4 packet to stop and freeze counters
  regCP_PERFMON_CNTL cp_perfmon_cntl;
  cp_perfmon_cntl.u32All = 0;
  cp_perfmon_cntl.bits.PERFMON_STATE = 2;
  cp_perfmon_cntl.bits.PERFMON_SAMPLE_ENABLE = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL, cp_perfmon_cntl.u32All);

  // Reset Grbm to its default state - broadcast
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, reset_grbm_);

  // Iterate through the list of blocks to create PM4 packets to read counter values
  uint32_t total_counter_num = 0;
  for (CountersMap::const_iterator block_it = countersMap.begin(); block_it != countersMap.end();
       ++block_it) {
    const uint32_t block_id = block_it->first;
    const uint32_t counter_count = block_it->second.size();

    for (uint32_t ind = 0; ind < counter_count; ++ind) {
      // retrieve the registers to be set
      uint32_t reg_addr[MAX_REG_NUM], reg_val[MAX_REG_NUM];
      const uint32_t reg_num = BuildCounterReadRegisters(ind, block_id, reg_addr, reg_val);

      for (uint32_t n = 0; n < reg_num; n++) {
        if (reg_val[n] == COPY_DATA_FLAG) {
          cmdWriter->BuildCopyDataPacket(cmdBuff, COPY_DATA_SEL_REG, reg_addr[n], 0,
                                         ((uint32_t*)dataBuff) + total_counter_num,
                                         COPY_DATA_SEL_COUNT_1DW, false);
          total_counter_num++;
        } else {
          cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, reg_addr[n], reg_val[n]);
        }
      }
    }
  }

  // Reset Grbm to its default state - broadcast
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, reset_grbm_);

  // Enable RLC Perfmon Clock Gating. On Vega this is
  // was disabled during Perf Cntrs collection session
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmRLC_PERFMON_CLK_CNTL, 0);

  return total_counter_num * sizeof(uint32_t);
}

int Gfx9PerfCounter::getLastError() { return error_code_; }

std::string Gfx9PerfCounter::getErrorString(int error) {
  if ((error >= 0) && (error < kErrorCodeMax)) {
    std::string err_string(errorString[error]);
    return err_string;
  }
  return string("Error input code!");
}

uint32_t Gfx9PerfCounter::ProgramTcpCntrs(uint32_t tcpRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                          uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaAiCounterBlockIdTcp0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX;
  regIdx++;

  regTCP_PERFCOUNTER0_SELECT tcp_perf_counter_select;
  tcp_perf_counter_select.u32All = 0;
  tcp_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

  regVal[regIdx] = tcp_perf_counter_select.u32All;
  regAddr[regIdx] = AiTcpCounterRegAddr[tcpRegIdx].counterSelRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx9PerfCounter::ProgramTdCntrs(uint32_t tdRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                         uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaAiCounterBlockIdTd0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX;
  regIdx++;

  regTD_PERFCOUNTER0_SELECT td_perf_counter_select;
  td_perf_counter_select.u32All = 0;
  td_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
  regVal[regIdx] = td_perf_counter_select.u32All;
  regAddr[regIdx] = AiTdCounterRegAddr[tdRegIdx].counterSelRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx9PerfCounter::ProgramTccCntrs(uint32_t tccRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                          uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaAiCounterBlockIdTcc0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX;
  regIdx++;

  regTCC_PERFCOUNTER0_SELECT tcc_perf_counter_select;
  tcc_perf_counter_select.u32All = 0;
  tcc_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

  regVal[regIdx] = tcc_perf_counter_select.u32All;
  regAddr[regIdx] = AiTccCounterRegAddr[tccRegIdx].counterSelRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx9PerfCounter::ProgramTcaCntrs(uint32_t tcaRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                          uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaAiCounterBlockIdTca0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX;
  regIdx++;

  regTCA_PERFCOUNTER0_SELECT tca_perf_counter_select;
  tca_perf_counter_select.u32All = 0;
  tca_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

  regVal[regIdx] = tca_perf_counter_select.u32All;
  regAddr[regIdx] = AiTcaCounterRegAddr[tcaRegIdx].counterSelRegAddr;
  regIdx++;
  return regIdx;
}

uint32_t Gfx9PerfCounter::ProgramTaCntrs(uint32_t taRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                         uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaAiCounterBlockIdTa0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX;
  regIdx++;

  regTA_PERFCOUNTER0_SELECT ta_perf_counter_select;
  ta_perf_counter_select.u32All = 0;
  ta_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

  regVal[regIdx] = ta_perf_counter_select.u32All;
  regAddr[regIdx] = AiTaCounterRegAddr[taRegIdx].counterSelRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx9PerfCounter::ProgramSQCntrs(uint32_t sqRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                         uint32_t blkId, uint32_t blkCntrIdx) {
  uint32_t regIdx = 0;

  // Program the SQ Counter Select Register
  regSQ_PERFCOUNTER0_SELECT sq_cntr_sel;
  sq_cntr_sel.u32All = 0;
  sq_cntr_sel.bits.SIMD_MASK = 0xF;
  sq_cntr_sel.bits.SQC_BANK_MASK = 0xF;
  sq_cntr_sel.bits.SQC_CLIENT_MASK = 0xF;
  sq_cntr_sel.bits.PERF_SEL = blkCntrIdx;
  regVal[regIdx] = sq_cntr_sel.u32All;
  regAddr[regIdx] = AiSqCounterRegAddr[sqRegIdx].counterSelRegAddr;
  regIdx++;

  // Program the SQ Counter Mask Register
  regSQ_PERFCOUNTER_MASK sq_cntr_mask;
  sq_cntr_mask.u32All = 0;
  sq_cntr_mask.bits.SH0_MASK = 0xFFFF;
  sq_cntr_mask.bits.SH1_MASK = 0xFFFF;
  regVal[regIdx] = sq_cntr_mask.u32All;
  regAddr[regIdx] = mmSQ_PERFCOUNTER_MASK;
  regIdx++;

  // Initialize the register content
  // Program the SQ Counter Control Register
  regSQ_PERFCOUNTER_CTRL sq_cntr_ctrl;
  sq_cntr_ctrl.u32All = 0;
  if (blkId == kHsaAiCounterBlockIdSq) {
    sq_cntr_ctrl.bits.PS_EN = 0x1;
    sq_cntr_ctrl.bits.VS_EN = 0x1;
    sq_cntr_ctrl.bits.GS_EN = 0x1;
    sq_cntr_ctrl.bits.HS_EN = 0x1;
    sq_cntr_ctrl.bits.CS_EN = 0x1;
  } else if (blkId == kHsaAiCounterBlockIdSqGs) {
    sq_cntr_ctrl.bits.GS_EN = 0x1;
  } else if (blkId == kHsaAiCounterBlockIdSqVs) {
    sq_cntr_ctrl.bits.VS_EN = 0x1;
  } else if (blkId == kHsaAiCounterBlockIdSqPs) {
    sq_cntr_ctrl.bits.PS_EN = 0x1;
  } else if (blkId == kHsaAiCounterBlockIdSqHs) {
    sq_cntr_ctrl.bits.HS_EN = 0x1;
  } else if (blkId == kHsaAiCounterBlockIdSqCs) {
    sq_cntr_ctrl.bits.CS_EN = 0x1;
  }

  regVal[regIdx] = sq_cntr_ctrl.u32All;
  regAddr[regIdx] = AiSqCounterRegAddr[sqRegIdx].counterCntlRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx9PerfCounter::BuildCounterSelRegister(uint32_t cntrIdx, uint32_t* regAddr,
                                                  uint32_t* regVal, uint32_t blkId,
                                                  uint32_t blkCntrIdx) {
  uint32_t instance_index = 0;
  regGRBM_GFX_INDEX grbm_gfx_index = {0};
  uint32_t regIdx = 0;

  switch (blkId) {
    // Program counters belonging to SQ block
    case kHsaAiCounterBlockIdSq:
    case kHsaAiCounterBlockIdSqGs:
    case kHsaAiCounterBlockIdSqVs:
    case kHsaAiCounterBlockIdSqPs:
    case kHsaAiCounterBlockIdSqHs:
    case kHsaAiCounterBlockIdSqCs:
      return ProgramSQCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaAiCounterBlockIdCb0:
    case kHsaAiCounterBlockIdCb1:
    case kHsaAiCounterBlockIdCb2:
    case kHsaAiCounterBlockIdCb3: {
      regIdx = 0;
      instance_index = blkId - kHsaAiCounterBlockIdCb0;
      grbm_gfx_index.u32All = 0;
      grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
      grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
      grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

      regVal[regIdx] = grbm_gfx_index.u32All;
      regAddr[regIdx] = mmGRBM_GFX_INDEX;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER0_LO;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER0_HI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER1_LO;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER1_HI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER2_LO;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER2_HI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER3_LO;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER3_HI;
      regIdx++;

      regCB_PERFCOUNTER0_SELECT cb_perf_counter_select;
      cb_perf_counter_select.u32All = 0;
      cb_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

      regVal[regIdx] = cb_perf_counter_select.u32All;
      regAddr[regIdx] = AiCbCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx++;

      break;
    }

    // Temp commented for Vega10
    /*
    case kHsaAiCounterBlockIdCpf: {
      regCPF_PERFCOUNTER0_SELECT cpf_perf_counter_select;
      cpf_perf_counter_select.u32All = 0;
      cpf_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

      regVal[0] = cpf_perf_counter_select.u32All;
      regAddr[0] = AiCpfCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }
    */

    case kHsaAiCounterBlockIdDb0:
    case kHsaAiCounterBlockIdDb1:
    case kHsaAiCounterBlockIdDb2:
    case kHsaAiCounterBlockIdDb3: {
      instance_index = blkId - kHsaAiCounterBlockIdDb0;
      regIdx = 0;
      grbm_gfx_index.u32All = 0;
      grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
      grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
      grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

      regVal[regIdx] = grbm_gfx_index.u32All;
      regAddr[regIdx] = mmGRBM_GFX_INDEX;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER0_LO;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER0_HI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER1_LO;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER1_HI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER2_LO;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER2_HI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER3_LO;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER3_HI;
      regIdx++;

      regDB_PERFCOUNTER0_SELECT db_perf_counter_select;
      db_perf_counter_select.u32All = 0;
      db_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[regIdx] = db_perf_counter_select.u32All;
      regAddr[regIdx] = AiDbCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx++;
      break;
    }

    case kHsaAiCounterBlockIdGrbm: {
      regGRBM_PERFCOUNTER0_SELECT grbm_perf_counter_select;
      grbm_perf_counter_select.u32All = 0;
      grbm_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = grbm_perf_counter_select.u32All;
      regAddr[0] = AiGrbmCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaAiCounterBlockIdGrbmSe: {
      regGRBM_SE0_PERFCOUNTER_SELECT grbm_se0_perf_counter_select;
      grbm_se0_perf_counter_select.u32All = 0;
      grbm_se0_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = grbm_se0_perf_counter_select.u32All;
      regAddr[0] = AiGrbmSeCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaAiCounterBlockIdPaSu: {
      regPA_SU_PERFCOUNTER0_SELECT pa_su_perf_counter_select;
      pa_su_perf_counter_select.u32All = 0;
      pa_su_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = pa_su_perf_counter_select.u32All;
      regAddr[0] = AiPaSuCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaAiCounterBlockIdPaSc: {
      regPA_SC_PERFCOUNTER0_SELECT pa_sc_perf_counter_select;
      pa_sc_perf_counter_select.u32All = 0;
      pa_sc_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = pa_sc_perf_counter_select.u32All;
      regAddr[0] = AiPaScCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaAiCounterBlockIdSpi: {
      regSPI_PERFCOUNTER0_SELECT spi_perf_counter_select;
      spi_perf_counter_select.u32All = 0;
      spi_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = spi_perf_counter_select.u32All;
      regAddr[0] = AiSpiCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaAiCounterBlockIdSx: {
      regIdx = 0;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER0_LO;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER0_HI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER1_LO;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER1_HI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER2_LO;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER2_HI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER3_LO;
      regIdx++;

      regSX_PERFCOUNTER0_SELECT sx_perf_counter_select;
      sx_perf_counter_select.u32All = 0;
      sx_perf_counter_select.bits.PERFCOUNTER_SELECT = blkCntrIdx;
      regVal[regIdx] = sx_perf_counter_select.u32All;
      regAddr[regIdx] = AiSxCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx++;
      break;
    }

    case kHsaAiCounterBlockIdTa0:
    case kHsaAiCounterBlockIdTa1:
    case kHsaAiCounterBlockIdTa2:
    case kHsaAiCounterBlockIdTa3:
    case kHsaAiCounterBlockIdTa4:
    case kHsaAiCounterBlockIdTa5:
    case kHsaAiCounterBlockIdTa6:
    case kHsaAiCounterBlockIdTa7:
    case kHsaAiCounterBlockIdTa8:
    case kHsaAiCounterBlockIdTa9:
    case kHsaAiCounterBlockIdTa10:
    case kHsaAiCounterBlockIdTa11:
    case kHsaAiCounterBlockIdTa12:
    case kHsaAiCounterBlockIdTa13:
    case kHsaAiCounterBlockIdTa14:
    case kHsaAiCounterBlockIdTa15:
      return ProgramTaCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaAiCounterBlockIdTca0:
    case kHsaAiCounterBlockIdTca1:
      return ProgramTcaCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaAiCounterBlockIdTcc0:
    case kHsaAiCounterBlockIdTcc1:
    case kHsaAiCounterBlockIdTcc2:
    case kHsaAiCounterBlockIdTcc3:
    case kHsaAiCounterBlockIdTcc4:
    case kHsaAiCounterBlockIdTcc5:
    case kHsaAiCounterBlockIdTcc6:
    case kHsaAiCounterBlockIdTcc7:
    case kHsaAiCounterBlockIdTcc8:
    case kHsaAiCounterBlockIdTcc9:
    case kHsaAiCounterBlockIdTcc10:
    case kHsaAiCounterBlockIdTcc11:
    case kHsaAiCounterBlockIdTcc12:
    case kHsaAiCounterBlockIdTcc13:
    case kHsaAiCounterBlockIdTcc14:
    case kHsaAiCounterBlockIdTcc15:
      return ProgramTccCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaAiCounterBlockIdTd0:
    case kHsaAiCounterBlockIdTd1:
    case kHsaAiCounterBlockIdTd2:
    case kHsaAiCounterBlockIdTd3:
    case kHsaAiCounterBlockIdTd4:
    case kHsaAiCounterBlockIdTd5:
    case kHsaAiCounterBlockIdTd6:
    case kHsaAiCounterBlockIdTd7:
    case kHsaAiCounterBlockIdTd8:
    case kHsaAiCounterBlockIdTd9:
    case kHsaAiCounterBlockIdTd10:
    case kHsaAiCounterBlockIdTd11:
    case kHsaAiCounterBlockIdTd12:
    case kHsaAiCounterBlockIdTd13:
    case kHsaAiCounterBlockIdTd14:
    case kHsaAiCounterBlockIdTd15:
      return ProgramTdCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaAiCounterBlockIdTcp0:
    case kHsaAiCounterBlockIdTcp1:
    case kHsaAiCounterBlockIdTcp2:
    case kHsaAiCounterBlockIdTcp3:
    case kHsaAiCounterBlockIdTcp4:
    case kHsaAiCounterBlockIdTcp5:
    case kHsaAiCounterBlockIdTcp6:
    case kHsaAiCounterBlockIdTcp7:
    case kHsaAiCounterBlockIdTcp8:
    case kHsaAiCounterBlockIdTcp9:
    case kHsaAiCounterBlockIdTcp10:
    case kHsaAiCounterBlockIdTcp11:
    case kHsaAiCounterBlockIdTcp12:
    case kHsaAiCounterBlockIdTcp13:
    case kHsaAiCounterBlockIdTcp14:
    case kHsaAiCounterBlockIdTcp15:
      return ProgramTcpCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaAiCounterBlockIdGds: {
      regGDS_PERFCOUNTER0_SELECT gds_perf_counter_select;
      gds_perf_counter_select.u32All = 0;
      gds_perf_counter_select.bits.PERFCOUNTER_SELECT = blkCntrIdx;
      regVal[0] = gds_perf_counter_select.u32All;
      regAddr[0] = AiGdsCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaAiCounterBlockIdVgt: {
      regVGT_PERFCOUNTER0_SELECT vgt_perf_counter_select;
      vgt_perf_counter_select.u32All = 0;
      vgt_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = vgt_perf_counter_select.u32All;
      regAddr[0] = AiVgtCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaAiCounterBlockIdIa: {
      regIA_PERFCOUNTER0_SELECT ia_perf_counter_select;
      ia_perf_counter_select.u32All = 0;
      ia_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = ia_perf_counter_select.u32All;
      regAddr[0] = AiIaCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    /*
        case kHsaAiCounterBlockIdMc: {
          // To be investigated later
          //regMC_SEQ_PERF_SEQ_CTL mc_perfcounter_select;
          //mc_perfcounter_select.u32All = 0;
          //mc_perfcounter_select.bits.PERF_SEL = blkCntrIdx;
          //regVal[0] = mc_perfcounter_select.u32All;
          //regAddr[0] = AiMcCounterRegAddr[cntrIdx].counterSelRegAddr;
          //regIdx = 1;
        }
        break;
    */

    // Temp Commented out for Vega10
    /*
    case kHsaAiCounterBlockIdSrbm: {
      regSRBM_PERFCOUNTER0_SELECT srbm_perf_counter_select;
      srbm_perf_counter_select.u32All = 0;
      srbm_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = srbm_perf_counter_select.u32All;
      regAddr[0] = AiSrbmCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }
    */

    /*
        case kHsaAiCounterBlockIdTcs: {
          regTCS_PERFCOUNTER0_SELECT__CI tcs_perf_counter_select;
          tcs_perf_counter_select.u32All = 0;
          tcs_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
          regVal[0] = tcs_perf_counter_select.u32All;
          regAddr[0] = AiTcsCounterRegAddr[cntrIdx].counterSelRegAddr;
          regIdx = 1;
          break;
        }
    */

    case kHsaAiCounterBlockIdWd: {
      regWD_PERFCOUNTER0_SELECT wd_perf_counter_select;
      wd_perf_counter_select.u32All = 0;
      wd_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = wd_perf_counter_select.u32All;
      regAddr[0] = AiWdCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    // Temp commented for Vega10
    /*
    case kHsaAiCounterBlockIdCpg: {
      regCPG_PERFCOUNTER0_SELECT cpg_perf_counter_select;
      cpg_perf_counter_select.u32All = 0;
      cpg_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = cpg_perf_counter_select.u32All;
      regAddr[0] = AiCpgCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }
    */

    // Temp commented for Vega10
    /*
    case kHsaAiCounterBlockIdCpc: {
      regCPC_PERFCOUNTER0_SELECT cpc_perf_counter_select;
      cpc_perf_counter_select.u32All = 0;
      cpc_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = cpc_perf_counter_select.u32All;
      regAddr[0] = AiCpcCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }
    */

    /*
    case kHsaAiCounterBlockIdMc: {
      AddPriviledgedCountersToList(AiBlockIdMc, blkCntrIdx);
      //Num of regs equals to 0 means it is processed by KFD
      regIdx = 0;
      break;
    }

    case kHsaAiCounterBlockIdIommuV2: {
      AddPriviledgedCountersToList(AiBlockIdIommuV2, blkCntrIdx);
      //Num of regs equals to 0 means it is processed by KFD
      regIdx = 0;
      break;
    }

    case kHsaAiCounterBlockIdKernelDriver: {
      AddPriviledgedCountersToList(AiBlockIdKernelDriver, blkCntrIdx);
      //Num of regs equals to 0 means it is processed by KFD
      regIdx = 0;
      break;
    }
    */

    default: {
      regIdx = 0;
      break;
    }
  }

  return regIdx;
}

uint32_t Gfx9PerfCounter::BuildCounterReadRegisters(uint32_t reg_index, uint32_t block_id,
                                                    uint32_t* reg_addr, uint32_t* reg_val) {
  uint32_t ii;
  uint32_t reg_num = 0;
  uint32_t instance_index;
  regGRBM_GFX_INDEX grbm_gfx_index;
  switch (block_id) {
    case kHsaAiCounterBlockIdSq:
    case kHsaAiCounterBlockIdSqGs:
    case kHsaAiCounterBlockIdSqVs:
    case kHsaAiCounterBlockIdSqPs:
    case kHsaAiCounterBlockIdSqHs:
    case kHsaAiCounterBlockIdSqCs: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiSqCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiSqCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdCb0:
    case kHsaAiCounterBlockIdCb1:
    case kHsaAiCounterBlockIdCb2:
    case kHsaAiCounterBlockIdCb3: {
      instance_index = block_id - kHsaAiCounterBlockIdCb0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiCbCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiCbCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    // Temp commented for Vega10
    /*
    case kHsaAiCounterBlockIdCpf: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = AiCpfCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiCpfCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }
    */

    case kHsaAiCounterBlockIdDb0:
    case kHsaAiCounterBlockIdDb1:
    case kHsaAiCounterBlockIdDb2:
    case kHsaAiCounterBlockIdDb3: {
      instance_index = block_id - kHsaAiCounterBlockIdDb0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiDbCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiDbCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdGrbm: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = AiGrbmCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiGrbmCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaAiCounterBlockIdGrbmSe: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = AiGrbmSeCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiGrbmSeCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaAiCounterBlockIdPaSu: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiPaSuCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiPaSuCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdPaSc: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiPaScCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiPaScCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdSpi: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiSpiCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiSpiCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdSx: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiSxCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiSxCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdTa0:
    case kHsaAiCounterBlockIdTa1:
    case kHsaAiCounterBlockIdTa2:
    case kHsaAiCounterBlockIdTa3:
    case kHsaAiCounterBlockIdTa4:
    case kHsaAiCounterBlockIdTa5:
    case kHsaAiCounterBlockIdTa6:
    case kHsaAiCounterBlockIdTa7:
    case kHsaAiCounterBlockIdTa8:
    case kHsaAiCounterBlockIdTa9:
    case kHsaAiCounterBlockIdTa10:
    case kHsaAiCounterBlockIdTa11:
    case kHsaAiCounterBlockIdTa12:
    case kHsaAiCounterBlockIdTa13:
    case kHsaAiCounterBlockIdTa14:
    case kHsaAiCounterBlockIdTa15: {
      instance_index = block_id - kHsaAiCounterBlockIdTa0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiTaCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiTaCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdTca0:
    case kHsaAiCounterBlockIdTca1: {
      instance_index = block_id - kHsaAiCounterBlockIdTca0;
      grbm_gfx_index.u32All = 0;
      grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
      grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
      grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = grbm_gfx_index.u32All;
      reg_num++;

      reg_addr[reg_num] = AiTcaCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiTcaCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaAiCounterBlockIdTcc0:
    case kHsaAiCounterBlockIdTcc1:
    case kHsaAiCounterBlockIdTcc2:
    case kHsaAiCounterBlockIdTcc3:
    case kHsaAiCounterBlockIdTcc4:
    case kHsaAiCounterBlockIdTcc5:
    case kHsaAiCounterBlockIdTcc6:
    case kHsaAiCounterBlockIdTcc7:
    case kHsaAiCounterBlockIdTcc8:
    case kHsaAiCounterBlockIdTcc9:
    case kHsaAiCounterBlockIdTcc10:
    case kHsaAiCounterBlockIdTcc11:
    case kHsaAiCounterBlockIdTcc12:
    case kHsaAiCounterBlockIdTcc13:
    case kHsaAiCounterBlockIdTcc14:
    case kHsaAiCounterBlockIdTcc15: {
      instance_index = block_id - kHsaAiCounterBlockIdTcc0;
      grbm_gfx_index.u32All = 0;
      grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
      grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
      grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = grbm_gfx_index.u32All;
      reg_num++;

      reg_addr[reg_num] = AiTccCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiTccCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaAiCounterBlockIdTd0:
    case kHsaAiCounterBlockIdTd1:
    case kHsaAiCounterBlockIdTd2:
    case kHsaAiCounterBlockIdTd3:
    case kHsaAiCounterBlockIdTd4:
    case kHsaAiCounterBlockIdTd5:
    case kHsaAiCounterBlockIdTd6:
    case kHsaAiCounterBlockIdTd7:
    case kHsaAiCounterBlockIdTd8:
    case kHsaAiCounterBlockIdTd9:
    case kHsaAiCounterBlockIdTd10:
    case kHsaAiCounterBlockIdTd11:
    case kHsaAiCounterBlockIdTd12:
    case kHsaAiCounterBlockIdTd13:
    case kHsaAiCounterBlockIdTd14:
    case kHsaAiCounterBlockIdTd15: {
      instance_index = block_id - kHsaAiCounterBlockIdTd0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiTdCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiTdCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdTcp0:
    case kHsaAiCounterBlockIdTcp1:
    case kHsaAiCounterBlockIdTcp2:
    case kHsaAiCounterBlockIdTcp3:
    case kHsaAiCounterBlockIdTcp4:
    case kHsaAiCounterBlockIdTcp5:
    case kHsaAiCounterBlockIdTcp6:
    case kHsaAiCounterBlockIdTcp7:
    case kHsaAiCounterBlockIdTcp8:
    case kHsaAiCounterBlockIdTcp9:
    case kHsaAiCounterBlockIdTcp10:
    case kHsaAiCounterBlockIdTcp11:
    case kHsaAiCounterBlockIdTcp12:
    case kHsaAiCounterBlockIdTcp13:
    case kHsaAiCounterBlockIdTcp14:
    case kHsaAiCounterBlockIdTcp15: {
      instance_index = block_id - kHsaAiCounterBlockIdTcp0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiTcpCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiTcpCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdGds: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = AiGdsCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiGdsCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaAiCounterBlockIdVgt: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiVgtCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiVgtCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaAiCounterBlockIdIa: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = AiIaCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = AiIaCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }
    /*
        case kHsaAiCounterBlockIdMc: {
          reg_addr[reg_num] = mmGRBM_GFX_INDEX;
          reg_val[reg_num] = reset_grbm_;
          reg_num++;

          reg_addr[reg_num] = AiMcCounterRegAddr[reg_index].counterReadRegAddrLo;
          reg_val[reg_num] = COPY_DATA_FLAG;
          reg_num++;

          reg_addr[reg_num] = AiMcCounterRegAddr[reg_index].counterReadRegAddrHi;
          reg_val[reg_num] = COPY_DATA_FLAG;
          reg_num++;
          break;
        }
    */
    // Temp Commented out for Vega10
    /*
    case kHsaAiCounterBlockIdSrbm: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = AiSrbmCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiSrbmCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }
    */
    /*
        case kHsaAiCounterBlockIdTcs: {
          reg_addr[reg_num] = mmGRBM_GFX_INDEX;
          reg_val[reg_num] = reset_grbm_;
          reg_num++;

          reg_addr[reg_num] = AiTcsCounterRegAddr[reg_index].counterReadRegAddrLo;
          reg_val[reg_num] = COPY_DATA_FLAG;
          reg_num++;

          reg_addr[reg_num] = AiTcsCounterRegAddr[reg_index].counterReadRegAddrHi;
          reg_val[reg_num] = COPY_DATA_FLAG;
          reg_num++;
          break;
        }
    */
    case kHsaAiCounterBlockIdWd: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = AiWdCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiWdCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    // Temp commented for Vega10
    /*
    case kHsaAiCounterBlockIdCpg: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = AiCpgCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiCpgCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }
    */

    // Temp commented for Vega10
    /*
    case kHsaAiCounterBlockIdCpc: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = AiCpcCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = AiCpcCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }
    */

    // IommuV2, MC, kernel driver counters are retrieved via
    // KFD implementation
    case kHsaAiCounterBlockIdMc:
    case kHsaAiCounterBlockIdIommuV2:
    case kHsaAiCounterBlockIdKernelDriver: {
      reg_num = 0;
      break;
    }

    default: { break; }
  }

  return reg_num;
}

} /* namespace */
