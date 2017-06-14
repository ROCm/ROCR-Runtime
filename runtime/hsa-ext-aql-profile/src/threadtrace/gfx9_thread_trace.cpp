#include "gfx9_thread_trace.h"

/// @brief Returns the lower 32-bits of a value
inline uint32_t Low32(uint64_t u) { return (u & 0xFFFFFFFFUL); }

/// @brief Returns the upper 32-bits of a value
inline uint32_t High32(uint64_t u) { return (u >> 32); }

namespace pm4_profile {

Gfx9ThreadTrace::Gfx9ThreadTrace() {
  // Initialize the number of shader engines
  numSE_ = 4;
}

Gfx9ThreadTrace::~Gfx9ThreadTrace() {}

bool Gfx9ThreadTrace::Init(const ThreadTraceConfig* config) {
  // Initialize SQTT Configuration and Register objects
  if (!ThreadTrace::Init(config)) return false;
  InitThreadTraceCfgRegs();
  return true;
}

void Gfx9ThreadTrace::InitThreadTraceCfgRegs() {
  // Indicates the size of buffer to use per Shader Engine instance.
  // The size is specified in terms of 4KB blocks
  ttCfgRegs_.ttRegSize.u32All = 0;

  // Indicates various attributes of a thread trace session.
  //
  // MASK_CS: Which shader types should be enabled for data collection
  //      Enable CS Shader types.
  //
  // WRAP: How trace buffer should be used as a ring buffer or as a linear
  //      buffer - Disable WRAP mode i.e use it as a linear buffer
  //
  // MODE: Enables a thread trace session
  //
  // CAPTURE_MODE: When thread trace data is collected immediately after MODE
  //      is enabled or wait until a Thread Trace Start event is received
  //
  // AUTOFLUSH_EN: Flush thread trace data to buffer often automatically
  //
  ttCfgRegs_.ttRegMode.u32All = 0;
  ttCfgRegs_.ttRegMode.bits.WRAP = 0;
  ttCfgRegs_.ttRegMode.bits.CAPTURE_MODE = 0;
  ttCfgRegs_.ttRegMode.bits.MASK_CS = 1;
  ttCfgRegs_.ttRegMode.bits.AUTOFLUSH_EN = 1;
  ttCfgRegs_.ttRegMode.bits.MODE = SQ_THREAD_TRACE_MODE_OFF;

  // Enable Thread Trace for all VM Id's
  // Enable all of the SIMD's of the compute unit
  // Enable Compute Unit (CU) at index Zero to be used for fine-grained data
  // Enable Shader Array (SH) at index Zero to be used for fine-grained data
  //
  // @note: Not enabling REG_STALL_EN, SPI_STALL_EN and SQ_STALL_EN bits. They
  // are useful if we wish to program buffer throttling.
  //
  ttCfgRegs_.ttRegMask.u32All = 0;
  ttCfgRegs_.ttRegMask.bits.SH_SEL = 0x0;
  ttCfgRegs_.ttRegMask.bits.SIMD_EN = 0xF;
  ttCfgRegs_.ttRegMask.bits.CU_SEL = GetCuId();
  ttCfgRegs_.ttRegMask.bits.SQ_STALL_EN = 0x1;
  ttCfgRegs_.ttRegMask.bits.SPI_STALL_EN = 0x1;
  ttCfgRegs_.ttRegMask.bits.REG_STALL_EN = 0x1;
  ttCfgRegs_.ttRegMask.bits.VM_ID_MASK = GetVmId();

  // Override Mask value if a user value is available
  uint32_t ttMask = GetMask();
  if (ttMask) {
    ttCfgRegs_.ttRegMask.u32All = ttMask;
  }

  // Mask of compute units to get thread trace data from
  ttCfgRegs_.ttRegPerfMask.u32All = 0;
  ttCfgRegs_.ttRegPerfMask.bits.SH0_MASK = 0xFFFF;
  ttCfgRegs_.ttRegPerfMask.bits.SH1_MASK = 0xFFFF;

  // Indicate the different TT messages/tokens that should be enabled/logged
  // Indicate the different TT tokens that specify register operations to be logged
  ttCfgRegs_.ttRegTokenMask.u32All = 0;
  ttCfgRegs_.ttRegTokenMask.bits.REG_MASK = 0xFF;
  ttCfgRegs_.ttRegTokenMask.bits.TOKEN_MASK = 0xFFFF;
  ttCfgRegs_.ttRegTokenMask.bits.REG_DROP_ON_STALL = 0x1;

  // Override TokenMask1 value if a user value is available
  uint32_t tokenMask1 = GetTokenMask();
  if (tokenMask1) {
    ttCfgRegs_.ttRegTokenMask.u32All = tokenMask1;
  }

  // Indicate the different TT tokens that specify instruction operations to be logged
  // Disabling specifically instruction operations updating Program Counter (PC).
  // @note: The field is defined in the spec incorrectly as a 16-bit value
  ttCfgRegs_.ttRegTokenMask2.u32All = 0;
  ttCfgRegs_.ttRegTokenMask2.bits.INST_MASK = 0xFFFFFF7F;

  // Override TokenMask2 value if a user value is available
  uint32_t tokenMask2 = GetTokenMask2();
  if (tokenMask2) {
    ttCfgRegs_.ttRegTokenMask2.u32All = tokenMask2;
  }
}

void Gfx9ThreadTrace::setSqttDataBuff(uint8_t* sqttBuffer, uint32_t sqttBuffSz) {
  // Compute the size of buffer available for each shader engine
  ttBuffSize_ = sqttBuffSz / numSE_;

  // Populate the sqtt buffer array submitted to device
  for (int idx = 0; idx < numSE_; idx++) {
    uint64_t sqttSEAddr = uint64_t(sqttBuffer + (ttBuffSize_ * idx));
    devMemList_.push_back(sqttSEAddr);
  }

  // Update the size bit-field of sqtt ctrl register
  ttCfgRegs_.ttRegSize.bits.SIZE = ttBuffSize_ >> TT_BUFF_ALIGN_SHIFT;
}

void Gfx9ThreadTrace::BeginSession(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter) {
  // Program Grbm to broadcast messages to all shader engines
  regGRBM_GFX_INDEX grbm_gfx_index;
  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, grbm_gfx_index.u32All);

  // Issue a CSPartialFlush cmd including cache flush
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);

  // Disable RLC Perfmon Clock Gating
  // On Vega this is needed to collect Perf Cntrs
  // cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmRLC_PERFMON_CLK_CNTL, 1);

  // Program the Compute register to indicate SQTT is enabled
  /*
  regCOMPUTE_THREAD_TRACE_ENABLE enableTT = {0};
  enableTT.bits.THREAD_TRACE_ENABLE = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff,
                                        mmCOMPUTE_THREAD_TRACE_ENABLE,
                                        enableTT.u32All);
  */

  // Program the thread trace mask - specifies SH, CU, SIMD and
  // VM Id masks to apply. Enabling SQ/SPI/REG_STALL_EN bits
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_MASK,
                                        ttCfgRegs_.ttRegMask.u32All);

  // Program the thread trace Perf mask
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_PERF_MASK,
                                        ttCfgRegs_.ttRegPerfMask.u32All);

  // Program the thread trace token mask
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_TOKEN_MASK,
                                        ttCfgRegs_.ttRegTokenMask.u32All);

  // Program the thread trace token mask2 to specify the list of instruction
  // tokens to record. Disabling INST_PC instruction tokens
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_TOKEN_MASK2,
                                        ttCfgRegs_.ttRegTokenMask2.u32All);

  // Program the thread trace mode register
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_MODE,
                                        ttCfgRegs_.ttRegMode.u32All);

  // Program the HiWaterMark register to support stalling
  if ((ttCfgRegs_.ttRegMask.bits.SQ_STALL_EN) || (ttCfgRegs_.ttRegMask.bits.SPI_STALL_EN) ||
      (ttCfgRegs_.ttRegMask.bits.REG_STALL_EN) ||
      (ttCfgRegs_.ttRegTokenMask.bits.REG_DROP_ON_STALL)) {
    cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_HIWATER, 0x06);
  }

  // Iterate through the list of SE's and program the register
  // for carrying address of thread trace buffer which is aligned
  // to 4KB per thread trace specification
  uint64_t baseAddr = 0;
  for (int idx = 0; idx < numSE_; idx++) {
    // Program Grbm to direct writes to one SE
    grbm_gfx_index.bitfields.SH_INDEX = 0;
    grbm_gfx_index.bitfields.SE_INDEX = idx;
    grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 0;
    grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 0;
    cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, grbm_gfx_index.u32All);

    // Program base2 address of buffer to use for thread trace
    /*
    regSQ_THREAD_TRACE_BASE2 sqttBase2 = {};
    sqttBase2.u32All = 0;
    sqttBase2.bits.ADDR_HI = 0;
    cmdWriter->BuildWriteUConfigRegPacket(cmdBuff,
                                          mmSQ_THREAD_TRACE_BASE2,
                                          sqttBase2.u32All);
    */

    // Program the base address to use
    baseAddr = devMemList_[idx] >> TT_BUFF_ALIGN_SHIFT;

    // Program base address of buffer to use for thread trace
    regSQ_THREAD_TRACE_BASE sqttBase = {};
    sqttBase.bits.ADDR = Low32(baseAddr);
    cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_BASE, sqttBase.u32All);

    // Program the size of thread trace buffer
    cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_SIZE,
                                          ttCfgRegs_.ttRegSize.u32All);

    // Program the thread trace ctrl register
    regSQ_THREAD_TRACE_CTRL sqttCtrl = {};
    sqttCtrl.u32All = 0;
    sqttCtrl.bits.RESET_BUFFER = 1;
    cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_CTRL, sqttCtrl.u32All);
  }

  // Reset the GRBM to broadcast mode
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, grbm_gfx_index.u32All);

  // Issue a CSPartialFlush cmd including cache flush
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);

  // Program the thread trace mode register
  ttCfgRegs_.ttRegMode.bits.MODE = SQ_THREAD_TRACE_MODE_ON;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_MODE,
                                        ttCfgRegs_.ttRegMode.u32All);
  ttCfgRegs_.ttRegMode.bits.MODE = SQ_THREAD_TRACE_MODE_OFF;

  // Issue a CSPartialFlush cmd including cache flush
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);
  return;
}

void Gfx9ThreadTrace::StopSession(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter) {
  // Program Grbm to broadcast messages to all shader engines
  regGRBM_GFX_INDEX grbm_gfx_index;
  grbm_gfx_index.u32All = 0;
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.INSTANCE_BROADCAST_WRITES = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, grbm_gfx_index.u32All);

  // Issue a CSPartialFlush cmd including cache flush
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);

  // Program the thread trace mode register to disable thread trace
  // The MODE register is set to disable thread trace by default
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_MODE,
                                        ttCfgRegs_.ttRegMode.u32All);

  // Issue a CSPartialFlush cmd including cache flush
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);

  // Iterate through the list of SE's and read the Status, Counter and
  // Write Pointer registers of Thread Trace subsystem
  uint64_t baseAddr = 0;
  for (int idx = 0; idx < numSE_; idx++) {
    // Program Grbm to direct writes to one SE
    grbm_gfx_index.bitfields.SH_INDEX = 0;
    grbm_gfx_index.bitfields.SE_INDEX = idx;
    grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 0;
    grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 0;
    cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, grbm_gfx_index.u32All);

    // Issue WaitRegMem command to wait until SQTT event has completed
    bool funcEq = false;
    bool memSpace = false;
    uint32_t waitVal = 0x01;
    uint32_t maskVal = 0x40000000L;
    uint32_t statusOffset = mmSQ_THREAD_TRACE_STATUS - UCONFIG_SPACE_START;
    cmdWriter->BuildWaitRegMemCommand(cmdBuff, memSpace, statusOffset, funcEq, maskVal, waitVal);

    // Retrieve the values from various status registers
    cmdWriter->BuildCopyDataPacket(cmdBuff, COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                   mmSQ_THREAD_TRACE_STATUS, 0,
                                   ttStatus_ + ((TT_STATUS_IDX_MAX * idx) + TT_STATUS_IDX_STATUS),
                                   COPY_DATA_SEL_COUNT_1DW, true);

    cmdWriter->BuildCopyDataPacket(cmdBuff, COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                   mmSQ_THREAD_TRACE_CNTR, 0,
                                   ttStatus_ + ((TT_STATUS_IDX_MAX * idx) + TT_STATUS_IDX_CNTR),
                                   COPY_DATA_SEL_COUNT_1DW, true);

    uint32_t wptrIdx = ((TT_STATUS_IDX_MAX * idx) + TT_STATUS_IDX_WPTR);
    cmdWriter->BuildCopyDataPacket(cmdBuff, COPY_DATA_SEL_SRC_SYS_PERF_COUNTER,
                                   mmSQ_THREAD_TRACE_WPTR, 0, ttStatus_ + wptrIdx,
                                   COPY_DATA_SEL_COUNT_1DW, true);
  }

  // Reset the GRBM to broadcast mode
  grbm_gfx_index.bitfields.SH_BROADCAST_WRITES = 1;
  grbm_gfx_index.bitfields.SE_BROADCAST_WRITES = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmGRBM_GFX_INDEX, grbm_gfx_index.u32All);

  // Initialize cache flush request object
  FlushCacheOptions flush;
  flush.l1 = true;
  flush.l2 = true;
  flush.icache = true;
  flush.kcache = true;
  cmdWriter->BuildFlushCacheCmd(cmdBuff, &flush, NULL, 0);

  // Program the size of thread trace buffer
  regSQ_THREAD_TRACE_SIZE ttRegSize = {0};
  ttRegSize.u32All = 0;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_SIZE, ttRegSize.u32All);

  // Program the thread trace ctrl register
  regSQ_THREAD_TRACE_CTRL sqttCtrl = {};
  sqttCtrl.u32All = 0;
  sqttCtrl.bits.RESET_BUFFER = 1;
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmSQ_THREAD_TRACE_CTRL, sqttCtrl.u32All);

  // Program the compute_thread_trace_enable register
  /*
  regCOMPUTE_THREAD_TRACE_ENABLE disableTT = {0};
  cmdWriter->BuildWriteUConfigRegPacket(cmdBuff,
                                        mmCOMPUTE_THREAD_TRACE_ENABLE,
                                        disableTT.u32All);
  */

  // Disable RLC Perfmon Clock Gating
  // On Vega this is needed to collect Perf Cntrs
  // cmdWriter->BuildWriteUConfigRegPacket(cmdBuff, mmRLC_PERFMON_CLK_CNTL, 0);

  // Issue a CSPartialFlush cmd including cache flush
  cmdWriter->BuildWriteWaitIdlePacket(cmdBuff);
  return;
}

bool Gfx9ThreadTrace::Validate() {
  // Iterate through the list of SE to verify
  for (int idx = 0; idx < numSE_; idx++) {
    // Determine if the buffer has wrapped
    uint32_t statusIdx = ((TT_STATUS_IDX_MAX * idx) + TT_STATUS_IDX_STATUS);
    if (ttStatus_[statusIdx] & 0x80000000) {
      return false;
    }

    // Adjust the value of Write Ptr which is bits [29-0]
    uint32_t wptrIdx = ((TT_STATUS_IDX_MAX * idx) + TT_STATUS_IDX_WPTR);
    ttStatus_[wptrIdx] = (ttStatus_[wptrIdx] & TT_WRITE_PTR_MASK);
  }

  return true;
}

}  // pm4_profile
