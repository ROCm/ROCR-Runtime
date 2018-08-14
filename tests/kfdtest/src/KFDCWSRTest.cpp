/*
 * Copyright (C) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "KFDCWSRTest.hpp"
#include "Dispatch.hpp"

static const char* iterate_isa_gfx8 = \
"\
shader iterate_isa\n\
asic(VI)\n\
type(CS)\n\
/*copy the parameters from scalar registers to vector registers*/\n\
    v_mov_b32 v1, s1\n\
    v_mov_b32 v2, s2\n\
    v_mov_b32 v3, s3\n\
/*v0 stores the workitem ID inside the workgroup, use it to caculate the dest*/\n\
//    v_lshlrev_b32 v0, 2, v0\n\
//    v_add_u32 v2, vcc, v0, v2\n\
    v_mov_b32 v0, s0\n\
    flat_load_dword v4, v[0:1] slc  /*load target iteration value*/\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    v_mov_b32 v5, 0\n\
LOOP:\n\
    v_add_u32 v5, vcc, 1, v5\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    /*compare the result value (v5) to iteration value (v4), and jump if equal (i.e. if VCC is not zero after the comparison)*/\n\
    v_cmp_lt_u32 vcc, v5, v4\n\
    s_cbranch_vccnz LOOP\n\
    flat_store_dword v[2,3], v5\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    s_endpgm\n\
end\n\
";

static const char* iterate_isa_gfx9 = \
"\
shader iterate_isa\n\
asic(GFX9)\n\
type(CS)\n\
/*copy the parameters from scalar registers to vector registers*/\n\
    v_mov_b32 v0, s0\n\
    v_mov_b32 v1, s1\n\
    v_mov_b32 v2, s2\n\
    v_mov_b32 v3, s3\n\
    flat_load_dword v4, v[0:1] slc    /*load target iteration value*/\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    v_mov_b32 v5, 0\n\
LOOP:\n\
    v_add_co_u32 v5, vcc, 1, v5\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    /*compare the result value (v5) to iteration value (v4), and jump if equal (i.e. if VCC is not zero after the comparison)*/\n\
    v_cmp_lt_u32 vcc, v5, v4\n\
    s_cbranch_vccnz LOOP\n\
    flat_store_dword v[2,3], v5\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    s_endpgm\n\
end\n\
";

/*
v[0:1] = target iteration value
v[2:3] = iterate result
*/

void KFDCWSRTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);

    /* TODO: In the ISA, the workitem_id is not obtained as expected, so the destination cannot
     * be set based on workitem_id. Set the wave_num to 1 for now as a workarpound.
     * Will set it to 8 or even 256 in the future.
     */
    wave_number = 1;

    ROUTINE_END
}

void KFDCWSRTest::TearDown() {
    ROUTINE_START
    if (m_pIsaGen)
        delete m_pIsaGen;
    m_pIsaGen = NULL;

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

/**
 * KFDCWSRTest.BasicTest
 *
 * This test dispatches the loop_inc_isa shader and lets it run, ensuring its destination pointer gets incremented.
 * It then triggers CWSR and ensures the shader stops running.
 * It then resumes the shader, ensures that it's running again and terminates it.
 */

TEST_F(KFDCWSRTest, BasicTest) {
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();

    if (m_FamilyId >= FAMILY_VI) {
        HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
        HsaMemoryBuffer iterateBuf(PAGE_SIZE, defaultGPUNode, true, false, false);
        HsaMemoryBuffer resultBuf(PAGE_SIZE, defaultGPUNode, true, false, false);

        unsigned int* iter = iterateBuf.As<unsigned int*>();
        unsigned int* result = resultBuf.As<unsigned int*>();

        m_pIsaGen->CompileShader((m_FamilyId >= FAMILY_AI) ? iterate_isa_gfx9 : iterate_isa_gfx8 ,
                "iterate_isa", isaBuffer);

        PM4Queue queue1, queue2;

        EXPECT_SUCCESS(queue1.Create(defaultGPUNode));

        Dispatch *dispatch1, *dispatch2;

        dispatch1 = new Dispatch(isaBuffer);
        dispatch2 = new Dispatch(isaBuffer);

        dispatch1->SetArgs(&iter[0], &result[0]);
        dispatch1->SetDim(wave_number, 1, 1);
        dispatch2->SetArgs(&iter[1], &result[wave_number]);
        dispatch2->SetDim(wave_number, 1, 1);

        iter[0] = 40000000;
        iter[1] = 20000000;

        // Submit the shader, queue1
        dispatch1->Submit(queue1);
        // Create queue2 during queue1 still running will trigger the CWSR
        EXPECT_SUCCESS(queue2.Create(defaultGPUNode));
        // Submit the shader
        dispatch2->Submit(queue2);
        dispatch1->Sync();
        dispatch2->Sync();
        // Ensure all the waves complete as expected
        int i;
        for (i = 0 ; i < wave_number; ++i) {
             if (result[i] != iter[0]) {
                 LOG() << "Dispatch 1, work item " << i << ' ' << result[i] << std::endl;
                 break;
             }
             if (result[i + wave_number] != iter[1]) {
                 LOG() << "Dispatch 2, work item " << i << ' ' << result[i] << std::endl;
                 break;
             }
        }
        EXPECT_EQ(i, wave_number);

        EXPECT_SUCCESS(queue1.Destroy());
        EXPECT_SUCCESS(queue2.Destroy());

        delete dispatch1;
        delete dispatch2;

    } else {
        LOG() << "Skipping test: No CWSR present for family ID 0x" << m_FamilyId << "." << std::endl;
    }

    TEST_END
}
