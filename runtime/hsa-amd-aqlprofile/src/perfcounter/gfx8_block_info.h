#ifndef _VI_BLOCKINFO_H_
#define _VI_BLOCKINFO_H_

#include "gpu_block_info.h"

namespace pm4_profile {

// MAX Number of block instances for VOLCANIC ISLANDS (From Fiji)
// Values are found here //gfxip/gfx8/main/src/meta/features/variant/Fiji/album.dj

// @brief Number of block instances.

// We index per SE and instance
#define VI_NUM_CB 4  // CB has 4 instances per SE
#define VI_NUM_DB 4  // DB has 4 instances per SE

// For TA, TD and TCP, the values below are the same as the number of CUs
// per SH. We index per SE and instance
#define VI_NUM_TA 16   // TA has 11 instances
#define VI_NUM_TD 16   // TD has 11 instances
#define VI_NUM_TCP 16  // TCP has 11 instances

// These values are per chip, we index directly per instance
#define VI_NUM_TCA 2   // TCA has 2 instances per chip
#define VI_NUM_TCC 16  // TCC has 16 instances per chip
#define VI_NUM_SDMA 2  // There are two SDMA blocks on VI, exposed as 2
                       // instances here

// Number of counter registers per block for volcanic islands
#define VI_COUNTER_NUM_PER_DRM 2
#define VI_COUNTER_NUM_PER_DRMDMA 2
#define VI_COUNTER_NUM_PER_IH 2
#define VI_COUNTER_NUM_PER_SRBM 2
#define VI_COUNTER_NUM_PER_CB 4
#define VI_COUNTER_NUM_PER_CPF 2
#define VI_COUNTER_NUM_PER_DB 4
#define VI_COUNTER_NUM_PER_GRBM 2
#define VI_COUNTER_NUM_PER_GRBMSE 4
#define VI_COUNTER_NUM_PER_PA_SU 4
#define VI_COUNTER_NUM_PER_RLC 2
#define VI_COUNTER_NUM_PER_PA_SC 8
#define VI_COUNTER_NUM_PER_SPI 6  // [Shucai: To do: double check the value]
#define VI_COUNTER_NUM_PER_SQ 16
#define VI_COUNTER_NUM_PER_SX 4
#define VI_COUNTER_NUM_PER_TA 2
#define VI_COUNTER_NUM_PER_TCA 4
#define VI_COUNTER_NUM_PER_TCC 4
#define VI_COUNTER_NUM_PER_TD 2  // [Shucai: To do: double check the value]
#define VI_COUNTER_NUM_PER_TCP 4
#define VI_COUNTER_NUM_PER_GDS 4
#define VI_COUNTER_NUM_PER_VGT 4
#define VI_COUNTER_NUM_PER_IA 4
#define VI_COUNTER_NUM_PER_MC 4
#define VI_COUNTER_NUM_PER_TCS 4
#define VI_COUNTER_NUM_PER_WD 4
#define VI_COUNTER_NUM_PER_CPG 2
#define VI_COUNTER_NUM_PER_CPC 2
#define VI_COUNTER_NUM_PER_VM 1
#define VI_COUNTER_NUM_PER_VM_MD 1
#define VI_COUNTER_NUM_PER_PIPESTATS 12

#define VI_MAX_NUM_SHADER_ENGINES 1

// Enumeration of VI hardware counter blocks
typedef enum HsaViCounterBlockId {
  kHsaViCounterBlockIdCb0 = 0,
  kHsaViCounterBlockIdCb1,
  kHsaViCounterBlockIdCb2,
  kHsaViCounterBlockIdCb3,

  kHsaViCounterBlockIdCpf,

  kHsaViCounterBlockIdDb0,
  kHsaViCounterBlockIdDb1,
  kHsaViCounterBlockIdDb2,
  kHsaViCounterBlockIdDb3,

  kHsaViCounterBlockIdGrbm,
  kHsaViCounterBlockIdGrbmSe,
  kHsaViCounterBlockIdPaSu,
  kHsaViCounterBlockIdPaSc,
  kHsaViCounterBlockIdSpi,

  kHsaViCounterBlockIdSq,
  kHsaViCounterBlockIdSqEs,
  kHsaViCounterBlockIdSqGs,
  kHsaViCounterBlockIdSqVs,
  kHsaViCounterBlockIdSqPs,
  kHsaViCounterBlockIdSqLs,
  kHsaViCounterBlockIdSqHs,
  kHsaViCounterBlockIdSqCs,

  kHsaViCounterBlockIdSx,

  kHsaViCounterBlockIdTa0,
  kHsaViCounterBlockIdTa1,
  kHsaViCounterBlockIdTa2,
  kHsaViCounterBlockIdTa3,
  kHsaViCounterBlockIdTa4,
  kHsaViCounterBlockIdTa5,
  kHsaViCounterBlockIdTa6,
  kHsaViCounterBlockIdTa7,
  kHsaViCounterBlockIdTa8,
  kHsaViCounterBlockIdTa9,
  kHsaViCounterBlockIdTa10,
  kHsaViCounterBlockIdTa11,
  kHsaViCounterBlockIdTa12,
  kHsaViCounterBlockIdTa13,
  kHsaViCounterBlockIdTa14,
  kHsaViCounterBlockIdTa15,

  kHsaViCounterBlockIdTca0,
  kHsaViCounterBlockIdTca1,

  kHsaViCounterBlockIdTcc0,
  kHsaViCounterBlockIdTcc1,
  kHsaViCounterBlockIdTcc2,
  kHsaViCounterBlockIdTcc3,
  kHsaViCounterBlockIdTcc4,
  kHsaViCounterBlockIdTcc5,
  kHsaViCounterBlockIdTcc6,
  kHsaViCounterBlockIdTcc7,
  kHsaViCounterBlockIdTcc8,
  kHsaViCounterBlockIdTcc9,
  kHsaViCounterBlockIdTcc10,
  kHsaViCounterBlockIdTcc11,
  kHsaViCounterBlockIdTcc12,
  kHsaViCounterBlockIdTcc13,
  kHsaViCounterBlockIdTcc14,
  kHsaViCounterBlockIdTcc15,

  kHsaViCounterBlockIdTd0,
  kHsaViCounterBlockIdTd1,
  kHsaViCounterBlockIdTd2,
  kHsaViCounterBlockIdTd3,
  kHsaViCounterBlockIdTd4,
  kHsaViCounterBlockIdTd5,
  kHsaViCounterBlockIdTd6,
  kHsaViCounterBlockIdTd7,
  kHsaViCounterBlockIdTd8,
  kHsaViCounterBlockIdTd9,
  kHsaViCounterBlockIdTd10,
  kHsaViCounterBlockIdTd11,
  kHsaViCounterBlockIdTd12,
  kHsaViCounterBlockIdTd13,
  kHsaViCounterBlockIdTd14,
  kHsaViCounterBlockIdTd15,

  kHsaViCounterBlockIdTcp0,
  kHsaViCounterBlockIdTcp1,
  kHsaViCounterBlockIdTcp2,
  kHsaViCounterBlockIdTcp3,
  kHsaViCounterBlockIdTcp4,
  kHsaViCounterBlockIdTcp5,
  kHsaViCounterBlockIdTcp6,
  kHsaViCounterBlockIdTcp7,
  kHsaViCounterBlockIdTcp8,
  kHsaViCounterBlockIdTcp9,
  kHsaViCounterBlockIdTcp10,
  kHsaViCounterBlockIdTcp11,
  kHsaViCounterBlockIdTcp12,
  kHsaViCounterBlockIdTcp13,
  kHsaViCounterBlockIdTcp14,
  kHsaViCounterBlockIdTcp15,

  kHsaViCounterBlockIdGds,
  kHsaViCounterBlockIdVgt,
  kHsaViCounterBlockIdIa,
  kHsaViCounterBlockIdMc,
  kHsaViCounterBlockIdSrbm,

  kHsaViCounterBlockIdTcs,
  kHsaViCounterBlockIdWd,
  kHsaViCounterBlockIdCpg,
  kHsaViCounterBlockIdCpc,

  // Counters retrieved by KFD
  kHsaViCounterBlockIdIommuV2,
  kHsaViCounterBlockIdKernelDriver,

  kHsaViCounterBlockIdCpPipeStats,
  kHsaViCounterBlockIdHwInfo,
  kHsaViCounterBlockIdBlocksFirst = kHsaViCounterBlockIdCb0,
  kHsaViCounterBlockIdBlocksLast = kHsaViCounterBlockIdHwInfo
} HsaViCounterBlockId;

extern GpuBlockInfo Gfx8HwBlocks[];
extern GpuCounterRegInfo ViSqCounterRegAddr[];
extern GpuCounterRegInfo ViCbCounterRegAddr[];
extern GpuCounterRegInfo ViDrmdmaCounterRegAddr[];
extern GpuCounterRegInfo ViIhCounterRegAddr[];
extern GpuCounterRegInfo ViCpfCounterRegAddr[];
extern GpuCounterRegInfo ViCpgCounterRegAddr[];
extern GpuCounterRegInfo ViCpcCounterRegAddr[];
extern GpuCounterRegInfo ViDrmCounterRegAddr[];
extern GpuCounterRegInfo ViGrbmCounterRegAddr[];
extern GpuCounterRegInfo ViGrbmSeCounterRegAddr[];
extern GpuCounterRegInfo ViPaSuCounterRegAddr[];
extern GpuCounterRegInfo ViPaScCounterRegAddr[];
extern GpuCounterRegInfo ViSpiCounterRegAddr[];
extern GpuCounterRegInfo ViTcaCounterRegAddr[];
extern GpuCounterRegInfo ViTccCounterRegAddr[];
extern GpuCounterRegInfo ViTcpCounterRegAddr[];
extern GpuCounterRegInfo ViDbCounterRegAddr[];
extern GpuCounterRegInfo ViRlcCounterRegAddr[];
extern GpuCounterRegInfo ViScCounterRegAddr[];
extern GpuCounterRegInfo ViSxCounterRegAddr[];
extern GpuCounterRegInfo ViTaCounterRegAddr[];
extern GpuCounterRegInfo ViTdCounterRegAddr[];
extern GpuCounterRegInfo ViGdsCounterRegAddr[];
extern GpuCounterRegInfo ViVgtCounterRegAddr[];
extern GpuCounterRegInfo ViIaCounterRegAddr[];
extern GpuCounterRegInfo ViMcCounterRegAddr[];
extern GpuCounterRegInfo ViSrbmCounterRegAddr[];

// No Tcs Counter block on VI
// extern GpuCounterRegInfo ViTcsCounterRegAddr[];
extern GpuCounterRegInfo ViWdCounterRegAddr[];
extern GpuCounterRegInfo ViCpgCounterRegAddr[];
extern GpuCounterRegInfo ViCpcCounterRegAddr[];

extern GpuPrivCounterBlockId ViBlockIdSq;
extern GpuPrivCounterBlockId ViBlockIdMc;
extern GpuPrivCounterBlockId ViBlockIdIommuV2;
extern GpuPrivCounterBlockId ViBlockIdKernelDriver;
}
#endif
