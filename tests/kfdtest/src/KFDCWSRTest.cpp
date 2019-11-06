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


/* Initial state:
 *   s[0:1] - 64 bits iteration number; only the lower 32 bits are useful.
 *   s[2:3] - result buffer base address
 *   s4 - workgroup id
 *   v0 - workitem id, always 0 because
 *        NUM_THREADS_X(number of threads) in workgroup set to 1
 * Registers:
 *   v0 - calculated workitem = v0 + s4 * NUM_THREADS_X, which is s4
 *   v2 - = s0, 32 bits iteration number
 *   v[4:5] - corresponding output buf address: s[2:3] + v0 * 4
 *   v6 - counter
 */

static const char* iterate_isa_gfx8 = \
"\
shader iterate_isa\n\
wave_size(32)\n\
type(CS)\n\
    // copy the parameters from scalar registers to vector registers\n\
    v_mov_b32       v2, s0   // v[2:3] = s[0:1] \n\
    v_mov_b32       v3, s1   // v[2:3] = s[0:1] \n\
    v_mov_b32       v0, s4   // use workgroup id as index \n\
    v_lshlrev_b32   v0, 2, v0   // v0 *= 4 \n\
    v_add_u32       v4, vcc, s2, v0   // v[4:5] = s[2:3] + v0 * 4 \n\
    v_mov_b32       v5, s3   // v[4:5] = s[2:3] + v0 * 4 \n\
    v_add_u32       v5, vcc, v5, vcc_lo   // v[4:5] = s[2:3] + v0 * 4 \n\
    v_mov_b32       v6, 0 \n\
LOOP: \n\
    v_add_u32       v6, vcc, 1, v6 \n\
    // compare the result value (v6) to iteration value (v2), and \n\
    // jump if equal (i.e. if VCC is not zero after the comparison) \n\
    v_cmp_lt_u32 vcc, v6, v2 \n\
    s_cbranch_vccnz LOOP \n\
    flat_store_dword v[4:5], v6 \n\
    s_waitcnt vmcnt(0)&lgkmcnt(0) \n\
    s_endpgm \n\
end \n\
";

//This shader can be used by gfx9 and gfx10
static const char* iterate_isa_gfx9 = \
"\
shader iterate_isa\n\
wave_size(32)\n\
type(CS)\n\
    // copy the parameters from scalar registers to vector registers\n\
    v_mov_b32       v2, s0   // v[2:3] = s[0:1] \n\
    v_mov_b32       v3, s1   // v[2:3] = s[0:1] \n\
    v_mov_b32       v0, s4   // use workgroup id as index \n\
    v_lshlrev_b32   v0, 2, v0   // v0 *= 4 \n\
    v_add_co_u32    v4, vcc, s2, v0   // v[4:5] = s[2:3] + v0 * 4 \n\
    v_mov_b32       v5, s3   // v[4:5] = s[2:3] + v0 * 4 \n\
    v_add_co_u32    v5, vcc, v5, vcc_lo   // v[4:5] = s[2:3] + v0 * 4 \n\
    v_mov_b32       v6, 0 \n\
LOOP: \n\
    v_add_co_u32    v6, vcc, 1, v6 \n\
    // compare the result value (v6) to iteration value (v2), and \n\
    // jump if equal (i.e. if VCC is not zero after the comparison) \n\
    v_cmp_lt_u32 vcc, v6, v2 \n\
    s_cbranch_vccnz LOOP \n\
    flat_store_dword v[4:5], v6 \n\
    s_waitcnt vmcnt(0)&lgkmcnt(0) \n\
    s_endpgm \n\
end \n\
";

void KFDCWSRTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);

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
        HsaMemoryBuffer resultBuf1(PAGE_SIZE, defaultGPUNode, true, false, false);
        HsaMemoryBuffer resultBuf2(PAGE_SIZE, defaultGPUNode, true, false, false);

        int count1 = 40000000;
        int count2 = 20000000;

        unsigned int* result1 = resultBuf1.As<unsigned int*>();
        unsigned int* result2 = resultBuf2.As<unsigned int*>();
        const char *pIterateIsa;

        if (m_FamilyId < FAMILY_AI)
            pIterateIsa = iterate_isa_gfx8;
        else
            pIterateIsa = iterate_isa_gfx9;

        m_pIsaGen->CompileShader(pIterateIsa, "iterate_isa", isaBuffer);

        PM4Queue queue1, queue2;

        ASSERT_SUCCESS(queue1.Create(defaultGPUNode));

        Dispatch *dispatch1, *dispatch2;

        dispatch1 = new Dispatch(isaBuffer);
        dispatch2 = new Dispatch(isaBuffer);

        dispatch1->SetArgs(reinterpret_cast<void *>(count1), result1);
        dispatch1->SetDim(wave_number, 1, 1);
        dispatch2->SetArgs(reinterpret_cast<void *>(count2), result2);
        dispatch2->SetDim(wave_number, 1, 1);

        // Submit the shader, queue1
        dispatch1->Submit(queue1);
        // Create queue2 during queue1 still running will trigger the CWSR
        ASSERT_SUCCESS(queue2.Create(defaultGPUNode));
        // Submit the shader
        dispatch2->Submit(queue2);
        dispatch1->Sync();
        dispatch2->Sync();
        // Ensure all the waves complete as expected
        int i;
        for (i = 0 ; i < wave_number; ++i) {
             if (result1[i] != count1) {
                 LOG() << "Dispatch 1, work item " << i << ' ' << result1[i] << std::endl;
                 break;
             }
             if (result2[i] != count2) {
                 LOG() << "Dispatch 2, work item " << i << ' ' << result2[i] << std::endl;
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
