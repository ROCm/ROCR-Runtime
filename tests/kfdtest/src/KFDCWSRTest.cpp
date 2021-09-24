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

static const char* IterateIsa_gfx8 = R"(
        .text
        // Copy the parameters from scalar registers to vector registers
        v_mov_b32       v2, s0              // v[2:3] = s[0:1]
        v_mov_b32       v3, s1              // v[2:3] = s[0:1]
        v_mov_b32       v0, s4              // use workgroup id as index
        v_lshlrev_b32   v0, 2, v0           // v0 *= 4
        v_add_u32       v4, vcc, s2, v0     // v[4:5] = s[2:3] + v0 * 4
        v_mov_b32       v5, s3              // v[4:5] = s[2:3] + v0 * 4
        v_add_u32       v5, vcc, v5, vcc_lo // v[4:5] = s[2:3] + v0 * 4
        v_mov_b32       v6, 0
        LOOP:
        v_add_u32       v6, vcc, 1, v6
        // Compare the result value (v6) to iteration value (v2), and
        // jump if equal (i.e. if VCC is not zero after the comparison)
        v_cmp_lt_u32 vcc, v6, v2
        s_cbranch_vccnz LOOP
        flat_store_dword v[4:5], v6
        s_waitcnt vmcnt(0) & lgkmcnt(0)
        s_endpgm
)";

// This shader can be used by gfx9 and gfx10
static const char* IterateIsa_gfx9 = R"(
        .text
        // Copy the parameters from scalar registers to vector registers
        v_mov_b32       v2, s0              // v[2:3] = s[0:1]
        v_mov_b32       v3, s1              // v[2:3] = s[0:1]
        v_mov_b32       v0, s4              // use workgroup id as index
        v_lshlrev_b32   v0, 2, v0           // v0 *= 4
        v_add_co_u32    v4, vcc, s2, v0     // v[4:5] = s[2:3] + v0 * 4
        v_mov_b32       v5, s3              // v[4:5] = s[2:3] + v0 * 4
        v_add_co_u32    v5, vcc, v5, vcc_lo // v[4:5] = s[2:3] + v0 * 4
        v_mov_b32       v6, 0
        LOOP:
        v_add_co_u32    v6, vcc, 1, v6
        // Compare the result value (v6) to iteration value (v2), and
        // jump if equal (i.e. if VCC is not zero after the comparison)
        v_cmp_lt_u32 vcc, v6, v2
        s_cbranch_vccnz LOOP
        flat_store_dword v[4:5], v6
        s_waitcnt vmcnt(0) & lgkmcnt(0)
        s_endpgm
)";

static const char* InfiniteIsa = R"(
        .text
        LOOP:
        s_branch LOOP
        s_endpgm
)";

void KFDCWSRTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    wave_number = 1;

    ROUTINE_END
}

void KFDCWSRTest::TearDown() {
    ROUTINE_START

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

bool isOnEmulator() {
    uint32_t isEmuMode = 0;

    fscanf_dec("/sys/module/amdgpu/parameters/emu_mode", &isEmuMode);

    return isEmuMode;
}

static inline uint32_t checkCWSREnabled() {
    uint32_t cwsr_enable = 0;

    fscanf_dec("/sys/module/amdgpu/parameters/cwsr_enable", &cwsr_enable);

    return cwsr_enable;
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

    if ((m_FamilyId >= FAMILY_VI) && (checkCWSREnabled())) {
        const char *pIterateIsa;
        HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
        HsaMemoryBuffer resultBuf1(PAGE_SIZE, defaultGPUNode, true, false, false);
        uint64_t count1 = 400000000;

        if (m_FamilyId < FAMILY_AI)
            pIterateIsa = IterateIsa_gfx8;
        else
            pIterateIsa = IterateIsa_gfx9;

        if (isOnEmulator()) {
            // Divide the iterator times by 10000 so that the test can
            // finish in a reasonable time.
            count1 /= 10000;
            LOG() << "On Emulators" << std::endl;
        }

        unsigned int* result1 = resultBuf1.As<unsigned int*>();

        ASSERT_SUCCESS(m_pAsm->RunAssembleBuf(pIterateIsa, isaBuffer.As<char*>()));

        PM4Queue queue1;

        ASSERT_SUCCESS(queue1.Create(defaultGPUNode));

        Dispatch *dispatch1;

        dispatch1 = new Dispatch(isaBuffer);

        dispatch1->SetArgs(reinterpret_cast<void *>(count1), result1);
        dispatch1->SetDim(wave_number, 1, 1);

        // Submit the shader, queue1
        dispatch1->Submit(queue1);

        //Give time for waves to launch before disabling queue.
        Delay(1);
        EXPECT_SUCCESS(queue1.Update(0/*percentage*/, BaseQueue::DEFAULT_PRIORITY, false));
        Delay(5);
        EXPECT_SUCCESS(queue1.Update(100/*percentage*/, BaseQueue::DEFAULT_PRIORITY, false));

        dispatch1->Sync();
        // Ensure all the waves complete as expected
        int i;
        for (i = 0 ; i < wave_number; ++i) {
            if (result1[i] != count1) {
                LOG() << "Dispatch 1, work item [" << std::dec << i << "] "
                      << result1[i] << " != " << count1 << std::endl;
                break;
            }
        }
        EXPECT_EQ(i, wave_number);

        EXPECT_SUCCESS(queue1.Destroy());

        delete dispatch1;
    } else {
        LOG() << "Skipping test: No CWSR present for family ID 0x" << m_FamilyId << "." << std::endl;
    }

    TEST_END
}

/**
 * KFDCWSRTest.InterruptRestore
 *
 * This test verifies that CP can preempt an HQD while it is restoring a dispatch.
 * Create queue 1.
 * Start a dispatch on queue 1 which runs indefinitely and fills all CU wave slots.
 * Create queue 2, triggering context save on queue 1.
 * Start a dispatch on queue 2 which runs indefinitely and fills all CU wave slots.
 * Create queue 3, triggering context save and restore on queues 1 and 2.
 * Preempt runlist. One or both queues must interrupt context restore to preempt.
 */

TEST_F(KFDCWSRTest, InterruptRestore) {
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();

    if ((m_FamilyId >= FAMILY_VI) && (checkCWSREnabled())) {
        HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);

        ASSERT_SUCCESS(m_pAsm->RunAssembleBuf(InfiniteIsa, isaBuffer.As<char*>()));

        PM4Queue queue1, queue2, queue3;

        ASSERT_SUCCESS(queue1.Create(defaultGPUNode));

        Dispatch *dispatch1, *dispatch2;

        dispatch1 = new Dispatch(isaBuffer);
        dispatch2 = new Dispatch(isaBuffer);

        dispatch1->SetDim(0x10000, 1, 1);
        dispatch2->SetDim(0x10000, 1, 1);

        dispatch1->Submit(queue1);

        ASSERT_SUCCESS(queue2.Create(defaultGPUNode));

        dispatch2->Submit(queue2);

        // Give waves time to launch.
        Delay(1);

        ASSERT_SUCCESS(queue3.Create(defaultGPUNode));

        EXPECT_SUCCESS(queue1.Destroy());
        EXPECT_SUCCESS(queue2.Destroy());
        EXPECT_SUCCESS(queue3.Destroy());

        delete dispatch1;
        delete dispatch2;
    } else {
        LOG() << "Skipping test: No CWSR present for family ID 0x" << m_FamilyId << "." << std::endl;
    }

    TEST_END
}
