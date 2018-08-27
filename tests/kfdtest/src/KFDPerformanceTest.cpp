/*
 * Copyright (C) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "SDMAPacket.hpp"
#include "SDMAQueue.hpp"
#include "AqlQueue.hpp"
#include "KFDTestUtilQueue.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include "KFDBaseComponentTest.hpp"

class KFDPerformanceTest: public KFDBaseComponentTest {
 protected:
    virtual void SetUp();
    virtual void TearDown();
};

void KFDPerformanceTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    ROUTINE_END
}

void KFDPerformanceTest::TearDown() {
    ROUTINE_START

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

enum P2PDirection {
    IN = 1,
    OUT = 2,
    IN_OUT = 3,
    NONE = 4,
};

/*
 * Do the copy of one GPU from & to multiple GPUs.
 */
static void
testNodeToNodes(HSAuint32 n1, const HSAuint32 *const n2Array, int n, P2PDirection n1Direction,
        P2PDirection n2Direction, HSAuint64 size, HSAuint64 &speed, HSAuint64 &speed2, std::stringstream &msg) {
    ASSERT_GT(16, unsigned(n - 1));
    HSAuint32 n2[n];
    void *n1Mem, *n2Mem[n];
    HsaMemFlags memFlags = {0};
    memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    memFlags.ui32.HostAccess = 1;
    memFlags.ui32.NonPaged = 1;
    SDMACopyParams array[n * 4];
    int array_count = 0;
    int i;

    ASSERT_SUCCESS(hsaKmtAllocMemory(n1, size, memFlags, &n1Mem));
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(n1Mem, size, NULL));

    for (i = 0; i < n; i++) {
        n2[i] = n2Array[i];
        ASSERT_SUCCESS(hsaKmtAllocMemory(n2[i], size, memFlags, &n2Mem[i]));
        ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(n2Mem[i], size, NULL));
    }

    for (i = 0; i < n; i++) {
        if (n1Direction != NONE)
            ASSERT_NE(n1, 0);
        if (n2Direction != NONE)
            ASSERT_NE(n2[i], 0);

        do {
            if (n1Direction == IN || n1Direction == IN_OUT)
                /* n2Mem -> n1Mem*/
                array[array_count++] = {n1, n2Mem[i], n1Mem, size};
            if (n1Direction == OUT || n1Direction == IN_OUT)
                /* n1Mem -> n2Mem*/
                array[array_count++] = {n1, n1Mem, n2Mem[i], size};
            /* Issue two copies to make full use of sdma.*/
        } while (n1Direction < IN_OUT && n == 1 && array_count % 2);
        /* Do nothing if no IN or OUT specified.*/

        do {
            if (n2Direction == IN || n2Direction == IN_OUT)
                /* n1Mem -> n2Mem*/
                array[array_count++] = {n2[i], n1Mem, n2Mem[i], size};
            if (n2Direction == OUT || n2Direction == IN_OUT)
                /* n2Mem -> n1Mem*/
                array[array_count++] = {n2[i], n2Mem[i], n1Mem, size};
        } while (n2Direction < IN_OUT && array_count % 2);
    }

    sdma_multicopy(array, array_count, &speed, &speed2, &msg);

    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(n1Mem));
    EXPECT_SUCCESS(hsaKmtFreeMemory(n1Mem, size));

    for (i = 0; i < n; i++) {
        EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(n2Mem[i]));
        EXPECT_SUCCESS(hsaKmtFreeMemory(n2Mem[i], size));
    }
}

TEST_F(KFDPerformanceTest, P2PBandWidthTest) {
    TEST_START(TESTPROFILE_RUNALL);
    if (!is_dgpu()) {
        LOG() << "Skipping test: Can't have 2 APUs on the same system." << std::endl;
        return;
    }

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    std::vector<HSAuint32> nodes;
    const bool isSpecified = g_TestDstNodeId != -1 && g_TestNodeId != -1;

    for (unsigned i = 0; i < gpuNodes.size(); i++)
        if (m_NodeInfo.IsGPUNodeLargeBar(gpuNodes.at(i)) &&
                /* Users can use "--node=gpu1 --dst_node=gpu2" to specify devices */
                (!isSpecified || gpuNodes.at(i) == g_TestDstNodeId || gpuNodes.at(i) == g_TestNodeId))
            nodes.push_back(gpuNodes.at(i));

    if (nodes.size() < 2) {
        LOG() << "Skipping test: Need at least two large bar GPU." << std::endl;
        return;
    }

    std::vector<HSAuint32> sysNodes(nodes); // include sysMem node 0...
    sysNodes.insert(sysNodes.begin(),0);

    const int total_tests = 7;
    const char *test_suits_string[total_tests] = {
        "Copy from node to node by [push, NONE]",
        "Copy from node to node by [pull, NONE]",
        "Full duplex copy from node to node by [push|pull, NONE]",
        "Full duplex copy from node to node by [push, push]",
        "Full duplex copy from node to node by [pull, pull]",
        "Copy from node to multiple nodes by [push, NONE]",
        "Copy from multiple nodes to node by [push, NONE]",
    };
    const P2PDirection test_suits[total_tests][2] = {
        /* One node used.*/
        {OUT,   NONE},
        {IN,    NONE},
        {IN_OUT,NONE},
        /* two nodes used.*/
        {OUT,   OUT},
        {IN,    IN},
        /* Multi nodes used*/
        {OUT,   NONE},
        {NONE,  OUT},
    };
    const int twoNodesIdx = 3;
    const int multiNodesIdx = 5;
    const HSAuint32 size = 32ULL << 20;
    int s = 0; //test index;
    std::stringstream msg;
    char str[64];

    for (; s < twoNodesIdx; s++) {
        LOG() << test_suits_string[s] << std::endl;
        msg << test_suits_string[s] << std::endl;

        for (unsigned i = 0; i < nodes.size(); i++) {
            /* Src node is a GPU.*/
            HSAuint32 n1 = nodes[i];
            HSAuint64 speed, speed2;

            /* Pick up dst node which can be sysMem.*/
            for (unsigned j = 0; j < sysNodes.size(); j++) {
                HSAuint32 n2 = sysNodes[j];
                if (n1 == n2)
                    continue;

                snprintf(str, sizeof(str), "[%d -> %d] ", n1, n2);
                msg << str << std::endl;
                testNodeToNodes(n1, &n2, 1, test_suits[s][0], test_suits[s][1], size, speed, speed2, msg);

                LOG() << std::dec << str << (float)speed / 1024 << " - " <<
                                            (float)speed2 / 1024 << " GB/s" << std::endl;
            }
        }
    }

    for (; s < multiNodesIdx; s++) {
        LOG() << test_suits_string[s] << std::endl;
        msg << test_suits_string[s] << std::endl;

        for (unsigned i = 0; i < nodes.size(); i++) {
            HSAuint32 n1 = nodes[i];
            HSAuint64 speed, speed2;

            for (unsigned j = i + 1; j < nodes.size(); j++) {
                HSAuint32 n2 = nodes[j];

                snprintf(str, sizeof(str), "[%d <-> %d] ", n1, n2);
                msg << str << std::endl;
                testNodeToNodes(n1, &n2, 1, test_suits[s][0], test_suits[s][1], size, speed, speed2, msg);

                LOG() << std::dec << str << (float)speed / 1024 << " - " <<
                                            (float)speed2 / 1024 << " GB/s" << std::endl;
            }
        }
    }

    for (; s < total_tests && !isSpecified; s++) {
        LOG() << test_suits_string[s] << std::endl;
        msg << test_suits_string[s] << std::endl;
        /* Just use GPU nodes to do copy.*/
        std::vector<HSAuint32> &src = test_suits[s][0] != NONE ? nodes : sysNodes;
        std::vector<HSAuint32> &dst = test_suits[s][1] != NONE ? nodes : sysNodes;

        for (unsigned i = 0; i < src.size(); i++) {
            HSAuint32 n1 = src[i];
            HSAuint64 speed, speed2;
            HSAuint32 n2[dst.size()];
            int n = 0;
            char str[64];

            for (unsigned j = 0; j < dst.size(); j++)
                if (dst[j] != n1)
                    n2[n++] = dst[j];
            /* At least 2 dst GPUs.*/
            if (n < 2)
                continue;

            if (test_suits[s][1] == OUT)
                snprintf(str, sizeof(str), "[[%d...%d] -> %d] ", dst.front(), dst.back(), n1);
            else
                snprintf(str, sizeof(str), "[%d -> [%d...%d]] ", n1, dst.front(), dst.back());
            msg << str << std::endl;
            testNodeToNodes(n1, n2, n, test_suits[s][0], test_suits[s][1], size, speed, speed2, msg);

            LOG() << std::dec << str << (float)speed / 1024 << " - " <<
                                        (float)speed2 / 1024 << " GB/s" << std::endl;
        }
    }

    /* New line.*/
    LOG() << std::endl << msg.str() << std::endl;

    TEST_END
}
