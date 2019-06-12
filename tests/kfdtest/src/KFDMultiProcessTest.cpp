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

#include "KFDMultiProcessTest.hpp"

void KFDMultiProcessTest::ForkChildProcesses(int nprocesses) {
    int i;

    for (i = 0; i < nprocesses - 1; ++i) {
        pid_t pid = fork();
        ASSERT_GE(pid, 0);

        if (pid == 0) {
            /* Child process */
            /* Cleanup file descriptors copied from parent process
             * then call SetUp->hsaKmtOpenKFD to create new process
             */
            m_psName = "Test process " + std::to_string(i) + " ";
            TearDown();
            SetUp();
            m_ChildPids.clear();
            m_IsParent = false;
            m_ProcessIndex = i;
            return;
        }

        /* Parent process */
        m_ChildPids.push_back(pid);
    }

    m_psName = "Test process " + std::to_string(i) + " ";
    m_ProcessIndex = i;
}

void KFDMultiProcessTest::WaitChildProcesses() {
    if (m_IsParent) {
        /* Only run by parent process */
        int childStatus;
        int childExitOkNum = 0;
        int size = m_ChildPids.size();

        for (HSAuint32 i = 0; i < size; i++) {
            pid_t pid = m_ChildPids.front();

            waitpid(pid, &childStatus, 0);
            if (WIFEXITED(childStatus) == 1 && WEXITSTATUS(childStatus) == 0)
                childExitOkNum++;

            m_ChildPids.erase(m_ChildPids.begin());
        }

        EXPECT_EQ(childExitOkNum, size);
    }

    /* Child process or parent process finished successfully */
    m_ChildStatus = HSAKMT_STATUS_SUCCESS;
}
