/*
 * Copyright (C) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "BaseDebug.hpp"
#include "KFDDBGTest.hpp"
#include <sys/ptrace.h>
#include <poll.h>
#include "linux/kfd_ioctl.h"
#include "KFDQMTest.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "Dispatch.hpp"
#include <string>

void KFDDBGTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    ROUTINE_END
}

void KFDDBGTest::TearDown() {
    ROUTINE_START

    /* Reset the user trap handler */
    hsaKmtSetTrapHandler(m_NodeInfo.HsaDefaultGPUNode(), 0, 0, 0, 0);

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

/*
 * To test debug attaching to a spawned process (i.e. attach prior to the tracee
 * opening a KFD device), have the child request the parent to PTRACE attach and
 * wait for the parent to debug attach then allow the child to runtime enable.
 *
 * The following will be exercised:
 * - The KFD shall create a KFD process on behalf of the tracee during debug
 *   attach since the tracee has not opened a KFD device.
 * - Runtime enable on the tracee shall raise an event to the debugging parent
 *   and block until parent has signalled that it has recieved the runtime
 *   enable event.
 * - Tracee should follow a similar hand shake for runtime disable and debug
 *   detach should follow.
 *
 * */
TEST_F(KFDDBGTest, AttachToSpawnedProcess) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId >= FAMILY_AI) {

        if (hsaKmtCheckRuntimeDebugSupport()) {
            LOG() << "Skip test as debug API not supported";
            goto exit;
        }

        pid_t childPid = fork();

        if (childPid == 0) { /* Debugged process */
            uint32_t rDebug;
            int r;

            /* Let parent become the debugger and wait for attach. */
            ptrace(PTRACE_TRACEME);
            raise(SIGSTOP);

            r = hsaKmtOpenKFD();

            if (r != HSAKMT_STATUS_SUCCESS) {
                WARN() << "KFD open failed in debugged process" << std::endl;
                exit(1);
            }

            LOG() << std::dec << "--- Debugged PID " << getpid() << " runtime enable" << std::endl;

            r = hsaKmtRuntimeEnable(&rDebug, true);

            if (r != HSAKMT_STATUS_SUCCESS) {
                WARN() << "Runtime enabled failed" << std::endl;
                exit(1);
            }

            LOG() << std::dec << "--- Debugged PID " << getpid() << " runtime disable and exit" << std::endl;

            hsaKmtRuntimeDisable();

            exit(0);
        } else {
            BaseDebug *debug = new BaseDebug();
            struct kfd_runtime_info r_info = {0};
            uint64_t runtimeMask = KFD_EC_MASK(EC_PROCESS_RUNTIME);
            int childStatus;

            waitpid(childPid, &childStatus, 0);
            while (!WIFSTOPPED(childStatus));

            /* Attach and let new debugged process continue with runtime enable */
            LOG() << std::dec << "Attaching to PID " << childPid  << std::endl;
            ASSERT_SUCCESS(debug->Attach(&r_info, sizeof(r_info), childPid, runtimeMask));
            ASSERT_EQ(r_info.runtime_state, DEBUG_RUNTIME_STATE_DISABLED);
            ASSERT_EQ(r_info.ttmp_setup, false);

            ptrace(PTRACE_CONT, childPid, NULL, NULL);

            /* Wait and unblock runtime enable */
            ASSERT_SUCCESS(debug->QueryDebugEvent(&runtimeMask, NULL, NULL, 5000));
            ASSERT_EQ(runtimeMask, KFD_EC_MASK(EC_PROCESS_RUNTIME));
            ASSERT_SUCCESS(debug->SendRuntimeEvent(runtimeMask, 0, 0));

            /* Wait and unblock runtime disable */
            ASSERT_SUCCESS(debug->QueryDebugEvent(&runtimeMask, NULL, NULL, 5000));
            ASSERT_EQ(runtimeMask, KFD_EC_MASK(EC_PROCESS_RUNTIME));
            ASSERT_SUCCESS(debug->SendRuntimeEvent(runtimeMask, 0, 0));

            LOG() << std::dec << "Detaching from PID " << childPid << std::endl;
            debug->Detach();

            ptrace(PTRACE_DETACH, childPid, NULL, NULL);

            LOG() << std::dec << "Waiting on PID " << childPid << " to exit" << std::endl;
            waitpid(childPid, &childStatus, 0);
            EXPECT_EQ(WIFEXITED(childStatus), true);
            EXPECT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        }
    } else {
        LOG() << "Skipping test: Test not supported on family ID 0x"
              << m_FamilyId << "." << std::endl;
    }
exit:
    LOG() << std::endl;
    TEST_END
}

/*
 * Unlike AttachToSpawnedProcess, the debug parent will only attach after
 * a non-blocked runtime enable by the tracee.  The parent should expect
 * a status update that the tracee is runtime enabled on debug attach.
 * Cleanup with appropriate runtime disable and debug detach handshake.
 */
TEST_F(KFDDBGTest, AttachToRunningProcess) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId >= FAMILY_AI) {

        if (hsaKmtCheckRuntimeDebugSupport()) {
            LOG() << "Skip test as debug API not supported";
            goto exit;
        }

    pid_t childPid = fork();

    if (childPid == 0) { /* Debugged process */
            uint32_t rDebug;
            int r;

            r = hsaKmtOpenKFD();

            if (r != HSAKMT_STATUS_SUCCESS) {
                WARN() << "KFD open failed in debugged process" << std::endl;
                exit(1);
             }

             LOG() << std::dec << "--- Debugged PID " << getpid() << " runtime enable" << std::endl;

             r = hsaKmtRuntimeEnable(&rDebug, true);
             if (r != HSAKMT_STATUS_SUCCESS) {
                 WARN() << "Runtime enabled failed" << std::endl;
                 exit(1);
             }

             /* Let parent become the debugger and wait for attach. */
             ptrace(PTRACE_TRACEME);
             raise(SIGSTOP);

             LOG() << std::dec << "--- Debugged PID " << getpid() << " runtime disable and exit" << std::endl;

             hsaKmtRuntimeDisable();

             exit(0);
        } else {
            BaseDebug *debug = new BaseDebug();
            struct kfd_runtime_info r_info = {0};
            uint64_t runtimeMask = KFD_EC_MASK(EC_PROCESS_RUNTIME);
            int childStatus;

            waitpid(childPid, &childStatus, 0);
            while (!WIFSTOPPED(childStatus));

            /* Attach to running process and let it continue */
            LOG() << std::dec << "Attaching to PID " << childPid  << std::endl;
            ASSERT_SUCCESS(debug->Attach(&r_info, sizeof(r_info), childPid, runtimeMask));
            ASSERT_EQ(r_info.runtime_state, DEBUG_RUNTIME_STATE_ENABLED);
            ASSERT_EQ(r_info.ttmp_setup, true);

            ptrace(PTRACE_CONT, childPid, NULL, NULL);

            /* Wait and unblock runtime disable */
            ASSERT_SUCCESS(debug->QueryDebugEvent(&runtimeMask, NULL, NULL, 5000));
            ASSERT_EQ(runtimeMask, KFD_EC_MASK(EC_PROCESS_RUNTIME));
            ASSERT_SUCCESS(debug->SendRuntimeEvent(runtimeMask, 0, 0));

            LOG() << std::dec << "Detaching from PID " << childPid << std::endl;
            debug->Detach();

            ptrace(PTRACE_DETACH, childPid, NULL, NULL);

            LOG() << std::dec << "Waiting on PID " << childPid << " to exit" << std::endl;
            waitpid(childPid, &childStatus, 0);
            EXPECT_EQ(WIFEXITED(childStatus), true);
            EXPECT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        }
    } else {
        LOG() << "Skipping test: Test not supported on family ID 0x"
              << m_FamilyId << "." << std::endl;
    }
exit:
    LOG() << std::endl;
    TEST_END
}
