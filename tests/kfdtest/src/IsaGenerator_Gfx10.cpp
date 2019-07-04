/*
 * Copyright (C) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "IsaGenerator_Gfx10.hpp"

#include <algorithm>
#include <string>

/* The binaries are generated from following ISA */
const std::string IsaGenerator_Gfx10::ASIC_NAME = "GFX10";
#if 0
static const char * atomic_add = \
"\
shader atomic_add \n\
asic(GFX10) \n\
wave_size(32) \n\
type(CS) \n\
    v_mov_b32 v0, s0 \n\
    v_mov_b32 v1, s1 \n\
    v_mov_b32 v2, 1 \n\
    flat_atomic_add v3, v[0:1], v2 slc glc \n\
    s_waitcnt 0 \n\
    s_endpgm \n\
end \n\
";

static const char * copy_dword = \
"\
shader copy_dword \n\
asic(GFX10) \n\
wave_size(32) \n\
type(CS) \n\
    v_mov_b32 v0, s0 \n\
    v_mov_b32 v1, s1 \n\
    v_mov_b32 v2, s2 \n\
    v_mov_b32 v3, s3 \n\
    flat_load_dword v4, v[0:1] slc glc \n\
    s_waitcnt 0 \n\
    flat_store_dword v[2:3], v4 slc glc \n\
    s_endpgm \n\
end \n\
";

static const char * loop= \
"\
shader loop \n\
asic(GFX10) \n\
type(CS) \n\
wave_size(32) \n\
loop: \n\
    s_branch loop \n\
    s_endpgm \n\
end \n\
";

static const char * noop= \
"\
shader noop \n\
asic(GFX10) \n\
type(CS) \n\
wave_size(32) \n\
    s_endpgm \n\
end \n\
";
#endif

const uint32_t IsaGenerator_Gfx10::NOOP_ISA[] = {
0xb0804004, 0xbf810000,
0xbf9f0000, 0xbf9f0000,
0xbf9f0000, 0xbf9f0000,
0xbf9f0000
};

const uint32_t IsaGenerator_Gfx10::COPY_DWORD_ISA[] = {
0xb0804004, 0x7e000200,
0x7e020201, 0x7e040202,
0x7e060203, 0xdc330000,
0x47d0000, 0xbf8c0000,
0xdc730000, 0x7d0402,
0xbf810000, 0xbf9f0000,
0xbf9f0000, 0xbf9f0000,
0xbf9f0000, 0xbf9f0000
};

const uint32_t IsaGenerator_Gfx10::INFINITE_LOOP_ISA[] = {
0xbf82ffff, 0xb0804004,
0xbf810000, 0xbf9f0000,
0xbf9f0000, 0xbf9f0000,
0xbf9f0000, 0xbf9f0000
};

const uint32_t IsaGenerator_Gfx10::ATOMIC_ADD_ISA[] = {
0xb0804004, 0x7e000200,
0x7e020201, 0x7e040281,
0xdccb0000, 0x37d0200,
0xbf8c0000, 0xbf810000,
0xbf9f0000, 0xbf9f0000,
0xbf9f0000, 0xbf9f0000,
0xbf9f0000
};


void IsaGenerator_Gfx10::GetNoopIsa(HsaMemoryBuffer& rBuf) {
    std::copy(NOOP_ISA, NOOP_ISA+ARRAY_SIZE(NOOP_ISA), rBuf.As<uint32_t*>());
}

void IsaGenerator_Gfx10::GetCopyDwordIsa(HsaMemoryBuffer& rBuf) {
    std::copy(COPY_DWORD_ISA, COPY_DWORD_ISA+ARRAY_SIZE(COPY_DWORD_ISA), rBuf.As<uint32_t*>());
}

void IsaGenerator_Gfx10::GetInfiniteLoopIsa(HsaMemoryBuffer& rBuf) {
    std::copy(INFINITE_LOOP_ISA, INFINITE_LOOP_ISA+ARRAY_SIZE(INFINITE_LOOP_ISA), rBuf.As<uint32_t*>());
}

void IsaGenerator_Gfx10::GetAtomicIncIsa(HsaMemoryBuffer& rBuf) {
    std::copy(ATOMIC_ADD_ISA, ATOMIC_ADD_ISA+ARRAY_SIZE(ATOMIC_ADD_ISA), rBuf.As<uint32_t*>());
}

const std::string& IsaGenerator_Gfx10::GetAsicName() {
    return ASIC_NAME;
}

