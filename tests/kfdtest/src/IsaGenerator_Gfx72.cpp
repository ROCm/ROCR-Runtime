/*
 * Copyright (C) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "IsaGenerator_Gfx72.hpp"

#include <algorithm>
#include <string>

const std::string IsaGenerator_Gfx72::ASIC_NAME = "CI";

const uint32_t IsaGenerator_Gfx72::NOOP_ISA[] = {
    0xbf810000  // S_ENDPGM
};

/* The below arrays are filled with hex values in order not to reference
 * proprietary header files, but we still leave the code here for future
 * reference.
 */
#if 0
const uint32_t IsaGenerator_Gfx72::COPY_DWORD_ISA[] = {
    (63u << SQ_VOP1__ENCODING__SHIFT) | (0 << SQ_VOP1__VDST__SHIFT) | (SQ_V_MOV_B32 << SQ_VOP1__OP__SHIFT) | (0 << SQ_VOP1__SRC0__SHIFT),  // v_mov_b32 v0, s0 (VOP1)
    (63u << SQ_VOP1__ENCODING__SHIFT) | (1 << SQ_VOP1__VDST__SHIFT) | (SQ_V_MOV_B32 << SQ_VOP1__OP__SHIFT) | (1 << SQ_VOP1__SRC0__SHIFT),  // v_mov_b32 v1, s1 (VOP1)
    (63u << SQ_VOP1__ENCODING__SHIFT) | (2 << SQ_VOP1__VDST__SHIFT) | (SQ_V_MOV_B32 << SQ_VOP1__OP__SHIFT) | (2 << SQ_VOP1__SRC0__SHIFT),  // v_mov_b32 v2, s2 (VOP1)
    (63u << SQ_VOP1__ENCODING__SHIFT) | (3 << SQ_VOP1__VDST__SHIFT) | (SQ_V_MOV_B32 << SQ_VOP1__OP__SHIFT) | (3 << SQ_VOP1__SRC0__SHIFT),  // v_mov_b32 v3, s3 (VOP1)

    (55u << SQ_FLAT_0__ENCODING__SHIFT) | (SQ_FLAT_LOAD_DWORD << SQ_FLAT_0__OP__SHIFT) | (1 << SQ_FLAT_0__SLC__SHIFT) | (1 << SQ_FLAT_0__GLC__SHIFT)/*(3 << 16)*/,    // SQ_FLAT_0, flat_load_dword, slc = 1, glc = 1 (FLAT_0)
    (4u << SQ_FLAT_1__VDST__SHIFT) | (0 << SQ_FLAT_1__ADDR__SHIFT),       // ADDR = V0:V1, VDST = V4 (FLAT_1)

    (383u << SQ_SOPP__ENCODING__SHIFT) | (SQ_S_WAITCNT << SQ_SOPP__OP__SHIFT) | (0 << SQ_SOPP__SIMM16__SHIFT),  // s_waitcnt 0 (SOPP)

    (55u << SQ_FLAT_0__ENCODING__SHIFT) | (SQ_FLAT_STORE_DWORD << SQ_FLAT_0__OP__SHIFT) | (1 << SQ_FLAT_0__SLC__SHIFT) | (1 << SQ_FLAT_0__GLC__SHIFT),    // SQ_FLAT_0, flat_store_dword, slc = 1, glc = 1 (FLAT_0)
    (4u << SQ_FLAT_1__DATA__SHIFT) | (2 << SQ_FLAT_1__ADDR__SHIFT),        // ADDR = V2:V3, DATA = V4 (FLAT_1)

    0xBF810000u  // s_endpgm, note that we rely on the implicit s_waitcnt 0,0,0
};

const uint32_t IsaGenerator_Gfx72::INFINITE_LOOP_ISA[] = {
    (0x17F << SQ_SOPP__ENCODING__SHIFT) | (SQ_S_BRANCH << SQ_SOPP__OP__SHIFT) | ( (const uint32_t)-1 << SQ_SOPP__SIMM16__SHIFT),  // s_branch -1 (PC <- PC + SIMM*4)+4
    0xBF810000u  // S_ENDPGM
};

const uint32_t IsaGenerator_Gfx72::ATOMIC_INC_ISA[] = {
    (63u << SQ_VOP1__ENCODING__SHIFT) | (0 << SQ_VOP1__VDST__SHIFT) | (SQ_V_MOV_B32 << SQ_VOP1__OP__SHIFT) | (0 << SQ_VOP1__SRC0__SHIFT),  // v_mov_b32 v0, s0 (VOP1)
    (63u << SQ_VOP1__ENCODING__SHIFT) | (1 << SQ_VOP1__VDST__SHIFT) | (SQ_V_MOV_B32 << SQ_VOP1__OP__SHIFT) | (1 << SQ_VOP1__SRC0__SHIFT),  // v_mov_b32 v1, s1 (VOP1)
    (63u << SQ_VOP1__ENCODING__SHIFT) | (2 << SQ_VOP1__VDST__SHIFT) | (SQ_V_MOV_B32 << SQ_VOP1__OP__SHIFT) | (0xC1 << SQ_VOP1__SRC0__SHIFT),  // v_mov_b32 0xFFFFFFFF, s2 (VOP1)

    (55u << SQ_FLAT_0__ENCODING__SHIFT) | (SQ_FLAT_ATOMIC_INC << SQ_FLAT_0__OP__SHIFT) | (1 << SQ_FLAT_0__SLC__SHIFT) | (0 << SQ_FLAT_0__GLC__SHIFT),    // SQ_FLAT_0, flat_atomic_inc, slc = 1, glc = 0 (FLAT_0)
    (3u << SQ_FLAT_1__VDST__SHIFT) | (2u << SQ_FLAT_1__DATA__SHIFT) | (0 << SQ_FLAT_1__ADDR__SHIFT),        // ADDR/dst = V0:V1, VDST/ret = V3, DATA/src=V2 (FLAT_1)
    0xBF810000u  // s_endpgm, note that we rely on the implicit s_waitcnt 0,0,0
};
#endif

const uint32_t IsaGenerator_Gfx72::COPY_DWORD_ISA[] = {
    0x7e000200,  // v_mov_b32 v0, s0 (VOP1)
    0x7e020201,  // v_mov_b32 v1, s1 (VOP1)
    0x7e040202,  // v_mov_b32 v2, s2 (VOP1)
    0x7e060203,  // v_mov_b32 v3, s3 (VOP1)

    0xdc330000,  // SQ_FLAT_0, flat_load_dword, slc = 1, glc = 1 (FLAT_0)
    0x04000000,  // ADDR = V0:V1, VDST = V4 (FLAT_1)

    0xbf8c0000,  // s_waitcnt 0 (SOPP)

    0xdc730000,  // SQ_FLAT_0, flat_store_dword, slc = 1, glc = 1 (FLAT_0)
    0x00000402,  // ADDR = V2:V3, DATA = V4 (FLAT_1)

    0xbf810000   // s_endpgm, note that we rely on the implicit s_waitcnt 0,0,0
};

const uint32_t IsaGenerator_Gfx72::INFINITE_LOOP_ISA[] = {
    0xbf82ffff,  // s_branch -1 (PC <- PC + SIMM*4)+4
    0xbf810000   // S_ENDPGM
};

const uint32_t IsaGenerator_Gfx72::ATOMIC_INC_ISA[] = {
    0x7e000200,  // v_mov_b32 v0, s0 (VOP1)
    0x7e020201,  // v_mov_b32 v1, s1 (VOP1)
    0x7e0402c1,  // v_mov_b32 0xFFFFFFFF, s2 (VOP1)

    0xdcf20000,  // SQ_FLAT_0, flat_atomic_inc, slc = 1, glc = 0 (FLAT_0)
    0x03000200,  // ADDR/dst = V0:V1, VDST/ret = V3, DATA/src=V2 (FLAT_1)
    0xbf810000  // s_endpgm, note that we rely on the implicit s_waitcnt 0,0,0
};

void IsaGenerator_Gfx72::GetNoopIsa(HsaMemoryBuffer& rBuf) {
    std::copy(NOOP_ISA, NOOP_ISA+ARRAY_SIZE(NOOP_ISA), rBuf.As<uint32_t*>());
}

void IsaGenerator_Gfx72::GetCopyDwordIsa(HsaMemoryBuffer& rBuf) {
    std::copy(COPY_DWORD_ISA, COPY_DWORD_ISA+ARRAY_SIZE(COPY_DWORD_ISA), rBuf.As<uint32_t*>());
}

void IsaGenerator_Gfx72::GetInfiniteLoopIsa(HsaMemoryBuffer& rBuf) {
    std::copy(INFINITE_LOOP_ISA, INFINITE_LOOP_ISA+ARRAY_SIZE(INFINITE_LOOP_ISA), rBuf.As<uint32_t*>());
}

void IsaGenerator_Gfx72::GetAtomicIncIsa(HsaMemoryBuffer& rBuf) {
    std::copy(ATOMIC_INC_ISA, ATOMIC_INC_ISA+ARRAY_SIZE(ATOMIC_INC_ISA), rBuf.As<uint32_t*>());
}

const std::string& IsaGenerator_Gfx72::GetAsicName() {
    return ASIC_NAME;
}
