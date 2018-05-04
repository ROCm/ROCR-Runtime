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
  var SQ_WAVE_PC_HI_TRAP_ID_SHIFT           = 16
  var SQ_WAVE_PC_HI_TRAP_ID_SIZE            = 8
  var SQ_WAVE_PC_HI_TRAP_ID_BFE             = (SQ_WAVE_PC_HI_TRAP_ID_SHIFT | (SQ_WAVE_PC_HI_TRAP_ID_SIZE << 16))
  var SQ_WAVE_STATUS_HALT_MASK              = 0x2000
  var SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK = 0x8000
  var SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     = 15
  var IB_STS_SAVE_RCNT_FIRST_REPLAY_SHIFT   = 26

  // ABI between first and second level trap handler.
  var s_trap_info_lo = ttmp0
  var s_trap_info_hi = ttmp1
  var s_ib_sts_save  = ttmp11 // [31:26] = SQ_WAVE_IB_STS[20:15]
  var s_status_save  = ttmp12

  // SPI debug data is not present/needed in these registers.
  var s_tmp0         = ttmp2
  var s_tmp1         = ttmp3
  var s_tmp2         = ttmp4
  var s_tmp3         = ttmp5

  shader main
    type(CS)

    // If this is not a trap then return to the shader.
    s_bfe_u32            s_tmp0, s_trap_info_hi, SQ_WAVE_PC_HI_TRAP_ID_BFE
    s_cbranch_scc0       L_EXIT_TRAP

    // If llvm.trap then signal queue error.
    s_cmp_eq_u32         s_tmp0, 0x2
    s_cbranch_scc1       L_SIGNAL_QUEUE

    // For other traps advance PC and return to shader.
    s_add_u32            s_trap_info_lo, s_trap_info_lo, 0x4
    s_addc_u32           s_trap_info_hi, s_trap_info_hi, 0x0
    s_branch             L_EXIT_TRAP

  L_SIGNAL_QUEUE:
    // Retrieve queue_inactive_signal from amd_queue_t* passed in s[0:1].
    s_load_dwordx2       [s_tmp0, s_tmp1], s[0:1], 0xC0 glc:1
    s_waitcnt            lgkmcnt(0)

    // Set queue signal value to unhandled exception error.
    s_mov_b32            s_tmp2, 0x80000000
    s_mov_b32            s_tmp3, 0x0
    s_atomic_swap_x2     [s_tmp2, s_tmp3], [s_tmp0, s_tmp1], 0x8 glc:1
    s_waitcnt            lgkmcnt(0)

    // Skip event trigger if the signal value was already non-zero.
    s_or_b32             s_tmp2, s_tmp2, s_tmp3
    s_cbranch_scc1       L_SIGNAL_DONE

    // Check for a non-NULL signal event mailbox.
    s_load_dwordx2       [s_tmp2, s_tmp3], [s_tmp0, s_tmp1], 0x10 glc:1
    s_waitcnt            lgkmcnt(0)
    s_and_b64            [s_tmp2, s_tmp3], [s_tmp2, s_tmp3], [s_tmp2, s_tmp3]
    s_cbranch_scc0       L_SIGNAL_DONE

    // Load the signal event value.
    s_load_dword         s_tmp0, [s_tmp0, s_tmp1], 0x18 glc:1
    s_waitcnt            lgkmcnt(0)

    // Write the signal event value to the mailbox.
    s_store_dword        s_tmp0, [s_tmp2, s_tmp3], 0x0 glc:1
    s_waitcnt            lgkmcnt(0)

    // Send an interrupt to trigger event notification.
    s_sendmsg            sendmsg(MSG_INTERRUPT)

  L_SIGNAL_DONE:
    // Halt the wavefront.
    s_or_b32             s_status_save, s_status_save, SQ_WAVE_STATUS_HALT_MASK

  L_EXIT_TRAP:
    // Restore SQ_WAVE_IB_STS.
    s_lshr_b32           s_tmp0, s_ib_sts_save, (IB_STS_SAVE_RCNT_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
    s_and_b32            s_tmp0, s_tmp0, SQ_WAVE_IB_STS_RCNT_FIRST_REPLAY_MASK
    s_setreg_b32         hwreg(HW_REG_IB_STS), s_tmp0

    // Restore SQ_WAVE_STATUS.
    s_and_b64            exec, exec, exec // Restore STATUS.EXECZ, not writable by s_setreg_b32
    s_and_b64            vcc, vcc, vcc    // Restore STATUS.VCCZ, not writable by s_setreg_b32
    s_setreg_b32         hwreg(HW_REG_STATUS), s_status_save

    // Return to shader at unmodified PC.
    s_rfe_b64            [s_trap_info_lo, s_trap_info_hi]
  end
*/
    0x92eeff6d, 0x00080010, 0xbf84001e, 0xbf06826e, 0xbf850003, 0x806c846c,
    0x826d806d, 0xbf820019, 0xc0071b80, 0x000000c0, 0xbf8cc07f, 0xbef000ff,
    0x80000000, 0xbef10080, 0xc2831c37, 0x00000008, 0xbf8cc07f, 0x87707170,
    0xbf85000c, 0xc0071c37, 0x00000010, 0xbf8cc07f, 0x86f07070, 0xbf840007,
    0xc0031bb7, 0x00000018, 0xbf8cc07f, 0xc0431bb8, 0x00000000, 0xbf8cc07f,
    0xbf900001, 0x8778ff78, 0x00002000, 0x8f6e8b77, 0x866eff6e, 0x00008000,
    0xb96ef807, 0x86fe7e7e, 0x86ea6a6a, 0xb978f802, 0xbe801f6c, 0x00000000,
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
