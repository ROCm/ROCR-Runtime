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
