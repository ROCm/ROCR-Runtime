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
#include "KFDQMTest.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "Dispatch.hpp"


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

TEST_F(KFDDBGTest, BasicDebuggerSuspendResume) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId >= FAMILY_AI) {
        int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();

        ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

        HSAuint32 Flags = 0;
        HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
        HsaMemoryBuffer iterateBuf(PAGE_SIZE, defaultGPUNode, true, false, false);
        HsaMemoryBuffer resultBuf(PAGE_SIZE, defaultGPUNode, true, false, false);

        unsigned int* iter = iterateBuf.As<unsigned int*>();
        unsigned int* result = resultBuf.As<unsigned int*>();

        int suspendTimeout = 500;
        int syncStatus;

        m_pIsaGen->CompileShader(iterate_isa_gfx9, "iterate_isa", isaBuffer);

        PM4Queue queue1;
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
        queue_ids[0] = 0;

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
    TEST_END
}

