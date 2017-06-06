#include <string.h>
#include <iomanip>

#include "os.h"

#include "gfxip/gfx8/si_ci_vi_merged_typedef.h"
#include "gfxip/gfx8/si_ci_vi_merged_offset.h"
#include "gfxip/gfx8/si_ci_vi_merged_enum.h"
#include "gfxip/gfx8/si_pm4defs.h"
#include "cmdwriter.h"

#include "vi_pmu.h"
#include "gpu_countergroup.h"
#include "vi_blockinfo.h"
#include "gpu_enum.h"

using namespace std;
using namespace pm4_profile;

namespace pm4_profile {

static char errorString[][64] = {{"No error"},
                                 {"unknow countergroup id"},
                                 {"no countergroup id"},
                                 {"invalid operation"},
                                 {"counter is not available"},
                                 {"countegroup error state"},
                                 {"countegroup is not completed"}};

ViPmu::ViPmu() {
  // Initialize the number of shader engines
  num_se_ = 4;
  Init();
}

void ViPmu::Init() {
  error_code_ = 0;
  info_set_ = new InfoSet();
  parameter_set_ = new ParameterSet();

  // Initialize pointer to stored counter block list to NULL
  blk_list_ = NULL;
  initCounterBlock();

  // Initialize the value to use in resetting GRBM
  regGRBM_GFX_INDEX grbm_gfx_index;
  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  reset_grbm_ = grbm_gfx_index.u32All;

  // Update state of Perf Mgmt Unit
  profiler_state_ = ROCR_PMU_STATE_IDLE;
}

ViPmu::~ViPmu() {
  // Remove all counter blocks
  RemoveCounterBlocks();
  blk_map_.clear();
  delete parameter_set_;
  delete info_set_;

  if (blk_list_) {
    free(blk_list_);
    blk_list_ = NULL;
  }
}

// Initializes the handle of buffer used to collect PMC data
// @param cmdBufSz Size in terms of bytes
bool ViPmu::setPmcDataBuff(uint8_t* pmcBuffer, uint32_t pmcBuffSz) {
  // Update counter data buffer addr and size params
  pmcDataSz_ = pmcBuffSz;
  pmcData_ = (uint32_t*)pmcBuffer;
  return true;
}

//
// The logic is quite simple and is as follows
//
//    Issue CsPartialFlush
//    Issue Cmd to stop Perf Counters
//    Issue Cmd to Disable & Reset Perf Counters
//
void ViPmu::ResetCounterBlocks(pm4_profile::DefaultCmdBuf* cmdBuff,
                               pm4_profile::CommandWriter* cmdWriter) {
  // Waits until all outstanding commands have completed
  // by issing CS Partial Flush command
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);

  // Program CP Perfmon Cntrl Rgstr to disable and reset counters
  regCP_PERFMON_CNTL cp_perfmon_cntl;
  cp_perfmon_cntl.u32All = 0;
  cp_perfmon_cntl.bits.PERFMON_STATE = 0;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL__CI__VI, cp_perfmon_cntl.u32All);
}

bool ViPmu::begin(pm4_profile::DefaultCmdBuf* cmdBuff, pm4_profile::CommandWriter* cmdWriter,
                  bool reset_counter) {
  if (profiler_state_ != ROCR_PMU_STATE_IDLE) {
    error_code_ = kHsaPmuErrorCodeErrorState;
    return false;
  }

  // Reset Grbm to its default state - broadcast
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX__CI__VI, reset_grbm_);

  // Program CP Perfmon Cntrl Rgstr to disable and reset counters
  regCP_PERFMON_CNTL cp_perfmon_cntl;
  cp_perfmon_cntl.u32All = 0;
  cp_perfmon_cntl.bits.PERFMON_STATE = 0;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL__CI__VI, cp_perfmon_cntl.u32All);

  // Collect all the program counter blocks
  uint32_t reg_val[MAX_REG_NUM], reg_addr[MAX_REG_NUM], reg_num;

  // Retrieve the list of blocks whose perf counters have been enabled
  uint32_t blk_cnt = 0;
  CounterBlock** blk_list = getAllCounterBlocks(blk_cnt);

  // Iterate through the list of blocks to generate Pm4 commands to
  // program corresponding perf counters of each block
  for (uint32_t blkIdx = 0; blkIdx < blk_cnt; blkIdx++) {
    // Retrieve the list of perf counters and their count
    uint32_t counter_num;
    Counter** cntr_list;
    cntr_list = blk_list[blkIdx]->getEnabledCounters(counter_num);
    if (counter_num == 0) {
      continue;
    }

    // Retrieve the  block Id of perf counters
    void* p_data;
    uint32_t block_id;
    uint32_t data_size;
    blk_list[blkIdx]->getInfo(GPU_BLK_INFO_ID, data_size, (void**)&p_data);
    block_id = *(static_cast<uint32_t*>(p_data));

    // Iterate through each enabled perf counter and building
    // corresponding Pm4 commands to program the various control
    // registers involved
    for (uint32_t cntrIdx = 0; cntrIdx < counter_num; cntrIdx++) {
      // Build the list of control registers to program which
      // varies per perf counter block
      reg_num = BuildCounterSelRegister(cntrIdx, reg_addr, reg_val, block_id, cntr_list[cntrIdx]);

      // Build the list of Pm4 commands that support control
      // register programming
      for (uint32_t regIdx = 0; regIdx < reg_num; regIdx++) {
        cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, reg_addr[regIdx], reg_val[regIdx]);
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

  // Start the counter list
  cp_perfmon_cntl.u32All = 0;
  cp_perfmon_cntl.bits.PERFMON_STATE = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL__CI__VI, cp_perfmon_cntl.u32All);

  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);

  profiler_state_ = ROCR_PMU_STATE_START;
  return true;
}

bool ViPmu::end(pm4_profile::DefaultCmdBuf* cmdBuff, pm4_profile::CommandWriter* cmdWriter) {
  if (profiler_state_ != ROCR_PMU_STATE_START) {
    error_code_ = kHsaPmuErrorCodeErrorState;
    return false;
  }

  void* p_data;
  regGRBM_GFX_INDEX grbm_gfx_index;

  // Issue CsPartialFlush command to wait for dispatch to complete
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);

  // Build PM4 packet for starting counters
  regCP_PERFMON_CNTL cp_perfmon_cntl;
  cp_perfmon_cntl.u32All = 0;
  cp_perfmon_cntl.bits.PERFMON_STATE = 2;
  cp_perfmon_cntl.bits.PERFMON_SAMPLE_ENABLE = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmCP_PERFMON_CNTL__CI__VI, cp_perfmon_cntl.u32All);

  // Collect all the program counter blocks
  uint32_t i, j, k, reg_addr[MAX_REG_NUM], reg_val[MAX_REG_NUM], reg_num, data_size;

  uint32_t blk_cnt = 0;
  CounterBlock** blk_list = getAllCounterBlocks(blk_cnt);

  uint32_t counter_num;
  Counter** cntr_list;
  uint32_t total_counter_num = 0;
  for (i = 0; i < blk_cnt; i++) {
    // Retrieve all enabled cntr_list in each counter block
    cntr_list = blk_list[i]->getEnabledCounters(counter_num);
    if (!blk_list[i]->getInfo(GPU_BLK_INFO_CONTROL_METHOD, data_size, &p_data)) {
      return false;
    }

    CntlMethod method;
    method = static_cast<CntlMethod>(*(static_cast<uint32_t*>(p_data)));

    // Need to read counter values from each shader engine
    if (method == CntlMethodBySe || method == CntlMethodBySeAndInstance) {
      counter_num = counter_num * num_se_;
    }

    total_counter_num += counter_num;
  }

  size_t cntrSize = sizeof(int32_t) * 2 * total_counter_num;
  if (cntrSize > pmcDataSz_) {
    return false;
  }

  // Reset Grbm to its default state - broadcast
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX__CI__VI, reset_grbm_);

  // Create PM4 packet to read counter values
  total_counter_num = 0;
  for (i = 0; i < blk_cnt; i++) {
    // Retrieve all enabled cntr_list in each counter block
    cntr_list = blk_list[i]->getEnabledCounters(counter_num);
    if (counter_num > 0) {
      uint32_t block_id;
      uint32_t data_size;
      if (!blk_list[i]->getInfo(GPU_BLK_INFO_ID, data_size, (void**)&p_data)) {
        return false;
      }
      block_id = *(static_cast<uint32_t*>(p_data));

      for (j = 0; j < counter_num; j++) {
        // retrieve the registers to be set
        reg_num = BuildCounterReadRegisters(j, block_id, reg_addr, reg_val);
        for (k = 0; k < reg_num; k++) {
          if (reg_val[k] == COPY_DATA_FLAG) {
            cmdWriter->BuildCopyDataPacket(cmdBuff, COPY_DATA_SEL_REG, reg_addr[k], 0,
                                           pmcData_ + total_counter_num, COPY_DATA_SEL_COUNT_1DW,
                                           false);
            total_counter_num++;
          } else {
            cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, reg_addr[k], reg_val[k]);
          }
        }
      }
    }
  }

  // Reset Grbm to its default state - broadcast
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX__CI__VI, reset_grbm_);

  profiler_state_ = ROCR_PMU_STATE_STOP;
  return true;
}

bool ViPmu::initCounterBlock() {
  for (int i = 0; !(std::string(ViPmuHwBlocks[i].blockName).empty()); i++) {
    // Override the value of max number of shader engines
    ViPmuHwBlocks[i].maxShaderEngineCount = num_se_;

    // Instantiate a perf counter block and its properties
    GpuCounterBlock* cntr_blk = new GpuCounterBlock();
    if (!cntr_blk) {
      blk_map_.clear();
      return false;
    }

    cntr_blk->setInfo(GPU_BLK_INFO_BLOCK_NAME, GPU_BLOCK_NAME_SIZE,
                      (void*)ViPmuHwBlocks[i].blockName);

    cntr_blk->setInfo(GPU_BLK_INFO_ID, sizeof(uint32_t), (void*)&ViPmuHwBlocks[i].counterGroupId);

    cntr_blk->setInfo(GPU_BLK_INFO_MAX_SHADER_ENGINE_COUNT, sizeof(uint32_t),
                      (void*)&(ViPmuHwBlocks[i].maxShaderEngineCount));

    cntr_blk->setInfo(GPU_BLK_INFO_MAX_SHADER_ARRAY_COUNT, sizeof(uint32_t),
                      (void*)&(ViPmuHwBlocks[i].maxShaderArrayCount));

    cntr_blk->setInfo(GPU_BLK_INFO_MAX_INSTANCE_COUNT, sizeof(uint32_t),
                      (void*)&(ViPmuHwBlocks[i].maxInstanceCount));

    cntr_blk->setInfo(GPU_BLK_INFO_CONTROL_METHOD, sizeof(uint32_t),
                      (void*)&(ViPmuHwBlocks[i].method));

    cntr_blk->setInfo(GPU_BLK_INFO_MAX_EVENT_ID, sizeof(uint32_t),
                      (void*)&(ViPmuHwBlocks[i].maxEventId));

    cntr_blk->setInfo(GPU_BLK_INFO_MAX_SIMULTANEOUS_COUNTERS, sizeof(uint32_t),
                      (void*)&(ViPmuHwBlocks[i].maxSimultaneousCounters));

    cntr_blk->setInfo(GPU_BLK_INFO_MAX_STREAMING_COUNTERS, sizeof(uint32_t),
                      (void*)&(ViPmuHwBlocks[i].maxStreamingCounters));

    cntr_blk->setInfo(GPU_BLK_INFO_SHARED_HW_COUNTERS, sizeof(uint32_t),
                      (void*)&(ViPmuHwBlocks[i].sharedHWCounters));

    cntr_blk->setInfo(GPU_BLK_INFO_HAS_FILTERS, sizeof(bool),
                      (void*)&(ViPmuHwBlocks[i].hasFilters));

    // TODO: Need to fill in the Threadtrace stuff here
    HsaViCounterBlockId blk_id;
    blk_id = static_cast<HsaViCounterBlockId>(ViPmuHwBlocks[i].counterGroupId);
    blk_map_.insert(ViCounterBlockMap::value_type(blk_id, cntr_blk));
  }

  // Initiate the PMU state and error code
  error_code_ = 0;
  profiler_state_ = ROCR_PMU_STATE_IDLE;
  return true;
}

int ViPmu::getLastError() { return error_code_; }

std::string ViPmu::getErrorString(int error) {
  if ((error >= 0) && (error < kHsaPmuErrorCodeMax)) {
    std::string err_string(errorString[error]);
    return err_string;
  }
  return string("Error input code!");
}

bool ViPmu::getParameter(uint32_t param, uint32_t& retSize, void** ppData) {
  return parameter_set_->getParameter(param, retSize, ppData);
}

bool ViPmu::setParameter(uint32_t param, uint32_t paramSize, const void* p_data) {
  return parameter_set_->setParameter(param, paramSize, p_data);
}

bool ViPmu::getInfo(uint32_t info, uint32_t& retSize, void** ppData) {
  return info_set_->getInfo(info, retSize, ppData);
}

pm4_profile::CounterBlock* ViPmu::getCounterBlockById(uint32_t id) {
  HsaViCounterBlockId block_id = static_cast<HsaViCounterBlockId>(id);

  // Carrizo has only 8 instances of TA, TD, TCP Perf Blocks
  /*
  if (asic_ == HsaAmdDeviceAsicTypeCZ) {
    if ( ((id >= kHsaViCounterBlockIdTa8) && (id <= kHsaViCounterBlockIdTa15)) ||
         ((id >= kHsaViCounterBlockIdTd8) && (id <= kHsaViCounterBlockIdTd15)) ||
         ((id >= kHsaViCounterBlockIdTcp8) && (id <= kHsaViCounterBlockIdTcp15))) {
      return NULL;
    }
  }
  */

  return blk_map_[block_id];
}

pm4_profile::CounterBlock** ViPmu::getAllCounterBlocks(uint32_t& num_blocks) {
  size_t block_size = blk_map_.size();

  if (block_size <= 0) {
    error_code_ = kHsaPmuErrorCodeNoCounterBlock;
    return NULL;
  }

  if (blk_list_) {
    free(blk_list_);
    blk_list_ = NULL;
  }

  blk_list_size_ = (uint32_t)(sizeof(GpuCounterBlock*) * block_size);
  blk_list_size_ = ((blk_list_size_ % 4096) != 0) ? 4096 : blk_list_size_;
  blk_list_ = (CounterBlock**)malloc(blk_list_size_);
  if (blk_list_ == NULL) {
    return NULL;
  }

  ViCounterBlockMap::iterator it;
  uint32_t blk_cnt = 0;
  for (it = blk_map_.begin(); it != blk_map_.end(); it++) {
    blk_list_[blk_cnt] = it->second;
    blk_cnt++;
  }

  num_blocks = blk_cnt;
  return blk_list_;
}

uint32_t ViPmu::ProgramTcpCntrs(uint32_t tcpRegIdx, uint32_t* regAddr, uint32_t* regVal,
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

uint32_t ViPmu::ProgramTdCntrs(uint32_t tdRegIdx, uint32_t* regAddr, uint32_t* regVal,
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

uint32_t ViPmu::ProgramTccCntrs(uint32_t tccRegIdx, uint32_t* regAddr, uint32_t* regVal,
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

uint32_t ViPmu::ProgramTcaCntrs(uint32_t tcaRegIdx, uint32_t* regAddr, uint32_t* regVal,
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

uint32_t ViPmu::ProgramTaCntrs(uint32_t taRegIdx, uint32_t* regAddr, uint32_t* regVal,
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

uint32_t ViPmu::ProgramSQCntrs(uint32_t sqRegIdx, uint32_t* regAddr, uint32_t* regVal,
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

uint32_t ViPmu::BuildCounterSelRegister(uint32_t cntrIdx, uint32_t* regAddr, uint32_t* regVal,
                                        uint32_t blkId, pm4_profile::Counter* blkCntr) {
  void* p_data;
  uint32_t data_size;
  uint32_t blkCntrIdx;
  uint32_t instance_index;
  regGRBM_GFX_INDEX grbm_gfx_index;

  // Get the blkCntr selection value
  if (!blkCntr->getParameter(HSA_EXT_TOOLS_COUNTER_PARAMETER_EVENT_INDEX, data_size,
                             (void**)&p_data)) {
    return 0;
  }
  blkCntrIdx = *(static_cast<uint32_t*>(p_data));

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

uint32_t ViPmu::BuildCounterReadRegisters(uint32_t reg_index, uint32_t block_id, uint32_t* reg_addr,
                                          uint32_t* reg_val) {
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

hsa_status_t ViPmu::RemoveCounterBlocks() {
  ViCounterBlockMap::iterator it = blk_map_.begin();
  ViCounterBlockMap::iterator block_end = blk_map_.end();

  for (; it != block_end; it++) {
    delete it->second;
  }

  return HSA_STATUS_SUCCESS;
}


} /* namespace */
