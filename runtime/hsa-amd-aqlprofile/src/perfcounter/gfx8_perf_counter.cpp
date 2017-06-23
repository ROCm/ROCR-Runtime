#include <assert.h>

#include "gfxip/gfx8/si_ci_vi_merged_typedef.h"
#include "gfxip/gfx8/si_ci_vi_merged_offset.h"
#include "gfxip/gfx8/si_ci_vi_merged_enum.h"
#include "gfxip/gfx8/si_pm4defs.h"

#include "gfx8_perf_counter.h"
#include "gfx8_block_info.h"
#include "cmdwriter.h"

using namespace std;
using namespace pm4_profile;

// A flag to indicate the current packet is for copy register value
#define MAX_REG_NUM 100
#define COPY_DATA_FLAG 0xFFFFFFFF

namespace pm4_profile {

Gfx8PerfCounter::Gfx8PerfCounter() {
  // Initialize the number of shader engines
  num_se_ = 4;
  Init();
}

void Gfx8PerfCounter::Init() {
  // Initialize the value to use in resetting GRBM
  regGRBM_GFX_INDEX grbm_gfx_index;
  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  reset_grbm_ = grbm_gfx_index.u32All;
}

void Gfx8PerfCounter::begin(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter,
                            const CountersMap& countersMap) {
  // Reset Grbm to its default state - broadcast
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX__CI__VI, reset_grbm_);

  // Reset the counter list
  regCP_PERFMON_CNTL cp_perfmon_cntl = {0};
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL__CI__VI, cp_perfmon_cntl.u32All);

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
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX__CI__VI, reset_grbm_);

  // Program Compute_Perfcount_Enable register to support perf counting
  regCOMPUTE_PERFCOUNT_ENABLE__CI__VI cp_perfcount_enable;
  cp_perfcount_enable.u32All = 0;
  cp_perfcount_enable.bits.PERFCOUNT_ENABLE = 1;
  cmdWriter->BuildWriteShRegPacket(cmdBuff, mmCOMPUTE_PERFCOUNT_ENABLE__CI__VI,
                                   cp_perfcount_enable.u32All);

  // Reset the counter list
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL__CI__VI, cp_perfmon_cntl.u32All);

  // Start the counter list
  cp_perfmon_cntl.bits.PERFMON_STATE = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL__CI__VI, cp_perfmon_cntl.u32All);

  // Issue barrier command to apply the commands to configure perfcounters
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);
}

uint32_t Gfx8PerfCounter::end(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter,
                              const CountersMap& countersMap, void* dataBuff) {
  // Issue barrier command to wait for dispatch to complete
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);

  // Build PM4 packet to stop and freeze counters
  regCP_PERFMON_CNTL cp_perfmon_cntl;
  cp_perfmon_cntl.u32All = 0;
  cp_perfmon_cntl.bits.PERFMON_STATE = 2;
  cp_perfmon_cntl.bits.PERFMON_SAMPLE_ENABLE = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL__CI__VI, cp_perfmon_cntl.u32All);

  // Reset Grbm to its default state - broadcast
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX__CI__VI, reset_grbm_);

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
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX__CI__VI, reset_grbm_);

  return total_counter_num * sizeof(uint32_t);
}

uint32_t Gfx8PerfCounter::ProgramTcpCntrs(uint32_t tcpRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                          uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaViCounterBlockIdTcp0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX__CI__VI;
  regIdx++;

  regTCP_PERFCOUNTER0_SELECT__CI__VI tcp_perf_counter_select;
  tcp_perf_counter_select.u32All = 0;
  tcp_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

  regVal[regIdx] = tcp_perf_counter_select.u32All;
  regAddr[regIdx] = ViTcpCounterRegAddr[tcpRegIdx].counterSelRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx8PerfCounter::ProgramTdCntrs(uint32_t tdRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                         uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaViCounterBlockIdTd0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX__CI__VI;
  regIdx++;

  regTD_PERFCOUNTER0_SELECT td_perf_counter_select;
  td_perf_counter_select.u32All = 0;
  td_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
  regVal[regIdx] = td_perf_counter_select.u32All;
  regAddr[regIdx] = ViTdCounterRegAddr[tdRegIdx].counterSelRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx8PerfCounter::ProgramTccCntrs(uint32_t tccRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                          uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaViCounterBlockIdTcc0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX__CI__VI;
  regIdx++;

  regTCC_PERFCOUNTER0_SELECT__CI__VI tcc_perf_counter_select;
  tcc_perf_counter_select.u32All = 0;
  tcc_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

  regVal[regIdx] = tcc_perf_counter_select.u32All;
  regAddr[regIdx] = ViTccCounterRegAddr[tccRegIdx].counterSelRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx8PerfCounter::ProgramTcaCntrs(uint32_t tcaRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                          uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaViCounterBlockIdTca0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX__CI__VI;
  regIdx++;

  regTCA_PERFCOUNTER0_SELECT__CI__VI tca_perf_counter_select;
  tca_perf_counter_select.u32All = 0;
  tca_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

  regVal[regIdx] = tca_perf_counter_select.u32All;
  regAddr[regIdx] = ViTcaCounterRegAddr[tcaRegIdx].counterSelRegAddr;
  regIdx++;
  return regIdx;
}

uint32_t Gfx8PerfCounter::ProgramTaCntrs(uint32_t taRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                         uint32_t blkId, uint32_t blkCntrIdx) {
  regGRBM_GFX_INDEX grbm_gfx_index;

  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_INDEX = blkId - kHsaViCounterBlockIdTa0;

  uint32_t regIdx = 0;
  regVal[regIdx] = grbm_gfx_index.u32All;
  regAddr[regIdx] = mmGRBM_GFX_INDEX__CI__VI;
  regIdx++;

  regTA_PERFCOUNTER0_SELECT ta_perf_counter_select;
  ta_perf_counter_select.u32All = 0;
  ta_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

  regVal[regIdx] = ta_perf_counter_select.u32All;
  regAddr[regIdx] = ViTaCounterRegAddr[taRegIdx].counterSelRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx8PerfCounter::ProgramSQCntrs(uint32_t sqRegIdx, uint32_t* regAddr, uint32_t* regVal,
                                         uint32_t blkId, uint32_t blkCntrIdx) {
  uint32_t regIdx = 0;

  // Program the SQ Counter Select Register
  regSQ_PERFCOUNTER0_SELECT__CI__VI sq_cntr_sel;
  sq_cntr_sel.u32All = 0;
  sq_cntr_sel.bits.SIMD_MASK = 0xF;
  sq_cntr_sel.bits.SQC_BANK_MASK = 0xF;
  sq_cntr_sel.bits.SQC_CLIENT_MASK = 0xF;
  sq_cntr_sel.bits.PERF_SEL = blkCntrIdx;
  regVal[regIdx] = sq_cntr_sel.u32All;
  regAddr[regIdx] = ViSqCounterRegAddr[sqRegIdx].counterSelRegAddr;
  regIdx++;

  // Program the SQ Counter Mask Register
  regSQ_PERFCOUNTER_MASK__CI__VI sq_cntr_mask;
  sq_cntr_mask.u32All = 0;
  sq_cntr_mask.bits.SH0_MASK = 0xFFFF;
  sq_cntr_mask.bits.SH1_MASK = 0xFFFF;
  regVal[regIdx] = sq_cntr_mask.u32All;
  regAddr[regIdx] = mmSQ_PERFCOUNTER_MASK__CI__VI;
  regIdx++;

  // Initialize the register content
  // Program the SQ Counter Control Register
  regSQ_PERFCOUNTER_CTRL sq_cntr_ctrl;
  sq_cntr_ctrl.u32All = 0;
  if (blkId == kHsaViCounterBlockIdSq) {
    sq_cntr_ctrl.bits.ES_EN = 0x1;
    sq_cntr_ctrl.bits.GS_EN = 0x1;
    sq_cntr_ctrl.bits.VS_EN = 0x1;
    sq_cntr_ctrl.bits.PS_EN = 0x1;
    sq_cntr_ctrl.bits.LS_EN = 0x1;
    sq_cntr_ctrl.bits.HS_EN = 0x1;
    sq_cntr_ctrl.bits.CS_EN = 0x1;
  } else if (blkId == kHsaViCounterBlockIdSqEs) {
    sq_cntr_ctrl.bits.ES_EN = 0x1;
  } else if (blkId == kHsaViCounterBlockIdSqGs) {
    sq_cntr_ctrl.bits.GS_EN = 0x1;
  } else if (blkId == kHsaViCounterBlockIdSqVs) {
    sq_cntr_ctrl.bits.VS_EN = 0x1;
  } else if (blkId == kHsaViCounterBlockIdSqPs) {
    sq_cntr_ctrl.bits.PS_EN = 0x1;
  } else if (blkId == kHsaViCounterBlockIdSqLs) {
    sq_cntr_ctrl.bits.LS_EN = 0x1;
  } else if (blkId == kHsaViCounterBlockIdSqHs) {
    sq_cntr_ctrl.bits.HS_EN = 0x1;
  } else if (blkId == kHsaViCounterBlockIdSqCs) {
    sq_cntr_ctrl.bits.CS_EN = 0x1;
  }

  regVal[regIdx] = sq_cntr_ctrl.u32All;
  regAddr[regIdx] = ViSqCounterRegAddr[sqRegIdx].counterCntlRegAddr;
  regIdx++;

  return regIdx;
}

uint32_t Gfx8PerfCounter::BuildCounterSelRegister(uint32_t cntrIdx, uint32_t* regAddr,
                                                  uint32_t* regVal, uint32_t blkId,
                                                  uint32_t blkCntrIdx) {
  uint32_t instance_index = 0;
  regGRBM_GFX_INDEX grbm_gfx_index = {0};
  uint32_t regIdx = 0;

  switch (blkId) {
    // Program counters belonging to SQ block
    case kHsaViCounterBlockIdSq:
    case kHsaViCounterBlockIdSqEs:
    case kHsaViCounterBlockIdSqGs:
    case kHsaViCounterBlockIdSqVs:
    case kHsaViCounterBlockIdSqPs:
    case kHsaViCounterBlockIdSqLs:
    case kHsaViCounterBlockIdSqHs:
    case kHsaViCounterBlockIdSqCs:
      return ProgramSQCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaViCounterBlockIdCb0:
    case kHsaViCounterBlockIdCb1:
    case kHsaViCounterBlockIdCb2:
    case kHsaViCounterBlockIdCb3: {
      regIdx = 0;
      instance_index = blkId - kHsaViCounterBlockIdCb0;
      grbm_gfx_index.u32All = 0;
      grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
      grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
      grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

      regVal[regIdx] = grbm_gfx_index.u32All;
      regAddr[regIdx] = mmGRBM_GFX_INDEX__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER0_LO__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER0_HI__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER1_LO__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER1_HI__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER2_LO__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER2_HI__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER3_LO__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmCB_PERFCOUNTER3_HI__CI__VI;
      regIdx++;

      regCB_PERFCOUNTER0_SELECT__CI__VI cb_perf_counter_select;
      cb_perf_counter_select.u32All = 0;
      cb_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

      regVal[regIdx] = cb_perf_counter_select.u32All;
      regAddr[regIdx] = ViCbCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx++;

      break;
    }

    case kHsaViCounterBlockIdCpf: {
      regCPF_PERFCOUNTER0_SELECT__CI__VI cpf_perf_counter_select;
      cpf_perf_counter_select.u32All = 0;
      cpf_perf_counter_select.bits.PERF_SEL = blkCntrIdx;

      regVal[0] = cpf_perf_counter_select.u32All;
      regAddr[0] = ViCpfCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdDb0:
    case kHsaViCounterBlockIdDb1:
    case kHsaViCounterBlockIdDb2:
    case kHsaViCounterBlockIdDb3: {
      instance_index = blkId - kHsaViCounterBlockIdDb0;
      regIdx = 0;
      grbm_gfx_index.u32All = 0;
      grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
      grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
      grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

      regVal[regIdx] = grbm_gfx_index.u32All;
      regAddr[regIdx] = mmGRBM_GFX_INDEX__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER0_LO__CI__VI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER0_HI__CI__VI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER1_LO__CI__VI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER1_HI__CI__VI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER2_LO__CI__VI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER2_HI__CI__VI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER3_LO__CI__VI;
      regIdx++;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmDB_PERFCOUNTER3_HI__CI__VI;
      regIdx++;

      regDB_PERFCOUNTER0_SELECT db_perf_counter_select;
      db_perf_counter_select.u32All = 0;
      db_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[regIdx] = db_perf_counter_select.u32All;
      regAddr[regIdx] = ViDbCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx++;
      break;
    }

    case kHsaViCounterBlockIdGrbm: {
      regGRBM_PERFCOUNTER0_SELECT grbm_perf_counter_select;
      grbm_perf_counter_select.u32All = 0;
      grbm_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = grbm_perf_counter_select.u32All;
      regAddr[0] = ViGrbmCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdGrbmSe: {
      regGRBM_SE0_PERFCOUNTER_SELECT grbm_se0_perf_counter_select;
      grbm_se0_perf_counter_select.u32All = 0;
      grbm_se0_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = grbm_se0_perf_counter_select.u32All;
      regAddr[0] = ViGrbmSeCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdPaSu: {
      regPA_SU_PERFCOUNTER0_SELECT pa_su_perf_counter_select;
      pa_su_perf_counter_select.u32All = 0;
      pa_su_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = pa_su_perf_counter_select.u32All;
      regAddr[0] = ViPaSuCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdPaSc: {
      regPA_SC_PERFCOUNTER0_SELECT pa_sc_perf_counter_select;
      pa_sc_perf_counter_select.u32All = 0;
      pa_sc_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = pa_sc_perf_counter_select.u32All;
      regAddr[0] = ViPaScCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdSpi: {
      regSPI_PERFCOUNTER0_SELECT spi_perf_counter_select;
      spi_perf_counter_select.u32All = 0;
      spi_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = spi_perf_counter_select.u32All;
      regAddr[0] = ViSpiCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdSx: {
      regIdx = 0;
      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER0_LO__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER0_HI__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER1_LO__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER1_HI__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER2_LO__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER2_HI__CI__VI;
      regIdx++;

      regVal[regIdx] = 0;
      regAddr[regIdx] = mmSX_PERFCOUNTER3_LO__CI__VI;
      regIdx++;

      regSX_PERFCOUNTER0_SELECT sx_perf_counter_select;
      sx_perf_counter_select.u32All = 0;
      sx_perf_counter_select.bits.PERFCOUNTER_SELECT = blkCntrIdx;
      regVal[regIdx] = sx_perf_counter_select.u32All;
      regAddr[regIdx] = ViSxCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx++;
      break;
    }

    case kHsaViCounterBlockIdTa0:
    case kHsaViCounterBlockIdTa1:
    case kHsaViCounterBlockIdTa2:
    case kHsaViCounterBlockIdTa3:
    case kHsaViCounterBlockIdTa4:
    case kHsaViCounterBlockIdTa5:
    case kHsaViCounterBlockIdTa6:
    case kHsaViCounterBlockIdTa7:
    case kHsaViCounterBlockIdTa8:
    case kHsaViCounterBlockIdTa9:
    case kHsaViCounterBlockIdTa10:
    case kHsaViCounterBlockIdTa11:
    case kHsaViCounterBlockIdTa12:
    case kHsaViCounterBlockIdTa13:
    case kHsaViCounterBlockIdTa14:
    case kHsaViCounterBlockIdTa15:
      return ProgramTaCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaViCounterBlockIdTca0:
    case kHsaViCounterBlockIdTca1:
      return ProgramTcaCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaViCounterBlockIdTcc0:
    case kHsaViCounterBlockIdTcc1:
    case kHsaViCounterBlockIdTcc2:
    case kHsaViCounterBlockIdTcc3:
    case kHsaViCounterBlockIdTcc4:
    case kHsaViCounterBlockIdTcc5:
    case kHsaViCounterBlockIdTcc6:
    case kHsaViCounterBlockIdTcc7:
    case kHsaViCounterBlockIdTcc8:
    case kHsaViCounterBlockIdTcc9:
    case kHsaViCounterBlockIdTcc10:
    case kHsaViCounterBlockIdTcc11:
    case kHsaViCounterBlockIdTcc12:
    case kHsaViCounterBlockIdTcc13:
    case kHsaViCounterBlockIdTcc14:
    case kHsaViCounterBlockIdTcc15:
      return ProgramTccCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaViCounterBlockIdTd0:
    case kHsaViCounterBlockIdTd1:
    case kHsaViCounterBlockIdTd2:
    case kHsaViCounterBlockIdTd3:
    case kHsaViCounterBlockIdTd4:
    case kHsaViCounterBlockIdTd5:
    case kHsaViCounterBlockIdTd6:
    case kHsaViCounterBlockIdTd7:
    case kHsaViCounterBlockIdTd8:
    case kHsaViCounterBlockIdTd9:
    case kHsaViCounterBlockIdTd10:
    case kHsaViCounterBlockIdTd11:
    case kHsaViCounterBlockIdTd12:
    case kHsaViCounterBlockIdTd13:
    case kHsaViCounterBlockIdTd14:
    case kHsaViCounterBlockIdTd15:
      return ProgramTdCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaViCounterBlockIdTcp0:
    case kHsaViCounterBlockIdTcp1:
    case kHsaViCounterBlockIdTcp2:
    case kHsaViCounterBlockIdTcp3:
    case kHsaViCounterBlockIdTcp4:
    case kHsaViCounterBlockIdTcp5:
    case kHsaViCounterBlockIdTcp6:
    case kHsaViCounterBlockIdTcp7:
    case kHsaViCounterBlockIdTcp8:
    case kHsaViCounterBlockIdTcp9:
    case kHsaViCounterBlockIdTcp10:
    case kHsaViCounterBlockIdTcp11:
    case kHsaViCounterBlockIdTcp12:
    case kHsaViCounterBlockIdTcp13:
    case kHsaViCounterBlockIdTcp14:
    case kHsaViCounterBlockIdTcp15:
      return ProgramTcpCntrs(cntrIdx, regAddr, regVal, blkId, blkCntrIdx);

    case kHsaViCounterBlockIdGds: {
      regGDS_PERFCOUNTER0_SELECT gds_perf_counter_select;
      gds_perf_counter_select.u32All = 0;
      gds_perf_counter_select.bits.PERFCOUNTER_SELECT = blkCntrIdx;
      regVal[0] = gds_perf_counter_select.u32All;
      regAddr[0] = ViGdsCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdVgt: {
      regVGT_PERFCOUNTER0_SELECT__CI__VI vgt_perf_counter_select;
      vgt_perf_counter_select.u32All = 0;
      vgt_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = vgt_perf_counter_select.u32All;
      regAddr[0] = ViVgtCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdIa: {
      regIA_PERFCOUNTER0_SELECT__CI__VI ia_perf_counter_select;
      ia_perf_counter_select.u32All = 0;
      ia_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = ia_perf_counter_select.u32All;
      regAddr[0] = ViIaCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    /*
        case kHsaViCounterBlockIdMc: {
          // To be investigated later
          //regMC_SEQ_PERF_SEQ_CTL mc_perfcounter_select;
          //mc_perfcounter_select.u32All = 0;
          //mc_perfcounter_select.bits.PERF_SEL = blkCntrIdx;
          //regVal[0] = mc_perfcounter_select.u32All;
          //regAddr[0] = ViMcCounterRegAddr[cntrIdx].counterSelRegAddr;
          //regIdx = 1;
        }
        break;
    */

    case kHsaViCounterBlockIdSrbm: {
      regSRBM_PERFCOUNTER0_SELECT srbm_perf_counter_select;
      srbm_perf_counter_select.u32All = 0;
      srbm_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = srbm_perf_counter_select.u32All;
      regAddr[0] = ViSrbmCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    /*
        case kHsaViCounterBlockIdTcs: {
          regTCS_PERFCOUNTER0_SELECT__CI tcs_perf_counter_select;
          tcs_perf_counter_select.u32All = 0;
          tcs_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
          regVal[0] = tcs_perf_counter_select.u32All;
          regAddr[0] = ViTcsCounterRegAddr[cntrIdx].counterSelRegAddr;
          regIdx = 1;
          break;
        }
    */

    case kHsaViCounterBlockIdWd: {
      regWD_PERFCOUNTER0_SELECT__CI__VI wd_perf_counter_select;
      wd_perf_counter_select.u32All = 0;
      wd_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = wd_perf_counter_select.u32All;
      regAddr[0] = ViWdCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdCpg: {
      regCPG_PERFCOUNTER0_SELECT__CI__VI cpg_perf_counter_select;
      cpg_perf_counter_select.u32All = 0;
      cpg_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = cpg_perf_counter_select.u32All;
      regAddr[0] = ViCpgCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    case kHsaViCounterBlockIdCpc: {
      regCPC_PERFCOUNTER0_SELECT__CI__VI cpc_perf_counter_select;
      cpc_perf_counter_select.u32All = 0;
      cpc_perf_counter_select.bits.PERF_SEL = blkCntrIdx;
      regVal[0] = cpc_perf_counter_select.u32All;
      regAddr[0] = ViCpcCounterRegAddr[cntrIdx].counterSelRegAddr;
      regIdx = 1;
      break;
    }

    /*
    case kHsaViCounterBlockIdMc: {
      AddPriviledgedCountersToList(ViBlockIdMc, blkCntrIdx);
      //Num of regs equals to 0 means it is processed by KFD
      regIdx = 0;
      break;
    }

    case kHsaViCounterBlockIdIommuV2: {
      AddPriviledgedCountersToList(ViBlockIdIommuV2, blkCntrIdx);
      //Num of regs equals to 0 means it is processed by KFD
      regIdx = 0;
      break;
    }

    case kHsaViCounterBlockIdKernelDriver: {
      AddPriviledgedCountersToList(ViBlockIdKernelDriver, blkCntrIdx);
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

uint32_t Gfx8PerfCounter::BuildCounterReadRegisters(uint32_t reg_index, uint32_t block_id,
                                                    uint32_t* reg_addr, uint32_t* reg_val) {
  uint32_t ii;
  uint32_t reg_num = 0;
  uint32_t instance_index;
  regGRBM_GFX_INDEX grbm_gfx_index;
  switch (block_id) {
    case kHsaViCounterBlockIdSq:
    case kHsaViCounterBlockIdSqEs:
    case kHsaViCounterBlockIdSqGs:
    case kHsaViCounterBlockIdSqVs:
    case kHsaViCounterBlockIdSqPs:
    case kHsaViCounterBlockIdSqLs:
    case kHsaViCounterBlockIdSqHs:
    case kHsaViCounterBlockIdSqCs: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViSqCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViSqCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdCb0:
    case kHsaViCounterBlockIdCb1:
    case kHsaViCounterBlockIdCb2:
    case kHsaViCounterBlockIdCb3: {
      instance_index = block_id - kHsaViCounterBlockIdCb0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViCbCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViCbCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdCpf: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = ViCpfCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViCpfCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaViCounterBlockIdDb0:
    case kHsaViCounterBlockIdDb1:
    case kHsaViCounterBlockIdDb2:
    case kHsaViCounterBlockIdDb3: {
      instance_index = block_id - kHsaViCounterBlockIdDb0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViDbCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViDbCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdGrbm: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = ViGrbmCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViGrbmCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaViCounterBlockIdGrbmSe: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = ViGrbmSeCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViGrbmSeCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaViCounterBlockIdPaSu: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViPaSuCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViPaSuCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdPaSc: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViPaScCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViPaScCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdSpi: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViSpiCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViSpiCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdSx: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViSxCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViSxCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdTa0:
    case kHsaViCounterBlockIdTa1:
    case kHsaViCounterBlockIdTa2:
    case kHsaViCounterBlockIdTa3:
    case kHsaViCounterBlockIdTa4:
    case kHsaViCounterBlockIdTa5:
    case kHsaViCounterBlockIdTa6:
    case kHsaViCounterBlockIdTa7:
    case kHsaViCounterBlockIdTa8:
    case kHsaViCounterBlockIdTa9:
    case kHsaViCounterBlockIdTa10:
    case kHsaViCounterBlockIdTa11:
    case kHsaViCounterBlockIdTa12:
    case kHsaViCounterBlockIdTa13:
    case kHsaViCounterBlockIdTa14:
    case kHsaViCounterBlockIdTa15: {
      instance_index = block_id - kHsaViCounterBlockIdTa0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViTaCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViTaCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdTca0:
    case kHsaViCounterBlockIdTca1: {
      instance_index = block_id - kHsaViCounterBlockIdTca0;
      grbm_gfx_index.u32All = 0;
      grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
      grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
      grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = grbm_gfx_index.u32All;
      reg_num++;

      reg_addr[reg_num] = ViTcaCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViTcaCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaViCounterBlockIdTcc0:
    case kHsaViCounterBlockIdTcc1:
    case kHsaViCounterBlockIdTcc2:
    case kHsaViCounterBlockIdTcc3:
    case kHsaViCounterBlockIdTcc4:
    case kHsaViCounterBlockIdTcc5:
    case kHsaViCounterBlockIdTcc6:
    case kHsaViCounterBlockIdTcc7:
    case kHsaViCounterBlockIdTcc8:
    case kHsaViCounterBlockIdTcc9:
    case kHsaViCounterBlockIdTcc10:
    case kHsaViCounterBlockIdTcc11:
    case kHsaViCounterBlockIdTcc12:
    case kHsaViCounterBlockIdTcc13:
    case kHsaViCounterBlockIdTcc14:
    case kHsaViCounterBlockIdTcc15: {
      instance_index = block_id - kHsaViCounterBlockIdTcc0;
      grbm_gfx_index.u32All = 0;
      grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
      grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
      grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = grbm_gfx_index.u32All;
      reg_num++;

      reg_addr[reg_num] = ViTccCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViTccCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaViCounterBlockIdTd0:
    case kHsaViCounterBlockIdTd1:
    case kHsaViCounterBlockIdTd2:
    case kHsaViCounterBlockIdTd3:
    case kHsaViCounterBlockIdTd4:
    case kHsaViCounterBlockIdTd5:
    case kHsaViCounterBlockIdTd6:
    case kHsaViCounterBlockIdTd7:
    case kHsaViCounterBlockIdTd8:
    case kHsaViCounterBlockIdTd9:
    case kHsaViCounterBlockIdTd10:
    case kHsaViCounterBlockIdTd11:
    case kHsaViCounterBlockIdTd12:
    case kHsaViCounterBlockIdTd13:
    case kHsaViCounterBlockIdTd14:
    case kHsaViCounterBlockIdTd15: {
      instance_index = block_id - kHsaViCounterBlockIdTd0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViTdCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViTdCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdTcp0:
    case kHsaViCounterBlockIdTcp1:
    case kHsaViCounterBlockIdTcp2:
    case kHsaViCounterBlockIdTcp3:
    case kHsaViCounterBlockIdTcp4:
    case kHsaViCounterBlockIdTcp5:
    case kHsaViCounterBlockIdTcp6:
    case kHsaViCounterBlockIdTcp7:
    case kHsaViCounterBlockIdTcp8:
    case kHsaViCounterBlockIdTcp9:
    case kHsaViCounterBlockIdTcp10:
    case kHsaViCounterBlockIdTcp11:
    case kHsaViCounterBlockIdTcp12:
    case kHsaViCounterBlockIdTcp13:
    case kHsaViCounterBlockIdTcp14:
    case kHsaViCounterBlockIdTcp15: {
      instance_index = block_id - kHsaViCounterBlockIdTcp0;
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_INDEX = instance_index;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViTcpCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViTcpCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdGds: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = ViGdsCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViGdsCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaViCounterBlockIdVgt: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViVgtCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViVgtCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }

    case kHsaViCounterBlockIdIa: {
      for (ii = 0; ii < num_se_; ii++) {
        grbm_gfx_index.u32All = 0;
        grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
        grbm_gfx_index.bitfields.SE_INDEX = ii;
        grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;

        reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
        reg_val[reg_num] = grbm_gfx_index.u32All;
        reg_num++;

        reg_addr[reg_num] = ViIaCounterRegAddr[reg_index].counterReadRegAddrLo;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;

        reg_addr[reg_num] = ViIaCounterRegAddr[reg_index].counterReadRegAddrHi;
        reg_val[reg_num] = COPY_DATA_FLAG;
        reg_num++;
      }
      break;
    }
    /*
        case kHsaViCounterBlockIdMc: {
          reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
          reg_val[reg_num] = reset_grbm_;
          reg_num++;

          reg_addr[reg_num] = ViMcCounterRegAddr[reg_index].counterReadRegAddrLo;
          reg_val[reg_num] = COPY_DATA_FLAG;
          reg_num++;

          reg_addr[reg_num] = ViMcCounterRegAddr[reg_index].counterReadRegAddrHi;
          reg_val[reg_num] = COPY_DATA_FLAG;
          reg_num++;
          break;
        }
    */
    case kHsaViCounterBlockIdSrbm: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = ViSrbmCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViSrbmCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }
    /*
        case kHsaViCounterBlockIdTcs: {
          reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
          reg_val[reg_num] = reset_grbm_;
          reg_num++;

          reg_addr[reg_num] = ViTcsCounterRegAddr[reg_index].counterReadRegAddrLo;
          reg_val[reg_num] = COPY_DATA_FLAG;
          reg_num++;

          reg_addr[reg_num] = ViTcsCounterRegAddr[reg_index].counterReadRegAddrHi;
          reg_val[reg_num] = COPY_DATA_FLAG;
          reg_num++;
          break;
        }
    */
    case kHsaViCounterBlockIdWd: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = ViWdCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViWdCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaViCounterBlockIdCpg: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = ViCpgCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViCpgCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    case kHsaViCounterBlockIdCpc: {
      reg_addr[reg_num] = mmGRBM_GFX_INDEX__CI__VI;
      reg_val[reg_num] = reset_grbm_;
      reg_num++;

      reg_addr[reg_num] = ViCpcCounterRegAddr[reg_index].counterReadRegAddrLo;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;

      reg_addr[reg_num] = ViCpcCounterRegAddr[reg_index].counterReadRegAddrHi;
      reg_val[reg_num] = COPY_DATA_FLAG;
      reg_num++;
      break;
    }

    // IommuV2, MC, kernel driver counters are retrieved via
    // KFD implementation
    case kHsaViCounterBlockIdMc:
    case kHsaViCounterBlockIdIommuV2:
    case kHsaViCounterBlockIdKernelDriver: {
      reg_num = 0;
      break;
    }

    default: { break; }
  }

  return reg_num;
}

} /* namespace */
