////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
//
// Developed by:
//
//                 AMD Research and AMD HSA Software Development
//
//                 Advanced Micro Devices, Inc.
//
//                 www.amd.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef HSA_RUNTIME_CORE_INC_AMD_GPU_SHADERS_H_
#define HSA_RUNTIME_CORE_INC_AMD_GPU_SHADERS_H_

namespace rocr {
namespace AMD {

static const unsigned int kCodeCopyAligned7[] = {
    0xC0820100, 0xC0840104, 0xC0860108, 0xC088010C, 0xC08A0110, 0xC00C0114,
    0xBF8C007F, 0x8F028602, 0x4A000002, 0x7E060205, 0xD24A6A02, 0x00000900,
    0xD2506A03, 0x01A90103, 0x7E0A0207, 0xD24A6A04, 0x00000D00, 0xD2506A05,
    0x01A90105, 0xD1C2006A, 0x00001102, 0xBF86000F, 0x87FE6A7E, 0xDC200000,
    0x01000002, 0xBF8C0F70, 0xD24A6A02, 0x00003102, 0xD2506A03, 0x01A90103,
    0xDC600000, 0x00000104, 0xD24A6A04, 0x00003104, 0xD2506A05, 0x01A90105,
    0xBF82FFEE, 0xBEFE04C1, 0x8F198418, 0x34020084, 0x7E060209, 0xD24A6A02,
    0x00001101, 0xD2506A03, 0x01A90103, 0x7E0A020B, 0xD24A6A04, 0x00001501,
    0xD2506A05, 0x01A90105, 0xD1C2006A, 0x00001902, 0xBF86000E, 0xDC380000,
    0x08000002, 0xD24A6A02, 0x00003302, 0xD2506A03, 0x01A90103, 0xBF8C0F70,
    0xDC780000, 0x00000804, 0xD24A6A04, 0x00003304, 0xD2506A05, 0x01A90105,
    0xBF82FFEF, 0x8F198218, 0x34020082, 0x7E06020D, 0xD24A6A02, 0x00001901,
    0xD2506A03, 0x01A90103, 0x7E0A020F, 0xD24A6A04, 0x00001D01, 0xD2506A05,
    0x01A90105, 0xD1C2006A, 0x00002102, 0xBF86000F, 0x87FE6A7E, 0xDC300000,
    0x01000002, 0xD24A6A02, 0x00003302, 0xD2506A03, 0x01A90103, 0xBF8C0F70,
    0xDC700000, 0x00000104, 0xD24A6A04, 0x00003304, 0xD2506A05, 0x01A90105,
    0xBF82FFEE, 0xBEFE04C1, 0x7E060211, 0xD24A6A02, 0x00002100, 0xD2506A03,
    0x01A90103, 0x7E0A0213, 0xD24A6A04, 0x00002500, 0xD2506A05, 0x01A90105,
    0xD1C2006A, 0x00002902, 0xBF860006, 0x87FE6A7E, 0xDC200000, 0x01000002,
    0xBF8C0F70, 0xDC600000, 0x00000104, 0xBF810000,
};

static const unsigned int kCodeCopyMisaligned7[] = {
    0xC0820100, 0xC0840104, 0xC0860108, 0xC008010C, 0xBF8C007F, 0x8F028602,
    0x4A000002, 0x7E060205, 0xD24A6A02, 0x00000900, 0xD2506A03, 0x01A90103,
    0x7E0A0207, 0xD24A6A04, 0x00000D00, 0xD2506A05, 0x01A90105, 0xD1C2006A,
    0x00001102, 0xBF860032, 0xDC200000, 0x06000002, 0xD24A6A02, 0x00002102,
    0xD2506A03, 0x01A90103, 0xDC200000, 0x07000002, 0xD24A6A02, 0x00002102,
    0xD2506A03, 0x01A90103, 0xDC200000, 0x08000002, 0xD24A6A02, 0x00002102,
    0xD2506A03, 0x01A90103, 0xDC200000, 0x09000002, 0xD24A6A02, 0x00002102,
    0xD2506A03, 0x01A90103, 0xBF8C0F70, 0xDC600000, 0x00000604, 0xD24A6A04,
    0x00002104, 0xD2506A05, 0x01A90105, 0xDC600000, 0x00000704, 0xD24A6A04,
    0x00002104, 0xD2506A05, 0x01A90105, 0xDC600000, 0x00000804, 0xD24A6A04,
    0x00002104, 0xD2506A05, 0x01A90105, 0xDC600000, 0x00000904, 0xD24A6A04,
    0x00002104, 0xD2506A05, 0x01A90105, 0xBF82FFCB, 0x7E060209, 0xD24A6A02,
    0x00001100, 0xD2506A03, 0x01A90103, 0x7E0A020B, 0xD24A6A04, 0x00001500,
    0xD2506A05, 0x01A90105, 0xD1C2006A, 0x00001902, 0xBF86000F, 0x87FE6A7E,
    0xDC200000, 0x01000002, 0xD24A6A02, 0x00002102, 0xD2506A03, 0x01A90103,
    0xBF8C0F70, 0xDC600000, 0x00000104, 0xD24A6A04, 0x00002104, 0xD2506A05,
    0x01A90105, 0xBF82FFEE, 0xBF810000,
};

static const unsigned int kCodeFill7[] = {
    0xC0820100, 0xC0840104, 0xBF8C007F, 0x8F028602, 0x4A000002, 0x7E08020A,
    0x7E0A020A, 0x7E0C020A, 0x7E0E020A, 0x8F0C840B, 0x34020084, 0x7E060205,
    0xD24A6A02, 0x00000901, 0xD2506A03, 0x01A90103, 0xD1C2006A, 0x00000D02,
    0xBF860007, 0xDC780000, 0x00000402, 0xD24A6A02, 0x00001902, 0xD2506A03,
    0x01A90103, 0xBF82FFF6, 0x8F0C820B, 0x34020082, 0x7E060207, 0xD24A6A02,
    0x00000D01, 0xD2506A03, 0x01A90103, 0xD1C2006A, 0x00001102, 0xBF860008,
    0x87FE6A7E, 0xDC700000, 0x00000402, 0xD24A6A02, 0x00001902, 0xD2506A03,
    0x01A90103, 0xBF82FFF5, 0xBF810000,
};

static const unsigned int kCodeTrapHandler8[] = {
    0xC0061C80, 0x000000C0, 0xBF8C007F, 0xBEFE0181, 0x80728872, 0x82738073,
    0x7E000272, 0x7E020273, 0x7E0402FF, 0x80000000, 0x7E060280, 0xDD800000,
    0x00000200, 0xBF8C0F70, 0x7DD40500, 0xBF870011, 0xC0061D39, 0x00000008,
    0xBF8C007F, 0x86F47474, 0xBF84000C, 0x80729072, 0x82738073, 0xC0021CB9,
    0x00000000, 0xBF8C007F, 0x7E000274, 0x7E020275, 0x7E040272, 0xDC700000,
    0x00000200, 0xBF8C0F70, 0xBF900001, 0xBF8D0001, 0xBE801F70,
};

static const unsigned int kCodeTrapHandler9[] = {
/*
  .set SQ_WAVE_PC_HI_ADDRESS_MASK              , 0xFFFF
  .set SQ_WAVE_PC_HI_TRAP_ID_SHIFT             , 16
  .set SQ_WAVE_PC_HI_TRAP_ID_SIZE              , 8
  .set SQ_WAVE_PC_HI_TRAP_ID_BFE               , (SQ_WAVE_PC_HI_TRAP_ID_SHIFT | (SQ_WAVE_PC_HI_TRAP_ID_SIZE << 16))
  .set SQ_WAVE_PC_HI_HT_MASK                   , 0x1000000
  .set SQ_WAVE_STATUS_HALT_BIT                 , 13
  .set SQ_WAVE_STATUS_HALT_BFE                 , (SQ_WAVE_STATUS_HALT_BIT | (1 << 16))
  .set SQ_WAVE_TRAPSTS_ADDRESS_WATCH_MASK      , 0x7080
  .set SQ_WAVE_TRAPSTS_MEM_VIOL_MASK           , 0x100
  .set SQ_WAVE_TRAPSTS_ILLEGAL_INST_MASK       , 0x800
  .set SQ_WAVE_TRAPSTS_XNACK_ERROR_MASK        , 0x10000000
  .set SQ_WAVE_MODE_DEBUG_EN_SHIFT             , 11
  .set SIGNAL_CODE_MEM_VIOL                    , (1 << 29)
  .set SIGNAL_CODE_ILLEGAL_INST                , (1 << 30)
  .set SIGNAL_CODE_LLVM_TRAP                   , (1 << 31)
  .set MAX_NUM_DOORBELLS_MASK                  , ((1 << 10) - 1)
  .set SENDMSG_M0_DOORBELL_ID_BITS             , 12
  .set SENDMSG_M0_DOORBELL_ID_MASK             , ((1 << SENDMSG_M0_DOORBELL_ID_BITS) - 1)

  .set TTMP7_DISPATCH_ID_CONVERTED_BIT         , 31
  .set TTMP7_WAVE_STOPPED_BIT                  , 30
  .set TTMP7_SAVED_STATUS_HALT_BIT             , 29
  .set TTMP7_SAVED_TRAP_ID_SHIFT               , 25
  .set TTMP7_SAVED_TRAP_ID_BITS                , 4
  .set TTMP7_SAVED_TRAP_ID_MASK                , ((1 << TTMP7_SAVED_TRAP_ID_BITS) - 1)
  .set TTMP7_PACKET_INDEX_BITS                 , 25
  .set TTMP7_PACKET_INDEX_MASK                 , ((1 << TTMP7_PACKET_INDEX_BITS) - 1)
  .set TTMP11_PC_HI_SHIFT                      , 7

  .if .amdgcn.gfx_generation_number == 9
    .set DEBUG_INTERRUPT_CONTEXT_ID_BIT        , 23
    .set TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT   , 26
    .set SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     , 15
    .set SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK , 0x1F8000
  .elseif .amdgcn.gfx_generation_number == 10
    .set DEBUG_INTERRUPT_CONTEXT_ID_BIT        , 22
    .set TTMP11_SAVE_REPLAY_W64H_SHIFT         , 31
    .set TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT   , 24
    .set SQ_WAVE_IB_STS_REPLAY_W64H_SHIFT      , 25
    .set SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     , 15
    .set SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK , 0x3F8000
    .set SQ_WAVE_IB_STS_REPLAY_W64H_MASK       , 0x2000000
  .else
    .error "unsupported target"
  .endif

  // ABI between first and second level trap handler:
  //   ttmp0 = PC[31:0]
  //   ttmp1 = 0[2:0], PCRewind[3:0], HostTrap[0], TrapId[7:0], PC[47:32]
  //   ttmp12 = SQ_WAVE_STATUS
  //   ttmp14 = TMA[31:0]
  //   ttmp15 = TMA[63:32]
  // gfx9:
  //   ttmp11 = SQ_WAVE_IB_STS[20:15], 0[18:0], NoScratch[0], WaveIdInWG[5:0]
  // gfx10:
  //   ttmp11 = SQ_WAVE_IB_STS[25], SQ_WAVE_IB_STS[21:15], 0[16:0], NoScratch[0], WaveIdInWG[5:0]

  .macro mGetDoorbellId
    s_mov_b32            exec_lo, 0x80000000
    s_sendmsg            sendmsg(MSG_GET_DOORBELL)
  .wait_sendmsg_\@:
    s_nop                7
    s_bitcmp0_b32        exec_lo, 0x1F
    s_cbranch_scc0       .wait_sendmsg_\@
  .endm

  .macro mExitTrap
    // Restore SQ_WAVE_IB_STS.
  .if .amdgcn.gfx_generation_number == 9
    s_lshr_b32           ttmp2, ttmp11, (TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
    s_and_b32            ttmp2, ttmp2, SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK
    s_setreg_b32         hwreg(HW_REG_IB_STS), ttmp2
  .endif
  .if .amdgcn.gfx_generation_number == 10
    s_lshr_b32           ttmp2, ttmp11, (TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
    s_and_b32            ttmp3, ttmp2, SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK
    s_lshr_b32           ttmp2, ttmp11, (TTMP11_SAVE_REPLAY_W64H_SHIFT - SQ_WAVE_IB_STS_REPLAY_W64H_SHIFT)
    s_and_b32            ttmp2, ttmp2, SQ_WAVE_IB_STS_REPLAY_W64H_MASK
    s_or_b32             ttmp2, ttmp2, ttmp3
    s_setreg_b32         hwreg(HW_REG_IB_STS), ttmp2
  .endif

    // Restore SQ_WAVE_STATUS.
    s_and_b64            exec, exec, exec // Restore STATUS.EXECZ, not writable by s_setreg_b32
    s_and_b64            vcc, vcc, vcc    // Restore STATUS.VCCZ, not writable by s_setreg_b32
    s_setreg_b32         hwreg(HW_REG_STATUS), ttmp12

    // Return to shader at unmodified PC.
    s_rfe_b64            [ttmp0, ttmp1]
  .endm

  trap_entry:
    s_andn2_b32          ttmp7, ttmp7, (TTMP7_SAVED_TRAP_ID_MASK << TTMP7_SAVED_TRAP_ID_SHIFT) | (1 << TTMP7_SAVED_STATUS_HALT_BIT)

    // Save the entry status.halt in ttmp7.saved_status_halt
    s_bfe_u32            ttmp2, ttmp12, SQ_WAVE_STATUS_HALT_BFE
    s_lshl_b32           ttmp2, ttmp2, TTMP7_SAVED_STATUS_HALT_BIT
    s_or_b32             ttmp7, ttmp7, ttmp2

    // If trap raised (non-zero trap id) then branch.
    s_bfe_u32            ttmp2, ttmp1, SQ_WAVE_PC_HI_TRAP_ID_BFE
    s_cbranch_scc1       .trap_raised

    // If non-masked exception raised then branch.
    s_getreg_b32         ttmp2, hwreg(HW_REG_TRAPSTS)
    s_and_b32            ttmp3, ttmp2, (SQ_WAVE_TRAPSTS_MEM_VIOL_MASK | SQ_WAVE_TRAPSTS_ILLEGAL_INST_MASK)
    s_cbranch_scc1       .excp_raised

  .signal_debugger:
    // Fetch doorbell index for our queue.
    s_mov_b32            ttmp2, exec_lo
    s_mov_b32            ttmp3, exec_hi
    mGetDoorbellId
    s_mov_b32            exec_hi, ttmp3

    // Restore exec_lo, move the doorbell_id into ttmp3
    s_and_b32            ttmp3, exec_lo, SENDMSG_M0_DOORBELL_ID_MASK
    s_mov_b32            exec_lo, ttmp2

    // Set the debug interrupt context id.
    // FIXME: Make conditional when exceptions are handled.
    s_bitset1_b32        ttmp3, DEBUG_INTERRUPT_CONTEXT_ID_BIT

    // Send an interrupt to trigger event notification.
    s_mov_b32            ttmp2, m0
    s_mov_b32            m0, ttmp3
    s_nop                0x0 // Manually inserted wait states
    s_sendmsg            sendmsg(MSG_INTERRUPT)

    // Restore m0
    s_mov_b32            m0, ttmp2

    // Parking the wave requires saving the original pc in the preserved ttmps.
    // Since all ttmps are used, we must first free ttmp6 by compressing the
    // 40bit dispatch ptr in ttmp6:7 into a 25bit queue packet id.
    //
    // Register layout before parking the wave:
    //
    // ttmp6: dispatch_ptr[31:6] 0[5:0]
    // ttmp7: 0[0] wave_stopped[0] status_halt[0] trap_id[3:0] 0[16:0] dispatch_ptr[39:32]
    // ttmp11: 1st_level_ttmp11[31:23] 0[15:0] 1st_level_ttmp11[6:0]
    //
    // After parking the wave:
    //
    // ttmp6:  pc_lo[31:0]
    // ttmp7:  1[0] wave_stopped[0] status_halt[0] trap_id[3:0] packet_id[24:0]
    // ttmp11: 1st_level_ttmp11[31:23] pc_hi[15:0] 1st_level_ttmp11[6:0]
    //
    // The conversion from dispatch ptr to queue packet index only needs to be
    // done once, the first time the wave executes the trap handler.

  .if ((.amdgcn.gfx_generation_number == 10 && .amdgcn.gfx_generation_minor >= 3) || .amdgcn.gfx_generation_number > 10)
    s_branch             .halt_wave
  .else
    s_bitcmp1_b32        ttmp7, TTMP7_DISPATCH_ID_CONVERTED_BIT
    s_cbranch_scc1       .ttmp7_has_dispatch_index

    s_and_b32            ttmp3, ttmp3, MAX_NUM_DOORBELLS_MASK
    s_lshl_b32           ttmp3, ttmp3, 0x3

    // Map doorbell index to amd_queue_t* through TMA (doorbell_queue_map).
    s_load_dwordx2       [ttmp2, ttmp3], [ttmp14, ttmp15], ttmp3 glc
    s_waitcnt            lgkmcnt(0)

    // Retrieve queue base_address from hsa_queue_t*.
    s_load_dword         ttmp2, [ttmp2, ttmp3], 0x8 glc
    s_waitcnt            lgkmcnt(0)

    // The dispatch index is (dispatch_ptr.lo - base_address.lo) >> 6
    s_sub_u32            ttmp2, ttmp6, ttmp2
    s_lshr_b32           ttmp2, ttmp2, 0x6
    s_andn2_b32          ttmp7, ttmp7, TTMP7_PACKET_INDEX_MASK
    s_or_b32             ttmp7, ttmp7, ttmp2
    s_bitset1_b32        ttmp7, TTMP7_DISPATCH_ID_CONVERTED_BIT

  .ttmp7_has_dispatch_index:
    // Save the PC
    s_mov_b32            ttmp6, ttmp0
    s_and_b32            ttmp1, ttmp1, SQ_WAVE_PC_HI_ADDRESS_MASK
    s_lshl_b32           ttmp1, ttmp1, TTMP11_PC_HI_SHIFT
    s_andn2_b32          ttmp11, ttmp11, (SQ_WAVE_PC_HI_ADDRESS_MASK << TTMP11_PC_HI_SHIFT)
    s_or_b32             ttmp11, ttmp11, ttmp1

    // Park the wave
    s_getpc_b64          [ttmp0, ttmp1]
    s_add_u32            ttmp0, ttmp0, .parked - .
    s_addc_u32           ttmp1, ttmp1, 0x0
    s_branch             .halt_wave

  .parked:
    s_trap               0x2
    s_branch             .parked
  .endif

  .excp_raised:
    // If memory violation without XNACK error then signal queue error.
    // XNACK error will be handled by VM interrupt, since it has more information.
    s_and_b32            ttmp3, ttmp2, (SQ_WAVE_TRAPSTS_MEM_VIOL_MASK | SQ_WAVE_TRAPSTS_XNACK_ERROR_MASK)
    s_cmp_eq_u32         ttmp3, SQ_WAVE_TRAPSTS_MEM_VIOL_MASK
    s_mov_b32            ttmp3, SIGNAL_CODE_MEM_VIOL
    s_cbranch_scc1       .signal_error

    // If illegal instruction then signal queue error.
    s_and_b32            ttmp3, ttmp2, SQ_WAVE_TRAPSTS_ILLEGAL_INST_MASK
    s_mov_b32            ttmp3, SIGNAL_CODE_ILLEGAL_INST
    s_cbranch_scc1       .signal_error

    // Otherwise (memory violation with XNACK error) return to shader. Do not
    // send a signal as that will cause an interrupt storm. Instead let the
    // interrupt generated by the TLB miss cause the kernel to notify ROCr and
    // put the queue into an error state. This also ensures the TLB interrupt
    // is received which provides information about the page causing the fault.
    s_branch             .halt_wave

  .trap_raised:
    // Save the entry trap id in ttmp7.saved_trap_id
    s_min_u32            ttmp3, ttmp2, 0xF
    s_lshl_b32           ttmp3, ttmp3, TTMP7_SAVED_TRAP_ID_SHIFT
    s_or_b32             ttmp7, ttmp7, ttmp3

    // If debugger trap (s_trap >= 3) then signal debugger.
    s_cmp_ge_u32         ttmp2, 0x3;
    s_cbranch_scc1       .signal_debugger

    // If llvm.trap (s_trap 2) then signal queue error.
    s_cmp_eq_u32         ttmp2, 0x2
    s_mov_b32            ttmp3, SIGNAL_CODE_LLVM_TRAP
    s_cbranch_scc1       .signal_error

    // For other traps advance PC and return to shader.
    s_add_u32            ttmp0, ttmp0, 0x4
    s_addc_u32           ttmp1, ttmp1, 0x0
    s_branch             .exit_trap

  .signal_error:
  .if (.amdgcn.gfx_generation_number == 10 && .amdgcn.gfx_generation_minor >= 3)
    // This needs to be rewritten for gfx10.3 as scalar stores are not available.
  .else
    // FIXME: don't trash ttmp4/ttmp5 when exception handling is unified.
    s_mov_b32            ttmp4, ttmp3

    // Fetch doorbell index for our queue.
    s_mov_b32            ttmp2, exec_lo
    s_mov_b32            ttmp3, exec_hi
    mGetDoorbellId
    s_mov_b32            exec_hi, ttmp3

    // Restore exec_lo, move the doorbell index into ttmp3
    s_and_b32            exec_lo, exec_lo, MAX_NUM_DOORBELLS_MASK
    s_lshl_b32           ttmp3, exec_lo, 0x3
    s_mov_b32            exec_lo, ttmp2

    // Map doorbell index to amd_queue_t* through TMA (doorbell_queue_map).
    s_load_dwordx2       [ttmp2, ttmp3], [ttmp14, ttmp15], ttmp3 glc
    s_waitcnt            lgkmcnt(0)

    // Retrieve queue_inactive_signal from amd_queue_t*.
    s_load_dwordx2       [ttmp2, ttmp3], [ttmp2, ttmp3], 0xC0 glc
    s_waitcnt            lgkmcnt(0)

    // Set queue signal value to error code.
    s_mov_b32            ttmp5, 0x0
    s_atomic_swap_x2     [ttmp4, ttmp5], [ttmp2, ttmp3], 0x8 glc
    s_waitcnt            lgkmcnt(0)

    // Skip event trigger if the signal value was already non-zero.
    s_or_b32             ttmp4, ttmp4, ttmp5
    s_cbranch_scc1       .skip_event_trigger

    // Check for a non-NULL signal event mailbox.
    s_load_dwordx2       [ttmp4, ttmp5], [ttmp2, ttmp3], 0x10 glc
    s_waitcnt            lgkmcnt(0)
    s_and_b64            [ttmp4, ttmp5], [ttmp4, ttmp5], [ttmp4, ttmp5]
    s_cbranch_scc0       .skip_event_trigger

    // Load the signal event value.
    s_load_dword         ttmp2, [ttmp2, ttmp3], 0x18 glc
    s_waitcnt            lgkmcnt(0)

    // Write the signal event value to the mailbox.
    s_store_dword        ttmp2, [ttmp4, ttmp5], 0x0 glc
    s_waitcnt            lgkmcnt(0)

    // Send an interrupt to trigger event notification.
    s_mov_b32            m0, 0x0
    s_nop                0
    s_sendmsg            sendmsg(MSG_INTERRUPT)
  .endif

  .skip_event_trigger:
    // Since we trashed ttmp4/ttmp5, reset the wave_id to 0
    s_mov_b32            ttmp4, 0x0
    s_mov_b32            ttmp5, 0x0

  .halt_wave:
    s_bitset1_b32        ttmp7, TTMP7_WAVE_STOPPED_BIT

    // Halt the wavefront.
    s_bitset1_b32        ttmp12, SQ_WAVE_STATUS_HALT_BIT

  .exit_trap:
    mExitTrap
*/
    0x8973ff73, 0x3e000000, 0x92eeff78, 0x0001000d, 0x8e6e9d6e, 0x87736e73,
    0x92eeff6d, 0x00080010, 0xbf850041, 0xb8eef803, 0x866fff6e, 0x00000900,
    0xbf850031, 0xbeee007e, 0xbeef007f, 0xbefe00ff, 0x80000000, 0xbf90000a,
    0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff006f, 0x866fff7e, 0x00000fff,
    0xbefe006e, 0xbeef1a97, 0xbeee007c, 0xbefc006f, 0xbf800000, 0xbf900001,
    0xbefc006e, 0xbf0d9f73, 0xbf85000f, 0x866fff6f, 0x000003ff, 0x8e6f836f,
    0xc0051bbd, 0x0000006f, 0xbf8cc07f, 0xc0031bb7, 0x00000008, 0xbf8cc07f,
    0x80ee6e72, 0x8f6e866e, 0x8973ff73, 0x01ffffff, 0x87736e73, 0xbef31a9f,
    0xbef2006c, 0x866dff6d, 0x0000ffff, 0x8e6d876d, 0x8977ff77, 0x007fff80,
    0x87776d77, 0xbeec1c00, 0x806cff6c, 0x00000010, 0x826d806d, 0xbf820044,
    0xbf920002, 0xbf82fffe, 0x866fff6e, 0x10000100, 0xbf06ff6f, 0x00000100,
    0xbeef00ff, 0x20000000, 0xbf850011, 0x866fff6e, 0x00000800, 0xbeef00f4,
    0xbf85000d, 0xbf820036, 0x83ef8f6e, 0x8e6f996f, 0x87736f73, 0xbf09836e,
    0xbf85ffbe, 0xbf06826e, 0xbeef00ff, 0x80000000, 0xbf850003, 0x806c846c,
    0x826d806d, 0xbf82002c, 0xbef0006f, 0xbeee007e, 0xbeef007f, 0xbefe00ff,
    0x80000000, 0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff006f,
    0x867eff7e, 0x000003ff, 0x8e6f837e, 0xbefe006e, 0xc0051bbd, 0x0000006f,
    0xbf8cc07f, 0xc0071bb7, 0x000000c0, 0xbf8cc07f, 0xbef10080, 0xc2831c37,
    0x00000008, 0xbf8cc07f, 0x87707170, 0xbf85000e, 0xc0071c37, 0x00000010,
    0xbf8cc07f, 0x86f07070, 0xbf840009, 0xc0031bb7, 0x00000018, 0xbf8cc07f,
    0xc0431bb8, 0x00000000, 0xbf8cc07f, 0xbefc0080, 0xbf800000, 0xbf900001,
    0xbef00080, 0xbef10080, 0xbef31a9e, 0xbef81a8d, 0x8f6e8b77, 0x866eff6e,
    0x001f8000, 0xb96ef807, 0x86fe7e7e, 0x86ea6a6a, 0xb978f802, 0xbe801f6c,
};

static const unsigned int kCodeTrapHandler90a[] = {
    0x8973ff73, 0x3e000000, 0x92eeff78, 0x0001000d, 0x8e6e9d6e, 0x87736e73,
    0x92eeff6d, 0x00080010, 0xbf850041, 0xb8eef803, 0x866fff6e, 0x00000900,
    0xbf850031, 0xbeee007e, 0xbeef007f, 0xbefe00ff, 0x80000000, 0xbf90000a,
    0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff006f, 0x866fff7e, 0x00000fff,
    0xbefe006e, 0xbeef1a97, 0xbeee007c, 0xbefc006f, 0xbf800000, 0xbf900001,
    0xbefc006e, 0xbf0d9f73, 0xbf85000f, 0x866fff6f, 0x000003ff, 0x8e6f836f,
    0xc0051bbd, 0x0000006f, 0xbf8cc07f, 0xc0031bb7, 0x00000008, 0xbf8cc07f,
    0x80ee6e72, 0x8f6e866e, 0x8973ff73, 0x01ffffff, 0x87736e73, 0xbef31a9f,
    0xbef2006c, 0x866dff6d, 0x0000ffff, 0x8e6d876d, 0x8977ff77, 0x007fff80,
    0x87776d77, 0xbeec1c00, 0x806cff6c, 0x00000010, 0x826d806d, 0xbf820044,
    0xbf920002, 0xbf82fffe, 0x866fff6e, 0x10000100, 0xbf06ff6f, 0x00000100,
    0xbeef00ff, 0x20000000, 0xbf850011, 0x866fff6e, 0x00000800, 0xbeef00f4,
    0xbf85000d, 0xbf820036, 0x83ef8f6e, 0x8e6f996f, 0x87736f73, 0xbf09836e,
    0xbf85ffbe, 0xbf06826e, 0xbeef00ff, 0x80000000, 0xbf850003, 0x806c846c,
    0x826d806d, 0xbf82002c, 0xbef0006f, 0xbeee007e, 0xbeef007f, 0xbefe00ff,
    0x80000000, 0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff006f,
    0x867eff7e, 0x000003ff, 0x8e6f837e, 0xbefe006e, 0xc0051bbd, 0x0000006f,
    0xbf8cc07f, 0xc0071bb7, 0x000000c0, 0xbf8cc07f, 0xbef10080, 0xc2831c37,
    0x00000008, 0xbf8cc07f, 0x87707170, 0xbf85000e, 0xc0071c37, 0x00000010,
    0xbf8cc07f, 0x86f07070, 0xbf840009, 0xc0031bb7, 0x00000018, 0xbf8cc07f,
    0xc0431bb8, 0x00000000, 0xbf8cc07f, 0xbefc0080, 0xbf800000, 0xbf900001,
    0xbef00080, 0xbef10080, 0xbef31a9e, 0xbef81a8d, 0x8f6e8b77, 0x866eff6e,
    0x001f8000, 0xb96ef807, 0x86fe7e7e, 0x86ea6a6a, 0xb978f802, 0xbe801f6c,
};

static const unsigned int kCodeCopyAligned8[] = {
    0xC00A0100, 0x00000000, 0xC00A0200, 0x00000010, 0xC00A0300, 0x00000020,
    0xC00A0400, 0x00000030, 0xC00A0500, 0x00000040, 0xC0020600, 0x00000050,
    0xBF8C007F, 0x8E028602, 0x32000002, 0x7E060205, 0xD1196A02, 0x00000900,
    0xD11C6A03, 0x01A90103, 0x7E0A0207, 0xD1196A04, 0x00000D00, 0xD11C6A05,
    0x01A90105, 0xD0E9006A, 0x00001102, 0xBF86000F, 0x86FE6A7E, 0xDC400000,
    0x01000002, 0xBF8C0F70, 0xD1196A02, 0x00003102, 0xD11C6A03, 0x01A90103,
    0xDC600000, 0x00000104, 0xD1196A04, 0x00003104, 0xD11C6A05, 0x01A90105,
    0xBF82FFEE, 0xBEFE01C1, 0x8E198418, 0x24020084, 0x7E060209, 0xD1196A02,
    0x00001101, 0xD11C6A03, 0x01A90103, 0x7E0A020B, 0xD1196A04, 0x00001501,
    0xD11C6A05, 0x01A90105, 0xD0E9006A, 0x00001902, 0xBF86000E, 0xDC5C0000,
    0x08000002, 0xD1196A02, 0x00003302, 0xD11C6A03, 0x01A90103, 0xBF8C0F70,
    0xDC7C0000, 0x00000804, 0xD1196A04, 0x00003304, 0xD11C6A05, 0x01A90105,
    0xBF82FFEF, 0x8E198218, 0x24020082, 0x7E06020D, 0xD1196A02, 0x00001901,
    0xD11C6A03, 0x01A90103, 0x7E0A020F, 0xD1196A04, 0x00001D01, 0xD11C6A05,
    0x01A90105, 0xD0E9006A, 0x00002102, 0xBF86000F, 0x86FE6A7E, 0xDC500000,
    0x01000002, 0xD1196A02, 0x00003302, 0xD11C6A03, 0x01A90103, 0xBF8C0F70,
    0xDC700000, 0x00000104, 0xD1196A04, 0x00003304, 0xD11C6A05, 0x01A90105,
    0xBF82FFEE, 0xBEFE01C1, 0x7E060211, 0xD1196A02, 0x00002100, 0xD11C6A03,
    0x01A90103, 0x7E0A0213, 0xD1196A04, 0x00002500, 0xD11C6A05, 0x01A90105,
    0xD0E9006A, 0x00002902, 0xBF860006, 0x86FE6A7E, 0xDC400000, 0x01000002,
    0xBF8C0F70, 0xDC600000, 0x00000104, 0xBF810000,
};

static const unsigned int kCodeCopyMisaligned8[] = {
    0xC00A0100, 0x00000000, 0xC00A0200, 0x00000010, 0xC00A0300, 0x00000020,
    0xC0020400, 0x00000030, 0xBF8C007F, 0x8E028602, 0x32000002, 0x7E060205,
    0xD1196A02, 0x00000900, 0xD11C6A03, 0x01A90103, 0x7E0A0207, 0xD1196A04,
    0x00000D00, 0xD11C6A05, 0x01A90105, 0xD0E9006A, 0x00001102, 0xBF860032,
    0xDC400000, 0x06000002, 0xD1196A02, 0x00002102, 0xD11C6A03, 0x01A90103,
    0xDC400000, 0x07000002, 0xD1196A02, 0x00002102, 0xD11C6A03, 0x01A90103,
    0xDC400000, 0x08000002, 0xD1196A02, 0x00002102, 0xD11C6A03, 0x01A90103,
    0xDC400000, 0x09000002, 0xD1196A02, 0x00002102, 0xD11C6A03, 0x01A90103,
    0xBF8C0F70, 0xDC600000, 0x00000604, 0xD1196A04, 0x00002104, 0xD11C6A05,
    0x01A90105, 0xDC600000, 0x00000704, 0xD1196A04, 0x00002104, 0xD11C6A05,
    0x01A90105, 0xDC600000, 0x00000804, 0xD1196A04, 0x00002104, 0xD11C6A05,
    0x01A90105, 0xDC600000, 0x00000904, 0xD1196A04, 0x00002104, 0xD11C6A05,
    0x01A90105, 0xBF82FFCB, 0x7E060209, 0xD1196A02, 0x00001100, 0xD11C6A03,
    0x01A90103, 0x7E0A020B, 0xD1196A04, 0x00001500, 0xD11C6A05, 0x01A90105,
    0xD0E9006A, 0x00001902, 0xBF86000F, 0x86FE6A7E, 0xDC400000, 0x01000002,
    0xD1196A02, 0x00002102, 0xD11C6A03, 0x01A90103, 0xBF8C0F70, 0xDC600000,
    0x00000104, 0xD1196A04, 0x00002104, 0xD11C6A05, 0x01A90105, 0xBF82FFEE,
    0xBF810000,
};

static const unsigned int kCodeFill8[] = {
    0xC00A0100, 0x00000000, 0xC00A0200, 0x00000010, 0xBF8C007F, 0x8E028602,
    0x32000002, 0x7E08020A, 0x7E0A020A, 0x7E0C020A, 0x7E0E020A, 0x8E0C840B,
    0x24020084, 0x7E060205, 0xD1196A02, 0x00000901, 0xD11C6A03, 0x01A90103,
    0xD0E9006A, 0x00000D02, 0xBF860007, 0xDC7C0000, 0x00000402, 0xD1196A02,
    0x00001902, 0xD11C6A03, 0x01A90103, 0xBF82FFF6, 0x8E0C820B, 0x24020082,
    0x7E060207, 0xD1196A02, 0x00000D01, 0xD11C6A03, 0x01A90103, 0xD0E9006A,
    0x00001102, 0xBF860008, 0x86FE6A7E, 0xDC700000, 0x00000402, 0xD1196A02,
    0x00001902, 0xD11C6A03, 0x01A90103, 0xBF82FFF5, 0xBF810000,
};

static const unsigned int kCodeCopyAligned10[] = {
    0xF4080100, 0xFA000000, 0xF4080200, 0xFA000010, 0xF4080300, 0xFA000020,
    0xF4080400, 0xFA000030, 0xF4080500, 0xFA000040, 0xF4000600, 0xFA000050,
    0xBF8CC07F, 0x8F028602, 0xD70F6A00, 0x00020002, 0x7E060205, 0xD70F6A02,
    0x00020004, 0xD5286A03, 0x01A90103, 0x7E0A0207, 0xD70F6A04, 0x00020006,
    0xD5286A05, 0x01A90105, 0xD4E1006A, 0x00001102, 0xBF86000F, 0x87FE6A7E,
    0xDC200000, 0x017D0002, 0xBF8C3F70, 0xD70F6A02, 0x00020418, 0xD5286A03,
    0x01A90103, 0xDC600000, 0x007D0104, 0xD70F6A04, 0x00020818, 0xD5286A05,
    0x01A90105, 0xBF82FFEE, 0xBEFE04C1, 0x8F198418, 0x34020084, 0x7E060209,
    0xD70F6A02, 0x00020208, 0xD5286A03, 0x01A90103, 0x7E0A020B, 0xD70F6A04,
    0x0002020A, 0xD5286A05, 0x01A90105, 0xD4E1006A, 0x00001902, 0xBF86000E,
    0xDC380000, 0x087D0002, 0xD70F6A02, 0x00020419, 0xD5286A03, 0x01A90103,
    0xBF8C3F70, 0xDC780000, 0x007D0804, 0xD70F6A04, 0x00020819, 0xD5286A05,
    0x01A90105, 0xBF82FFEF, 0x8F198218, 0x34020082, 0x7E06020D, 0xD70F6A02,
    0x0002020C, 0xD5286A03, 0x01A90103, 0x7E0A020F, 0xD70F6A04, 0x0002020E,
    0xD5286A05, 0x01A90105, 0xD4E1006A, 0x00002102, 0xBF86000F, 0x87FE6A7E,
    0xDC300000, 0x017D0002, 0xD70F6A02, 0x00020419, 0xD5286A03, 0x01A90103,
    0xBF8C3F70, 0xDC700000, 0x007D0104, 0xD70F6A04, 0x00020819, 0xD5286A05,
    0x01A90105, 0xBF82FFEE, 0xBEFE04C1, 0x7E060211, 0xD70F6A02, 0x00020010,
    0xD5286A03, 0x01A90103, 0x7E0A0213, 0xD70F6A04, 0x00020012, 0xD5286A05,
    0x01A90105, 0xD4E1006A, 0x00002902, 0xBF860006, 0x87FE6A7E, 0xDC200000,
    0x017D0002, 0xBF8C3F70, 0xDC600000, 0x007D0104, 0xBF810000,
};

static const unsigned int kCodeCopyMisaligned10[] = {
    0xF4080100, 0xFA000000, 0xF4080200, 0xFA000010, 0xF4080300, 0xFA000020,
    0xF4000400, 0xFA000030, 0xBF8CC07F, 0x8F028602, 0xD70F6A00, 0x00020002,
    0x7E060205, 0xD70F6A02, 0x00020004, 0xD5286A03, 0x01A90103, 0x7E0A0207,
    0xD70F6A04, 0x00020006, 0xD5286A05, 0x01A90105, 0xD4E1006A, 0x00001102,
    0xBF860032, 0xDC200000, 0x067D0002, 0xD70F6A02, 0x00020410, 0xD5286A03,
    0x01A90103, 0xDC200000, 0x077D0002, 0xD70F6A02, 0x00020410, 0xD5286A03,
    0x01A90103, 0xDC200000, 0x087D0002, 0xD70F6A02, 0x00020410, 0xD5286A03,
    0x01A90103, 0xDC200000, 0x097D0002, 0xD70F6A02, 0x00020410, 0xD5286A03,
    0x01A90103, 0xBF8C3F70, 0xDC600000, 0x007D0604, 0xD70F6A04, 0x00020810,
    0xD5286A05, 0x01A90105, 0xDC600000, 0x007D0704, 0xD70F6A04, 0x00020810,
    0xD5286A05, 0x01A90105, 0xDC600000, 0x007D0804, 0xD70F6A04, 0x00020810,
    0xD5286A05, 0x01A90105, 0xDC600000, 0x007D0904, 0xD70F6A04, 0x00020810,
    0xD5286A05, 0x01A90105, 0xBF82FFCB, 0x7E060209, 0xD70F6A02, 0x00020008,
    0xD5286A03, 0x01A90103, 0x7E0A020B, 0xD70F6A04, 0x0002000A, 0xD5286A05,
    0x01A90105, 0xD4E1006A, 0x00001902, 0xBF86000F, 0x87FE6A7E, 0xDC200000,
    0x017D0002, 0xD70F6A02, 0x00020410, 0xD5286A03, 0x01A90103, 0xBF8C3F70,
    0xDC600000, 0x007D0104, 0xD70F6A04, 0x00020810, 0xD5286A05, 0x01A90105,
    0xBF82FFEE, 0xBF810000,
};

static const unsigned int kCodeFill10[] = {
    0xF4080100, 0xFA000000, 0xF4080200, 0xFA000010, 0xBF8CC07F, 0x8F028602,
    0xD70F6A00, 0x00020002, 0x7E08020A, 0x7E0A020A, 0x7E0C020A, 0x7E0E020A,
    0x8F0C840B, 0x34020084, 0x7E060205, 0xD70F6A02, 0x00020204, 0xD5286A03,
    0x01A90103, 0xD4E1006A, 0x00000D02, 0xBF860007, 0xDC780000, 0x007D0402,
    0xD70F6A02, 0x0002040C, 0xD5286A03, 0x01A90103, 0xBF82FFF6, 0x8F0C820B,
    0x34020082, 0x7E060207, 0xD70F6A02, 0x00020206, 0xD5286A03, 0x01A90103,
    0xD4E1006A, 0x00001102, 0xBF860008, 0x87FE6A7E, 0xDC700000, 0x007D0402,
    0xD70F6A02, 0x0002040C, 0xD5286A03, 0x01A90103, 0xBF82FFF5, 0xBF810000,
};

static const unsigned int kCodeTrapHandler1010[] = {
    0x8a73ff73, 0x3e000000, 0x93eeff78, 0x0001000d, 0x8f6e9d6e, 0x88736e73,
    0x93eeff6d, 0x00080010, 0xbf850041, 0xb96ef803, 0x876fff6e, 0x00000900,
    0xbf850031, 0xbeee037e, 0xbeef037f, 0xbefe03ff, 0x80000000, 0xbf90000a,
    0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff036f, 0x876fff7e, 0x00000fff,
    0xbefe036e, 0xbeef1d96, 0xbeee037c, 0xbefc036f, 0xbf800000, 0xbf900001,
    0xbefc036e, 0xbf0d9f73, 0xbf85000f, 0x876fff6f, 0x000003ff, 0x8f6f836f,
    0xf4051bbd, 0xde000000, 0xbf8cc07f, 0xf4011bb7, 0xfa000008, 0xbf8cc07f,
    0x80ee6e72, 0x906e866e, 0x8a73ff73, 0x01ffffff, 0x88736e73, 0xbef31d9f,
    0xbef2036c, 0x876dff6d, 0x0000ffff, 0x8f6d876d, 0x8a77ff77, 0x007fff80,
    0x88776d77, 0xbeec1f00, 0x806cff6c, 0x00000010, 0x826d806d, 0xbf820044,
    0xbf920002, 0xbf82fffe, 0x876fff6e, 0x10000100, 0xbf06ff6f, 0x00000100,
    0xbeef03ff, 0x20000000, 0xbf850011, 0x876fff6e, 0x00000800, 0xbeef03f4,
    0xbf85000d, 0xbf820036, 0x83ef8f6e, 0x8f6f996f, 0x88736f73, 0xbf09836e,
    0xbf85ffbe, 0xbf06826e, 0xbeef03ff, 0x80000000, 0xbf850003, 0x806c846c,
    0x826d806d, 0xbf82002c, 0xbef0036f, 0xbeee037e, 0xbeef037f, 0xbefe03ff,
    0x80000000, 0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff036f,
    0x877eff7e, 0x000003ff, 0x8f6f837e, 0xbefe036e, 0xf4051bbd, 0xde000000,
    0xbf8cc07f, 0xf4051bb7, 0xfa0000c0, 0xbf8cc07f, 0xbef10380, 0xf6811c37,
    0xfa000008, 0xbf8cc07f, 0x88707170, 0xbf85000e, 0xf4051c37, 0xfa000010,
    0xbf8cc07f, 0x87f07070, 0xbf840009, 0xf4011bb7, 0xfa000018, 0xbf8cc07f,
    0xf4411bb8, 0xfa000000, 0xbf8cc07f, 0xbefc0380, 0xbf800000, 0xbf900001,
    0xbef00380, 0xbef10380, 0xbef31d9e, 0xbef81d8d, 0x906e8977, 0x876fff6e,
    0x003f8000, 0x906e8677, 0x876eff6e, 0x02000000, 0x886e6f6e, 0xb9eef807,
    0x87fe7e7e, 0x87ea6a6a, 0xb9f8f802, 0xbe80226c,
};

static const unsigned int kCodeTrapHandler10[] = {
    0x8a73ff73, 0x3e000000, 0x93eeff78, 0x0001000d, 0x8f6e9d6e, 0x88736e73,
    0x93eeff6d, 0x00080010, 0xbf850023, 0xb96ef803, 0x876fff6e, 0x00000900,
    0xbf850013, 0xbeee037e, 0xbeef037f, 0xbefe03ff, 0x80000000, 0xbf90000a,
    0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff036f, 0x876fff7e, 0x00000fff,
    0xbefe036e, 0xbeef1d96, 0xbeee037c, 0xbefc036f, 0xbf800000, 0xbf900001,
    0xbefc036e, 0xbf82001a, 0x876fff6e, 0x10000100, 0xbf06ff6f, 0x00000100,
    0xbeef03ff, 0x20000000, 0xbf850011, 0x876fff6e, 0x00000800, 0xbeef03f4,
    0xbf85000d, 0xbf82000e, 0x83ef8f6e, 0x8f6f996f, 0x88736f73, 0xbf09836e,
    0xbf85ffdc, 0xbf06826e, 0xbeef03ff, 0x80000000, 0xbf850003, 0x806c846c,
    0x826d806d, 0xbf820004, 0xbef00380, 0xbef10380, 0xbef31d9e, 0xbef81d8d,
    0x906e8977, 0x876fff6e, 0x003f8000, 0x906e8677, 0x876eff6e, 0x02000000,
    0x886e6f6e, 0xb9eef807, 0x87fe7e7e, 0x87ea6a6a, 0xb9f8f802, 0xbe80226c,
};

/*
.set SQ_WAVE_PC_HI_ADDRESS_MASK              , 0xFFFF
.set SQ_WAVE_PC_HI_HT_SHIFT                  , 24
.set SQ_WAVE_PC_HI_TRAP_ID_SHIFT             , 16
.set SQ_WAVE_PC_HI_TRAP_ID_SIZE              , 8
.set SQ_WAVE_PC_HI_TRAP_ID_BFE               , (SQ_WAVE_PC_HI_TRAP_ID_SHIFT | (SQ_WAVE_PC_HI_TRAP_ID_SIZE << 16))
.set SQ_WAVE_STATUS_HALT_SHIFT               , 13
.set SQ_WAVE_STATUS_HALT_BFE                 , (SQ_WAVE_STATUS_HALT_SHIFT | (1 << 16))
.set SQ_WAVE_TRAPSTS_MEM_VIOL_SHIFT          , 8
.set SQ_WAVE_TRAPSTS_ILLEGAL_INST_SHIFT      , 11
.set SQ_WAVE_TRAPSTS_XNACK_ERROR_SHIFT       , 28
.set SQ_WAVE_TRAPSTS_MATH_EXCP               , 0x7F
.set SQ_WAVE_MODE_EXCP_EN_SHIFT              , 12
.set TRAP_ID_ABORT                           , 2
.set TRAP_ID_DEBUGTRAP                       , 3
.set DOORBELL_ID_SIZE                        , 10
.set DOORBELL_ID_MASK                        , ((1 << DOORBELL_ID_SIZE) - 1)
.set EC_QUEUE_WAVE_ABORT_M0                  , (1 << (DOORBELL_ID_SIZE + 0))
.set EC_QUEUE_WAVE_TRAP_M0                   , (1 << (DOORBELL_ID_SIZE + 1))
.set EC_QUEUE_WAVE_MATH_ERROR_M0             , (1 << (DOORBELL_ID_SIZE + 2))
.set EC_QUEUE_WAVE_ILLEGAL_INSTRUCTION_M0    , (1 << (DOORBELL_ID_SIZE + 3))
.set EC_QUEUE_WAVE_MEMORY_VIOLATION_M0       , (1 << (DOORBELL_ID_SIZE + 4))
.set EC_QUEUE_WAVE_APERTURE_VIOLATION_M0     , (1 << (DOORBELL_ID_SIZE + 5))

.set TTMP6_WAVE_STOPPED_SHIFT                , 30
.set TTMP6_SAVED_STATUS_HALT_SHIFT           , 29
.set TTMP6_SAVED_STATUS_HALT_MASK            , (1 << TTMP6_SAVED_STATUS_HALT_SHIFT)
.set TTMP6_SAVED_TRAP_ID_SHIFT               , 25
.set TTMP6_SAVED_TRAP_ID_SIZE                , 4
.set TTMP6_SAVED_TRAP_ID_MASK                , (((1 << TTMP6_SAVED_TRAP_ID_SIZE) - 1) << TTMP6_SAVED_TRAP_ID_SHIFT)
.set TTMP6_SAVED_TRAP_ID_BFE                 , (TTMP6_SAVED_TRAP_ID_SHIFT | (TTMP6_SAVED_TRAP_ID_SIZE << 16))
.set TTMP11_PC_HI_SHIFT                      , 7
.set TTMP11_DEBUG_ENABLED_SHIFT              , 23

.if .amdgcn.gfx_generation_number == 9
  .set TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT   , 26
  .set SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     , 15
  .set SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK , 0x1F8000
.elseif .amdgcn.gfx_generation_number == 10 && .amdgcn.gfx_generation_minor < 3
  .set TTMP11_SAVE_REPLAY_W64H_SHIFT         , 31
  .set TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT   , 24
  .set SQ_WAVE_IB_STS_REPLAY_W64H_SHIFT      , 25
  .set SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     , 15
  .set SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK , 0x3F8000
  .set SQ_WAVE_IB_STS_REPLAY_W64H_MASK       , 0x2000000
.endif

// ABI between first and second level trap handler:
//   ttmp0 = PC[31:0]
//   ttmp12 = SQ_WAVE_STATUS
//   ttmp14 = TMA[31:0]
//   ttmp15 = TMA[63:32]
// gfx9:
//   ttmp1 = 0[2:0], PCRewind[3:0], HostTrap[0], TrapId[7:0], PC[47:32]
//   ttmp11 = SQ_WAVE_IB_STS[20:15], 0[1:0], DebugEnabled[0], 0[15:0], NoScratch[0], WaveIdInWG[5:0]
// gfx10:
//   ttmp1 = 0[0], PCRewind[5:0], HostTrap[0], TrapId[7:0], PC[47:32]
// gfx1010:
//   ttmp11 = SQ_WAVE_IB_STS[25], SQ_WAVE_IB_STS[21:15], DebugEnabled[0], 0[15:0], NoScratch[0], WaveIdInWG[5:0]
// gfx1030:
//   ttmp11 = 0[7:0], DebugEnabled[0], 0[15:0], NoScratch[0], WaveIdInWG[5:0]

trap_entry:
  // Branch if not a trap (an exception instead).
  s_bfe_u32            ttmp2, ttmp1, SQ_WAVE_PC_HI_TRAP_ID_BFE
  s_cbranch_scc0       .no_skip_debugtrap

  // If caused by s_trap then advance PC.
  s_bitcmp1_b32        ttmp1, SQ_WAVE_PC_HI_HT_SHIFT
  s_cbranch_scc1       .not_s_trap
  s_add_u32            ttmp0, ttmp0, 0x4
  s_addc_u32           ttmp1, ttmp1, 0x0

.not_s_trap:
  // If llvm.debugtrap and debugger is not attached.
  s_cmp_eq_u32         ttmp2, TRAP_ID_DEBUGTRAP
  s_cbranch_scc0       .no_skip_debugtrap
  s_bitcmp0_b32        ttmp11, TTMP11_DEBUG_ENABLED_SHIFT
  s_cbranch_scc0       .no_skip_debugtrap

  // Ignore llvm.debugtrap.
  s_branch             .exit_trap

.no_skip_debugtrap:
  // Save trap id and halt status in ttmp6.
  s_andn2_b32          ttmp6, ttmp6, (TTMP6_SAVED_TRAP_ID_MASK | TTMP6_SAVED_STATUS_HALT_MASK)
  s_min_u32            ttmp2, ttmp2, 0xF
  s_lshl_b32           ttmp2, ttmp2, TTMP6_SAVED_TRAP_ID_SHIFT
  s_or_b32             ttmp6, ttmp6, ttmp2
  s_bfe_u32            ttmp2, ttmp12, SQ_WAVE_STATUS_HALT_BFE
  s_lshl_b32           ttmp2, ttmp2, TTMP6_SAVED_STATUS_HALT_SHIFT
  s_or_b32             ttmp6, ttmp6, ttmp2

  // Fetch doorbell id for our queue.
  s_mov_b32            ttmp2, exec_lo
  s_mov_b32            ttmp3, exec_hi
  s_mov_b32            exec_lo, 0x80000000
  s_sendmsg            sendmsg(MSG_GET_DOORBELL)
.wait_sendmsg:
  s_nop                0x7
  s_bitcmp0_b32        exec_lo, 0x1F
  s_cbranch_scc0       .wait_sendmsg
  s_mov_b32            exec_hi, ttmp3

  // Restore exec_lo, move the doorbell_id into ttmp3
  s_and_b32            ttmp3, exec_lo, DOORBELL_ID_MASK
  s_mov_b32            exec_lo, ttmp2

  // Map trap reason to an exception code.
  s_getreg_b32         ttmp2, hwreg(HW_REG_TRAPSTS)

  s_bitcmp1_b32        ttmp2, SQ_WAVE_TRAPSTS_XNACK_ERROR_SHIFT
  s_cbranch_scc0       .not_memory_violation
  s_or_b32             ttmp3, ttmp3, EC_QUEUE_WAVE_MEMORY_VIOLATION_M0

  // Aperture violation requires XNACK_ERROR == 0.
  s_branch             .not_aperture_violation

.not_memory_violation:
  s_bitcmp1_b32        ttmp2, SQ_WAVE_TRAPSTS_MEM_VIOL_SHIFT
  s_cbranch_scc0       .not_aperture_violation
  s_or_b32             ttmp3, ttmp3, EC_QUEUE_WAVE_APERTURE_VIOLATION_M0

.not_aperture_violation:
  s_bitcmp1_b32        ttmp2, SQ_WAVE_TRAPSTS_ILLEGAL_INST_SHIFT
  s_cbranch_scc0       .not_illegal_instruction
  s_or_b32             ttmp3, ttmp3, EC_QUEUE_WAVE_ILLEGAL_INSTRUCTION_M0

.not_illegal_instruction:
  s_and_b32            ttmp2, ttmp2, SQ_WAVE_TRAPSTS_MATH_EXCP
  s_cbranch_scc0       .not_math_exception
  s_getreg_b32         ttmp7, hwreg(HW_REG_MODE)
  s_lshl_b32           ttmp2, ttmp2, SQ_WAVE_MODE_EXCP_EN_SHIFT
  s_and_b32            ttmp2, ttmp2, ttmp7
  s_cbranch_scc0       .not_math_exception
  s_or_b32             ttmp3, ttmp3, EC_QUEUE_WAVE_MATH_ERROR_M0

.not_math_exception:
  s_bfe_u32            ttmp2, ttmp6, TTMP6_SAVED_TRAP_ID_BFE
  s_cmp_eq_u32         ttmp2, TRAP_ID_ABORT
  s_cbranch_scc0       .not_abort_trap
  s_or_b32             ttmp3, ttmp3, EC_QUEUE_WAVE_ABORT_M0

.not_abort_trap:
  // If no other exception was flagged then report a generic error.
  s_andn2_b32          ttmp2, ttmp3, DOORBELL_ID_MASK
  s_cbranch_scc1       .send_interrupt
  s_or_b32             ttmp3, ttmp3, EC_QUEUE_WAVE_TRAP_M0

.send_interrupt:
  // m0 = interrupt data = (exception_code << DOORBELL_ID_SIZE) | doorbell_id
  s_mov_b32            ttmp2, m0
  s_mov_b32            m0, ttmp3
  s_nop                0x0 // Manually inserted wait states
  s_sendmsg            sendmsg(MSG_INTERRUPT)
  s_mov_b32            m0, ttmp2

  // Parking the wave requires saving the original pc in the preserved ttmps.
  // Register layout before parking the wave:
  //
  // ttmp7: 0[31:0]
  // ttmp11: 1st_level_ttmp11[31:23] 0[15:0] 1st_level_ttmp11[6:0]
  //
  // After parking the wave:
  //
  // ttmp7:  pc_lo[31:0]
  // ttmp11: 1st_level_ttmp11[31:23] pc_hi[15:0] 1st_level_ttmp11[6:0]

.if ((.amdgcn.gfx_generation_number == 10 && .amdgcn.gfx_generation_minor >= 3) || .amdgcn.gfx_generation_number > 10)
  s_branch             .halt_wave
.else
  // Save the PC
  s_mov_b32            ttmp7, ttmp0
  s_and_b32            ttmp1, ttmp1, SQ_WAVE_PC_HI_ADDRESS_MASK
  s_lshl_b32           ttmp1, ttmp1, TTMP11_PC_HI_SHIFT
  s_andn2_b32          ttmp11, ttmp11, (SQ_WAVE_PC_HI_ADDRESS_MASK << TTMP11_PC_HI_SHIFT)
  s_or_b32             ttmp11, ttmp11, ttmp1

  // Park the wave
  s_getpc_b64          [ttmp0, ttmp1]
  s_add_u32            ttmp0, ttmp0, .parked - .
  s_addc_u32           ttmp1, ttmp1, 0x0
  s_branch             .halt_wave

.parked:
  s_trap               0x2
  s_branch             .parked
.endif

.halt_wave:
  // Halt the wavefront upon restoring STATUS below.
  s_bitset1_b32        ttmp6, TTMP6_WAVE_STOPPED_SHIFT
  s_bitset1_b32        ttmp12, SQ_WAVE_STATUS_HALT_SHIFT

.exit_trap:
  // Restore SQ_WAVE_IB_STS.
.if .amdgcn.gfx_generation_number == 9
  s_lshr_b32           ttmp2, ttmp11, (TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
  s_and_b32            ttmp2, ttmp2, SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK
  s_setreg_b32         hwreg(HW_REG_IB_STS), ttmp2
.endif
.if .amdgcn.gfx_generation_number == 10 && .amdgcn.gfx_generation_minor < 3
  s_lshr_b32           ttmp2, ttmp11, (TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
  s_and_b32            ttmp3, ttmp2, SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK
  s_lshr_b32           ttmp2, ttmp11, (TTMP11_SAVE_REPLAY_W64H_SHIFT - SQ_WAVE_IB_STS_REPLAY_W64H_SHIFT)
  s_and_b32            ttmp2, ttmp2, SQ_WAVE_IB_STS_REPLAY_W64H_MASK
  s_or_b32             ttmp2, ttmp2, ttmp3
  s_setreg_b32         hwreg(HW_REG_IB_STS), ttmp2
.endif

  // Restore SQ_WAVE_STATUS.
  s_and_b64            exec, exec, exec // Restore STATUS.EXECZ, not writable by s_setreg_b32
  s_and_b64            vcc, vcc, vcc    // Restore STATUS.VCCZ, not writable by s_setreg_b32
  s_setreg_b32         hwreg(HW_REG_STATUS), ttmp12

  // Return to original (possibly modified) PC.
  s_rfe_b64            [ttmp0, ttmp1]
*/

static const unsigned int kCodeTrapHandlerV2_9[] = {
    0x92eeff6d, 0x00080010, 0xbf840009, 0xbf0d986d, 0xbf850002, 0x806c846c,
    0x826d806d, 0xbf06836e, 0xbf840003, 0xbf0c9777, 0xbf840001, 0xbf82004c,
    0x8972ff72, 0x3e000000, 0x83ee8f6e, 0x8e6e996e, 0x87726e72, 0x92eeff78,
    0x0001000d, 0x8e6e9d6e, 0x87726e72, 0xbeee007e, 0xbeef007f, 0xbefe00ff,
    0x80000000, 0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff006f,
    0x866fff7e, 0x000003ff, 0xbefe006e, 0xb8eef803, 0xbf0d9c6e, 0xbf840003,
    0x876fff6f, 0x00004000, 0xbf820004, 0xbf0d886e, 0xbf840002, 0x876fff6f,
    0x00008000, 0xbf0d8b6e, 0xbf840002, 0x876fff6f, 0x00002000, 0x866eff6e,
    0x0000007f, 0xbf840006, 0xb8f3f801, 0x8e6e8c6e, 0x866e736e, 0xbf840002,
    0x876fff6f, 0x00001000, 0x92eeff72, 0x00040019, 0xbf06826e, 0xbf840002,
    0x876fff6f, 0x00000400, 0x896eff6f, 0x000003ff, 0xbf850002, 0x876fff6f,
    0x00000800, 0xbeee007c, 0xbefc006f, 0xbf800000, 0xbf900001, 0xbefc006e,
    0xbef3006c, 0x866dff6d, 0x0000ffff, 0x8e6d876d, 0x8977ff77, 0x007fff80,
    0x87776d77, 0xbeec1c00, 0x806cff6c, 0x00000010, 0x826d806d, 0xbf820002,
    0xbf920002, 0xbf82fffe, 0xbef21a9e, 0xbef81a8d, 0x8f6e8b77, 0x866eff6e,
    0x001f8000, 0xb96ef807, 0x86fe7e7e, 0x86ea6a6a, 0xb978f802, 0xbe801f6c,
};

static const unsigned int kCodeTrapHandlerV2_1010[] = {
    0x93eeff6d, 0x00080010, 0xbf840009, 0xbf0d986d, 0xbf850002, 0x806c846c,
    0x826d806d, 0xbf06836e, 0xbf840003, 0xbf0c9777, 0xbf840001, 0xbf82004c,
    0x8a72ff72, 0x3e000000, 0x83ee8f6e, 0x8f6e996e, 0x88726e72, 0x93eeff78,
    0x0001000d, 0x8f6e9d6e, 0x88726e72, 0xbeee037e, 0xbeef037f, 0xbefe03ff,
    0x80000000, 0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff036f,
    0x876fff7e, 0x000003ff, 0xbefe036e, 0xb96ef803, 0xbf0d9c6e, 0xbf840003,
    0x886fff6f, 0x00004000, 0xbf820004, 0xbf0d886e, 0xbf840002, 0x886fff6f,
    0x00008000, 0xbf0d8b6e, 0xbf840002, 0x886fff6f, 0x00002000, 0x876eff6e,
    0x0000007f, 0xbf840006, 0xb973f801, 0x8f6e8c6e, 0x876e736e, 0xbf840002,
    0x886fff6f, 0x00001000, 0x93eeff72, 0x00040019, 0xbf06826e, 0xbf840002,
    0x886fff6f, 0x00000400, 0x8a6eff6f, 0x000003ff, 0xbf850002, 0x886fff6f,
    0x00000800, 0xbeee037c, 0xbefc036f, 0xbf800000, 0xbf900001, 0xbefc036e,
    0xbef3036c, 0x876dff6d, 0x0000ffff, 0x8f6d876d, 0x8a77ff77, 0x007fff80,
    0x88776d77, 0xbeec1f00, 0x806cff6c, 0x00000010, 0x826d806d, 0xbf820002,
    0xbf920002, 0xbf82fffe, 0xbef21d9e, 0xbef81d8d, 0x906e8977, 0x876fff6e,
    0x003f8000, 0x906e8677, 0x876eff6e, 0x02000000, 0x886e6f6e, 0xb9eef807,
    0x87fe7e7e, 0x87ea6a6a, 0xb9f8f802, 0xbe80226c,
};

static const unsigned int kCodeTrapHandlerV2_10[] = {
    0x93eeff6d, 0x00080010, 0xbf840009, 0xbf0d986d, 0xbf850002, 0x806c846c,
    0x826d806d, 0xbf06836e, 0xbf840003, 0xbf0c9777, 0xbf840001, 0xbf82003f,
    0x8a72ff72, 0x3e000000, 0x83ee8f6e, 0x8f6e996e, 0x88726e72, 0x93eeff78,
    0x0001000d, 0x8f6e9d6e, 0x88726e72, 0xbeee037e, 0xbeef037f, 0xbefe03ff,
    0x80000000, 0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff036f,
    0x876fff7e, 0x000003ff, 0xbefe036e, 0xb96ef803, 0xbf0d9c6e, 0xbf840003,
    0x886fff6f, 0x00004000, 0xbf820004, 0xbf0d886e, 0xbf840002, 0x886fff6f,
    0x00008000, 0xbf0d8b6e, 0xbf840002, 0x886fff6f, 0x00002000, 0x876eff6e,
    0x0000007f, 0xbf840006, 0xb973f801, 0x8f6e8c6e, 0x876e736e, 0xbf840002,
    0x886fff6f, 0x00001000, 0x93eeff72, 0x00040019, 0xbf06826e, 0xbf840002,
    0x886fff6f, 0x00000400, 0x8a6eff6f, 0x000003ff, 0xbf850002, 0x886fff6f,
    0x00000800, 0xbeee037c, 0xbefc036f, 0xbf800000, 0xbf900001, 0xbefc036e,
    0xbf820000, 0xbef21d9e, 0xbef81d8d, 0x87fe7e7e, 0x87ea6a6a, 0xb9f8f802,
    0xbe80226c,
};


}  // namespace amd
}  // namespace rocr

#endif  // header guard
