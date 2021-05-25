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

#include "KFDDBGTest.hpp"
#include <sys/ptrace.h>
#include <poll.h>
#include "linux/kfd_ioctl.h"
#include "KFDQMTest.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "Dispatch.hpp"
#include <string>

#if 0
static const char* loop_inc_isa = \
"\
shader loop_inc_isa\n\
asic(VI)\n\
type(CS)\n\
vgpr_count(16)\n\
trap_present(1)\n\
/* TODO Enable here Address Watch Exception: */ \n\
/*copy the parameters from scalar registers to vector registers*/\n\
    v_mov_b32  v0, s0\n\
    v_mov_b32  v1, s1\n\
    v_mov_b32  v2, s2\n\
    v_mov_b32  v3, s3\n\
    v_mov_b32  v5, 0 \n\
/*Iteration 1*/\n\
    v_mov_b32 v4, -1\n\
    flat_atomic_inc v5, v[0:1], v4 glc \n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    flat_load_dword v8, v[2:3] \n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    v_cmp_gt_u32 vcc, 2, v5 \n\
/*Iteration 2*/\n\
    v_mov_b32 v4, -1\n\
    flat_atomic_inc v5, v[0:1], v4 glc \n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    flat_load_dword v8, v[2:3] \n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    v_cmp_gt_u32 vcc, 2, v5 \n\
/*Epilogue*/\n\
    v_mov_b32 v5, 0 \n\
    s_endpgm\n\
    \n\
end\n\
";
#endif

static const char* iterate_isa_gfx = \
"\
shader iterate_isa\n\
wave_size(32) \n\
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
    /*compare the result value (v5) to iteration value (v4),*/\n\
    /*and jump if equal (i.e. if VCC is not zero after the comparison)*/\n\
    v_cmp_lt_u32 vcc, v5, v4\n\
    s_cbranch_vccnz LOOP\n\
    flat_store_dword v[2,3], v5\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    s_endpgm\n\
    end\n\
";

static const char* jump_to_trap_gfx = \
"\
shader jump_to_trap\n\
wave_size(32) \n\
type(CS)\n\
/*copy the parameters from scalar registers to vector registers*/\n\
    s_trap 1\n\
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
    /*compare the result value (v5) to iteration value (v4),*/\n\
    /*and jump if equal (i.e. if VCC is not zero after the comparison)*/\n\
    v_cmp_lt_u32 vcc, v5, v4\n\
    s_cbranch_vccnz LOOP\n\
    flat_store_dword v[2,3], v5\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    s_endpgm\n\
    end\n\
";

static const char* trap_handler_gfx = \
"\
shader trap_handler\n\
wave_size(32) \n\
type(CS)\n\
CHECK_VMFAULT:\n\
    /*if trap jumped to by vmfault, restore skip m0 signalling*/\n\
    s_getreg_b32 ttmp14, hwreg(HW_REG_TRAPSTS)\n\
    s_and_b32 ttmp2, ttmp14, 0x800\n\
    s_cbranch_scc1 RESTORE_AND_EXIT\n\
GET_DOORBELL:\n\
    s_mov_b32 ttmp2, exec_lo\n\
    s_mov_b32 ttmp3, exec_hi\n\
    s_mov_b32 exec_lo, 0x80000000\n\
    s_sendmsg 10\n\
WAIT_SENDMSG:\n\
    /*wait until msb is cleared (i.e. doorbell fetched)*/\n\
    s_nop 7\n\
    s_bitcmp0_b32 exec_lo, 0x1F\n\
    s_cbranch_scc0 WAIT_SENDMSG\n\
SEND_INTERRUPT:\n\
    /* set context bit and doorbell and restore exec*/\n\
    s_mov_b32 exec_hi, ttmp3\n\
    s_and_b32 exec_lo, exec_lo, 0xfff\n\
    s_mov_b32 ttmp3, exec_lo\n\
    s_bitset1_b32 ttmp3, 23\n\
    s_mov_b32 exec_lo, ttmp2\n\
    s_mov_b32 ttmp2, m0\n\
    /* set m0, send interrupt and restore m0 and exit trap*/\n\
    s_mov_b32 m0, ttmp3\n\
    s_nop 0x0\n\
    s_sendmsg sendmsg(MSG_INTERRUPT)\n\
    s_mov_b32 m0, ttmp2\n\
RESTORE_AND_EXIT:\n\
    /* restore and increment program counter to skip shader trap jump*/\n\
    s_add_u32 ttmp0, ttmp0, 4\n\
    s_addc_u32 ttmp1, ttmp1, 0\n\
    s_and_b32 ttmp1, ttmp1, 0xffff\n\
    /* restore SQ_WAVE_IB_STS */\n\
    s_lshr_b32 ttmp2, ttmp11, (26 - 15)\n\
    s_and_b32 ttmp2, ttmp2, (0x8000 | 0x1F0000)\n\
    s_setreg_b32 hwreg(HW_REG_IB_STS), ttmp2\n\
    /* restore SQ_WAVE_STATUS */\n\
    s_and_b64 exec, exec, exec\n\
    s_and_b64 vcc, vcc, vcc\n\
    s_setreg_b32 hwreg(HW_REG_STATUS), ttmp12\n\
    s_rfe_b64 [ttmp0, ttmp1]\n\
    end\n\
";

void KFDDBGTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);

    ROUTINE_END
}

void KFDDBGTest::TearDown() {
    ROUTINE_START
    if (m_pIsaGen)
        delete m_pIsaGen;
    m_pIsaGen = NULL;

    /* Reset the user trap handler */
    hsaKmtSetTrapHandler(m_NodeInfo.HsaDefaultGPUNode(), 0, 0, 0, 0);

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

#if 0
// Functionality is deprecated now, keeping code for reference
TEST_F(KFDDBGTest, BasicAddressWatch) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId >= FAMILY_VI) {
        int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
        static const unsigned int TMA_TRAP_COUNT_OFFSET        = 3;
        static const unsigned int TMA_TRAP_COUNT_VALUE         = 3;

        ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

        // Allocate Buffers on System Memory
        HsaMemoryBuffer trapHandler(PAGE_SIZE*2, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
        HsaMemoryBuffer isaBuf(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
        HsaMemoryBuffer dstBuf(PAGE_SIZE*4, defaultGPUNode, true, false, false);
        HsaMemoryBuffer tmaBuf(PAGE_SIZE, defaultGPUNode, false, false, false);

        // Get Address Watch TrapHandler
        m_pIsaGen->GetAwTrapHandler(trapHandler);

        ASSERT_SUCCESS(hsaKmtSetTrapHandler(defaultGPUNode,
                                            trapHandler.As<void *>(),
                                            0x1000,
                                            tmaBuf.As<void*>(),
                                            0x1000));

        m_pIsaGen->CompileShader(loop_inc_isa, "loop_inc_isa", isaBuf);
        PM4Queue queue, queue_flush;

        ASSERT_SUCCESS(queue.Create(defaultGPUNode));
        ASSERT_SUCCESS(queue_flush.Create(defaultGPUNode));

        // Set Address Watch Params
        // TODO: Set WatchMode[1] to Atomic in case we want to test this mode.

        HSA_DBG_WATCH_MODE  WatchMode[2];
        HSAuint64           WatchAddress[2];
        HSAuint64           WatchMask[2];
        HSAKMT_STATUS       AddressWatchSuccess;
        HSAuint64           AddressMask64 = 0x0;
        unsigned char *secDstBuf = (unsigned char *)dstBuf.As<void*>() + 8192;

        WatchMode[0]    = HSA_DBG_WATCH_ALL;
        WatchAddress[0] = (HSAuint64) dstBuf.As<void*>() & (~AddressMask64);
        WatchMask[0]    = ~AddressMask64;
        WatchMode[1]    = HSA_DBG_WATCH_ALL;
        WatchAddress[1] = (HSAuint64)secDstBuf & (~AddressMask64);
        WatchMask[1]    = ~AddressMask64;

        queue_flush.PlaceAndSubmitPacket(PM4WriteDataPacket(dstBuf.As<unsigned int*>(), 0x0, 0x0));
        queue_flush.PlaceAndSubmitPacket(PM4WriteDataPacket((unsigned int *)secDstBuf, 0x0, 0x0));
        Delay(50);
        ASSERT_SUCCESS(hsaKmtDbgRegister(defaultGPUNode));

        AddressWatchSuccess = hsaKmtDbgAddressWatch(
                defaultGPUNode,                                 // IN
                2,                                              // # watch points
                &WatchMode[0],                                  // IN
                reinterpret_cast<void **>(&WatchAddress[0]),    // IN
                &WatchMask[0],                                  // IN, optional
                NULL);                                          // IN, optional

        EXPECT_EQ(AddressWatchSuccess, HSAKMT_STATUS_SUCCESS);

        Dispatch dispatch(isaBuf);
        dispatch.SetArgs(dstBuf.As<void*>(), reinterpret_cast<void *>(secDstBuf));
        dispatch.SetDim(1, 1, 1);

        /* TODO: Use Memory ordering rules w/ atomics for host-GPU memory syncs.
         * Set to std::memory_order_seq_cst
         */

        dispatch.Submit(queue);

        Delay(50);
        dispatch.Sync(g_TestTimeOut);

        // Check that we got trap handler calls due to add watch triggers
        EXPECT_GE(*(tmaBuf.As<unsigned int*>()+ TMA_TRAP_COUNT_OFFSET), TMA_TRAP_COUNT_VALUE);

        EXPECT_SUCCESS(hsaKmtDbgUnregister(defaultGPUNode));
        EXPECT_SUCCESS(queue.Destroy());
        EXPECT_SUCCESS(queue_flush.Destroy());
    } else {
        LOG() << "Skipping test: Test not supported on family ID 0x" << m_FamilyId << "." << std::endl;
    }
    TEST_END
}
#endif

#if 0
// Deprecated - Reference Use Only
/**
 * checkDebugVersion:
 *   Inputs:
 *       HSAuint32 requiredMajor
 *           -- Required major version number
 *       HSAuint32 requiredMinor
 *           -- Required minor version number
 *   Output:
 *     bool:
 *            i)   false if major version of thunk and kernel are not the same
 *            ii)  false if kernel minor version is less than the required
 *                    version if the major version is the required version.
 *            iii) false if thunk minor version is less than the required
 *                    version if the major version is the required version.
 *            iv)  false if hsaKmtGetKernelDebugTrapVersionInfo() call fails.
 *            v)   true otherwise.
 *
*/
static bool checkDebugVersion(HSAuint32 requiredMajor, HSAuint32 requiredMinor)
{
    HSAuint32 kernelMajorNumber = 0;
    HSAuint32 kernelMinorNumber = 0;
    HSAuint32 thunkMajorNumber = 0;
    HSAuint32 thunkMinorNumber = 0;

    hsaKmtGetThunkDebugTrapVersionInfo(&thunkMajorNumber, &thunkMinorNumber);

    if (hsaKmtGetKernelDebugTrapVersionInfo(&kernelMajorNumber,
                &kernelMinorNumber)) {
        LOG() << "Failed to get kernel debugger version!" << std::endl;
        return false;
    }

    if (kernelMajorNumber != thunkMajorNumber)
        return false;

    if (kernelMajorNumber < requiredMajor ||
            (kernelMajorNumber == requiredMajor &&
             kernelMinorNumber < requiredMinor))
        return false;

    if (thunkMajorNumber < requiredMajor ||
            (thunkMajorNumber == requiredMajor &&
             thunkMinorNumber < requiredMinor))
        return false;

    return true;
}
#endif

#if 0
// Deprecated - Reference Use Only
TEST_F(KFDDBGTest, BasicDebuggerSuspendResume) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId >= FAMILY_AI) {
        int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();

        ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

        HSAuint32 Flags = 0;

        if(!checkDebugVersion(0, 2)) {
                LOG() << "Test disabled due to debug API version mismatch";
                goto exit;
        }

        HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
        HsaMemoryBuffer iterateBuf(PAGE_SIZE, defaultGPUNode, true, false, false);
        HsaMemoryBuffer resultBuf(PAGE_SIZE, defaultGPUNode, true, false, false);

        unsigned int* iter = iterateBuf.As<unsigned int*>();
        unsigned int* result = resultBuf.As<unsigned int*>();

        int suspendTimeout = 500;
        int syncStatus;

        m_pIsaGen->CompileShader(iterate_isa_gfx,
                                    "iterate_isa",
                                    isaBuffer);

        PM4Queue queue1;
        HsaQueueResource *qResources;
        HSA_QUEUEID queue_ids[2];

        ASSERT_SUCCESS(queue1.Create(defaultGPUNode));

        Dispatch *dispatch1;

        dispatch1 = new Dispatch(isaBuffer);

        dispatch1->SetArgs(&iter[0], &result[0]);
        dispatch1->SetDim(1, 1, 1);

        // Need a loop large enough so we don't finish before we call Suspend.
        //  150000000 takes between 5 and 6 seconds, which is long enough
        //  to test the suspend/resume.
        iter[0] = 150000000;

        ASSERT_EQ(ptrace(PTRACE_TRACEME, 0, 0, 0), 0);
        ASSERT_SUCCESS(hsaKmtEnableDebugTrap(defaultGPUNode, INVALID_QUEUEID));

        // Submit the shader, queue1
        dispatch1->Submit(queue1);
        qResources = queue1.GetResource();
        queue_ids[0] = qResources->QueueId;

        ASSERT_SUCCESS(hsaKmtQueueSuspend(
                    INVALID_PID,
                    1,  // one queue
                    queue_ids,
                    10,  // grace period
                    Flags));

        syncStatus = dispatch1->SyncWithStatus(suspendTimeout);
        ASSERT_NE(syncStatus, HSAKMT_STATUS_SUCCESS);

        ASSERT_NE(iter[0], result[0]);

        // The shader hasn't finished, we will wait for 20 seconds,
        // and then check if it has finished.  If it was suspended,
        // it should not have finished.
        Delay(20000);

        // Check that the shader has not finished yet.
        syncStatus = dispatch1->SyncWithStatus(suspendTimeout);
        ASSERT_NE(syncStatus, HSAKMT_STATUS_SUCCESS);

        ASSERT_NE(iter[0], result[0]);

        ASSERT_SUCCESS(hsaKmtQueueResume(
                    INVALID_PID,
                    1,  // Num queues
                    queue_ids,
                    Flags));

        dispatch1->Sync();
        ASSERT_EQ(iter[0], result[0]);

        EXPECT_SUCCESS(queue1.Destroy());

        ASSERT_SUCCESS(hsaKmtDisableDebugTrap(defaultGPUNode));

    } else {
        LOG() << "Skipping test: Test not supported on family ID 0x" << m_FamilyId << "." << std::endl;
    }
exit:
    LOG() << std::endl;
    TEST_END
}
#endif

#if 0
// Deprecated - Reference Use Only
TEST_F(KFDDBGTest, BasicDebuggerQueryQueueStatus) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId >= FAMILY_AI) {
        int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
        HSAint32 PollFd;

        ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

        if (!checkDebugVersion(0, 2)) {
                LOG() << "Test disabled due to debug API version mismatch";
                goto exit;
        }

        // enable debug trap and check poll fd creation
        ASSERT_SUCCESS(hsaKmtEnableDebugTrapWithPollFd(defaultGPUNode,
                                                       INVALID_QUEUEID,
                                                       &PollFd));
        ASSERT_GT(PollFd, 0) << "failed to create polling file descriptor";

        // create shader and trap bufs then enable 2nd level trap
        HsaMemoryBuffer isaBuf(PAGE_SIZE, defaultGPUNode, true, false, true);
        HsaMemoryBuffer iterBuf(PAGE_SIZE, defaultGPUNode, true, false, false);
        HsaMemoryBuffer resBuf(PAGE_SIZE, defaultGPUNode, true, false, false);

        HsaMemoryBuffer trap(PAGE_SIZE*2, defaultGPUNode, true, false, true);
        HsaMemoryBuffer tmaBuf(PAGE_SIZE, defaultGPUNode, false, false, false);

        ASSERT_SUCCESS(hsaKmtSetTrapHandler(defaultGPUNode,
                                            trap.As<void *>(),
                                            0x1000,
                                            tmaBuf.As<void*>(),
                                            0x1000));

        // compile and dispatch shader
        m_pIsaGen->CompileShader(jump_to_trap_gfx, "jump_to_trap", isaBuf);
        m_pIsaGen->CompileShader(trap_handler_gfx, "trap_handler", trap);

        PM4Queue queue;
        HsaQueueResource *qResources;
        ASSERT_SUCCESS(queue.Create(defaultGPUNode));

        unsigned int* iter = iterBuf.As<unsigned int*>();
        unsigned int* result = resBuf.As<unsigned int*>();
        int suspendTimeout = 500;
        int syncStatus;
        iter[0] = 150000000;
        Dispatch *dispatch;
        dispatch = new Dispatch(isaBuf);
        dispatch->SetArgs(&iter[0], &result[0]);
        dispatch->SetDim(1, 1, 1);

        dispatch->Submit(queue);
        qResources = queue.GetResource();

        // poll, read and query for pending trap event
        struct pollfd fds[1];
        fds[0].fd = PollFd;
        fds[0].events = POLLIN | POLLRDNORM;
        ASSERT_GT(poll(fds, 1, 5000), 0);

        char trapChar;
        ASSERT_GT(read(PollFd, &trapChar, 1), 0);
        ASSERT_EQ('t', trapChar);

        HSAuint32 invalidQid = 0xffffffff;
        HSAuint32 qid = invalidQid;
        HSA_QUEUEID queueIds[1] = { qResources->QueueId};
        HSA_DEBUG_EVENT_TYPE EventReceived;
        bool IsSuspended = false;
        bool IsNew = false;

        ASSERT_SUCCESS(hsaKmtQueryDebugEvent(defaultGPUNode, INVALID_PID, &qid,
                                             false, &EventReceived,
                                             &IsSuspended, &IsNew));
        ASSERT_NE(qid, invalidQid);
        ASSERT_EQ(IsSuspended, false);
        ASSERT_EQ(IsNew, true);
        ASSERT_EQ(EventReceived, HSA_DEBUG_EVENT_TYPE_TRAP);

        // suspend queue, get snapshot, query suspended queue
        // and clear pending event
        ASSERT_SUCCESS(hsaKmtQueueSuspend(INVALID_PID, 1, queueIds, 10, 0));

        syncStatus = dispatch->SyncWithStatus(suspendTimeout);
        ASSERT_NE(syncStatus, HSAKMT_STATUS_SUCCESS);
        ASSERT_NE(iter[0], result[0]);

        struct kfd_queue_snapshot_entry qssBuf[1] = {};
        HSAuint32 QssEntries = 0;

        // get only number of queues and don't update the snapshot buffer
        ASSERT_SUCCESS(hsaKmtGetQueueSnapshot(INVALID_NODEID, INVALID_PID,
                                              false,
                                              reinterpret_cast<void *>(qssBuf),
                                              &QssEntries));

        ASSERT_EQ(QssEntries, 1);
        ASSERT_EQ(qssBuf[0].ctx_save_restore_address, 0);
        ASSERT_EQ(qssBuf[0].ring_base_address, 0);
        ASSERT_EQ(qssBuf[0].ring_size, 0);

        // update the snapshot buffer
        QssEntries = 1;
        ASSERT_SUCCESS(hsaKmtGetQueueSnapshot(INVALID_NODEID, INVALID_PID,
                                              false,
                                              reinterpret_cast<void *>(qssBuf),
                                              &QssEntries));

        ASSERT_EQ(QssEntries, 1);
        ASSERT_NE(qssBuf[0].ctx_save_restore_address, 0);
        ASSERT_NE(qssBuf[0].ring_base_address, 0);
        ASSERT_NE(qssBuf[0].ring_size, 0);

        ASSERT_SUCCESS(hsaKmtQueryDebugEvent(defaultGPUNode, INVALID_PID,
                                             &qid, true, &EventReceived,
                                             &IsSuspended, &IsNew));
        ASSERT_EQ(IsSuspended, true);

        ASSERT_SUCCESS(hsaKmtQueueResume(INVALID_PID, 1, queueIds, 0));

        ASSERT_SUCCESS(hsaKmtQueryDebugEvent(defaultGPUNode, INVALID_PID, &qid,
                                             false, &EventReceived,
                                             &IsSuspended, &IsNew));

        ASSERT_EQ(IsSuspended, false);
        ASSERT_EQ(IsNew, false);
        ASSERT_EQ(EventReceived, HSA_DEBUG_EVENT_TYPE_NONE);

        dispatch->Sync();
        ASSERT_EQ(iter[0], result[0]);
        EXPECT_SUCCESS(queue.Destroy());
        ASSERT_SUCCESS(hsaKmtDisableDebugTrap(defaultGPUNode));
        ASSERT_EQ(close(PollFd), 0);

    } else {
        LOG() << "Skipping test: Test not supported on family ID 0x"
              << m_FamilyId << "." << std::endl;
    }
exit:
    LOG() << std::endl;
    TEST_END
}
#endif

#if 0
// clean up routine
// Deprecated - Reference Use Only
static void ExitVMFaultQueryChild(std::string errMsg,
                                  int exitStatus,
                                  HSAint32 pollFd,
                                  PM4Queue *queue1,
                                  PM4Queue *queue2,
                                  HsaEvent *event,
                                  int gpuNode) {
    if (queue1)
        queue1->Destroy();

    if (queue2)
        queue2->Destroy();

    if (event) {
        int ret = hsaKmtDestroyEvent(event);
        if (ret) {
            exitStatus = 1;
            errMsg = "event failed to be destroyed";
        }
    }

    if (pollFd >= 0)
        close(pollFd);

    if (gpuNode >= 0) {
        int ret = hsaKmtDisableDebugTrap(gpuNode);
        if (ret) {
            exitStatus = 1;
            errMsg = "debug trap failed to disable";
        }
    }

    if (!errMsg.empty())
        WARN() << errMsg << std::endl;

    exit(exitStatus);
}
#endif

#if 0
// Deprecated - Reference Use Only
TEST_F(KFDDBGTest, BasicDebuggerQueryVMFaultQueueStatus) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId >= FAMILY_AI) {
        int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();

        ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

        if (!checkDebugVersion(0, 2)) {
                LOG() << "Test disabled due to debug API version mismatch";
                goto exit;
        }

        pid_t childPid = fork();
        ASSERT_GE(childPid, 0);

        // fork to child since vm faults halt all queues
        if (childPid == 0) {
            HSAint32 PollFd;
            HSAKMT_STATUS ret;
            bool childStatus;

            ret = hsaKmtOpenKFD();
            if (ret != HSAKMT_STATUS_SUCCESS)
                ExitVMFaultQueryChild("KFD open failed",
                                      1, -1, NULL, NULL, NULL, -1);

            // enable debug trap and check poll fd creation
            ret = hsaKmtEnableDebugTrapWithPollFd(defaultGPUNode,
                                                  INVALID_QUEUEID,
                                                  &PollFd);

            if (ret != HSAKMT_STATUS_SUCCESS || PollFd <= 0)
                ExitVMFaultQueryChild("enable debug trap with poll fd failed",
                                      1, -1, NULL, NULL, NULL, defaultGPUNode);

            // create shader, vmfault and trap bufs then enable 2nd level trap
            HsaMemoryBuffer vmFaultBuf(PAGE_SIZE, defaultGPUNode, true, false,
                                       true);
            HsaMemoryBuffer srcBuf(PAGE_SIZE, defaultGPUNode, false);
            srcBuf.Fill(0xABCDABCD);

            HsaMemoryBuffer isaBuf(PAGE_SIZE, defaultGPUNode, true, false,
                                   true);
            HsaMemoryBuffer iterBuf(PAGE_SIZE, defaultGPUNode, true, false,
                                    false);
            HsaMemoryBuffer resBuf(PAGE_SIZE, defaultGPUNode, true, false,
                                   false);

            HsaMemoryBuffer trap(PAGE_SIZE*2, defaultGPUNode, true, false,
                                 true);
            HsaMemoryBuffer tmaBuf(PAGE_SIZE, defaultGPUNode, false, false,
                                   false);

            ret = hsaKmtSetTrapHandler(defaultGPUNode,
                                       trap.As<void *>(), 0x1000,
                                       tmaBuf.As<void*>(), 0x1000);

            if (ret != HSAKMT_STATUS_SUCCESS)
                ExitVMFaultQueryChild("setting trap handler failed",
                                      1, PollFd, NULL, NULL, NULL,
                                      defaultGPUNode);

            // compile and dispatch shader
            m_pIsaGen->CompileShader(jump_to_trap_gfx, "jump_to_trap",
                                     isaBuf);
            m_pIsaGen->CompileShader(trap_handler_gfx, "trap_handler", trap);

            PM4Queue queue1, queue2;
            HSAuint32 qid1;
            if (queue1.Create(defaultGPUNode) != HSAKMT_STATUS_SUCCESS)
                ExitVMFaultQueryChild("queue 1 creation failed",
                                      1, PollFd, NULL, NULL, NULL,
                                      defaultGPUNode);

            if (queue2.Create(defaultGPUNode) != HSAKMT_STATUS_SUCCESS)
                ExitVMFaultQueryChild("queue 2 creation failed",
                                      1, PollFd, &queue1, NULL, NULL,
                                      defaultGPUNode);

            unsigned int* iter = iterBuf.As<unsigned int*>();
            unsigned int* result = resBuf.As<unsigned int*>();
            int suspendTimeout = 500;
            iter[0] = 150000000;
            Dispatch *dispatch1;
            dispatch1 = new Dispatch(isaBuf);
            dispatch1->SetArgs(&iter[0], &result[0]);
            dispatch1->SetDim(1, 1, 1);
            dispatch1->Submit(queue1);

            // poll, read and query pending trap event
            struct pollfd fds[1];
            fds[0].fd = PollFd;
            fds[0].events = POLLIN | POLLRDNORM;
            if (poll(fds, 1, 5000) <= 0)
                ExitVMFaultQueryChild("poll wake on pending trap event failed",
                                      1, PollFd, &queue1, &queue2, NULL,
                                      defaultGPUNode);

            int kMaxSize = 4096;
            char fifoBuf[kMaxSize];
            childStatus = read(PollFd, fifoBuf, 1) == -1\
                          || strchr(fifoBuf, 't') == NULL;
            if (childStatus)
                ExitVMFaultQueryChild("read on pending trap event failed",
                                      1, PollFd, &queue1, &queue2, NULL,
                                      defaultGPUNode);

            memset(fifoBuf, 0, sizeof(fifoBuf));

            HSA_DEBUG_EVENT_TYPE EventReceived;
            bool IsSuspended;
            bool IsNew;
            HSAuint32 invalidQid = 0xffffffff;
            qid1 = invalidQid;

            ret = hsaKmtQueryDebugEvent(defaultGPUNode, INVALID_PID, &qid1,
                                        false, &EventReceived, &IsSuspended,
                                        &IsNew);

            childStatus = ret != HSAKMT_STATUS_SUCCESS
                                 || EventReceived != HSA_DEBUG_EVENT_TYPE_TRAP;
            if (childStatus)
                ExitVMFaultQueryChild("query on pending trap event failed",
                                      1, PollFd, &queue1, &queue2, NULL,
                                      defaultGPUNode);

            // create and wait on pending vmfault event
            HsaEvent *vmFaultEvent;
            HsaEventDescriptor eventDesc;
            eventDesc.EventType = HSA_EVENTTYPE_MEMORY;
            eventDesc.NodeId = defaultGPUNode;
            eventDesc.SyncVar.SyncVar.UserData = NULL;
            eventDesc.SyncVar.SyncVarSize = 0;
            ret = hsaKmtCreateEvent(&eventDesc, true, false,
                                            &vmFaultEvent);
            if (ret != HSAKMT_STATUS_SUCCESS)
                ExitVMFaultQueryChild("create vmfault event failed",
                                      1, PollFd, &queue1, &queue2, NULL,
                                      defaultGPUNode);

            Dispatch dispatch2(vmFaultBuf, false);
            dispatch2.SetArgs(
                reinterpret_cast<void *>(srcBuf.As<HSAuint64>()),
                reinterpret_cast<void *>(0xABBAABBAULL));
            dispatch2.SetDim(1, 1, 1);
            dispatch2.Submit(queue2);

            ret = hsaKmtWaitOnEvent(vmFaultEvent, g_TestTimeOut);
            if (ret != HSAKMT_STATUS_SUCCESS)
                ExitVMFaultQueryChild("wait on vmfault event failed",
                                      1, PollFd, &queue1, &queue2,
                                      vmFaultEvent, defaultGPUNode);

            // poll, read and query on pending vmfault event
            if (poll(fds, 1, 5000) <= 0)
                ExitVMFaultQueryChild("poll wake on vmfault event failed",
                                      1, PollFd, &queue1, &queue2,
                                      vmFaultEvent, defaultGPUNode);

            childStatus = read(PollFd, fifoBuf, kMaxSize) == -1
                          || strchr(fifoBuf, 'v') == NULL
                          || strchr(fifoBuf, 't');

            if (childStatus)
                ExitVMFaultQueryChild("read on vmfault event failed",
                                      1, PollFd, &queue1, &queue2,
                                      vmFaultEvent, defaultGPUNode);

            ret = hsaKmtQueryDebugEvent(defaultGPUNode, INVALID_PID,
                                                &qid1, true, &EventReceived,
                                                &IsSuspended, &IsNew);

            childStatus = ret != HSAKMT_STATUS_SUCCESS
                                 || EventReceived !=
                                     HSA_DEBUG_EVENT_TYPE_TRAP_VMFAULT;
            if (childStatus)
                ExitVMFaultQueryChild("query on vmfault event failed",
                                      1, PollFd, &queue1, &queue2,
                                      vmFaultEvent, defaultGPUNode);

            ExitVMFaultQueryChild("", 0, PollFd, &queue1, &queue2,
                                  vmFaultEvent, defaultGPUNode);

        } else {
            int childStatus;
            ASSERT_EQ(childPid, waitpid(childPid, &childStatus, 0));
            ASSERT_NE(0, WIFEXITED(childStatus));
            ASSERT_EQ(0, WEXITSTATUS(childStatus));
        }
    } else {
        LOG() << "Skipping test: Test not supported on family ID 0x"
              << m_FamilyId << "." << std::endl;
    }
exit:
    LOG() << std::endl;
    TEST_END
}
#endif
