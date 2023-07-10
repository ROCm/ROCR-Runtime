////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2022, Advanced Micro Devices, Inc. All rights reserved.
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

/// Trap Handler V2 source
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

.set TTMP_PC_HI_SHIFT                        , 7
.set TTMP_DEBUG_ENABLED_SHIFT                , 23

.if .amdgcn.gfx_generation_number == 9
  .set TTMP_SAVE_RCNT_FIRST_REPLAY_SHIFT     , 26
  .set SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     , 15
  .set SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK , 0x1F8000
.elseif .amdgcn.gfx_generation_number == 10 && .amdgcn.gfx_generation_minor < 3
  .set TTMP_SAVE_REPLAY_W64H_SHIFT           , 31
  .set TTMP_SAVE_RCNT_FIRST_REPLAY_SHIFT     , 24
  .set SQ_WAVE_IB_STS_REPLAY_W64H_SHIFT      , 25
  .set SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     , 15
  .set SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK , 0x3F8000
  .set SQ_WAVE_IB_STS_REPLAY_W64H_MASK       , 0x2000000
.endif

.if .amdgcn.gfx_generation_number == 9 && .amdgcn.gfx_generation_minor >= 4
  .set TTMP11_TTMPS_SETUP_SHIFT              , 31
.endif

// ABI between first and second level trap handler:
//   ttmp0 = PC[31:0]
//   ttmp12 = SQ_WAVE_STATUS
//   ttmp14 = TMA[31:0]
//   ttmp15 = TMA[63:32]
// gfx9:
//   ttmp1 = 0[2:0], PCRewind[3:0], HostTrap[0], TrapId[7:0], PC[47:32]
// gfx906/gfx908/gfx90a:
//   ttmp11 = SQ_WAVE_IB_STS[20:15], 0[1:0], DebugEnabled[0], 0[15:0], NoScratch[0], WaveIdInWG[5:0]
// gfx940/gfx941/gfx942:
//   ttmp13 = SQ_WAVE_IB_STS[20:15], 0[1:0], DebugEnabled[0], 0[22:0]
// gfx10:
//   ttmp1 = 0[0], PCRewind[5:0], HostTrap[0], TrapId[7:0], PC[47:32]
// gfx1010:
//   ttmp11 = SQ_WAVE_IB_STS[25], SQ_WAVE_IB_STS[21:15], DebugEnabled[0], 0[15:0], NoScratch[0], WaveIdInWG[5:0]
// gfx1030/gfx1100:
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
.if (.amdgcn.gfx_generation_number == 9 && .amdgcn.gfx_generation_minor < 4) || .amdgcn.gfx_generation_number == 10
  s_bitcmp0_b32        ttmp11, TTMP_DEBUG_ENABLED_SHIFT
.else
  s_bitcmp0_b32        ttmp13, TTMP_DEBUG_ENABLED_SHIFT
.endif
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
.if .amdgcn.gfx_generation_number < 11
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
.else
  s_sendmsg_rtn_b32    ttmp3, sendmsg(MSG_RTN_GET_DOORBELL)
  s_waitcnt            lgkmcnt(0)
  s_and_b32            ttmp3, ttmp3, DOORBELL_ID_MASK
.endif

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
  s_waitcnt            lgkmcnt(0) // Wait for the message to go out.
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
.if (.amdgcn.gfx_generation_number == 9 && .amdgcn.gfx_generation_minor < 4) || (.amdgcn.gfx_generation_number == 10 && .amdgcn.gfx_generation_minor < 3) || (.amdgcn.gfx_generation_number == 11)
  // Save the PC
  s_mov_b32            ttmp7, ttmp0
  s_and_b32            ttmp1, ttmp1, SQ_WAVE_PC_HI_ADDRESS_MASK
  s_lshl_b32           ttmp1, ttmp1, TTMP_PC_HI_SHIFT
  s_andn2_b32          ttmp11, ttmp11, (SQ_WAVE_PC_HI_ADDRESS_MASK << TTMP_PC_HI_SHIFT)
  s_or_b32             ttmp11, ttmp11, ttmp1

  // Park the wave
  s_getpc_b64          [ttmp0, ttmp1]
  s_add_u32            ttmp0, ttmp0, .parked - .
  s_addc_u32           ttmp1, ttmp1, 0x0
.endif

.halt_wave:
  // Halt the wavefront upon restoring STATUS below.
  s_bitset1_b32        ttmp6, TTMP6_WAVE_STOPPED_SHIFT
  s_bitset1_b32        ttmp12, SQ_WAVE_STATUS_HALT_SHIFT

.if (.amdgcn.gfx_generation_number == 9 && .amdgcn.gfx_generation_minor >= 4)
  s_bitcmp1_b32        ttmp11, TTMP11_TTMPS_SETUP_SHIFT
  s_cbranch_scc1       .ttmps_initialized
  s_mov_b32            ttmp4, 0
  s_mov_b32            ttmp5, 0
  s_bitset1_b32        ttmp11, TTMP11_TTMPS_SETUP_SHIFT
.ttmps_initialized:
.endif

.exit_trap:
  // Restore SQ_WAVE_IB_STS.
.if .amdgcn.gfx_generation_number == 9
.if .amdgcn.gfx_generation_minor < 4
  s_lshr_b32           ttmp2, ttmp11, (TTMP_SAVE_RCNT_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
.else
  s_lshr_b32           ttmp2, ttmp13, (TTMP_SAVE_RCNT_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
.endif
  s_and_b32            ttmp2, ttmp2, SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK
  s_setreg_b32         hwreg(HW_REG_IB_STS), ttmp2
.elseif .amdgcn.gfx_generation_number == 10 && .amdgcn.gfx_generation_minor < 3
  s_lshr_b32           ttmp2, ttmp11, (TTMP_SAVE_RCNT_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
  s_and_b32            ttmp3, ttmp2, SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK
  s_lshr_b32           ttmp2, ttmp11, (TTMP_SAVE_REPLAY_W64H_SHIFT - SQ_WAVE_IB_STS_REPLAY_W64H_SHIFT)
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

.parked:
  s_trap               0x2
  s_branch             .parked

// For gfx11, add padding instructions so we can ensure instruction cache
// prefetch always has something to load.
.if .amdgcn.gfx_generation_number == 11
.rept (256 - ((. - trap_entry) % 64)) / 4
  s_code_end
.endr
.endif
