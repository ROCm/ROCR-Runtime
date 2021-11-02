/*
 * Copyright (C) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/**
 * Common
 */

const char *NoopIsa = R"(
        .text
        s_endpgm
)";

const char *CopyDwordIsa = R"(
        .text
        v_mov_b32 v0, s0
        v_mov_b32 v1, s1
        v_mov_b32 v2, s2
        v_mov_b32 v3, s3
        flat_load_dword v4, v[0:1] glc slc
        s_waitcnt 0
        flat_store_dword v[2:3], v4 glc slc
        s_endpgm
)";

const char *InfiniteLoopIsa = R"(
        .text
        LOOP:
        s_branch LOOP
        s_endpgm
)";

const char *AtomicIncIsa = R"(
        .text
        v_mov_b32 v0, s0
        v_mov_b32 v1, s1
        .if (.amdgcn.gfx_generation_number >= 8)
            v_mov_b32 v2, 1
            flat_atomic_add v3, v[0:1], v2 glc slc
        .else
            v_mov_b32 v2, -1
            flat_atomic_inc v3, v[0:1], v2 glc slc
        .endif
        s_waitcnt 0
        s_endpgm
)";

/**
 * KFDMemoryTest
 */

const char *ScratchCopyDwordIsa = R"(
        .text
        // Copy the parameters from scalar registers to vector registers
        .if (.amdgcn.gfx_generation_number >= 9)
            v_mov_b32 v0, s0
            v_mov_b32 v1, s1
            v_mov_b32 v2, s2
            v_mov_b32 v3, s3
        .else
            v_mov_b32_e32 v0, s0
            v_mov_b32_e32 v1, s1
            v_mov_b32_e32 v2, s2
            v_mov_b32_e32 v3, s3
        .endif
        // Setup the scratch parameters. This assumes a single 16-reg block
        .if (.amdgcn.gfx_generation_number >= 10)
            s_setreg_b32 hwreg(HW_REG_FLAT_SCR_LO), s4
            s_setreg_b32 hwreg(HW_REG_FLAT_SCR_HI), s5
        .elseif (.amdgcn.gfx_generation_number == 9)
            s_mov_b32 flat_scratch_lo, s4
            s_mov_b32 flat_scratch_hi, s5
        .else
            s_mov_b32 flat_scratch_lo, 8
            s_mov_b32 flat_scratch_hi, 0
        .endif
        // Copy a dword between the passed addresses
        flat_load_dword v4, v[0:1] slc
        s_waitcnt vmcnt(0) & lgkmcnt(0)
        flat_store_dword v[2:3], v4 slc
        s_endpgm
)";

/* Continuously poll src buffer and check buffer value
 * After src buffer is filled with specific value (0x5678,
 * by host program), fill dst buffer with specific
 * value(0x5678) and quit
 */
const char *PollMemoryIsa = R"(
        .text
        // Assume src address in s0, s1, and dst address in s2, s3
        s_movk_i32 s18, 0x5678
        .if (.amdgcn.gfx_generation_number >= 10)
            v_mov_b32 v0, s2
            v_mov_b32 v1, s3
            v_mov_b32 v2, 0x5678
        .endif
        LOOP:
        s_load_dword s16, s[0:1], 0x0 glc
        s_cmp_eq_i32 s16, s18
        s_cbranch_scc0   LOOP
        .if (.amdgcn.gfx_generation_number >= 10)
            flat_store_dword v[0:1], v2 slc
        .else
            s_store_dword s18, s[2:3], 0x0 glc
        .endif
        s_endpgm
)";

/* Similar to PollMemoryIsa except that the buffer
 * polled can be Non-coherant memory. SCC system-level
 * cache coherence is not supported in scalar (smem) path.
 * Use vmem operations with scc
 *
 * Note: Only works on Aldebaran, and even then the scc modifier
 *       has been defeatured. This shader is more or less
 *       deprecated.
 */
const char *PollNCMemoryIsa = R"(
        .text
        // Assume src address in s0, s1, and dst address in s2, s3
        v_mov_b32 v6, 0x5678
        v_mov_b32 v0, s0
        v_mov_b32 v1, s1
        LOOP:
        flat_load_dword v4, v[0:1] scc
        v_cmp_eq_u32 vcc, v4, v6
        s_cbranch_vccz   LOOP
        v_mov_b32 v0, s2
        v_mov_b32 v1, s3
        flat_store_dword v[0:1], v6 scc
        s_endpgm
)";

/* Input: A buffer of at least 3 dwords.
 * DW0: used as a signal. 0xcafe means it is signaled
 * DW1: Input buffer for device to read.
 * DW2: Output buffer for device to write.
 * Once receive signal, device will copy DW1 to DW2
 * This shader continously poll the signal buffer,
 * Once signal buffer is signaled, it copies input buffer
 * to output buffer
 */
const char *CopyOnSignalIsa = R"(
        .text
        // Assume input buffer in s0, s1
        .if (.amdgcn.gfx_generation_number >= 10)
            s_add_u32 s2, s0, 0x8
            s_addc_u32 s3, s1, 0x0
            s_mov_b32 s18, 0xcafe
            v_mov_b32 v0, s0
            v_mov_b32 v1, s1
            v_mov_b32 v4, s2
            v_mov_b32 v5, s3
        .else
            s_mov_b32 s18, 0xcafe
        .endif
        POLLSIGNAL:
        s_load_dword s16, s[0:1], 0x0 glc
        s_cmp_eq_i32 s16, s18
        s_cbranch_scc0   POLLSIGNAL
        s_load_dword s17, s[0:1], 0x4 glc
        s_waitcnt vmcnt(0) & lgkmcnt(0)
        .if (.amdgcn.gfx_generation_number >= 10)
            v_mov_b32 v2, s17
            flat_store_dword v[4:5], v2 glc
        .else
            s_store_dword s17, s[0:1], 0x8 glc
        .endif
        s_waitcnt vmcnt(0) & lgkmcnt(0)
        s_endpgm
)";

/* Continuously poll the flag at src buffer
 * After the flag of s[0:1] is 1 filled,
 * copy the value from s[0:1]+4 to dst buffer
 *
 * Note: Only works on GFX9 (only used in
 *       aldebaran tests)
 */
const char *PollAndCopyIsa = R"(
        .text
        // Assume src buffer in s[0:1] and dst buffer in s[2:3]
        .if (.amdgcn.gfx_generation_number == 9 && .amdgcn.gfx_generation_stepping == 10)
            // Path for Aldebaran
            v_mov_b32 v0, s0
            v_mov_b32 v1, s1
            v_mov_b32 v18, 0x1
            LOOP_ALDBRN:
            flat_load_dword v16, v[0:1] glc
            s_waitcnt vmcnt(0) & lgkmcnt(0)
            v_cmp_eq_i32 vcc, v16, v18
            s_cbranch_vccz   LOOP_ALDBRN
            buffer_invl2
            s_load_dword s17, s[0:1], 0x4 glc
            s_waitcnt vmcnt(0) & lgkmcnt(0)
            s_store_dword s17, s[2:3], 0x0 glc
            s_waitcnt vmcnt(0) & lgkmcnt(0)
            buffer_wbl2
        .else
            s_movk_i32 s18, 0x1
            LOOP:
            s_load_dword s16, s[0:1], 0x0 glc
            s_cmp_eq_i32 s16, s18
            s_cbranch_scc0   LOOP
            s_load_dword s17, s[0:1], 0x4 glc
            s_waitcnt vmcnt(0) & lgkmcnt(0)
            s_store_dword s17, s[2:3], 0x0 glc
        .endif
        s_waitcnt vmcnt(0) & lgkmcnt(0)
        s_endpgm
)";

/* Input0: A buffer of at least 2 dwords.
 * DW0: used as a signal. Write 0x1 to signal
 * DW1: Write the value from 2nd input buffer
 *      for other device to read.
 * Input1: A buffer of at least 2 dwords.
 * DW0: used as the value to be written.
 *
 * Note: Only works on Aldebaran
 */
const char *WriteFlagAndValueIsa = R"(
        .text
        // Assume two inputs buffer in s[0:1] and s[2:3]
        v_mov_b32 v0, s0
        v_mov_b32 v1, s1
        s_load_dword s18, s[2:3], 0x0 glc
        s_waitcnt vmcnt(0) & lgkmcnt(0)
        s_store_dword s18, s[0:1], 0x4 glc
        s_waitcnt vmcnt(0) & lgkmcnt(0)
        buffer_wbl2
        s_waitcnt vmcnt(0) & lgkmcnt(0)
        v_mov_b32 v16, 0x1
        flat_store_dword v[0:1], v16 glc
        s_endpgm
)";

/* Input0: A buffer of at least 2 dwords.
 * DW0: used as a signal. Write 0xcafe to signal
 * DW1: Write to this buffer for other device to read.
 * Input1: mmio base address
 */
const char *WriteAndSignalIsa = R"(
        .text
        // Assume input buffer in s0, s1
        .if (.amdgcn.gfx_generation_number >= 10)
            s_add_u32 s4, s0, 0x4
            s_addc_u32 s5, s1, 0x0
            v_mov_b32 v0, s0
            v_mov_b32 v1, s1
            v_mov_b32 v2, s2
            v_mov_b32 v3, s3
            v_mov_b32 v4, s4
            v_mov_b32 v5, s5
            v_mov_b32 v18, 0xbeef
            flat_store_dword v[4:5], v18 glc
            v_mov_b32 v18, 0x1
            flat_store_dword v[2:3], v18 glc
            v_mov_b32 v18, 0xcafe
            flat_store_dword v[0:1], v18 glc
        .else
            s_mov_b32 s18, 0xbeef
            s_store_dword s18, s[0:1], 0x4 glc
            s_mov_b32 s18, 0x1
            s_store_dword s18, s[2:3], 0 glc
            s_mov_b32 s18, 0xcafe
            s_store_dword s18, s[0:1], 0x0 glc
        .endif
        s_endpgm
)";

/**
 * KFDQMTest
 */

/* A simple isa loop program with dense mathematic operations
 * s1 controls the number iterations of the loop
 * This shader can be used by GFX8, GFX9 and GFX10
 */
const char *LoopIsa = R"(
        .text
        s_movk_i32    s0, 0x0008
        s_movk_i32    s1, 0x00ff
        v_mov_b32     v0, 0
        v_mov_b32     v1, 0
        v_mov_b32     v2, 0
        v_mov_b32     v3, 0
        v_mov_b32     v4, 0
        v_mov_b32     v5, 0
        v_mov_b32     v6, 0
        v_mov_b32     v7, 0
        v_mov_b32     v8, 0
        v_mov_b32     v9, 0
        v_mov_b32     v10, 0
        v_mov_b32     v11, 0
        v_mov_b32     v12, 0
        v_mov_b32     v13, 0
        v_mov_b32     v14, 0
        v_mov_b32     v15, 0
        v_mov_b32     v16, 0
        LOOP:
        s_mov_b32     s8, s4
        s_mov_b32     s9, s1
        s_mov_b32     s10, s6
        s_mov_b32     s11, s7
        s_cmp_le_i32  s1, s0
        s_cbranch_scc1  END_OF_PGM
        s_buffer_load_dwordx8  s[8:15], s[8:11], 0x10
        v_add_f32     v0, 2.0, v0
        v_cvt_f32_i32 v17, s1
        s_waitcnt     lgkmcnt(0)
        v_add_f32     v18, s8, v17
        v_add_f32     v19, s9, v17
        v_add_f32     v20, s10, v17
        v_add_f32     v21, s11, v17
        v_add_f32     v22, s12, v17
        v_add_f32     v23, s13, v17
        v_add_f32     v24, s14, v17
        v_add_f32     v17, s15, v17
        v_log_f32     v25, v18
        v_mul_f32     v25, v22, v25
        v_exp_f32     v25, v25
        v_log_f32     v26, v19
        v_mul_f32     v26, v23, v26
        v_exp_f32     v26, v26
        v_log_f32     v27, v20
        v_mul_f32     v27, v24, v27
        v_exp_f32     v27, v27
        v_log_f32     v28, v21
        v_mul_f32     v28, v17, v28
        v_exp_f32     v28, v28
        v_add_f32     v5, v5, v25
        v_add_f32     v6, v6, v26
        v_add_f32     v7, v7, v27
        v_add_f32     v8, v8, v28
        v_mul_f32     v18, 0x3fb8aa3b, v18
        v_exp_f32     v18, v18
        v_mul_f32     v19, 0x3fb8aa3b, v19
        v_exp_f32     v19, v19
        v_mul_f32     v20, 0x3fb8aa3b, v20
        v_exp_f32     v20, v20
        v_mul_f32     v21, 0x3fb8aa3b, v21
        v_exp_f32     v21, v21
        v_add_f32     v9, v9, v18
        v_add_f32     v10, v10, v19
        v_add_f32     v11, v11, v20
        v_add_f32     v12, v12, v21
        v_sqrt_f32    v18, v22
        v_sqrt_f32    v19, v23
        v_sqrt_f32    v20, v24
        v_sqrt_f32    v21, v17
        v_add_f32     v13, v13, v18
        v_add_f32     v14, v14, v19
        v_add_f32     v15, v15, v20
        v_add_f32     v16, v16, v21
        v_rsq_f32     v18, v22
        v_rsq_f32     v19, v23
        v_rsq_f32     v20, v24
        v_rsq_f32     v17, v17
        v_add_f32     v1, v1, v18
        v_add_f32     v2, v2, v19
        v_add_f32     v3, v3, v20
        v_add_f32     v4, v4, v17
        s_add_u32     s0, s0, 1
        s_branch      LOOP
        END_OF_PGM:
        s_endpgm
)";
