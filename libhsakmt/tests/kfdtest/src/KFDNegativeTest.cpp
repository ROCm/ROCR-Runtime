/*
 * Copyright (C) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "KFDNegativeTest.hpp"
#include "Dispatch.hpp"
#include <sys/ptrace.h>

void KFDNegativeTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    ROUTINE_END
}

void KFDNegativeTest::TearDown() {
    ROUTINE_START

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

/**
 *  Basic Pipe Reset Test
 *
 *  KFD pipe reset sequence:
 *  - on HWS preemption hang KFD will scan the device and find the blocked
 *    hardware queue slot.
 *  - KFD will attempt to queue reset.
 *  - Bad packet lengths should cause queue reset to fail and the KFD will
 *    automatically fall back to pipe reset.
 *  - KFD will verify success by checking blocked hardware slot is now unnoccupied.
 *  - KFD should only signal a reset exception to processes that have had queues
 *    reset.
 */
TEST_F(KFDNegativeTest, BasicPipeReset) {
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    const HsaNodeProperties *nodeProps = m_NodeInfo.GetNodeProperties(defaultGPUNode);
    bool perQueueResetSupported = nodeProps->Capability.ui32.PerQueueResetSupported;

    if (perQueueResetSupported) {
        int pipefd[2];
        pipe(pipefd);

        pid_t childPid = fork();

        if (childPid == 0) {
            // Refresh setup for HSA device and mem buffer use in child
            KFDBaseComponentTest::TearDown();
            KFDBaseComponentTest::SetUp();

            HsaEvent *resetEvent;
            ASSERT_SUCCESS(CreateHWExceptionEvent(false, false, defaultGPUNode, &resetEvent));

            LOG() << "Child ==> Wait on parent to set reset event" << std::endl;
            char buf;
            read(pipefd[0], &buf, 1);

            PM4Queue queue;
            ASSERT_SUCCESS(queue.Create(defaultGPUNode));

            PM4ReleaseMemoryPacket packet = PM4ReleaseMemoryPacket(m_FamilyId, true, 0, 0, false, false, 1);
            queue.PlaceAndSubmitPacket(packet);
            LOG() << "Child ==> Launching packet with bad header then dequeue" << std::endl;
            queue.Wait4PacketConsumption();
            queue.Destroy();

            // child expects hw exception event
            EXPECT_SUCCESS(hsaKmtWaitOnEvent(resetEvent, g_TestTimeOut));
            EXPECT_EQ(resetEvent->EventData.EventType, HSA_EVENTTYPE_HW_EXCEPTION);

            LOG() << "Child ==> Complete" << std::endl;

            exit(0);
	} else {
            int childStatus = 0;

            HsaEvent *resetEvent;

            ASSERT_SUCCESS(CreateHWExceptionEvent(false, false, defaultGPUNode, &resetEvent));

            char buf = 'x';
            write(pipefd[1], &buf, 1);
            LOG() << "Parent ==> Wait on child to launch bad packet" << std::endl;
            waitpid(childPid, &childStatus, 0);

            // parent process should not intercept reset event on child queue reset
            EXPECT_NE(HSAKMT_STATUS_SUCCESS, hsaKmtWaitOnEvent(resetEvent, 100));

            HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode, false);
            destBuf.Fill(0xFF);
            HsaEvent *event;
            ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, defaultGPUNode, &event));

            PM4Queue queue;
            ASSERT_SUCCESS(queue.Create(defaultGPUNode));

            LOG() << "Parent ==> Submit queue packet to verify process is healthy" << std::endl;
            queue.PlaceAndSubmitPacket(PM4WriteDataPacket(destBuf.As<unsigned int*>(), 0, 0));
            queue.Wait4PacketConsumption(event);
            EXPECT_TRUE(WaitOnValue(destBuf.As<unsigned int*>(), 0));

            hsaKmtDestroyEvent(event);
            hsaKmtDestroyEvent(resetEvent);
            EXPECT_SUCCESS(queue.Destroy());

            LOG() << "Parent ==> Complete" << std::endl;
	}
    } else {
        LOG() << "Skipping test: Family ID 0x" << m_FamilyId << " with per-queue reset support = "
              << perQueueResetSupported << std::endl;
    }

    TEST_END
}

