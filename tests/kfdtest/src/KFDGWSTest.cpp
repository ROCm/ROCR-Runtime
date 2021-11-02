/*
 * Copyright (C) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "KFDGWSTest.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "Dispatch.hpp"

void KFDGWSTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    ROUTINE_END
}

void KFDGWSTest::TearDown() {
    ROUTINE_START

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

TEST_F(KFDGWSTest, Allocate) {
    TEST_START(TESTPROFILE_RUNALL);

    HSAuint32 firstGWS;
    PM4Queue queue;
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    const HsaNodeProperties *pNodeProperties = m_NodeInfo.HsaDefaultGPUNodeProperties();
    if (!pNodeProperties || !pNodeProperties->NumGws) {
        LOG() << "Skip test: GPU node doesn't support GWS" << std::endl;
        return;
    }

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    ASSERT_SUCCESS(hsaKmtAllocQueueGWS(queue.GetResource()->QueueId,
			    pNodeProperties->NumGws,&firstGWS));
    EXPECT_EQ(0, firstGWS);
    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDGWSTest, Semaphore) {
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    const HsaNodeProperties *pNodeProperties = m_NodeInfo.HsaDefaultGPUNodeProperties();
    HSAuint32 firstGWS;
    HSAuint32 numResources = 1;
    PM4Queue queue;

    if (!pNodeProperties || !pNodeProperties->NumGws) {
        LOG() << "Skip test: GPU node doesn't support GWS" << std::endl;
        return;
    }

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer buffer(PAGE_SIZE, defaultGPUNode, true, false, false);
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    ASSERT_SUCCESS(hsaKmtAllocQueueGWS(queue.GetResource()->QueueId,
			    pNodeProperties->NumGws,&firstGWS));
    EXPECT_EQ(0, firstGWS);

    ASSERT_SUCCESS(m_pAsm->RunAssembleBuf(GwsInitIsa, isaBuffer.As<char*>()));

    Dispatch dispatch0(isaBuffer);
    buffer.Fill(numResources, 0, 4);
    dispatch0.SetArgs(buffer.As<void*>(), NULL);
    dispatch0.Submit(queue);
    dispatch0.Sync();

    ASSERT_SUCCESS(m_pAsm->RunAssembleBuf(GwsAtomicIncreaseIsa, isaBuffer.As<char*>()));

    Dispatch dispatch(isaBuffer);
    dispatch.SetArgs(buffer.As<void*>(), NULL);
    dispatch.SetDim(1024, 16, 16);

    dispatch.Submit(queue);
    dispatch.Sync();

    EXPECT_EQ(1024*16*16+1, *buffer.As<uint32_t *>());
    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

