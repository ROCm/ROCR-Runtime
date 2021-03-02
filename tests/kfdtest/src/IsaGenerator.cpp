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

#include "IsaGenerator.hpp"

#include <algorithm>
#include <string>

#include "IsaGenerator_Gfx72.hpp"
#include "IsaGenerator_Gfx8.hpp"
#include "IsaGenerator_Gfx9.hpp"
#include "IsaGenerator_Gfx10.hpp"
#include "IsaGenerator_Aldebaran.hpp"

#include "GoogleTestExtension.hpp"

#include "sp3.h"

const std::string IsaGenerator::ADDRESS_WATCH_SP3(
    "var REG_TRAPSTS_EXCP_MASK = 0x000001ff\n"
    "var WAVE_COUNT_OFFSET = 12\n"
    "var TMA_CYCLE_OFFSET  = 16\n"
    "\n"
    "/*\n"
    " * ttmp[0:1]   -- The ISA address that triggered this trap handler\n"
    " * ttmp[10:11] -- The TMA user provided, used to store the debug info in this shader\n"
    " * v[10:14] ttmp[7:8] -- temp use inside this shader\n"
    " * s5 -- store the counts that this trap been triggered\n"
    " * Each time when the trap is triggered , this shader will write\n"
    " * ttmp[0] : ttmp[1] : Trap_Status : [reserved]\n"
    " * to TMA + (trap count * TMA_CYCLE_OFFSET)\n"
    " * The TMA + WAVE_COUNT_OFFSET(the first [reserved] address)\n"
    " * used to store the total triggered trap count.\n"
    " */\n"
    "shader main\n"
    "\n"
    "    asic(VI)\n"
    "\n"
    "    type(CS)\n"
    "    v_mov_b32      v10, ttmp10\n"
    "    v_mov_b32      v11, ttmp11\n"
    "    s_mov_b32      ttmp7, s5\n"
    "    s_mulk_i32     ttmp7, TMA_CYCLE_OFFSET\n"
    "    s_addk_i32     s5, 1\n"
    "    v_mov_b32      v12, ttmp0\n"
    "    v_add_u32      v10, vcc, ttmp7, v10\n"
    "    flat_store_dword   v[10,11], v12 slc glc\n"
    "    v_mov_b32      v12, ttmp1\n"
    "    v_add_u32      v10, vcc, 4, v10\n"
    "    flat_store_dword   v[10,11], v12 slc  glc\n"
    "    s_getreg_b32   ttmp8, hwreg(HW_REG_TRAPSTS)\n"
    "    s_and_b32      ttmp8, ttmp8, REG_TRAPSTS_EXCP_MASK\n"
    "    v_mov_b32      v12, ttmp8\n"
    "    v_add_u32      v10, vcc, 4, v10\n"
    "    flat_store_dword   v[10,11], v12  glc\n"
    "    v_mov_b32      v10, ttmp10\n"
    "    v_add_u32      v10, vcc, WAVE_COUNT_OFFSET, v10\n"
    "    v_mov_b32      v13, 1\n"
    "    flat_atomic_add    v14, v[10:11], v13 slc glc\n"
    "    s_and_b32      ttmp1, ttmp1, 0xffff\n"
    "    s_rfe_b64      [ttmp0,ttmp1]\n"
    "end\n"
);

IsaGenerator* IsaGenerator::Create(unsigned int familyId) {
    switch (familyId) {
    case FAMILY_CI:
    case FAMILY_KV:
        return new IsaGenerator_Gfx72;
    case FAMILY_VI:
    case FAMILY_CZ:
        return new IsaGenerator_Gfx8;
    case FAMILY_AI:
    case FAMILY_RV:
    case FAMILY_AR:
        return new IsaGenerator_Gfx9;
    case FAMILY_AL:
        return new IsaGenerator_Aldbrn;
    case FAMILY_NV:
        return new IsaGenerator_Gfx10;

    default:
        LOG() << "Error: Invalid ISA" << std::endl;
        return NULL;
    }
}

void IsaGenerator::GetAwTrapHandler(HsaMemoryBuffer& rBuf) {
    CompileShader(ADDRESS_WATCH_SP3.c_str(), "main", rBuf);
}

void IsaGenerator::CompileShader(const char* shaderCode, const char* shaderName, HsaMemoryBuffer& rBuf) {
    sp3_context* pSp3 = sp3_new();
    sp3_setasic(pSp3, GetAsicName().c_str());
    sp3_parse_string(pSp3, shaderCode);
    sp3_shader* pShader = sp3_compile(pSp3, shaderName);

    std::copy(pShader->data, pShader->data + pShader->size, rBuf.As<unsigned int*>());
    sp3_free_shader(pShader);

    /** Inside this close function, there is an unknown reason of free memory not used by compiler.
     *  Comment out this as a workaround. System will do the garbage collection after this
     *  application is closed.
     */
    // sp3_close(pSp3);
}
