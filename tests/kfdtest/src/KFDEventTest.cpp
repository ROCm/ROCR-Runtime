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

#include <math.h>
#include <limits.h>

#include "KFDEventTest.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"


void KFDEventTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();
    m_pHsaEvent = NULL;

    ROUTINE_END
}

void KFDEventTest::TearDown() {
    ROUTINE_START

    // not all tests create event, destroy only if there is one
    if (m_pHsaEvent != NULL) {
        // hsaKmtDestroyEvent moved to TearDown to make sure it is being called
        EXPECT_SUCCESS(hsaKmtDestroyEvent(m_pHsaEvent));
    }

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

TEST_F(KFDEventTest, CreateDestroyEvent) {
    TEST_START(TESTPROFILE_RUNALL);

    ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, m_NodeInfo.HsaDefaultGPUNode(), &m_pHsaEvent));
    EXPECT_NE(0, m_pHsaEvent->EventData.HWData2);

    // destroy event is being called in test TearDown
    TEST_END;
}

TEST_F(KFDEventTest, CreateMaxEvents) {
    TEST_START(TESTPROFILE_RUNALL);

    static const unsigned int MAX_EVENT_NUMBER = 256;

    HsaEvent* pHsaEvent[MAX_EVENT_NUMBER];

    unsigned int i = 0;

    for (i = 0; i < MAX_EVENT_NUMBER; i++) {
        pHsaEvent[i] = NULL;
        ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, m_NodeInfo.HsaDefaultGPUNode(), &pHsaEvent[i]));
    }

    for (i = 0; i < MAX_EVENT_NUMBER; i++) {
        EXPECT_SUCCESS(hsaKmtDestroyEvent(pHsaEvent[i]));
    }

    TEST_END;
}

TEST_F(KFDEventTest, SignalEvent) {
    TEST_START(TESTPROFILE_RUNALL);

    ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, m_NodeInfo.HsaDefaultGPUNode(), &m_pHsaEvent));
    ASSERT_NE(0, m_pHsaEvent->EventData.HWData2);

    PM4Queue queue;
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    queue.PlaceAndSubmitPacket(PM4ReleaseMemoryPacket(false, m_pHsaEvent->EventData.HWData2, m_pHsaEvent->EventId));

    queue.Wait4PacketConsumption();

    ASSERT_SUCCESS(hsaKmtWaitOnEvent(m_pHsaEvent, g_TestTimeOut));

    ASSERT_SUCCESS(queue.Destroy());

    TEST_END;
}

static uint64_t gettime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((int64_t)ts.tv_sec) * 1000 * 1000 * 1000 + ts.tv_nsec;
}

static inline double pow2_round_up(int num) {
    return pow(2, ceil(log(num)/log(2)));
}

class QueueAndSignalBenchmark {
 private:
    static const int HISTORY_SIZE = 100;

    int mNumEvents;
    int mHistorySlot;
    uint64_t mTimeHistory[HISTORY_SIZE];
    uint64_t mLatHistory[HISTORY_SIZE];

 public:
    QueueAndSignalBenchmark(int events) : mNumEvents(events), mHistorySlot(0) {
        memset(mTimeHistory, 0, sizeof(mTimeHistory));
        memset(mLatHistory, 0, sizeof(mLatHistory));
    }

    int queueAndSignalEvents(int node, int eventCount, uint64_t &time, uint64_t &latency) {
        int r;
        uint64_t startTime;
        PM4Queue queue;

        HsaEvent** pHsaEvent = (HsaEvent**) calloc(eventCount, sizeof(HsaEvent*));
        size_t packetSize = PM4ReleaseMemoryPacket(false, 0, 0).SizeInBytes();
        int qSize = fmax(PAGE_SIZE, pow2_round_up(packetSize*eventCount + 1));

        time = 0;

        r = queue.Create(node, qSize);
        if (r != HSAKMT_STATUS_SUCCESS)
            goto exit;

        for (int i = 0; i < eventCount; i++) {
            r = CreateQueueTypeEvent(false, false, node, &pHsaEvent[i]);
            if (r != HSAKMT_STATUS_SUCCESS)
                goto exit;

            queue.PlacePacket(PM4ReleaseMemoryPacket(false, pHsaEvent[i]->EventData.HWData2, pHsaEvent[i]->EventId));
        }

        startTime = gettime();
        queue.SubmitPacket();
        for (int i = 0; i < eventCount; i++) {
            r = hsaKmtWaitOnEvent(pHsaEvent[i], g_TestTimeOut);

            if (r != HSAKMT_STATUS_SUCCESS)
                goto exit;

            if (i == 0)
                latency = gettime() - startTime;
        }
        time = gettime() - startTime;

exit:
        for (int i = 0; i < eventCount; i++) {
            if (pHsaEvent[i])
                hsaKmtDestroyEvent(pHsaEvent[i]);
        }
        queue.Destroy();

        return r;
    }

    void run(int node) {
        int r = 0;
        uint64_t time = 0, latency = 0;
        uint64_t avgLat = 0, avgTime = 0;
        uint64_t minTime = ULONG_MAX, maxTime = 0;
        uint64_t minLat = ULONG_MAX, maxLat = 0;

        r = queueAndSignalEvents(node, mNumEvents, time, latency);
        ASSERT_EQ(r, HSAKMT_STATUS_SUCCESS);

        mTimeHistory[mHistorySlot%HISTORY_SIZE] = time;
        mLatHistory[mHistorySlot%HISTORY_SIZE] = latency;

        for (int i = 0; i < HISTORY_SIZE; i++) {
            minTime = mTimeHistory[i] < minTime ? mTimeHistory[i] : minTime;
            maxTime = mTimeHistory[i] > maxTime ? mTimeHistory[i] : maxTime;
            avgTime += mTimeHistory[i];

            minLat = mLatHistory[i] < minLat ? mLatHistory[i] : minLat;
            maxLat = mLatHistory[i] > maxLat ? mLatHistory[i] : maxLat;
            avgLat += mLatHistory[i];
        }

        avgTime /= HISTORY_SIZE;
        avgLat /= HISTORY_SIZE;
        mHistorySlot++;

        printf("\033[KEvents: %d History: %d/%d\n", mNumEvents, mHistorySlot, HISTORY_SIZE);
        printf("\033[KMin Latency: %f ms\n", (float)minLat/1000000);
        printf("\033[KMax Latency: %f ms\n", (float)maxLat/1000000);
        printf("\033[KAvg Latency: %f ms\n", (float)avgLat/1000000);
        printf("\033[K   Min Rate: %f IH/ms\n", ((float)mNumEvents)/maxTime*1000000);
        printf("\033[K   Max Rate: %f IH/ms\n", ((float)mNumEvents)/minTime*1000000);
        printf("\033[K   Avg Rate: %f IH/ms\n", ((float)mNumEvents)/avgTime*1000000);
    }
};

TEST_F(KFDEventTest, MeasureInterruptConsumption) {
    TEST_START(TESTPROFILE_RUNALL);
    QueueAndSignalBenchmark latencyBench(128);
    QueueAndSignalBenchmark sustainedBench(4096);

    printf("\033[2J");
    while (true) {
        printf("\033[H");
        printf("--------------------------\n");
        latencyBench.run(m_NodeInfo.HsaDefaultGPUNode());
        printf("--------------------------\n");
        sustainedBench.run(m_NodeInfo.HsaDefaultGPUNode());
        printf("--------------------------\n");
    }

    TEST_END;
}

TEST_F(KFDEventTest, SignalMaxEvents) {
    TEST_START(TESTPROFILE_RUNALL);

    static const unsigned int MAX_EVENT_NUMBER = 4096;
    uint64_t time, latency;

    QueueAndSignalBenchmark maxEventTest(MAX_EVENT_NUMBER);
    maxEventTest.queueAndSignalEvents(m_NodeInfo.HsaDefaultGPUNode(), MAX_EVENT_NUMBER,
            time, latency);

    TEST_END;
}

TEST_F(KFDEventTest, SignalMultipleEventsWaitForAll) {
    TEST_START(TESTPROFILE_RUNALL);

    static const unsigned int EVENT_NUMBER = 64;  // 64 is the maximum for hsaKmtWaitOnMultipleEvents
    static const unsigned int WAIT_BETWEEN_SUBMISSIONS_MS = 50;

    HsaEvent* pHsaEvent[EVENT_NUMBER];
    unsigned int i = 0;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    for (i = 0; i < EVENT_NUMBER; i++) {
        pHsaEvent[i] = NULL;
        ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, defaultGPUNode, &pHsaEvent[i]));
    }

    PM4Queue queue;

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    unsigned int pktSizeDwords = 0;
    for (i = 0; i < EVENT_NUMBER; i++) {
        queue.PlaceAndSubmitPacket(PM4ReleaseMemoryPacket(false, pHsaEvent[i]->EventData.HWData2, pHsaEvent[i]->EventId));
        queue.Wait4PacketConsumption();

        Delay(WAIT_BETWEEN_SUBMISSIONS_MS);
    }

    ASSERT_SUCCESS(hsaKmtWaitOnMultipleEvents(pHsaEvent, EVENT_NUMBER, true, g_TestTimeOut));

    ASSERT_SUCCESS(queue.Destroy());

    for (i = 0; i < EVENT_NUMBER; i++)
        EXPECT_SUCCESS(hsaKmtDestroyEvent(pHsaEvent[i]));

    TEST_END;
}
