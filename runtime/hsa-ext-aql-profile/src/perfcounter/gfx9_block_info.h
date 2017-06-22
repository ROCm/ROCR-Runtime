#ifndef _AI_BLOCKINFO_H_
#define _AI_BLOCKINFO_H_

#include "gpu_block_info.h"

namespace pm4_profile {

// MAX Number of block instances for ARCTIC ISLANDS (From Vega10)
// Values are found here //gfxip/gfx8/main/src/meta/features/variant/Fiji/album.dj

// @brief Number of block instances.

// Number of CB block instances per SE
// and number of Perf Cntrs per CB block
#define AI_NUM_CB 4
#define AI_COUNTER_NUM_PER_CB 4

// Number of DB block instances per SE
// and number of Perf Cntrs per DB block
#define AI_NUM_DB 4
#define AI_COUNTER_NUM_PER_DB 4

// Number of TA block instances per SE
// and number of Perf Cntrs per TA block
#define AI_NUM_TA 16
#define AI_COUNTER_NUM_PER_TA 2

// Number of TD block instances per SE
// and number of Perf Cntrs per TD block
#define AI_NUM_TD 16
#define AI_COUNTER_NUM_PER_TD 2

// Number of TCP block instances per SE
// and number of Perf Cntrs per TCP block
#define AI_NUM_TCP 16
#define AI_COUNTER_NUM_PER_TCP 4

// Number of TCA block instances per chip
// and number of Perf Cntrs per TCA block
#define AI_NUM_TCA 2
#define AI_COUNTER_NUM_PER_TCA 4

// Number of TCC block instances per chip
// and number of Perf Cntrs per TCC block
#define AI_NUM_TCC 16
#define AI_COUNTER_NUM_PER_TCC 4

// Number of SDMA block instances per chip
// and number of Perf Cntrs per SDMA block
#define AI_NUM_SDMA 2

// Number of counter registers per block for arctic islands
#define AI_COUNTER_NUM_PER_DRM 2
#define AI_COUNTER_NUM_PER_DRMDMA 2
#define AI_COUNTER_NUM_PER_IH 2
#define AI_COUNTER_NUM_PER_SRBM 2
#define AI_COUNTER_NUM_PER_CPF 2
#define AI_COUNTER_NUM_PER_GRBM 2
#define AI_COUNTER_NUM_PER_GRBMSE 4
#define AI_COUNTER_NUM_PER_PA_SU 4
#define AI_COUNTER_NUM_PER_RLC 2
#define AI_COUNTER_NUM_PER_PA_SC 8
#define AI_COUNTER_NUM_PER_SPI 6  // [Shucai: To do: double check the value]
#define AI_COUNTER_NUM_PER_SQ 16
#define AI_COUNTER_NUM_PER_SX 4
#define AI_COUNTER_NUM_PER_GDS 4
#define AI_COUNTER_NUM_PER_VGT 4
#define AI_COUNTER_NUM_PER_IA 4
#define AI_COUNTER_NUM_PER_MC 4
#define AI_COUNTER_NUM_PER_TCS 4
#define AI_COUNTER_NUM_PER_WD 4
#define AI_COUNTER_NUM_PER_CPG 2
#define AI_COUNTER_NUM_PER_CPC 2
#define AI_COUNTER_NUM_PER_VM 1
#define AI_COUNTER_NUM_PER_VM_MD 1
#define AI_COUNTER_NUM_PER_PIPESTATS 12

#define AI_MAX_NUM_SHADER_ENGINES 1

// Enumeration of AI hardware counter blocks
typedef enum HsaAiCounterBlockId {
  kHsaAiCounterBlockIdCb0 = 0,
  kHsaAiCounterBlockIdCb1,
  kHsaAiCounterBlockIdCb2,
  kHsaAiCounterBlockIdCb3,

  // Temp commented out for Vega10
  // kHsaAiCounterBlockIdCpf,

  kHsaAiCounterBlockIdDb0,
  kHsaAiCounterBlockIdDb1,
  kHsaAiCounterBlockIdDb2,
  kHsaAiCounterBlockIdDb3,

  kHsaAiCounterBlockIdGrbm,
  kHsaAiCounterBlockIdGrbmSe,
  kHsaAiCounterBlockIdPaSu,
  kHsaAiCounterBlockIdPaSc,
  kHsaAiCounterBlockIdSpi,

  kHsaAiCounterBlockIdSq,
  kHsaAiCounterBlockIdSqGs,
  kHsaAiCounterBlockIdSqVs,
  kHsaAiCounterBlockIdSqPs,
  kHsaAiCounterBlockIdSqHs,
  kHsaAiCounterBlockIdSqCs,

  kHsaAiCounterBlockIdSx,

  kHsaAiCounterBlockIdTa0,
  kHsaAiCounterBlockIdTa1,
  kHsaAiCounterBlockIdTa2,
  kHsaAiCounterBlockIdTa3,
  kHsaAiCounterBlockIdTa4,
  kHsaAiCounterBlockIdTa5,
  kHsaAiCounterBlockIdTa6,
  kHsaAiCounterBlockIdTa7,
  kHsaAiCounterBlockIdTa8,
  kHsaAiCounterBlockIdTa9,
  kHsaAiCounterBlockIdTa10,
  kHsaAiCounterBlockIdTa11,
  kHsaAiCounterBlockIdTa12,
  kHsaAiCounterBlockIdTa13,
  kHsaAiCounterBlockIdTa14,
  kHsaAiCounterBlockIdTa15,

  kHsaAiCounterBlockIdTca0,
  kHsaAiCounterBlockIdTca1,

  kHsaAiCounterBlockIdTcc0,
  kHsaAiCounterBlockIdTcc1,
  kHsaAiCounterBlockIdTcc2,
  kHsaAiCounterBlockIdTcc3,
  kHsaAiCounterBlockIdTcc4,
  kHsaAiCounterBlockIdTcc5,
  kHsaAiCounterBlockIdTcc6,
  kHsaAiCounterBlockIdTcc7,
  kHsaAiCounterBlockIdTcc8,
  kHsaAiCounterBlockIdTcc9,
  kHsaAiCounterBlockIdTcc10,
  kHsaAiCounterBlockIdTcc11,
  kHsaAiCounterBlockIdTcc12,
  kHsaAiCounterBlockIdTcc13,
  kHsaAiCounterBlockIdTcc14,
  kHsaAiCounterBlockIdTcc15,

  kHsaAiCounterBlockIdTd0,
  kHsaAiCounterBlockIdTd1,
  kHsaAiCounterBlockIdTd2,
  kHsaAiCounterBlockIdTd3,
  kHsaAiCounterBlockIdTd4,
  kHsaAiCounterBlockIdTd5,
  kHsaAiCounterBlockIdTd6,
  kHsaAiCounterBlockIdTd7,
  kHsaAiCounterBlockIdTd8,
  kHsaAiCounterBlockIdTd9,
  kHsaAiCounterBlockIdTd10,
  kHsaAiCounterBlockIdTd11,
  kHsaAiCounterBlockIdTd12,
  kHsaAiCounterBlockIdTd13,
  kHsaAiCounterBlockIdTd14,
  kHsaAiCounterBlockIdTd15,

  kHsaAiCounterBlockIdTcp0,
  kHsaAiCounterBlockIdTcp1,
  kHsaAiCounterBlockIdTcp2,
  kHsaAiCounterBlockIdTcp3,
  kHsaAiCounterBlockIdTcp4,
  kHsaAiCounterBlockIdTcp5,
  kHsaAiCounterBlockIdTcp6,
  kHsaAiCounterBlockIdTcp7,
  kHsaAiCounterBlockIdTcp8,
  kHsaAiCounterBlockIdTcp9,
  kHsaAiCounterBlockIdTcp10,
  kHsaAiCounterBlockIdTcp11,
  kHsaAiCounterBlockIdTcp12,
  kHsaAiCounterBlockIdTcp13,
  kHsaAiCounterBlockIdTcp14,
  kHsaAiCounterBlockIdTcp15,

  kHsaAiCounterBlockIdGds,
  kHsaAiCounterBlockIdVgt,
  kHsaAiCounterBlockIdIa,
  kHsaAiCounterBlockIdMc,

  // Temp commented out for Vega10
  // kHsaAiCounterBlockIdSrbm,

  kHsaAiCounterBlockIdTcs,
  kHsaAiCounterBlockIdWd,

  // Temp commented out for Vega10
  // kHsaAiCounterBlockIdCpg,

  // Temp commented out for Vega10
  // kHsaAiCounterBlockIdCpc,

  // Counters retrieved by KFD
  kHsaAiCounterBlockIdIommuV2,
  kHsaAiCounterBlockIdKernelDriver,

  kHsaAiCounterBlockIdCpPipeStats,
  kHsaAiCounterBlockIdHwInfo,
  kHsaAiCounterBlockIdBlocksFirst = kHsaAiCounterBlockIdCb0,
  kHsaAiCounterBlockIdBlocksLast = kHsaAiCounterBlockIdHwInfo
} HsaAiCounterBlockId;

extern GpuBlockInfo Gfx9HwBlocks[];
extern GpuCounterRegInfo AiSqCounterRegAddr[];
extern GpuCounterRegInfo AiCbCounterRegAddr[];
extern GpuCounterRegInfo AiDrmdmaCounterRegAddr[];
extern GpuCounterRegInfo AiIhCounterRegAddr[];
extern GpuCounterRegInfo AiCpfCounterRegAddr[];
extern GpuCounterRegInfo AiCpgCounterRegAddr[];
extern GpuCounterRegInfo AiCpcCounterRegAddr[];
extern GpuCounterRegInfo AiDrmCounterRegAddr[];
extern GpuCounterRegInfo AiGrbmCounterRegAddr[];
extern GpuCounterRegInfo AiGrbmSeCounterRegAddr[];
extern GpuCounterRegInfo AiPaSuCounterRegAddr[];
extern GpuCounterRegInfo AiPaScCounterRegAddr[];
extern GpuCounterRegInfo AiSpiCounterRegAddr[];
extern GpuCounterRegInfo AiTcaCounterRegAddr[];
extern GpuCounterRegInfo AiTccCounterRegAddr[];
extern GpuCounterRegInfo AiTcpCounterRegAddr[];
extern GpuCounterRegInfo AiDbCounterRegAddr[];
extern GpuCounterRegInfo AiRlcCounterRegAddr[];
extern GpuCounterRegInfo AiScCounterRegAddr[];
extern GpuCounterRegInfo AiSxCounterRegAddr[];
extern GpuCounterRegInfo AiTaCounterRegAddr[];
extern GpuCounterRegInfo AiTdCounterRegAddr[];
extern GpuCounterRegInfo AiGdsCounterRegAddr[];
extern GpuCounterRegInfo AiVgtCounterRegAddr[];
extern GpuCounterRegInfo AiIaCounterRegAddr[];
extern GpuCounterRegInfo AiMcCounterRegAddr[];
extern GpuCounterRegInfo AiSrbmCounterRegAddr[];

// No Tcs Counter block on AI
// extern GpuCounterRegInfo AiTcsCounterRegAddr[];
extern GpuCounterRegInfo AiWdCounterRegAddr[];
extern GpuCounterRegInfo AiCpgCounterRegAddr[];
extern GpuCounterRegInfo AiCpcCounterRegAddr[];

extern GpuPrivCounterBlockId AiBlockIdSq;
extern GpuPrivCounterBlockId AiBlockIdMc;
extern GpuPrivCounterBlockId AiBlockIdIommuV2;
extern GpuPrivCounterBlockId AiBlockIdKernelDriver;
}

#endif  //  _AI_BLOCKINFO_H_
