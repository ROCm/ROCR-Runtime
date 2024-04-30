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

#include "KFDHWSTest.hpp"

void KFDHWSTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    ROUTINE_END
}

void KFDHWSTest::TearDown() {
    ROUTINE_START

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

void KFDHWSTest::RunTest(unsigned nProcesses, unsigned nQueues, unsigned nLoops) {
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    unsigned q, l;
    bool timeout = false;

    /* Fork the child processes */
    ForkChildProcesses(nProcesses);

    // Create queues
    PM4Queue *queues = new PM4Queue[nQueues];
    for (q = 0; q < nQueues; q++)
        ASSERT_SUCCESS(queues[q].Create(defaultGPUNode));

    // Create dispatch pointers. Each loop iteration creates fresh dispatches
    Dispatch **dispatch = new Dispatch*[nQueues];
    for (q = 0; q < nQueues; q++)
        dispatch[q] = NULL;

    // Logging: Each process prints its index after each loop iteration, all in one line.
    std::ostream &log = LOG() << std::dec << "Process " << m_ProcessIndex << " starting." << std::endl;

    // Run work on all queues
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);

    ASSERT_SUCCESS(m_pAsm->RunAssembleBuf(NoopIsa, isaBuffer.As<char*>()));

    for (l = 0; l < nLoops; l++) {
        for (q = 0; q < nQueues; q++) {
            if (dispatch[q])
                delete dispatch[q];
            dispatch[q] = new Dispatch(isaBuffer);
            dispatch[q]->SetArgs(NULL, NULL);
            dispatch[q]->SetDim(1, 1, 1);
            dispatch[q]->Submit(queues[q]);
        }
        for (q = 0; q < nQueues; q++) {
            timeout = dispatch[q]->SyncWithStatus(g_TestTimeOut);
            if (timeout)
                goto timeout;
        }
        log << m_ProcessIndex;
    }

timeout:
    log << std::endl;
    if (timeout) {
        WARN() << "Process " << m_ProcessIndex << " timeout." << std::endl;
    } else {
        LOG() << "Process " << m_ProcessIndex << " done. Waiting ..." << std::endl;

        // Wait here before destroying queues. If another process' queues
        // are soft-hanging, destroying queues can resolve the soft-hang
        // by changing the run list. Make sure the other process's
        // dispatches have a chance to time out first.
        Delay(g_TestTimeOut+1000);
    }

    // Destroy queues and dispatches. Destroying the queues first
    // ensures that the memory allocated by the Dispatch is no longer
    // accessed by the GPU.
    LOG() << "Process " << m_ProcessIndex << " cleaning up." << std::endl;
    for (q = 0; q < nQueues; q++) {
        EXPECT_SUCCESS(queues[q].Destroy());
        if (dispatch[q])
            delete dispatch[q];
    }
    delete[] queues;
    delete[] dispatch;

    // This is after all the cleanup to avoid leaving any garbage
    // behind, but before WaitChildProcesses to ensure a child process
    // with a timeout exits with an error that can be detected by the
    // parent.
    ASSERT_FALSE(timeout);

    WaitChildProcesses();
}

TEST_F(KFDHWSTest, MultiProcessOversubscribed) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    RunTest(3, 13, 40);

    TEST_END
}
