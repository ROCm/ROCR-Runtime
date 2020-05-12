////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2015, Advanced Micro Devices, Inc. All rights reserved.
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

namespace amd {

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
  .set SQ_WAVE_STATUS_HALT_MASK                , 0x2000
  .set SQ_WAVE_TRAPSTS_MEM_VIOL_MASK           , 0x100
  .set SQ_WAVE_TRAPSTS_ILLEGAL_INST_MASK       , 0x800
  .set SQ_WAVE_TRAPSTS_XNACK_ERROR_MASK        , 0x10000000
  .set SIGNAL_CODE_MEM_VIOL                    , (1 << 29)
  .set SIGNAL_CODE_ILLEGAL_INST                , (1 << 30)
  .set SIGNAL_CODE_LLVM_TRAP                   , (1 << 31)
  .set MAX_NUM_DOORBELLS_MASK                  , ((1 << 10) - 1)
  .set SENDMSG_M0_DOORBELL_ID_BITS             , 12
  .set SENDMSG_M0_DOORBELL_ID_MASK             , ((1 << SENDMSG_M0_DOORBELL_ID_BITS) - 1)
  .set TTMP11_TRAP_RAISED_BIT                  , 7
  .set TTMP11_EXCP_RAISED_BIT                  , 8
  .set TTMP11_EVENTS_MASK                      , (1 << TTMP11_TRAP_RAISED_BIT) | (1 << TTMP11_EXCP_RAISED_BIT)
  .set DEBUG_INTERRUPT_CONTEXT_ID_BIT          , 23
  .set INSN_S_ENDPGM_OPCODE                    , 0xBF810000
  .set INSN_S_ENDPGM_MASK                      , 0xFFFF0000

  .if .amdgcn.gfx_generation_number == 9
    .set TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT   , 26
    .set SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     , 15
    .set SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK , 0x1F8000
  .elseif .amdgcn.gfx_generation_number == 10
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
  //   ttmp11 = SQ_WAVE_IB_STS[20:15], 0[16:0], TrapRaised[0], ExcpRaised[0], NoScratch[0], WaveIdInWG[5:0]
  // gfx10:
  //   ttmp11 = SQ_WAVE_IB_STS[25], SQ_WAVE_IB_STS[21:15], 0[14:0], TrapRaised[0], ExcpRaised[0], NoScratch[0], WaveIdInWG[5:0]

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
    // If trap raised (non-zero trap id) then branch.
    s_bfe_u32            ttmp2, ttmp1, SQ_WAVE_PC_HI_TRAP_ID_BFE
    s_cbranch_scc1       .trap_raised

    // If non-masked exception raised then branch.
    s_getreg_b32         ttmp2, hwreg(HW_REG_TRAPSTS)
    s_and_b32            ttmp3, ttmp2, (SQ_WAVE_TRAPSTS_MEM_VIOL_MASK | SQ_WAVE_TRAPSTS_ILLEGAL_INST_MASK)
    s_cbranch_scc1       .excp_raised

    // Otherwise trap entered due to single step exception.
    s_branch             .signal_debugger

  .signal_trap_debugger:
    s_bitset1_b32        ttmp11, TTMP11_TRAP_RAISED_BIT

  .signal_debugger:
    // Fetch doorbell index for our queue.
    s_mov_b32            ttmp2, exec_lo
    s_mov_b32            ttmp3, exec_hi
    mGetDoorbellId
    s_mov_b32            exec_hi, ttmp3

    // Restore exec_lo, move the doorbell_id into ttmp3
    s_and_b32            exec_lo, exec_lo, SENDMSG_M0_DOORBELL_ID_MASK
    s_mov_b32            ttmp3, exec_lo
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

    // If PC is at an s_endpgm instruction then don't halt the wavefront.
    s_and_b32            ttmp1, ttmp1, SQ_WAVE_PC_HI_ADDRESS_MASK
    s_load_dword         ttmp2, [ttmp0, ttmp1]
    s_waitcnt            lgkmcnt(0)
    s_and_b32            ttmp2, ttmp2, INSN_S_ENDPGM_MASK
    s_cmp_eq_u32         ttmp2, INSN_S_ENDPGM_OPCODE
    s_cbranch_scc1       .skip_halt
    s_or_b32             ttmp12, ttmp12, SQ_WAVE_STATUS_HALT_MASK

  .skip_halt:
    mExitTrap

  .excp_raised:
    s_bitset1_b32       ttmp11, TTMP11_EXCP_RAISED_BIT

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
    // If debugger trap (s_trap >= 3) then signal debugger.
    s_cmp_ge_u32         ttmp2, 0x3;
    s_cbranch_scc1       .signal_trap_debugger

    // If llvm.trap (s_trap 2) then signal queue error.
    s_cmp_eq_u32         ttmp2, 0x2
    s_mov_b32            ttmp3, SIGNAL_CODE_LLVM_TRAP
    s_cbranch_scc1       .signal_trap_error

    // For other traps advance PC and return to shader.
    s_add_u32            ttmp0, ttmp0, 0x4
    s_addc_u32           ttmp1, ttmp1, 0x0
    s_branch             .exit_trap

  .signal_trap_error:
    s_bitset1_b32       ttmp11, TTMP11_TRAP_RAISED_BIT

  .signal_error:
    // FIXME: don't trash ttmp4/ttmp5 when exception handling is unified.
    s_mov_b32            ttmp4, ttmp3

    // Fetch doorbell index for our queue.
    mGetDoorbellId

    // Map doorbell index to amd_queue_t* through TMA (doorbell_queue_map).
    s_and_b32            ttmp2, exec_lo, MAX_NUM_DOORBELLS_MASK
    s_lshl_b32           ttmp2, ttmp2, 0x3
    s_load_dwordx2       [ttmp2, ttmp3], [ttmp14, ttmp15], ttmp2 glc
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
    s_cbranch_scc1       .halt_wave

    // Check for a non-NULL signal event mailbox.
    s_load_dwordx2       [ttmp4, ttmp5], [ttmp2, ttmp3], 0x10 glc
    s_waitcnt            lgkmcnt(0)
    s_and_b64            [ttmp4, ttmp5], [ttmp4, ttmp5], [ttmp4, ttmp5]
    s_cbranch_scc0       .halt_wave

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

  .halt_wave:
    // Halt the wavefront.
    s_or_b32             ttmp12, ttmp12, SQ_WAVE_STATUS_HALT_MASK

  .exit_trap:
    mExitTrap
*/
    0x92eeff6d, 0x00080010, 0xbf85003a, 0xb8eef803, 0x866fff6e, 0x00000900,
    0xbf850029, 0xbf820001, 0xbef71a87, 0xbeee007e, 0xbeef007f, 0xbefe00ff,
    0x80000000, 0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff006f,
    0x867eff7e, 0x00000fff, 0xbeef007e, 0xbefe006e, 0xbeef1a97, 0xbeee007c,
    0xbefc006f, 0xbf800000, 0xbf900001, 0xbefc006e, 0x866dff6d, 0x0000ffff,
    0xc0021bb6, 0x00000000, 0xbf8cc07f, 0x866eff6e, 0xffff0000, 0xbf06ff6e,
    0xbf810000, 0xbf850002, 0x8778ff78, 0x00002000, 0x8f6e8b77, 0x866eff6e,
    0x001f8000, 0xb96ef807, 0x86fe7e7e, 0x86ea6a6a, 0xb978f802, 0xbe801f6c,
    0xbef71a88, 0x866fff6e, 0x10000100, 0xbf06ff6f, 0x00000100, 0xbeef00ff,
    0x20000000, 0xbf85000f, 0x866fff6e, 0x00000800, 0xbeef00f4, 0xbf85000b,
    0xbf82002e, 0xbf09836e, 0xbf85ffc9, 0xbf06826e, 0xbeef00ff, 0x80000000,
    0xbf850003, 0x806c846c, 0x826d806d, 0xbf820027, 0xbef71a87, 0xbef0006f,
    0xbefe00ff, 0x80000000, 0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd,
    0x866eff7e, 0x000003ff, 0x8e6e836e, 0xc0051bbd, 0x0000006e, 0xbf8cc07f,
    0xc0071bb7, 0x000000c0, 0xbf8cc07f, 0xbef10080, 0xc2831c37, 0x00000008,
    0xbf8cc07f, 0x87707170, 0xbf85000e, 0xc0071c37, 0x00000010, 0xbf8cc07f,
    0x86f07070, 0xbf840009, 0xc0031bb7, 0x00000018, 0xbf8cc07f, 0xc0431bb8,
    0x00000000, 0xbf8cc07f, 0xbefc0080, 0xbf800000, 0xbf900001, 0x8778ff78,
    0x00002000, 0x8f6e8b77, 0x866eff6e, 0x001f8000, 0xb96ef807, 0x86fe7e7e,
    0x86ea6a6a, 0xb978f802, 0xbe801f6c,
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

static const unsigned int kCodeTrapHandler10[] = {
    0x93eeff6d, 0x00080010, 0xbf85003e, 0xb96ef803, 0x876fff6e, 0x00000900,
    0xbf85002d, 0xbf820001, 0xbef71d87, 0xbeee037e, 0xbeef037f, 0xbefe03ff,
    0x80000000, 0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0xbeff036f,
    0x877eff7e, 0x00000fff, 0xbeef037e, 0xbefe036e, 0xbeef1d97, 0xbeee037c,
    0xbefc036f, 0xbf800000, 0xbf900001, 0xbefc036e, 0x876dff6d, 0x0000ffff,
    0xf4001bb6, 0xfa000000, 0xbf8cc07f, 0x876eff6e, 0xffff0000, 0xbf06ff6e,
    0xbf810000, 0xbf850002, 0x8878ff78, 0x00002000, 0x906e8977, 0x876fff6e,
    0x003f8000, 0x906e8677, 0x876eff6e, 0x02000000, 0x886e6f6e, 0xb9eef807,
    0x87fe7e7e, 0x87ea6a6a, 0xb9f8f802, 0xbe80226c, 0xbef71d88, 0x876fff6e,
    0x10000100, 0xbf06ff6f, 0x00000100, 0xbeef03ff, 0x20000000, 0xbf85000f,
    0x876fff6e, 0x00000800, 0xbeef03f4, 0xbf85000b, 0xbf82002e, 0xbf09836e,
    0xbf85ffc5, 0xbf06826e, 0xbeef03ff, 0x80000000, 0xbf850003, 0x806c846c,
    0x826d806d, 0xbf820027, 0xbef71d87, 0xbef0036f, 0xbefe03ff, 0x80000000,
    0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0x876eff7e, 0x000003ff,
    0x8f6e836e, 0xf4051bbd, 0xdc000000, 0xbf8cc07f, 0xf4051bb7, 0xfa0000c0,
    0xbf8cc07f, 0xbef10380, 0xf6811c37, 0xfa000008, 0xbf8cc07f, 0x88707170,
    0xbf85000e, 0xf4051c37, 0xfa000010, 0xbf8cc07f, 0x87f07070, 0xbf840009,
    0xf4011bb7, 0xfa000018, 0xbf8cc07f, 0xf4411bb8, 0xfa000000, 0xbf8cc07f,
    0xbefc0380, 0xbf800000, 0xbf900001, 0x8878ff78, 0x00002000, 0x906e8977,
    0x876fff6e, 0x003f8000, 0x906e8677, 0x876eff6e, 0x02000000, 0x886e6f6e,
    0xb9eef807, 0x87fe7e7e, 0x87ea6a6a, 0xb9f8f802, 0xbe80226c,
};

}  // namespace amd

#endif  // header guard
