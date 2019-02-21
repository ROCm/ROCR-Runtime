/*
 * Copyright (C) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <sys/time.h>
#include <vector>
#include "KFDQMTest.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "SDMAPacket.hpp"
#include "SDMAQueue.hpp"
#include "AqlQueue.hpp"
#include <algorithm>

#include "Dispatch.hpp"

void KFDQMTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);

    ROUTINE_END
}

void KFDQMTest::TearDown() {
    ROUTINE_START

    if (m_pIsaGen)
        delete m_pIsaGen;
    m_pIsaGen = NULL;

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

TEST_F(KFDQMTest, CreateCpQueue) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode, false);

    destBuf.Fill(0xFF);

    PM4Queue queue;

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    queue.PlaceAndSubmitPacket(PM4WriteDataPacket(destBuf.As<unsigned int*>(), 0, 0));

    queue.Wait4PacketConsumption();

    EXPECT_TRUE(WaitOnValue(destBuf.As<unsigned int*>(), 0));

    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDQMTest, CreateSdmaQueue) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode, false);

    destBuf.Fill(0xFF);

    SDMAQueue queue;

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    queue.PlaceAndSubmitPacket(SDMAWriteDataPacket(destBuf.As<void *>(), 0x02020202));

    queue.Wait4PacketConsumption();

    EXPECT_TRUE(WaitOnValue(destBuf.As<unsigned int*>(), 0x02020202));

    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDQMTest, CreateMultipleSdmaQueues) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    int bufSize = PAGE_SIZE;
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    const unsigned int numSdmaQueues = m_numSdmaEngines * m_numSdmaQueuesPerEngine;

    HsaMemoryBuffer destBuf(bufSize << 1 , defaultGPUNode, false);
    HsaMemoryBuffer srcBuf(bufSize, defaultGPUNode, false);
    destBuf.Fill(0xFF);

    std::vector<SDMAQueue> queues(numSdmaQueues);

    for (unsigned int qidx = 0; qidx < numSdmaQueues; ++qidx)
        ASSERT_SUCCESS(queues[qidx].Create(defaultGPUNode));

    for (unsigned int qidx = 0; qidx < numSdmaQueues; ++qidx) {
        destBuf.Fill(0x0);
        srcBuf.Fill(qidx + 0xa0);
        queues[qidx].PlaceAndSubmitPacket(
            SDMACopyDataPacket(destBuf.As<unsigned int*>(), srcBuf.As<unsigned int*>(), bufSize));
        queues[qidx].PlaceAndSubmitPacket(
            SDMAWriteDataPacket(destBuf.As<unsigned int*>() + bufSize/4, 0x02020202));

        queues[qidx].Wait4PacketConsumption();

        EXPECT_TRUE(WaitOnValue(destBuf.As<unsigned int*>() + bufSize/4, 0x02020202));

        EXPECT_SUCCESS(memcmp(
            destBuf.As<unsigned int*>(), srcBuf.As<unsigned int*>(), bufSize));
    }

    for (unsigned int qidx = 0; qidx < numSdmaQueues; ++qidx)
        EXPECT_SUCCESS(queues[qidx].Destroy());

    TEST_END
}

/* The following test is designed to reproduce an intermittent hang on
 * Fiji and other VI/Polaris GPUs. This test typically hangs in a few
 * seconds. According to analysis done by HW engineers, the culprit
 * seems to be PCIe speed switching. The problem can be worked around
 * by disabling the lowest DPM level on Fiji.
 */
TEST_F(KFDQMTest, SdmaConcurrentCopies) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

#define BUFFER_SIZE (64*1024)
#define NPACKETS 1
#define COPY_SIZE (BUFFER_SIZE / NPACKETS)
    HsaMemoryBuffer srcBuf(BUFFER_SIZE, 0, true);
    HsaMemoryBuffer dstBuf(BUFFER_SIZE, defaultGPUNode, false, is_dgpu() ? true : false);

    SDMAQueue queue;

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    std::ostream &log = LOG();
    char progress[] = "-\b";
    log << "Running ... ";

    for (unsigned i = 0; i < 100000; i++) {
        if (i % 1000 == 0) {
            const char progressSteps[4] = {'-', '\\', '|', '/'};
            progress[0] = progressSteps[(i/1000) % 4];
            log << progress;
        }

        for (unsigned j = 0; j < NPACKETS; j++)
            queue.PlacePacket(
                SDMACopyDataPacket(dstBuf.As<char *>()+COPY_SIZE*j,
                                   srcBuf.As<char *>()+COPY_SIZE*j, COPY_SIZE));
        queue.SubmitPacket();

        /* Waste a variable amount of time. Submission timing
         * while SDMA runs concurrently seems to be critical for
         * reproducing the hang
         */
        for (int k = 0; k < (i & 0xfff); k++)
            memcpy(srcBuf.As<char *>()+PAGE_SIZE, srcBuf.As<char *>(), 1024);

        /* Wait for idle every 8 packets to allow the SDMA engine to
         * run concurrently for a bit without getting too far ahead
         */
        if ((i & 0x7) == 0)
            queue.Wait4PacketConsumption();
    }
    log << "Done." << std::endl;

    queue.PlaceAndSubmitPacket(SDMAWriteDataPacket(srcBuf.As<unsigned *>(), 0x02020202));
    queue.Wait4PacketConsumption();
    EXPECT_TRUE(WaitOnValue(srcBuf.As<unsigned int*>(), 0x02020202));

    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDQMTest, CreateMultipleCpQueues) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    static const unsigned int MAX_CP_QUEUES = 16;

    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode, false);

    destBuf.Fill(0xFF);

    PM4Queue queues[MAX_CP_QUEUES];

    for (unsigned int qidx = 0; qidx < MAX_CP_QUEUES; ++qidx)
        ASSERT_SUCCESS(queues[qidx].Create(defaultGPUNode)) << " QueueId=" << qidx;

    for (unsigned int qidx = 0; qidx < MAX_CP_QUEUES; ++qidx) {
        queues[qidx].PlaceAndSubmitPacket(PM4WriteDataPacket(destBuf.As<unsigned int*>()+qidx*2, qidx, qidx));

        queues[qidx].Wait4PacketConsumption();

        WaitOnValue(destBuf.As<unsigned int*>()+qidx*2, qidx);
    }

    for (unsigned int qidx = 0; qidx < MAX_CP_QUEUES; ++qidx)
       EXPECT_SUCCESS(queues[qidx].Destroy());

    TEST_END
}

TEST_F(KFDQMTest, DisableCpQueueByUpdateWithNullAddress) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode, false);

    destBuf.Fill(0xFFFFFFFF);

    PM4Queue queue;

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    queue.PlaceAndSubmitPacket(PM4WriteDataPacket(destBuf.As<unsigned int*>(), 0, 0));

    queue.Wait4PacketConsumption();

    WaitOnValue(destBuf.As<unsigned int*>(), 0);

    destBuf.Fill(0xFFFFFFFF);

    EXPECT_SUCCESS(queue.Update(BaseQueue::DEFAULT_QUEUE_PERCENTAGE, BaseQueue::DEFAULT_PRIORITY, true));

    queue.PlaceAndSubmitPacket(PM4WriteDataPacket(destBuf.As<unsigned int*>(), 1, 1));

    // Don't sync since we don't expect rptr to change when the queue is disabled.
    Delay(2000);

    EXPECT_EQ(destBuf.As<unsigned int*>()[0], 0xFFFFFFFF)
        << "Packet executed even though the queue is supposed to be disabled!";

    EXPECT_SUCCESS(queue.Update(BaseQueue::DEFAULT_QUEUE_PERCENTAGE, BaseQueue::DEFAULT_PRIORITY, false));

    queue.Wait4PacketConsumption();

    WaitOnValue(destBuf.As<unsigned int*>(), 1);

    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDQMTest, DisableSdmaQueueByUpdateWithNullAddress) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode, false);

    destBuf.Fill(0xFFFFFFFF);

    SDMAQueue queue;

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    queue.PlaceAndSubmitPacket(SDMAWriteDataPacket(destBuf.As<void*>(), 0));

    WaitOnValue(destBuf.As<unsigned int*>(), 0);

    destBuf.Fill(0xFFFFFFFF);

    EXPECT_SUCCESS(queue.Update(BaseQueue::DEFAULT_QUEUE_PERCENTAGE, BaseQueue::DEFAULT_PRIORITY, true));

    queue.PlaceAndSubmitPacket(SDMAWriteDataPacket(destBuf.As<void*>(), 0));

    // Don't sync since we don't expect rptr to change when the queue is disabled.
    Delay(2000);

    EXPECT_EQ(destBuf.As<unsigned int*>()[0], 0xFFFFFFFF)
        << "Packet executed even though the queue is supposed to be disabled!";

    EXPECT_SUCCESS(queue.Update(BaseQueue::DEFAULT_QUEUE_PERCENTAGE, BaseQueue::DEFAULT_PRIORITY, false));

    queue.Wait4PacketConsumption();

    WaitOnValue(destBuf.As<unsigned int*>(), 0);

    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDQMTest, DisableCpQueueByUpdateWithZeroPercentage) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode, false);

    destBuf.Fill(0xFFFFFFFF);

    PM4Queue queue;

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    PM4WriteDataPacket packet1, packet2;
    packet1.InitPacket(destBuf.As<unsigned int*>(), 0, 0);
    packet2.InitPacket(destBuf.As<unsigned int*>(), 1, 1);

    queue.PlaceAndSubmitPacket(packet1);

    queue.Wait4PacketConsumption();

    WaitOnValue(destBuf.As<unsigned int*>(), 0);

    destBuf.Fill(0xFFFFFFFF);

    EXPECT_SUCCESS(queue.Update(0/*percentage*/, BaseQueue::DEFAULT_PRIORITY, false));

    queue.PlaceAndSubmitPacket(packet2);

    // Don't sync since we don't expect rptr to change when the queue is disabled.
    Delay(2000);

    EXPECT_EQ(destBuf.As<unsigned int*>()[0], 0xFFFFFFFF)
        << "Packet executed even though the queue is supposed to be disabled!";

    EXPECT_SUCCESS(queue.Update(BaseQueue::DEFAULT_QUEUE_PERCENTAGE, BaseQueue::DEFAULT_PRIORITY, false));

    queue.Wait4PacketConsumption();

    WaitOnValue(destBuf.As<unsigned int*>(), 1);

    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDQMTest, CreateQueueStressSingleThreaded) {
    TEST_START(TESTPROFILE_RUNALL)

    static const HSAuint64 TEST_TIME_SEC = 15;

    HSAuint64 initialTime = GetSystemTickCountInMicroSec();

    unsigned int numIter = 0;

    HSAuint64 timePassed = 0;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    do {
        // The following means we'll get the order 0,0 => 0,1 => 1,0 => 1,1 so we cover all options.
        unsigned int firstToCreate = (numIter % 2 != 0) ? 1 : 0;
        unsigned int firstToDestroy = (numIter % 4 > 1) ? 1 : 0;

        unsigned int secondToCreate = (firstToCreate + 1)%2;
        unsigned int secondToDestroy = (firstToDestroy + 1)%2;

        BaseQueue *queues[2] = {new PM4Queue(), new SDMAQueue()};

        ASSERT_SUCCESS(queues[firstToCreate]->Create(defaultGPUNode));
        ASSERT_SUCCESS(queues[secondToCreate]->Create(defaultGPUNode));

        EXPECT_SUCCESS(queues[firstToDestroy]->Destroy());
        EXPECT_SUCCESS(queues[secondToDestroy]->Destroy());

        delete queues[0];
        delete queues[1];
        ++numIter;

        HSAuint64 curTime = GetSystemTickCountInMicroSec();
        timePassed = (curTime - initialTime) / 1000000;
    } while (timePassed < TEST_TIME_SEC);

    TEST_END
}

TEST_F(KFDQMTest, OverSubscribeCpQueues) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId == FAMILY_CI || m_FamilyId == FAMILY_KV) {
        LOG() << "Skipping test: CI doesn't have HW scheduling." << std::endl;
        return;
    }

    static const unsigned int MAX_CP_QUEUES = 65;
    static const unsigned int MAX_PACKETS = 100;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode, false);

    destBuf.Fill(0xFF);

    PM4Queue queues[MAX_CP_QUEUES];

    for (unsigned int qidx = 0; qidx < MAX_CP_QUEUES; ++qidx)
        ASSERT_SUCCESS(queues[qidx].Create(defaultGPUNode)) << " QueueId=" << qidx;

    for (unsigned int qidx = 0; qidx < MAX_CP_QUEUES; ++qidx) {
        unsigned int pktSizeDw = 0;
        for (unsigned int i = 0; i < MAX_PACKETS; i++) {
            PM4WriteDataPacket packet;
            packet.InitPacket(destBuf.As<unsigned int*>()+qidx*2, qidx+i, qidx+i);  // two dwords per packet
            queues[qidx].PlacePacket(packet);
        }
    }

    for (unsigned int qidx = 0; qidx < MAX_CP_QUEUES; ++qidx)
        queues[qidx].SubmitPacket();

    // Delaying for 5 seconds in order to get all the results
    Delay(5000);

    for (unsigned int qidx = 0; qidx < MAX_CP_QUEUES; ++qidx)
        EXPECT_TRUE(queues[qidx].AllPacketsSubmitted())<< "QueueId=" << qidx;;

    for (unsigned int qidx = 0; qidx < MAX_CP_QUEUES; ++qidx)
        EXPECT_SUCCESS(queues[qidx].Destroy());

    TEST_END
}

/* A simple isa loop program with dense mathematic operations
 * s1 controls the number iterations of the loop
 */
static const char *loop_isa = \
"\
shader loop_isa\n\
type(CS)\n\
    s_movk_i32    s0, 0x0008\n\
    s_movk_i32    s1, 0x00ff\n\
    v_mov_b32     v0, 0\n\
    v_mov_b32     v1, 0\n\
    v_mov_b32     v2, 0\n\
    v_mov_b32     v3, 0\n\
    v_mov_b32     v4, 0\n\
    v_mov_b32     v5, 0\n\
    v_mov_b32     v6, 0\n\
    v_mov_b32     v7, 0\n\
    v_mov_b32     v8, 0\n\
    v_mov_b32     v9, 0\n\
    v_mov_b32     v10, 0\n\
    v_mov_b32     v11, 0\n\
    v_mov_b32     v12, 0\n\
    v_mov_b32     v13, 0\n\
    v_mov_b32     v14, 0\n\
    v_mov_b32     v15, 0\n\
    v_mov_b32     v16, 0\n\
    LOOP:\n\
    s_mov_b32     s8, s4\n\
    s_mov_b32     s9, s1\n\
    s_mov_b32     s10, s6\n\
    s_mov_b32     s11, s7\n\
    s_cmp_le_i32  s1, s0\n\
    s_cbranch_scc1  END_OF_PGM\n\
    s_buffer_load_dwordx8  s[8:15], s[8:11], 0x10\n\
    v_add_f32     v0, 2.0, v0\n\
    v_cvt_f32_i32  v17, s1\n\
s_waitcnt     lgkmcnt(0)\n\
    v_add_f32     v18, s8, v17\n\
    v_add_f32     v19, s9, v17\n\
    v_add_f32     v20, s10, v17\n\
    v_add_f32     v21, s11, v17\n\
    v_add_f32     v22, s12, v17\n\
    v_add_f32     v23, s13, v17\n\
    v_add_f32     v24, s14, v17\n\
    v_add_f32     v17, s15, v17\n\
    v_log_f32     v25, v18\n\
    v_mul_legacy_f32  v25, v22, v25\n\
    v_exp_f32     v25, v25\n\
    v_log_f32     v26, v19\n\
    v_mul_legacy_f32  v26, v23, v26\n\
    v_exp_f32     v26, v26\n\
    v_log_f32     v27, v20\n\
    v_mul_legacy_f32  v27, v24, v27\n\
    v_exp_f32     v27, v27\n\
    v_log_f32     v28, v21\n\
    v_mul_legacy_f32  v28, v17, v28\n\
    v_exp_f32     v28, v28\n\
    v_add_f32     v5, v5, v25\n\
    v_add_f32     v6, v6, v26\n\
    v_add_f32     v7, v7, v27\n\
    v_add_f32     v8, v8, v28\n\
    v_mul_legacy_f32  v18, 0x3fb8aa3b, v18\n\
    v_exp_f32     v18, v18\n\
    v_mul_legacy_f32  v19, 0x3fb8aa3b, v19\n\
    v_exp_f32     v19, v19\n\
    v_mul_legacy_f32  v20, 0x3fb8aa3b, v20\n\
    v_exp_f32     v20, v20\n\
    v_mul_legacy_f32  v21, 0x3fb8aa3b, v21\n\
    v_exp_f32     v21, v21\n\
    v_add_f32     v9, v9, v18\n\
    v_add_f32     v10, v10, v19\n\
    v_add_f32     v11, v11, v20\n\
    v_add_f32     v12, v12, v21\n\
    v_sqrt_f32    v18, v22\n\
    v_sqrt_f32    v19, v23\n\
    v_sqrt_f32    v20, v24\n\
    v_sqrt_f32    v21, v17\n\
    v_add_f32     v13, v13, v18\n\
    v_add_f32     v14, v14, v19\n\
    v_add_f32     v15, v15, v20\n\
    v_add_f32     v16, v16, v21\n\
    v_rsq_f32     v18, v22\n\
    v_rsq_f32     v19, v23\n\
    v_rsq_f32     v20, v24\n\
    v_rsq_f32     v17, v17\n\
    v_add_f32     v1, v1, v18\n\
    v_add_f32     v2, v2, v19\n\
    v_add_f32     v3, v3, v20\n\
    v_add_f32     v4, v4, v17\n\
    s_add_u32     s0, s0, 1\n\
    s_branch      LOOP\n\
    END_OF_PGM:\n\
    s_endpgm\n\
    end\n\
";

HSAint64 KFDQMTest::TimeConsumedwithCUMask(int node, uint32_t* mask, uint32_t mask_count) {
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, node, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer dstBuffer(PAGE_SIZE, node, true, false, false);
    HsaMemoryBuffer ctlBuffer(PAGE_SIZE, node, true, false, false);

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);
    m_pIsaGen->CompileShader(loop_isa, "loop_isa", isaBuffer);

    Dispatch dispatch(isaBuffer);
    dispatch.SetDim(1024, 16, 16);

    PM4Queue queue;
    EXPECT_SUCCESS(queue.Create(node));
    EXPECT_SUCCESS(queue.SetCUMask(mask, mask_count));
    queue.SetSkipWaitConsump(true);

    struct timeval start, now;
    gettimeofday(&start, NULL);
    dispatch.Submit(queue);
    dispatch.Sync();
    gettimeofday(&now, NULL);

    EXPECT_SUCCESS(queue.Destroy());
    return (now.tv_sec *1000LL + now.tv_usec/1000LL - start.tv_sec * 1000LL - start.tv_usec/1000LL);
}

/* To cover for outliers, allow us to get the Average time based on a specified number of iterations */
HSAint64 KFDQMTest::GetAverageTimeConsumedwithCUMask(int node, uint32_t* mask, uint32_t mask_count, int iterations) {
    HSAint64 timeArray[iterations];
    HSAint64 timeTotal = 0;
    if (iterations < 1) {
        LOG() << "ERROR: At least 1 iteration must be performed" << std::endl;
        return 0;
    }

    for (int x = 0; x < iterations; x++) {
        timeArray[x] = TimeConsumedwithCUMask(node, mask, mask_count);
        timeTotal += timeArray[x];
    }

    if (timeTotal == 0) {
        LOG() << "ERROR: Total time reported as 0. Exiting" << std::endl;
        return 0;
    }

    for (int x = 0; x < iterations; x++) {
        HSAint64 variance = timeArray[x] / (timeTotal / iterations);
        if (variance < CuNegVariance || variance > CuPosVariance)
            LOG() << "WARNING: Measurement #" << x << "/" << iterations << " (" << timeArray[x]
                  << ") is at least " << CuVariance*100 << "% away from the mean (" << timeTotal/iterations << ")"
                  << std::endl;
    }

    return timeTotal / iterations;
}

/*
 * Apply CU masking in a linear fashion, adding 1 CU per iteration
 * until all Shader Engines are full
 */
TEST_F(KFDQMTest, BasicCuMaskingLinear) {
    TEST_START(TESTPROFILE_RUNALL);
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    m_pIsaGen = IsaGenerator::Create(m_FamilyId);

    if (m_FamilyId >= FAMILY_VI) {
        const HsaNodeProperties *pNodeProperties = m_NodeInfo.GetNodeProperties(defaultGPUNode);
        uint32_t ActiveCU = (pNodeProperties->NumFComputeCores / pNodeProperties->NumSIMDPerCU);
        uint32_t numSEs = pNodeProperties->NumShaderBanks;
        LOG() << std::hex << "# Compute cores: 0x" << pNodeProperties->NumFComputeCores << std::endl;
        LOG() << std::hex << "# SIMDs per CPU: 0x" << pNodeProperties->NumSIMDPerCU << std::endl;
        LOG() << std::hex << "# Shader engines: 0x" << numSEs << std::endl;
        LOG() << std::hex << "# Active CUs: 0x" << ActiveCU << std::endl;
        HSAint64 TimewithCU1, TimewithCU;
        uint32_t maskNumDwords = (ActiveCU + 31) / 32; /* Round up to the nearest multiple of 32 */
        uint32_t maskNumBits = maskNumDwords * 32;
        uint32_t mask[maskNumDwords];
        double ratio;

        mask[0] = 0x1;
        for (int i = 1; i < maskNumDwords; i++)
            mask[i] = 0x0;

        /* Execute once to get any HW optimizations out of the way */
        TimeConsumedwithCUMask(defaultGPUNode, mask, maskNumBits);

        LOG() << "Getting baseline performance numbers (CU Mask: 0x1)" << std::endl;
        TimewithCU1 = GetAverageTimeConsumedwithCUMask(defaultGPUNode, mask, maskNumBits, 3);

        for (int nCUs = 2; nCUs <= ActiveCU; nCUs++) {
            int maskIndex = (nCUs - 1) / 32;
            mask[maskIndex] |= 1 << ((nCUs - 1) % 32);

            TimewithCU = TimeConsumedwithCUMask(defaultGPUNode, mask, maskNumBits);
            ratio = (double)(TimewithCU1) / ((double)(TimewithCU) * nCUs);

            LOG() << "Expected performance of " << nCUs << " CUs vs 1 CU:" << std::endl;
            LOG() << std::setprecision(2) << CuNegVariance << " <= " << std::fixed << std::setprecision(8)
                  << ratio << " <= " << std::setprecision(2) << CuPosVariance << std::endl;

            EXPECT_TRUE((ratio >= CuNegVariance) && (ratio <= CuPosVariance));

            RECORD(ratio) << "Ratio-" << nCUs << "-CUs";
        }
    } else {
        LOG() << "Skipping test: Test not supported for family ID 0x" << m_FamilyId << "." << std::endl;
    }

    TEST_END
}

/**
 * Apply CU masking where the number of CUs is equal across all Shader Engines
 * This will work due to the HW splitting the workload unevenly across the Shader
 * Engines when ((#ofCUs)/(#ofShaderEngines)) is not a whole number. The tests above
 * will not yield viable results when an uneven distribution of CUs is used over multiple
 * shader engines (e.g. 0x1000100030003), until the HW changes how it schedules work.
 */
TEST_F(KFDQMTest, BasicCuMaskingEven) {
    TEST_START(TESTPROFILE_RUNALL);
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId >= FAMILY_VI) {
        const HsaNodeProperties *pNodeProperties = m_NodeInfo.GetNodeProperties(defaultGPUNode);
        uint32_t ActiveCU = (pNodeProperties->NumFComputeCores / pNodeProperties->NumSIMDPerCU);
        uint32_t numShaderEngines = pNodeProperties->NumShaderBanks;
        if (numShaderEngines == 1) {
            LOG() << "Skipping test: Only 1 Shader Engine present." << std::endl;
            return;
        }

        LOG() << std::hex << "# Compute cores: 0x" << pNodeProperties->NumFComputeCores << std::endl;
        LOG() << std::hex << "# SIMDs per CPU: 0x" << pNodeProperties->NumSIMDPerCU << std::endl;
        LOG() << std::hex << "# Shader engines: 0x" << numShaderEngines << std::endl;
        LOG() << std::hex << "# Active CUs: 0x" << ActiveCU << std::endl;
        HSAint64 TimewithCU1, TimewithCU;
        uint32_t maskNumDwords = (ActiveCU + 31) / 32; /* Round up to the nearest multiple of 32 */
        uint32_t maskNumBits = maskNumDwords * 32;
        uint32_t mask[maskNumDwords];
        int numCuPerShader = ActiveCU / numShaderEngines;
        double ratio;

        /* Set Mask to 1 for a single CU */
        mask[0] = 0x1;
        for (int i = 1; i < maskNumDwords; i++)
            mask[i] = 0x0;

        /* Execute once to get any HW optimizations out of the way */
        TimeConsumedwithCUMask(defaultGPUNode, mask, maskNumBits);

        LOG() << "Getting baseline performance numbers (1 CU)" << std::endl;
        TimewithCU1 = GetAverageTimeConsumedwithCUMask(defaultGPUNode, mask, maskNumBits, 3);

        /* Each loop will add 1 more CU per SE. We use the mod and divide to handle
         * when SEs aren't distributed in multiples of 32 (e.g. Tonga)
         * OR the new bit in for simplicity instead of re-creating the mask each iteration
         */
        for (int x = 0; x < numCuPerShader; x++) {
            for (int se = 0; se < numShaderEngines; se++) {
                int offset = x * numShaderEngines + se;
                int maskIndex = (offset / 32) % maskNumDwords;
                mask[maskIndex] |= 1 << (offset % 32);
            }
            int nCUs = numShaderEngines * (x + 1);

            TimewithCU = TimeConsumedwithCUMask(defaultGPUNode, mask, maskNumBits);
            ratio = (double)(TimewithCU1) / ((double)(TimewithCU) * nCUs);

            LOG() << "Expected performance of " << nCUs << " CUs vs 1 CU:" << std::endl;
            LOG() << std::setprecision(2) << CuNegVariance << " <= " << std::fixed << std::setprecision(8)
                  << ratio << " <= " << std::setprecision(2) << CuPosVariance << std::endl;

            EXPECT_TRUE((ratio >= CuNegVariance) && (ratio <= CuPosVariance));

            RECORD(ratio) << "Ratio-" << nCUs << "-CUs";
        }
    } else {
        LOG() << "Skipping test: Test not supported for family ID 0x" << m_FamilyId << "." << std::endl;
    }

    TEST_END
}

TEST_F(KFDQMTest, QueuePriorityOnDifferentPipe) {
    TEST_START(TESTPROFILE_RUNALL);

    if (m_FamilyId < FAMILY_VI) {
        LOG() << "Skipping test: Shader won't run on CI." << std::endl;
        return;
    }

    int node = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(node, 0) << "failed to get default GPU Node";
    HsaMemoryBuffer syncBuf(PAGE_SIZE, node, true/*zero*/, false/*local*/, true/*exec*/);
    HSAint32 *syncBuffer = syncBuf.As<HSAint32*>();
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, node, true/*zero*/, false/*local*/, true/*exec*/);

    m_pIsaGen->CompileShader(loop_isa, "loop_isa", isaBuffer);

    Dispatch dispatch[2] = {
        Dispatch(isaBuffer, true),
        Dispatch(isaBuffer, true)
    };

    int activeTaskBitmap = 0x3;
    struct timeval start, end[2];
    HsaEvent *pHsaEvent[2];
    int numEvent = 2;
    PM4Queue queue[2];
    HSA_QUEUE_PRIORITY priority[2] = {
        HSA_QUEUE_PRIORITY_LOW,
        HSA_QUEUE_PRIORITY_HIGH
    };
    int i;

    for (i = 0; i < 2; i++) {
        syncBuffer[i] = -1;
        ASSERT_SUCCESS(queue[i].Create(node));
        queue[i].Update(BaseQueue::DEFAULT_QUEUE_PERCENTAGE, priority[i], false);
        pHsaEvent[i] = dispatch[i].GetHsaEvent();
        pHsaEvent[i]->EventData.EventData.SyncVar.SyncVar.UserData = &syncBuffer[i];
        dispatch[i].SetDim(1024, 16, 16);
    }

    gettimeofday(&start, NULL);
    for (i = 0; i < 2; i++)
        dispatch[i].Submit(queue[i]);

    while (activeTaskBitmap > 0) {
        hsaKmtWaitOnMultipleEvents(pHsaEvent, numEvent, false, g_TestTimeOut);
        for (i = 0; i < 2; i++) {
            if ((activeTaskBitmap & (1 << i)) && (syncBuffer[i] == pHsaEvent[i]->EventId)) {
                gettimeofday(&end[i], NULL);
                activeTaskBitmap &= ~(1 << i);
            }
        }
    }

    for (i = 0; i < 2; i++) {
        EXPECT_SUCCESS(queue[i].Destroy());
        int ms = end[i].tv_sec *1000LL + end[i].tv_usec/1000LL - start.tv_sec * 1000LL - start.tv_usec/1000LL;
        LOG() << "Task priority: " << std::dec << priority[i] << "\t";
        LOG() << "Task duration: " << std::dec << ms << "ms" << std::endl;
    }

    TEST_END
}

TEST_F(KFDQMTest, QueuePriorityOnSamePipe) {
    TEST_START(TESTPROFILE_RUNALL);

    if (m_FamilyId < FAMILY_VI) {
        LOG() << "Skipping test: Shader won't run on CI." << std::endl;
        return;
    }

    int node = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(node, 0) << "failed to get default GPU Node";
    HsaMemoryBuffer syncBuf(PAGE_SIZE, node, true/*zero*/, false/*local*/, true/*exec*/);
    HSAint32 *syncBuffer = syncBuf.As<HSAint32*>();
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, node, true/*zero*/, false/*local*/, true/*exec*/);

    m_pIsaGen->CompileShader(loop_isa, "loop_isa", isaBuffer);

    Dispatch dispatch[2] = {
        Dispatch(isaBuffer, true),
        Dispatch(isaBuffer, true)
    };

    int activeTaskBitmap = 0x3;
    struct timeval start, end[2];
    HsaEvent *pHsaEvent[2];
    int numEvent = 2;
    PM4Queue queue[13];
    HSA_QUEUE_PRIORITY priority[2] = {
        HSA_QUEUE_PRIORITY_LOW,
        HSA_QUEUE_PRIORITY_HIGH
    };
    int i;

    /* queue[2..12] are dummy queues. Create queue in this sequence to
     * render queue[0] and queue[1] on same pipe with no assumptions
     * about the number of pipes used by KFD. Queue #12 is a multiple
     * of 1, 2, 3 and 4, so it falls on pipe 0 for any number of pipes
     */
    EXPECT_SUCCESS(queue[0].Create(node));  // Queue 0 is on Pipe 0
    for (i = 2; i <= 12; i++)
        EXPECT_SUCCESS(queue[i].Create(node));
    EXPECT_SUCCESS(queue[1].Create(node));  // Queue 12 is on Pipe 0

    for (i = 0; i < 2; i++) {
        syncBuffer[i] = -1;
        queue[i].Update(BaseQueue::DEFAULT_QUEUE_PERCENTAGE, priority[i], false);
        pHsaEvent[i] = dispatch[i].GetHsaEvent();
        pHsaEvent[i]->EventData.EventData.SyncVar.SyncVar.UserData = &syncBuffer[i];
        dispatch[i].SetDim(1024, 16, 16);
    }

    gettimeofday(&start, NULL);
    for (i = 0; i < 2; i++)
        dispatch[i].Submit(queue[i]);

    while (activeTaskBitmap > 0) {
        hsaKmtWaitOnMultipleEvents(pHsaEvent, numEvent, false, g_TestTimeOut);
        for (i = 0; i < 2; i++) {
            if ((activeTaskBitmap & (1 << i)) && (syncBuffer[i] == pHsaEvent[i]->EventId)) {
                gettimeofday(&end[i], NULL);
                activeTaskBitmap &= ~(1 << i);
            }
        }
    }

    for (i = 0; i < 2; i++) {
        int ms = end[i].tv_sec *1000LL + end[i].tv_usec/1000LL - start.tv_sec * 1000LL - start.tv_usec/1000LL;
        LOG() << "Task priority: " << std::dec << priority[i] << "\t";
        LOG() << "Task duration: " << std::dec << ms << "ms" << std::endl;
    }

    for (i = 0; i <= 12; i++) {
        EXPECT_SUCCESS(queue[i].Destroy());
    }

    TEST_END
}


void KFDQMTest::SyncDispatch(const HsaMemoryBuffer& isaBuffer, void* pSrcBuf, void* pDstBuf, int node) {
    PM4Queue queue;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    if (node != -1)
        defaultGPUNode = node;

    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    Dispatch dispatch(isaBuffer);
    dispatch.SetArgs(pSrcBuf, pDstBuf);
    dispatch.SetDim(1, 1, 1);

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    dispatch.Submit(queue);
    dispatch.Sync();

    EXPECT_SUCCESS(queue.Destroy());
}

TEST_F(KFDQMTest, EmptyDispatch) {
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);

    m_pIsaGen->GetNoopIsa(isaBuffer);

    SyncDispatch(isaBuffer, NULL, NULL);

    TEST_END
}

TEST_F(KFDQMTest, SimpleWriteDispatch) {
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer srcBuffer(PAGE_SIZE, defaultGPUNode, false);
    HsaMemoryBuffer destBuffer(PAGE_SIZE, defaultGPUNode);

    srcBuffer.Fill(0x01010101);

    m_pIsaGen->GetCopyDwordIsa(isaBuffer);

    SyncDispatch(isaBuffer, srcBuffer.As<void*>(), destBuffer.As<void*>());

    EXPECT_EQ(destBuffer.As<unsigned int*>()[0], 0x01010101);

    TEST_END
}

TEST_F(KFDQMTest, MultipleCpQueuesStressDispatch) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    static const unsigned int MAX_CP_QUEUES = 16;

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer srcBuffer(PAGE_SIZE, defaultGPUNode, false);
    HsaMemoryBuffer destBuffer(PAGE_SIZE, defaultGPUNode);

    unsigned int* src = srcBuffer.As<unsigned int*>();
    unsigned int* dst = destBuffer.As<unsigned int*>();

    static const HSAuint64 TEST_TIME_SEC = 15;
    HSAuint64 initialTime, curTime;
    unsigned int numIter = 0;
    HSAuint64 timePassed = 0;

    unsigned int i;
    PM4Queue queues[MAX_CP_QUEUES];
    Dispatch* dispatch[MAX_CP_QUEUES];

    destBuffer.Fill(0xFF);

    m_pIsaGen->GetCopyDwordIsa(isaBuffer);

    for (i = 0; i < MAX_CP_QUEUES; ++i)
        ASSERT_SUCCESS(queues[i].Create(defaultGPUNode)) << " QueueId=" << i;

    initialTime = GetSystemTickCountInMicroSec();

    do {
        for (i = 0; i < MAX_CP_QUEUES; ++i) {
            dispatch[i] = new Dispatch(isaBuffer);
            src[i] = numIter;
            dst[i] = 0xff;
            dispatch[i]->SetArgs(&src[i], &dst[i]);
            dispatch[i]->SetDim(1, 1, 1);
            dispatch[i]->Submit(queues[i]);
        }
        for (i = 0; i < MAX_CP_QUEUES; ++i) {
            dispatch[i]->Sync();
            EXPECT_EQ(dst[i], src[i]);
            delete dispatch[i];
        }
        ++numIter;
        curTime = GetSystemTickCountInMicroSec();
        timePassed = (curTime - initialTime) / 1000000;
    } while (timePassed < TEST_TIME_SEC);

    LOG() << "Total iterated : " << std::dec << numIter << std::endl;

    for (i = 0; i < MAX_CP_QUEUES; ++i)
       EXPECT_SUCCESS(queues[i].Destroy());

    TEST_END
}



TEST_F(KFDQMTest, CpuWriteCoherence) {
    TEST_START(TESTPROFILE_RUNALL);

    PM4Queue queue;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode);

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    /* The queue might be full and we fail to submit. There is always one word space unused in queue.
     * So let rptr one step ahead then we continually submit packet.
     */
    queue.PlaceAndSubmitPacket(PM4NopPacket());
    queue.Wait4PacketConsumption();
    EXPECT_EQ(1, queue.Rptr());

    do {
        queue.PlaceAndSubmitPacket(PM4NopPacket());
    } while (queue.Wptr() != 0);

    queue.Wait4PacketConsumption();

    EXPECT_EQ(0, queue.Rptr());

    /* Now that the GPU has cached the PQ contents, we modify them in CPU cache and
     * ensure that the GPU sees the updated value:
     */
    queue.PlaceAndSubmitPacket(PM4WriteDataPacket(destBuf.As<unsigned int*>(), 0x42, 0x42));

    queue.Wait4PacketConsumption();

    WaitOnValue(destBuf.As<unsigned int*>(), 0x42);

    TEST_END
}

TEST_F(KFDQMTest, CreateAqlCpQueue) {
    TEST_START(TESTPROFILE_RUNALL)

    AqlQueue queue;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer pointers(PAGE_SIZE, defaultGPUNode);

    ASSERT_SUCCESS(queue.Create(defaultGPUNode, PAGE_SIZE, pointers.As<HSAuint64 *>()));

    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}


TEST_F(KFDQMTest, QueueLatency) {
    TEST_START(TESTPROFILE_RUNALL);

    PM4Queue queue;
    const int queueSize = PAGE_SIZE * 2;
    const int packetSize = PM4ReleaseMemoryPacket(0, 0, 0, 0, 0).SizeInBytes();
    /* We always leave one NOP(dword) empty after packet which is required by ring itself.
     * We also place NOPs when queue wraparound to avoid crossing buffer end. See PlacePacket().
     * So the worst case is that we need two packetSize space to place one packet.
     * Like below, N=NOP,E=Empty,P=Packet.
     * |E|E|E|E|E|E|E|rptr...wptr|E|E|E|E|E| ---> |P|P|P|P|P|P|E|rptr...wptr|N|N|N|N|N|
     * So to respect that, we reserve packetSize space for these additional NOPs.
     * Also we reserve the remainder of the division by packetSize explicitly.
     * Reserve another packetSize for event-based wait which uses a releseMemory packet.
     */
    const int reservedSpace = packetSize + queueSize % packetSize + packetSize;
    const int slots = (queueSize - reservedSpace) / packetSize;
    HSAint64 queue_latency_avg = 0, queue_latency_min, queue_latency_max, queue_latency_med;
    HSAint64 overhead, workload;
    HSAint64 *queue_latency_arr = reinterpret_cast<HSAint64*>(calloc(slots, sizeof(HSAint64)));
    const int skip = 2;
    const char *fs[skip] = {"1st", "2nd"};
    HsaClockCounters *ts;
    HSAuint64 *qts;
    int i = 0;

    ASSERT_NE((HSAuint64)queue_latency_arr, 0);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    ASSERT_SUCCESS(queue.Create(defaultGPUNode, queueSize));

    LOG() << std::dec << "Queue Submit NanoSeconds (" << slots << " Packets)" << std::endl;

    HsaMemoryBuffer buf(ALIGN_UP(slots * sizeof(HsaClockCounters), PAGE_SIZE), 0);
    ts = buf.As<HsaClockCounters*>();

    HsaMemoryBuffer qbuf(ALIGN_UP(slots * sizeof(HSAuint64), PAGE_SIZE), 0);
    qts = qbuf.As<HSAuint64*>();

    HsaEvent *event;
    ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, defaultGPUNode, &event));

    /* GpuCounter overhead*/
    do {
        hsaKmtGetClockCounters(defaultGPUNode, &ts[i]);
    } while (++i < slots);
    overhead = ts[slots-1].GPUClockCounter - ts[0].GPUClockCounter;
    overhead /= 2 * (slots - 1);

    /* Submit packets serially*/
    i = 0;
    do {
        queue.PlacePacket(PM4ReleaseMemoryPacket(true,
                    (HSAuint64)&qts[i],
                    0,
                    true,
                    1));
        hsaKmtGetClockCounters(defaultGPUNode, &ts[i]);
        queue.SubmitPacket();
        queue.Wait4PacketConsumption(event);
    } while (++i < slots);

    /* Calculate timing which includes workload and overhead*/
    i = 0;
    do {
        HSAint64 queue_latency = qts[i] - ts[i].GPUClockCounter;

        EXPECT_GE(queue_latency, 0);

        queue_latency_arr[i] = queue_latency;
        if (i >= skip)
            queue_latency_avg += queue_latency;
    } while (++i < slots);
    /* Calculate avg from packet[skip, slots-1] */
    queue_latency_avg /= (slots - skip);

    /* Workload of queue packet itself */
    i = 0;
    do {
        queue.PlacePacket(PM4ReleaseMemoryPacket(true,
                    (HSAuint64)&qts[i],
                    0,
                    true,
                    1));
    } while (++i < slots);
    queue.SubmitPacket();
    queue.Wait4PacketConsumption(event);

    hsaKmtDestroyEvent(event);
    /* qts[i] records the timestamp of the end of packet[i] which is
     * approximate that of the beginging of packet[i+1].
     * The workload total is [0, skip], [skip+1, slots-1].
     * And We ignore [0, skip], that means we ignore (skip+1) packets.
     */
    workload = qts[slots - 1] - qts[skip];
    workload /= (slots - 1 - skip);

    EXPECT_GE(workload, 0);

    i = 0;
    do {
        /* The queue_latency is not that correct as the workload and overhead are average*/
        queue_latency_arr[i] -= workload + overhead;
        /* The First submit takes an HSAint64 time*/
        if (i < skip)
            LOG() << "Queue Latency " << fs[i] << ": \t" << CounterToNanoSec(queue_latency_arr[i]) << std::endl;
    } while (++i < slots);

    std::sort(queue_latency_arr + skip, queue_latency_arr + slots);

    queue_latency_min = queue_latency_arr[skip];
    queue_latency_med = queue_latency_arr[(slots+skip)/2];
    queue_latency_max = queue_latency_arr[slots-1];

    LOG() << "Queue Latency Avg:     \t" << CounterToNanoSec(queue_latency_avg) << std::endl;
    LOG() << "Queue Latency Min:     \t" << CounterToNanoSec(queue_latency_min) << std::endl;
    LOG() << "Queue Latency Median:  \t" << CounterToNanoSec(queue_latency_med) << std::endl;
    LOG() << "Queue Latency Max:     \t" << CounterToNanoSec(queue_latency_max) << std::endl;
    LOG() << "Queue Packet Workload: \t" << CounterToNanoSec(workload) << std::endl;
    LOG() << "Get GpuCounter Overhead: \t" << CounterToNanoSec(overhead) << std::endl;

    RECORD(CounterToNanoSec(queue_latency_avg)) << "Queue-Latency-Avg";
    RECORD(CounterToNanoSec(queue_latency_min)) << "Queue-Latency-Min";
    RECORD(CounterToNanoSec(queue_latency_med)) << "Queue-Latency-Med";
    RECORD(CounterToNanoSec(queue_latency_max)) << "Queue-Latency-Max";
    RECORD(CounterToNanoSec(workload)) << "Queue-Packet-Workload";
    RECORD(CounterToNanoSec(overhead)) << "GpuCounter-Overhead";

    TEST_END
}


TEST_F(KFDQMTest, CpQueueWraparound) {
    TEST_START(TESTPROFILE_RUNALL);

    PM4Queue queue;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode);

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    for (unsigned int pktIdx = 0; pktIdx <= PAGE_SIZE/sizeof(PM4WRITE_DATA_CI); ++pktIdx) {
        queue.PlaceAndSubmitPacket(PM4WriteDataPacket(destBuf.As<unsigned int*>(), pktIdx, pktIdx));
        queue.Wait4PacketConsumption();
        WaitOnValue(destBuf.As<unsigned int*>(), pktIdx);
    }

    for (unsigned int pktIdx = 0; pktIdx <= PAGE_SIZE/sizeof(PM4WRITE_DATA_CI); ++pktIdx) {
        queue.PlaceAndSubmitPacket(PM4WriteDataPacket(destBuf.As<unsigned int*>(), pktIdx, pktIdx));
        queue.Wait4PacketConsumption();
        WaitOnValue(destBuf.As<unsigned int*>(), pktIdx);
    }

    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDQMTest, SdmaQueueWraparound) {
    TEST_START(TESTPROFILE_RUNALL);
    int bufSize = PAGE_SIZE;

    SDMAQueue queue;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(bufSize << 1, defaultGPUNode, false);
    HsaMemoryBuffer srcBuf(bufSize, defaultGPUNode, false);

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    for (unsigned int pktIdx = 0;  pktIdx <= queue.Size()/sizeof(SDMA_PKT_COPY_LINEAR); ++pktIdx) {
        destBuf.Fill(0x0);
        srcBuf.Fill(pktIdx);
        queue.PlaceAndSubmitPacket(
                SDMACopyDataPacket(destBuf.As<unsigned int*>(), srcBuf.As<unsigned int*>(), bufSize));
        queue.PlaceAndSubmitPacket(
                SDMAWriteDataPacket(destBuf.As<unsigned int*>() + bufSize/4, 0x02020202));
        queue.Wait4PacketConsumption();

        EXPECT_TRUE(WaitOnValue(destBuf.As<unsigned int*>() + bufSize/4, 0x02020202));

        EXPECT_SUCCESS(memcmp(
                destBuf.As<unsigned int*>(), srcBuf.As<unsigned int*>(), bufSize));
    }

    for (unsigned int pktIdx = 0; pktIdx <= queue.Size()/sizeof(SDMA_PKT_WRITE_UNTILED); ++pktIdx) {
        queue.PlaceAndSubmitPacket(SDMAWriteDataPacket(destBuf.As<unsigned int*>(), pktIdx));
        queue.Wait4PacketConsumption();
        WaitOnValue(destBuf.As<unsigned int*>(), pktIdx);
    }

    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

struct AtomicIncThreadParams {
    HSAint64* pDest;
    volatile unsigned int count;
    volatile bool stop;
};

unsigned int AtomicIncThread(void* pCtx) {
    AtomicIncThreadParams* pArgs = reinterpret_cast<AtomicIncThreadParams*>(pCtx);

    while (pArgs->stop)
        {}

    while (!pArgs->stop) {
        AtomicInc(pArgs->pDest);
        ++pArgs->count;
    }

    return 0;
}

TEST_F(KFDQMTest, Atomics) {
    TEST_START(TESTPROFILE_RUNALL);
    /* CI doesn't support Atomics. KV does, but gets its own FAMILY_KV def */
    if (m_FamilyId == FAMILY_CI) {
        LOG() << "Skipping test: CI doesn't support Atomics." << std::endl;
        return;
    }
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer isaBuf(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer destBuf(PAGE_SIZE, defaultGPUNode);

    PM4Queue queue;

    m_pIsaGen->GetAtomicIncIsa(isaBuf);

    Dispatch dispatch(isaBuf);
    dispatch.SetArgs(destBuf.As<void*>(), NULL);
    dispatch.SetDim(1024, 1, 1);

    hsaKmtSetMemoryPolicy(defaultGPUNode, HSA_CACHING_CACHED, HSA_CACHING_CACHED, NULL, 0);

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    AtomicIncThreadParams params;
    params.pDest = destBuf.As<HSAint64*>();
    params.stop = true;
    params.count = 0;

    uint64_t threadId;

    ASSERT_EQ(true, StartThread(&AtomicIncThread, &params, threadId));

    params.stop = false;

    while (params.count == 0)
        {}

    dispatch.Submit(queue);
    dispatch.Sync();

    params.stop = true;

    WaitForThread(threadId);

    EXPECT_EQ(destBuf.As<unsigned int*>()[0], 1024 + params.count);

    LOG() << "GPU increments: 1024, CPU increments: " << params.count << std::endl;

    queue.Destroy();

    TEST_END
}

TEST_F(KFDQMTest, mGPUShareBO) {
    TEST_START(TESTPROFILE_RUNALL);

    unsigned int src_node = 2;
    unsigned int dst_node = 1;

    if (g_TestDstNodeId != -1 && g_TestNodeId != -1) {
        src_node = g_TestNodeId;
        dst_node = g_TestDstNodeId;
    }

    HsaMemoryBuffer shared_addr(PAGE_SIZE, dst_node, true, false, false, false);

    HsaMemoryBuffer srcNodeMem(PAGE_SIZE, src_node);
    HsaMemoryBuffer dstNodeMem(PAGE_SIZE, dst_node);

    /* Handle ISA to write to local memory BO */
    HsaMemoryBuffer isaBufferSrc(PAGE_SIZE, src_node, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer isaBufferDst(PAGE_SIZE, dst_node, true/*zero*/, false/*local*/, true/*exec*/);

    srcNodeMem.Fill(0x05050505);

    m_pIsaGen->GetCopyDwordIsa(isaBufferSrc);
    SyncDispatch(isaBufferSrc, srcNodeMem.As<void*>(), shared_addr.As<void *>(), src_node);

    m_pIsaGen->GetCopyDwordIsa(isaBufferDst);
    SyncDispatch(isaBufferDst, shared_addr.As<void *>(), dstNodeMem.As<void*>(), dst_node);

    EXPECT_EQ(dstNodeMem.As<unsigned int*>()[0], 0x05050505);

    EXPECT_SUCCESS(shared_addr.UnmapMemToNodes(&dst_node, 1));

    TEST_END
}

static void
sdma_copy(HSAuint32 node, void *src, void *const dst[], int n, HSAuint64 size) {
    SDMAQueue sdmaQueue;
    HsaEvent *event;
    ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, node, &event));
    ASSERT_SUCCESS(sdmaQueue.Create(node));
    sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(dst, src, n, size));
    sdmaQueue.Wait4PacketConsumption(event);
    EXPECT_SUCCESS(sdmaQueue.Destroy());
    hsaKmtDestroyEvent(event);
}

static void
sdma_fill(HSAint32 node, void *dst, unsigned int data, HSAuint64 size) {
    SDMAQueue sdmaQueue;
    HsaEvent *event;
    ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, node, &event));
    ASSERT_SUCCESS(sdmaQueue.Create(node));
    sdmaQueue.PlaceAndSubmitPacket(SDMAFillDataPacket(dst, data, size));
    sdmaQueue.Wait4PacketConsumption(event);
    EXPECT_SUCCESS(sdmaQueue.Destroy());
    hsaKmtDestroyEvent(event);
}

TEST_F(KFDQMTest, P2PTest) {
    TEST_START(TESTPROFILE_RUNALL);
    if (!is_dgpu()) {
        LOG() << "Skipping test: Two GPUs are required, but no dGPUs are present." << std::endl;
        return;
    }

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    if (gpuNodes.size() < 2) {
        LOG() << "Skipping test: At least two GPUs are required." << std::endl;
        return;
    }
    std::vector<HSAuint32> nodes;

    /* This test simulates RT team's P2P part in IPCtest:
     *
     * +------------------------------------------------+
     * |         gpu1           gpu2           gpuX     |
     * |gpu1 mem ----> gpu2 mem ----> gpuX mem          |
     * |        \               \               \       |
     * |         \               \               \      |
     * |    system buffer   system buffer  system buffer|
     * +------------------------------------------------+
     *
     * Copy data from current GPU memory to next GPU memory and system memory
     * Using current GPU, aka p2p push.
     * Verify the system buffer has the expected content after each push.
     */

    /* Users can use "--node=gpu1 --dst_node=gpu2" to specify devices */
    if (g_TestDstNodeId != -1 && g_TestNodeId != -1) {
        nodes.push_back(g_TestNodeId);
        nodes.push_back(g_TestDstNodeId);
        if (!m_NodeInfo.IsGPUNodeLargeBar(nodes[1])) {
            LOG() << "Skipping test: Dst GPU is not a large bar GPU." << std::endl;
            return;
        }
        if (nodes[0] == nodes[1]) {
            LOG() << "Skipping test: Different GPUs must be specified (2 GPUs required)." << std::endl;
            return;
        }
    } else {
        HSAint32 defaultGPU = m_NodeInfo.HsaDefaultGPUNode();
        nodes.push_back(defaultGPU);
        for (unsigned i = 0; i < gpuNodes.size(); i++)
            if (m_NodeInfo.IsGPUNodeLargeBar(gpuNodes.at(i)) && gpuNodes.at(i) != defaultGPU)
                nodes.push_back(gpuNodes.at(i));
        if (nodes.size() < 2) {
            LOG() << "Skipping test: Test requires at least one large bar GPU." << std::endl;
            return;
        }
    }

    HSAuint32 *sysBuf;
    HSAuint32 size = 16ULL<<20;  // bigger than 16MB to test non-contiguous memory
    HsaMemFlags memFlags = {0};
    HsaMemMapFlags mapFlags = {0};
    memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    memFlags.ui32.HostAccess = 1;
    memFlags.ui32.NonPaged = 1;
    unsigned int end = size / sizeof(HSAuint32) - 1;

    /* 1. Allocate a system buffer and allow the access to GPUs */
    EXPECT_SUCCESS(hsaKmtAllocMemory(0, size, memFlags,
                                     reinterpret_cast<void **>(&sysBuf)));
    EXPECT_SUCCESS(hsaKmtMapMemoryToGPUNodes(sysBuf, size, NULL,
                                             mapFlags, nodes.size(), &nodes[0]));
#define MAGIC_NUM 0xdeadbeaf

    /* First GPU fills mem with MAGIC_NUM */
    void *src, *dst;
    HSAuint32 cur = nodes[0], next;
    ASSERT_SUCCESS(hsaKmtAllocMemory(cur, size, memFlags, reinterpret_cast<void**>(&src)));
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(src, size, NULL));
    sdma_fill(cur, src, MAGIC_NUM, size);

    for (unsigned i = 1; i <= nodes.size(); i++) {
        int n;
        memset(sysBuf, 0, size);

        /* Last GPU just copy mem to sysBuf*/
        if (i == nodes.size()) {
               n = 1;
               next = 0;/*system memory node*/
               dst = 0;
        } else {
            n = 2;
            next = nodes[i];
            ASSERT_SUCCESS(hsaKmtAllocMemory(next, size, memFlags, reinterpret_cast<void**>(&dst)));
            ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(dst, size, NULL));
        }

        LOG() << "Test " << cur << " -> " << next << std::endl;
        /* Copy to sysBuf and next GPU*/
        void *dst_array[] = {sysBuf, dst};
        sdma_copy(cur, src, dst_array, n, size);

        /* Verify the data*/
        EXPECT_EQ(sysBuf[0], MAGIC_NUM);
        EXPECT_EQ(sysBuf[end], MAGIC_NUM);

        LOG() << "PASS " << cur << " -> " << next << std::endl;

        EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(src));
        EXPECT_SUCCESS(hsaKmtFreeMemory(src, size));

        cur = next;
        src = dst;
    }

    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(sysBuf));
    EXPECT_SUCCESS(hsaKmtFreeMemory(sysBuf, size));

    TEST_END
}

TEST_F(KFDQMTest, PM4EventInterrupt) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    const HSAuint64 bufSize = PAGE_SIZE;
    const int packetCount = bufSize / sizeof(unsigned int);
    const int totalPacketSize = packetCount * PM4WriteDataPacket(0, 0).SizeInBytes() +
                                                PM4ReleaseMemoryPacket(0, 0, 0).SizeInBytes();
    const int queueSize = RoundToPowerOf2(totalPacketSize);

    /* 4 PM4 queues will be running at same time.*/
    const int numPM4Queue = 4;
    HsaEvent *event[numPM4Queue];
    PM4Queue queue[numPM4Queue];
    HsaMemoryBuffer *destBuf[numPM4Queue];
    unsigned int *buf[numPM4Queue];

    for (int i = 0; i < numPM4Queue; i++) {
        destBuf[i] = new HsaMemoryBuffer(bufSize, defaultGPUNode, true, false); // System memory
        buf[i] = destBuf[i]->As<unsigned int *>();
    }

    /* A simple loop here to give more pressure.*/
    for (int test_count = 0; test_count < 1024; test_count++) {
        for (int i = 0; i < numPM4Queue; i++) {
            ASSERT_SUCCESS(queue[i].Create(defaultGPUNode, queueSize));
            ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, defaultGPUNode, &event[i]));

            /* Let CP have some workload first.*/
            for(int index = 0; index < packetCount; index++)
                queue[i].PlacePacket(PM4WriteDataPacket(buf[i] + index, 0xdeadbeaf));

            /* releaseMemory packet makes sure all previous written data is visible.*/
            queue[i].PlacePacket(PM4ReleaseMemoryPacket(0,
                        reinterpret_cast<HSAuint64>(event[i]->EventData.HWData2),
                        event[i]->EventId,
                        true));
        }

        for (int i = 0; i < numPM4Queue; i++)
            queue[i].SubmitPacket();

        for (int i = 0; i < numPM4Queue; i++) {
            EXPECT_SUCCESS(hsaKmtWaitOnEvent(event[i], g_TestTimeOut));
            EXPECT_EQ(buf[i][0], 0xdeadbeaf);
            EXPECT_EQ(buf[i][packetCount - 1], 0xdeadbeaf);
            memset(buf[i], 0, bufSize);
        }

        for (int i = 0; i < numPM4Queue; i++) {
            EXPECT_SUCCESS(queue[i].Destroy());
            EXPECT_SUCCESS(hsaKmtDestroyEvent(event[i]));
        }
    }

    for (int i = 0; i < numPM4Queue; i++)
        delete destBuf[i];

    TEST_END
}

#include "KFDTestUtilQueue.hpp"
TEST_F(KFDQMTest, SdmaEventInterrupt) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    const HSAuint64 bufSize = 4 << 20;
    HsaMemoryBuffer srcBuf(bufSize, 0); // System memory.

    HSAuint64 *src = srcBuf.As<HSAuint64*>();
    TimeStamp *tsbuf = srcBuf.As<TimeStamp*>();
    tsbuf = reinterpret_cast<TimeStamp *>ALIGN_UP(tsbuf, sizeof(TimeStamp));

    /* Have 3 queues created for test.*/
    const int numSDMAQueue = 3;
    HsaEvent *event[numSDMAQueue];
    SDMAQueue queue[numSDMAQueue];
    HsaMemoryBuffer *destBuf[numSDMAQueue];
    HSAuint64 *dst[numSDMAQueue];

    for (int i = 0; i < numSDMAQueue; i++) {
        destBuf[i] = new HsaMemoryBuffer(bufSize, defaultGPUNode, true, false); // System memory
        dst[i] = destBuf[i]->As<HSAuint64*>();
    }

    /* Test 1 queue, 2 queues, 3 queues running at same time one by one.*/
    for (int testSDMAQueue = 1; testSDMAQueue <= numSDMAQueue; testSDMAQueue++)
        /* A simple loop here to give more pressure.*/
        for (int test_count = 0; test_count < 2048; test_count++) {
            for (int i = 0; i < testSDMAQueue; i++) {
                TimeStamp *ts = tsbuf + i * 32;
                ASSERT_SUCCESS(queue[i].Create(defaultGPUNode));
                /* FIXME
                 * We create event every time along with queue.
                 * However that will significantly enhance the failure of sdma event timeout.
                 */
                ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, defaultGPUNode, &event[i]));

                /* Get the timestamp directly. The first member of HsaClockCounters and TimeStamp is GPU clock counter.*/
                hsaKmtGetClockCounters(defaultGPUNode, reinterpret_cast<HsaClockCounters*>(&ts[0]));
                /* Let sDMA have some workload first.*/
                queue[i].PlacePacket(SDMATimePacket(&ts[1]));
                queue[i].PlacePacket(
                        SDMACopyDataPacket(dst[i], src, bufSize));
                queue[i].PlacePacket(SDMATimePacket(&ts[2]));
                queue[i].PlacePacket(
                        SDMAFencePacket(reinterpret_cast<void*>(event[i]->EventData.HWData2), event[i]->EventId));
                queue[i].PlacePacket(SDMATimePacket(&ts[3]));
                queue[i].PlacePacket(SDMATrapPacket(event[i]->EventId));
                queue[i].PlacePacket(SDMATimePacket(&ts[4]));

                /* Will verify the value of srcBuf and destBuf later. Give it a different value each time.*/
                src[0] = ts[0].timestamp;
            }

            for (int i = 0; i < testSDMAQueue; i++)
                queue[i].SubmitPacket();

            for (int i = 0; i < testSDMAQueue; i++) {
                TimeStamp *ts = tsbuf + i * 32;
                HSAKMT_STATUS ret = hsaKmtWaitOnEvent(event[i], g_TestTimeOut);

                if (dst[i][0] != src[0])
                    WARN() << "SDMACopyData FAIL! " << std::dec
                        << dst[i][0] << " VS " << src[0] << std::endl;

                if (ret == HSAKMT_STATUS_SUCCESS) {
                    for (int i = 1; i <= 4; i++)
                        /* Is queue latency too big? The workload is really small.*/
                        if (CounterToNanoSec(ts[i].timestamp - ts[i - 1].timestamp) > 1000000000)
                            WARN() << "SDMA queue latency is bigger than 1s!" << std::endl;
                } else {
                    WARN() << "Event On Queue " << testSDMAQueue << ":" << i
                        << " Timeout, try to resubmit packets!" << std::endl;

                    queue[i].SubmitPacket();

                    if (hsaKmtWaitOnEvent(event[i], g_TestTimeOut) == HSAKMT_STATUS_SUCCESS)
                        WARN() << "The timeout event is signaled!" << std::endl;
                    else
                        WARN() << "The timeout event is lost after resubmit!" << std::endl;

                    LOG() << "Time Consumption (ns)" << std::endl;
                    for (int i = 1; i <= 4; i++)
                        LOG() << std::dec << i << ": "
                            << CounterToNanoSec(ts[i].timestamp - ts[i - 1].timestamp) << std::endl;
                }

                EXPECT_SUCCESS(ret);
            }

            for (int i = 0; i < testSDMAQueue; i++) {
                EXPECT_SUCCESS(queue[i].Destroy());
                EXPECT_SUCCESS(hsaKmtDestroyEvent(event[i]));
            }
        }

    for (int i = 0; i < numSDMAQueue; i++)
        delete destBuf[i];

    TEST_END
}

#define DOORBELL_WRITE_USE_SDMA
TEST_F(KFDQMTest, GPUDoorbellWrite) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuf(PAGE_SIZE, 0, true);
    PM4Queue pm4Queue;
#ifdef DOORBELL_WRITE_USE_SDMA
    SDMAQueue otherQueue;
#else
    PM4Queue otherQueue;
#endif

    ASSERT_SUCCESS(pm4Queue.Create(defaultGPUNode));
    ASSERT_SUCCESS(otherQueue.Create(defaultGPUNode));

    /* Place PM4 packet in the queue, but don't submit it */
    pm4Queue.PlacePacket(PM4WriteDataPacket(destBuf.As<unsigned int*>(), 0x12345678, 0x87654321));

    HsaQueueResource *qRes = pm4Queue.GetResource();

    if (m_FamilyId < FAMILY_AI) {
        unsigned int pendingWptr = pm4Queue.GetPendingWptr();

#ifdef DOORBELL_WRITE_USE_SDMA
        /* Write the wptr and doorbell update using the GPU's SDMA
         * engine. This should submit the PM4 packet on the first
         * queue.
         */
        otherQueue.PlacePacket(SDMAWriteDataPacket(qRes->Queue_write_ptr,
                                                   pendingWptr));
        otherQueue.PlacePacket(SDMAWriteDataPacket(qRes->Queue_DoorBell,
                                                   pendingWptr));
#else
        /* Write the wptr and doorbell update using WRITE_DATA packets
         * on a second PM4 queue. This should submit the PM4 packet on
         * the first queue.
         */
        otherQueue.PlacePacket(
            PM4ReleaseMemoryPacket(true, (HSAuint64)qRes->Queue_write_ptr,
                                   pendingWptr, false));
        otherQueue.PlacePacket(
            PM4ReleaseMemoryPacket(true, (HSAuint64)qRes->Queue_DoorBell,
                                   pendingWptr, false));
#endif

        otherQueue.SubmitPacket();
    } else {
        HSAuint64 pendingWptr64 = pm4Queue.GetPendingWptr64();

#ifdef DOORBELL_WRITE_USE_SDMA
        /* Write the wptr and doorbell update using the GPU's SDMA
         * engine. This should submit the PM4 packet on the first
         * queue.
         */
        otherQueue.PlacePacket(SDMAWriteDataPacket(qRes->Queue_write_ptr,
                                                   2, &pendingWptr64));
        otherQueue.PlacePacket(SDMAWriteDataPacket(qRes->Queue_DoorBell,
                                                   2, &pendingWptr64));
#else
        /* Write the 64-bit wptr and doorbell update using RELEASE_MEM
         * packets without IRQs on a second PM4 queue. RELEASE_MEM
         * should perform one atomic 64-bit access. This should submit
         * the PM4 packet on the first queue.
         */
        otherQueue.PlacePacket(
            PM4ReleaseMemoryPacket(true, (HSAuint64)qRes->Queue_write_ptr,
                                   pendingWptr64, true));
        otherQueue.PlacePacket(
            PM4ReleaseMemoryPacket(true, (HSAuint64)qRes->Queue_DoorBell,
                                   pendingWptr64, true));
#endif

        otherQueue.SubmitPacket();
    }

    /* Check that the PM4 packet has been executed */
    EXPECT_TRUE(WaitOnValue(destBuf.As<unsigned int *>(), 0x12345678));
    EXPECT_TRUE(WaitOnValue(destBuf.As<unsigned int *>()+1, 0x87654321));

    EXPECT_SUCCESS(pm4Queue.Destroy());
    EXPECT_SUCCESS(otherQueue.Destroy());

    TEST_END
}
