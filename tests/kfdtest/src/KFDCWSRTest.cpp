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

void KFDCWSRTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    ROUTINE_END
}

void KFDCWSRTest::TearDown() {
    ROUTINE_START

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

static inline uint32_t checkCWSREnabled() {
    uint32_t cwsr_enable = 0;

    fscanf_dec("/sys/module/amdgpu/parameters/cwsr_enable", &cwsr_enable);

    return cwsr_enable;
}

/**
 * KFDCWSRTest.BasicTest
 *
 * This test dispatches the IterateIsa shader, which continuously increments a vgpr for
 * (num_witems / WAVE_SIZE) waves. While this shader is running, dequeue/requeue requests
 * are sent in a loop to trigger CWSRs.
 *
 * This is a paremeterized test. See the INSTANTIATE_TEST_CASE_P below for an explanation
 * on the parameters.
 *
 * This test defines a CWSR threshold. The shader will continuously loop until inputBuf is
 * filled with the known stop value, which occurs once cwsr_thresh CWSRs have been
 * successfully triggered.
 *
 * 4 parameterized tests are defined:
 *
 * KFDCWSRTest.BasicTest/0
 * KFDCWSRTest.BasicTest/1
 * KFDCWSRTest.BasicTest/2
 * KFDCWSRTest.BasicTest/3
 *
 * 0: 1 work-item, CWSR threshold of 10
 * 1: 256 work-items (multi-wave), CWSR threshold of 50
 * 2: 512 work-items (multi-wave), CWSR threshold of 100
 * 3: 1024 work-items (multi-wave), CWSR threshold of 1000
 */
TEST_P(KFDCWSRTest, BasicTest) {
    TEST_START(TESTPROFILE_RUNALL);

    int num_witems = std::get<0>(GetParam());
    int cwsr_thresh = std::get<1>(GetParam());
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();

    if ((m_FamilyId >= FAMILY_VI) && (checkCWSREnabled())) {
        HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true, false, true);
        ASSERT_SUCCESS(m_pAsm->RunAssembleBuf(IterateIsa, isaBuffer.As<char*>()));

        unsigned stopval = 0x1234'5678;
        unsigned outval  = 0x8765'4321;

        HsaMemoryBuffer inputBuf(PAGE_SIZE, defaultGPUNode, true, false, false);
        HsaMemoryBuffer outputBuf(PAGE_SIZE, defaultGPUNode, true, false, false);
        unsigned int* input = inputBuf.As<unsigned int*>();
        unsigned int* output = outputBuf.As<unsigned int*>();
        inputBuf.Fill(0);
        outputBuf.Fill(outval);

        PM4Queue queue;
        ASSERT_SUCCESS(queue.Create(defaultGPUNode));

        Dispatch dispatch(isaBuffer);
        dispatch.SetArgs(input, output);
        dispatch.SetDim(num_witems, 1, 1);
        dispatch.Submit(queue);

        Delay(5);

        LOG() << "Starting iteration for " << std::dec << num_witems
              << " work items(s) (targeting " << std::dec << cwsr_thresh
              << " CWSRs)" << std::endl;

        for (int num_cwsrs = 0; num_cwsrs < cwsr_thresh; num_cwsrs++) {

            // Send dequeue request
            EXPECT_SUCCESS(queue.Update(0, BaseQueue::DEFAULT_PRIORITY, false));

            Delay(5);

            // Send requeue request
            EXPECT_SUCCESS(queue.Update(100, BaseQueue::DEFAULT_PRIORITY, false));

            Delay(50);

            // Check for reg mangling
            for (int i = 0; i < num_witems; i++) {
                EXPECT_EQ(outval, output[i]);
            }
        }

        LOG() << "Successful completion for " << std::dec << num_witems
              << " work item(s) (CWSRs triggered: " << std::dec << cwsr_thresh
              << ")" << std::endl;
        LOG() << "Signalling shader stop..." << std::endl;

        inputBuf.Fill(stopval);

        // Wait for shader to finish or timeout if shader has vm page fault
        EXPECT_EQ(0, dispatch.SyncWithStatus(180000));

        EXPECT_SUCCESS(queue.Destroy());
    } else {
        LOG() << "Skipping test: No CWSR present for family ID 0x" << m_FamilyId << "." << std::endl;
    }

    TEST_END
}

/**
 * Instantiates various KFDCWSRTest.BasicTest parameterizations
 * Tuple Format: (num_witems, cwsr_thresh)
 *
 * num_witems:    Defines the number of work-items.
 * cwsr_thresh:   Defines the number of CWSRs to trigger.
 */
INSTANTIATE_TEST_CASE_P(
    , KFDCWSRTest,
    ::testing::Values(
            std::make_tuple(1, 10),     /* Single Wave Test,  10 CWSR Triggers */
            std::make_tuple(256, 50),   /* Multi Wave Test,   50 CWSR Triggers */
            std::make_tuple(512, 100),  /* Multi Wave Test,  100 CWSR Triggers */
            std::make_tuple(1024, 1000) /* Multi Wave Test, 1000 CWSR Triggers */
    )
);

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

        ASSERT_SUCCESS(m_pAsm->RunAssembleBuf(InfiniteLoopIsa, isaBuffer.As<char*>()));

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
