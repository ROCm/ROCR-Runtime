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

  .if .amdgcn.gfx_generation_number == 9
    .set TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT   , 26
    .set SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     , 15
    .set SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK , 0x1F8000
  .else
    .error "unsupported target"
  .endif

  trap_entry:
    // If memory violation without XNACK error then signal queue error.
    // XNACK error will be handled by VM interrupt, since it has more information.
    s_getreg_b32         ttmp2, hwreg(HW_REG_TRAPSTS)
    s_and_b32            ttmp4, ttmp2, (SQ_WAVE_TRAPSTS_MEM_VIOL_MASK | SQ_WAVE_TRAPSTS_XNACK_ERROR_MASK)
    s_cmp_eq_u32         ttmp4, SQ_WAVE_TRAPSTS_MEM_VIOL_MASK
    s_mov_b32            ttmp4, SIGNAL_CODE_MEM_VIOL
    s_cbranch_scc1       .signal_error

    // If illegal instruction then signal queue error.
    s_and_b32            ttmp4, ttmp2, SQ_WAVE_TRAPSTS_ILLEGAL_INST_MASK
    s_mov_b32            ttmp4, SIGNAL_CODE_ILLEGAL_INST
    s_cbranch_scc1       .signal_error

    // If any other exception then return to shader.
    s_bfe_u32            ttmp2, ttmp1, SQ_WAVE_PC_HI_TRAP_ID_BFE
    s_cbranch_scc0       .exit_trap

    // If llvm.trap then signal queue error.
    s_cmp_eq_u32         ttmp2, 0x2
    s_mov_b32            ttmp4, SIGNAL_CODE_LLVM_TRAP
    s_cbranch_scc1       .signal_error

    // For other traps advance PC and return to shader.
    s_add_u32            ttmp0, ttmp0, 0x4
    s_addc_u32           ttmp1, ttmp1, 0x0
    s_branch             .exit_trap

  .signal_error:
    // Fetch doorbell index for our queue.
    s_mov_b32            exec_lo, 0x80000000
    s_sendmsg            sendmsg(MSG_GET_DOORBELL)
  .wait_sendmsg:
    s_nop                7
    s_bitcmp0_b32        exec_lo, 0x1F
    s_cbranch_scc0       .wait_sendmsg

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
    s_cbranch_scc1       .signal_done

    // Check for a non-NULL signal event mailbox.
    s_load_dwordx2       [ttmp4, ttmp5], [ttmp2, ttmp3], 0x10 glc
    s_waitcnt            lgkmcnt(0)
    s_and_b64            [ttmp4, ttmp5], [ttmp4, ttmp5], [ttmp4, ttmp5]
    s_cbranch_scc0       .signal_done

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

  .signal_done:
    // Halt the wavefront.
    s_or_b32             ttmp12, ttmp12, SQ_WAVE_STATUS_HALT_MASK

  .exit_trap:
    // Restore SQ_WAVE_IB_STS.
  .if .amdgcn.gfx_generation_number == 9
    s_lshr_b32           ttmp2, ttmp11, (TTMP11_SAVE_RCNT_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
    s_and_b32            ttmp2, ttmp2, SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK
    s_setreg_b32         hwreg(HW_REG_IB_STS), ttmp2
  .endif

    // Restore SQ_WAVE_STATUS.
    s_and_b64            exec, exec, exec // Restore STATUS.EXECZ, not writable by s_setreg_b32
    s_and_b64            vcc, vcc, vcc    // Restore STATUS.VCCZ, not writable by s_setreg_b32
    s_setreg_b32         hwreg(HW_REG_STATUS), ttmp12

    // Return to shader at unmodified PC.
    s_rfe_b64            [ttmp0, ttmp1]
*/
    0xb8eef803, 0x8670ff6e, 0x10000100, 0xbf06ff70, 0x00000100, 0xbef000ff,
    0x20000000, 0xbf85000e, 0x8670ff6e, 0x00000800, 0xbef000f4, 0xbf85000a,
    0x92eeff6d, 0x00080010, 0xbf84002c, 0xbf06826e, 0xbef000ff, 0x80000000,
    0xbf850003, 0x806c846c, 0x826d806d, 0xbf820025, 0xbefe00ff, 0x80000000,
    0xbf90000a, 0xbf800007, 0xbf0c9f7e, 0xbf84fffd, 0x866eff7e, 0x000003ff,
    0x8e6e836e, 0xc0051bbd, 0x0000006e, 0xbf8cc07f, 0xc0071bb7, 0x000000c0,
    0xbf8cc07f, 0xbef10080, 0xc2831c37, 0x00000008, 0xbf8cc07f, 0x87707170,
    0xbf85000e, 0xc0071c37, 0x00000010, 0xbf8cc07f, 0x86f07070, 0xbf840009,
    0xc0031bb7, 0x00000018, 0xbf8cc07f, 0xc0431bb8, 0x00000000, 0xbf8cc07f,
    0xbefc0080, 0xbf800000, 0xbf900001, 0x8778ff78, 0x00002000, 0x8f6e8b77,
    0x866eff6e, 0x001f8000, 0xb96ef807, 0x86fe7e7e, 0x86ea6a6a, 0xb978f802,
    0xbe801f6c,
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

}  // namespace amd

#endif  // header guard
