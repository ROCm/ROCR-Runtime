/*
 * Copyright (C) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "KFDSVMRangeTest.hpp"
#include <sys/mman.h>
#include <vector>
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "SDMAPacket.hpp"
#include "SDMAQueue.hpp"
#include "Dispatch.hpp"

void KFDSVMRangeTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);

    ROUTINE_END
}

void KFDSVMRangeTest::TearDown() {
    ROUTINE_START

    if (m_pIsaGen)
        delete m_pIsaGen;
    m_pIsaGen = NULL;

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

TEST_F(KFDSVMRangeTest, BasicSystemMemTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    PM4Queue queue;
    HSAuint64 AlternateVAGPU;
    unsigned int BufferSize = PAGE_SIZE;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (!GetVramSize(defaultGPUNode)) {
        LOG() << "Skipping test: No VRAM found." << std::endl;
        return;
    }

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode);
    HsaSVMRange srcSysBuffer(BufferSize, defaultGPUNode);
    HsaSVMRange destSysBuffer(BufferSize, defaultGPUNode);

    srcSysBuffer.Fill(0x01010101);

    m_pIsaGen->GetCopyDwordIsa(isaBuffer);

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    queue.SetSkipWaitConsump(0);

    Dispatch dispatch(isaBuffer);

    dispatch.SetArgs(srcSysBuffer.As<void*>(), destSysBuffer.As<void*>());
    dispatch.Submit(queue);
    dispatch.Sync(g_TestTimeOut);

    EXPECT_SUCCESS(queue.Destroy());

    EXPECT_EQ(destSysBuffer.As<unsigned int*>()[0], 0x01010101);

    TEST_END
}

TEST_F(KFDSVMRangeTest, SetGetAttributesTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL)

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
        return;
    }

    int i;
    unsigned int BufSize = PAGE_SIZE;
    HsaSVMRange *sysBuffer;
    HSAuint32 nAttributes = 5;
    HSA_SVM_ATTRIBUTE outputAttributes[nAttributes];
    HSA_SVM_ATTRIBUTE inputAttributes[] = {
                                                {HSA_SVM_ATTR_PREFETCH_LOC, (HSAuint32)defaultGPUNode},
                                                {HSA_SVM_ATTR_PREFERRED_LOC, (HSAuint32)defaultGPUNode},
                                                {HSA_SVM_ATTR_SET_FLAGS,
                                                 HSA_SVM_FLAG_HOST_ACCESS | HSA_SVM_FLAG_GPU_EXEC | HSA_SVM_FLAG_COHERENT},
                                                {HSA_SVM_ATTR_GRANULARITY, 0xFF},
                                                {HSA_SVM_ATTR_ACCESS, (HSAuint32)defaultGPUNode},
                                          };

    HSAuint32 expectedDefaultResults[] = {
                                             INVALID_NODEID,
                                             INVALID_NODEID,
                                             HSA_SVM_FLAG_HOST_ACCESS | HSA_SVM_FLAG_COHERENT,
                                             9,
                                             0,
                                         };
    HSAint32 enable = -1;
    EXPECT_SUCCESS(hsaKmtGetXNACKMode(&enable));
    expectedDefaultResults[4] = (enable)?HSA_SVM_ATTR_ACCESS:HSA_SVM_ATTR_NO_ACCESS;
    sysBuffer = new HsaSVMRange(BufSize);
    char *pBuf = sysBuffer->As<char *>();

    LOG() << "Get default atrributes" << std::endl;
    memcpy(outputAttributes, inputAttributes, nAttributes * sizeof(HSA_SVM_ATTRIBUTE));
    EXPECT_SUCCESS(hsaKmtSVMGetAttr(pBuf, BufSize,
                                    nAttributes, outputAttributes));

    for (i = 0; i < nAttributes; i++) {
        if (outputAttributes[i].type == HSA_SVM_ATTR_ACCESS ||
            outputAttributes[i].type == HSA_SVM_ATTR_ACCESS_IN_PLACE ||
            outputAttributes[i].type == HSA_SVM_ATTR_NO_ACCESS)
            EXPECT_EQ(outputAttributes[i].type, expectedDefaultResults[i]);
        else
            EXPECT_EQ(outputAttributes[i].value, expectedDefaultResults[i]);
    }
    LOG() << "Setting/Getting atrributes" << std::endl;
    memcpy(outputAttributes, inputAttributes, nAttributes * sizeof(HSA_SVM_ATTRIBUTE));
    EXPECT_SUCCESS(hsaKmtSVMSetAttr(pBuf, BufSize,
                                    nAttributes, inputAttributes));
    EXPECT_SUCCESS(hsaKmtSVMGetAttr(pBuf, BufSize,
                                    nAttributes, outputAttributes));
   for (i = 0; i < nAttributes; i++) {
        if (outputAttributes[i].type == HSA_SVM_ATTR_ACCESS ||
            outputAttributes[i].type == HSA_SVM_ATTR_ACCESS_IN_PLACE ||
            outputAttributes[i].type == HSA_SVM_ATTR_NO_ACCESS)
            EXPECT_EQ(inputAttributes[i].type, outputAttributes[i].type);
        else
            EXPECT_EQ(inputAttributes[i].value, outputAttributes[i].value);
   }

    TEST_END
}

TEST_F(KFDSVMRangeTest, XNACKModeTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    HSAuint32 i, j;
    HSAint32 r;
    PM4Queue queue;
    HSAint32 enable = 0;
    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();

    EXPECT_SUCCESS(hsaKmtGetXNACKMode(&enable));
    for (i = 0; i < 2; i++) {
        enable = !enable;
        r = hsaKmtSetXNACKMode(enable);
        if (r == HSAKMT_STATUS_SUCCESS) {
            LOG() << "XNACK mode: " << std::boolalpha << enable <<
                     " supported" << std::endl;

            for (j = 0; j < gpuNodes.size(); j++) {
                LOG() << "Creating queue and try to set xnack mode on node: "
                      << gpuNodes.at(j) << std::endl;
                ASSERT_SUCCESS(queue.Create(gpuNodes.at(j)));
                EXPECT_EQ(HSAKMT_STATUS_ERROR,
                        hsaKmtSetXNACKMode(enable));
                EXPECT_SUCCESS(queue.Destroy());
            }
        } else if (r == HSAKMT_STATUS_NOT_SUPPORTED) {
            LOG() << "XNACK mode: " << std::boolalpha << enable <<
                     " NOT supported" << std::endl;
        }
    }
    TEST_END
}

TEST_F(KFDSVMRangeTest, InvalidRangeTest) {
    TEST_START(TESTPROFILE_RUNALL)

    if (!SVMAPISupported())
        return;

    HSAuint32 Flags;;
    HSAKMT_STATUS ret;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    Flags = HSA_SVM_FLAG_HOST_ACCESS | HSA_SVM_FLAG_COHERENT;

    ret = RegisterSVMRange(defaultGPUNode, reinterpret_cast<void *>(0x10000), 0x1000, 0, Flags);
    EXPECT_NE(ret, HSAKMT_STATUS_SUCCESS);

    TEST_END
}

void KFDSVMRangeTest::SplitRangeTest(int defaultGPUNode, int prefetch_location) {
    unsigned int BufSize = 16 * PAGE_SIZE;

    if (!SVMAPISupported())
        return;

    HsaSVMRange *sysBuffer;
    HsaSVMRange *sysBuffer2;
    HsaSVMRange *sysBuffer3;
    HsaSVMRange *sysBuffer4;

    void *pBuf;

    // case 1
    pBuf = mmap(0, BufSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sysBuffer = new HsaSVMRange(pBuf, BufSize, defaultGPUNode, prefetch_location);
    sysBuffer2 = new HsaSVMRange(reinterpret_cast<char *>(pBuf) + 8192, PAGE_SIZE, defaultGPUNode, prefetch_location);
    delete sysBuffer2;
    delete sysBuffer;
    munmap(pBuf, BufSize);

    // case 2.1
    pBuf = mmap(0, BufSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sysBuffer = new HsaSVMRange(pBuf, BufSize, defaultGPUNode, prefetch_location);
    sysBuffer2 = new HsaSVMRange(reinterpret_cast<char *>(pBuf) + 4096, BufSize - 4096, defaultGPUNode,
                                 prefetch_location);
    delete sysBuffer2;
    delete sysBuffer;
    munmap(pBuf, BufSize);

    // case 2.2
    pBuf = mmap(0, BufSize + 8192, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sysBuffer = new HsaSVMRange(pBuf, BufSize, defaultGPUNode, prefetch_location);
    sysBuffer2 = new HsaSVMRange(reinterpret_cast<char *>(pBuf) + 8192, BufSize, defaultGPUNode, prefetch_location);
    delete sysBuffer2;
    delete sysBuffer;
    munmap(pBuf, BufSize + 8192);

    // case 3
    pBuf = mmap(0, BufSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sysBuffer = new HsaSVMRange(pBuf, BufSize, defaultGPUNode, prefetch_location);
    sysBuffer2 = new HsaSVMRange(reinterpret_cast<char *>(pBuf), BufSize - 8192, defaultGPUNode, prefetch_location);
    delete sysBuffer2;
    delete sysBuffer;
    munmap(pBuf, BufSize);

    // case 4.1
    pBuf = mmap(0, BufSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sysBuffer = new HsaSVMRange(pBuf, BufSize, defaultGPUNode, prefetch_location);
    sysBuffer2 = new HsaSVMRange(pBuf, BufSize, defaultGPUNode, prefetch_location);
    delete sysBuffer2;
    delete sysBuffer;
    munmap(pBuf, BufSize);

    // case 4.2
    pBuf = mmap(0, BufSize + 8192, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sysBuffer = new HsaSVMRange(pBuf, BufSize, defaultGPUNode, prefetch_location);
    sysBuffer2 = new HsaSVMRange(pBuf, BufSize + 8192, defaultGPUNode, prefetch_location);
    delete sysBuffer2;
    delete sysBuffer;
    munmap(pBuf, BufSize + 8192);

    // case 5
    pBuf = mmap(0, BufSize + 65536, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sysBuffer = new HsaSVMRange(reinterpret_cast<char *>(pBuf) + 8192, 8192, defaultGPUNode, prefetch_location);
    sysBuffer2 = new HsaSVMRange(reinterpret_cast<char *>(pBuf) + 32768, 8192, defaultGPUNode, prefetch_location);
    sysBuffer3 = new HsaSVMRange(pBuf, BufSize + 65536, defaultGPUNode, prefetch_location);
    delete sysBuffer2;
    delete sysBuffer3;
    delete sysBuffer;
    munmap(pBuf, BufSize + 65536);

    // case 6, unregister after free
    pBuf = mmap(0, BufSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sysBuffer = new HsaSVMRange(reinterpret_cast<char *>(pBuf) + 8192, 8192, defaultGPUNode, prefetch_location);
    munmap(pBuf, BufSize);
    delete sysBuffer;
}

TEST_F(KFDSVMRangeTest, SplitSystemRangeTest) {
    const HsaNodeProperties *pNodeProperties = m_NodeInfo.HsaDefaultGPUNodeProperties();
    TEST_START(TESTPROFILE_RUNALL)

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
        return;
    }

    SplitRangeTest(defaultGPUNode, 0);

    TEST_END
}

TEST_F(KFDSVMRangeTest, EvictSystemRangeTest) {
    const HsaNodeProperties *pNodeProperties = m_NodeInfo.HsaDefaultGPUNodeProperties();
    TEST_START(TESTPROFILE_RUNALL)

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
        return;
    }

    HSAuint32 stackData[2 * PAGE_SIZE] = {0};
    char *pBuf = reinterpret_cast<char *>(((uint64_t)stackData + PAGE_SIZE) & ~(PAGE_SIZE - 1));
    HSAuint32 *globalData = reinterpret_cast<uint32_t *>(pBuf);
    const unsigned dstOffset = ((uint64_t)pBuf + 2 * PAGE_SIZE - (uint64_t)stackData) / 4;
    const unsigned sdmaOffset = dstOffset + PAGE_SIZE;

    *globalData = 0xdeadbeef;

    HsaSVMRange srcBuffer((globalData), PAGE_SIZE, defaultGPUNode);
    HsaSVMRange dstBuffer(&stackData[dstOffset], PAGE_SIZE, defaultGPUNode);
    HsaSVMRange sdmaBuffer(&stackData[sdmaOffset], PAGE_SIZE, defaultGPUNode);

    /* Create PM4 and SDMA queues before fork+COW to test queue
     * eviction and restore
     */
    PM4Queue pm4Queue;
    SDMAQueue sdmaQueue;
    ASSERT_SUCCESS(pm4Queue.Create(defaultGPUNode));
    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->GetCopyDwordIsa(isaBuffer);

    Dispatch dispatch0(isaBuffer);
    dispatch0.SetArgs(srcBuffer.As<void*>(), dstBuffer.As<void*>());
    dispatch0.Submit(pm4Queue);
    dispatch0.Sync(g_TestTimeOut);

    sdmaQueue.PlaceAndSubmitPacket(SDMAWriteDataPacket(sdmaQueue.GetFamilyId(),
                                   sdmaBuffer.As<HSAuint32 *>(), 0x12345678));

    sdmaQueue.Wait4PacketConsumption();
    EXPECT_TRUE(WaitOnValue(&stackData[sdmaOffset], 0x12345678));

    /* Fork a child process to mark pages as COW */
    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        /* Child process waits for a SIGTERM from the parent. It can't
         * make any write access to the stack because we want the
         * parent to make the first write access and get a new copy. A
         * busy loop is the safest way to do that, since any function
         * call (e.g. sleep) would write to the stack.
         */
        while (1)
        {}
        WARN() << "Shouldn't get here!" << std::endl;
        exit(0);
    }

    /* Parent process writes to COW page(s) and gets a new copy. MMU
     * notifier needs to update the GPU mapping(s) for the test to
     * pass.
     */
    *globalData = 0xD00BED00;
    stackData[dstOffset] = 0xdeadbeef;
    stackData[sdmaOffset] = 0xdeadbeef;

    /* Terminate the child process before a possible test failure that
     * would leave it spinning in the background indefinitely.
     */
    int status;
    EXPECT_EQ(0, kill(pid, SIGTERM));
    EXPECT_EQ(pid, waitpid(pid, &status, 0));
    EXPECT_NE(0, WIFSIGNALED(status));
    EXPECT_EQ(SIGTERM, WTERMSIG(status));

    /* Now check that the GPU is accessing the correct page */
    Dispatch dispatch1(isaBuffer);
    dispatch1.SetArgs(srcBuffer.As<void*>(), dstBuffer.As<void*>());
    dispatch1.Submit(pm4Queue);
    dispatch1.Sync(g_TestTimeOut);

    sdmaQueue.PlaceAndSubmitPacket(SDMAWriteDataPacket(sdmaQueue.GetFamilyId(),
                                   sdmaBuffer.As<HSAuint32 *>(), 0xD0BED0BE));
    sdmaQueue.Wait4PacketConsumption();

    EXPECT_SUCCESS(pm4Queue.Destroy());
    EXPECT_SUCCESS(sdmaQueue.Destroy());

    EXPECT_EQ(0xD00BED00, *globalData);
    EXPECT_EQ(0xD00BED00, stackData[dstOffset]);
    EXPECT_EQ(0xD0BED0BE, stackData[sdmaOffset]);

    TEST_END
}

TEST_F(KFDSVMRangeTest, PartialUnmapSysMemTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    unsigned int BufSize = 16 * PAGE_SIZE;
    void *pBuf;

    PM4Queue queue;
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode);
    HsaSVMRange *sysBuffer;
    HsaSVMRange destSysBuffer(BufSize, defaultGPUNode);

    pBuf = mmap(0, BufSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    sysBuffer = new HsaSVMRange(pBuf, BufSize, defaultGPUNode, 0);
    sysBuffer->Fill(0x01010101);

    char *pBuf2 = reinterpret_cast<char *>(pBuf) + 8192;
    unsigned int Buf2Size = 4 * PAGE_SIZE;
    char *pBuf3 = pBuf2 + Buf2Size;

    munmap(pBuf2, Buf2Size);

    m_pIsaGen->GetCopyDwordIsa(isaBuffer);
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    Dispatch dispatch(isaBuffer);
    Dispatch dispatch2(isaBuffer);

    dispatch.SetArgs(pBuf3, destSysBuffer.As<void*>());
    dispatch.Submit(queue);
    dispatch.Sync(g_TestTimeOut);
    EXPECT_EQ(destSysBuffer.As<unsigned int*>()[0], 0x01010101);

    dispatch2.SetArgs(pBuf, destSysBuffer.As<void*>());
    dispatch2.Submit(queue);
    dispatch2.Sync(g_TestTimeOut);

    EXPECT_EQ(destSysBuffer.As<unsigned int*>()[0], 0x01010101);

    EXPECT_SUCCESS(queue.Destroy());
    munmap(pBuf, BufSize);

    TEST_END
}

TEST_F(KFDSVMRangeTest, BasicVramTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    PM4Queue queue;
    HSAuint64 AlternateVAGPU;
    unsigned int BufferSize = PAGE_SIZE;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (!GetVramSize(defaultGPUNode)) {
        LOG() << "Skipping test: No VRAM found." << std::endl;
        return;
    }

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode);
    HsaSVMRange srcSysBuffer(BufferSize, defaultGPUNode);
    HsaSVMRange locBuffer(BufferSize, defaultGPUNode, defaultGPUNode);
    HsaSVMRange destSysBuffer(BufferSize, defaultGPUNode);

    srcSysBuffer.Fill(0x01010101);

    m_pIsaGen->GetCopyDwordIsa(isaBuffer);

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    queue.SetSkipWaitConsump(0);

    Dispatch dispatch(isaBuffer);
    Dispatch dispatch2(isaBuffer);

    dispatch.SetArgs(srcSysBuffer.As<void*>(), locBuffer.As<void*>());
    dispatch.Submit(queue);
    dispatch.Sync(g_TestTimeOut);

    dispatch2.SetArgs(locBuffer.As<void*>(), destSysBuffer.As<void*>());
    dispatch2.Submit(queue);
    dispatch2.Sync(g_TestTimeOut);

    EXPECT_SUCCESS(queue.Destroy());

    EXPECT_EQ(destSysBuffer.As<unsigned int*>()[0], 0x01010101);
    TEST_END
}

TEST_F(KFDSVMRangeTest, SplitVramRangeTest) {
    TEST_START(TESTPROFILE_RUNALL)

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
        return;
    }

    SplitRangeTest(defaultGPUNode, defaultGPUNode);
    TEST_END
}

TEST_F(KFDSVMRangeTest, PrefetchTest) {
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    unsigned int BufSize = 16 << 10;
    HsaSVMRange *sysBuffer;
    uint32_t node_id;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    sysBuffer = new HsaSVMRange(BufSize, defaultGPUNode);
    char *pBuf = sysBuffer->As<char *>();
    /* Using invalid svm range to get prefetch node should return failed */
    delete sysBuffer;
    EXPECT_SUCCESS(!SVMRangeGetPrefetchNode(pBuf, BufSize, &node_id));

    sysBuffer = new HsaSVMRange(BufSize, defaultGPUNode);
    pBuf = sysBuffer->As<char *>();
    char *pLocBuf = pBuf + BufSize / 2;

    EXPECT_SUCCESS(SVMRangeGetPrefetchNode(pBuf, BufSize, &node_id));
    EXPECT_EQ(node_id, 0);

    EXPECT_SUCCESS(SVMRangePrefetchToNode(pLocBuf, BufSize / 2, defaultGPUNode));

    EXPECT_SUCCESS(SVMRangeGetPrefetchNode(pLocBuf, BufSize / 2, &node_id));
    EXPECT_EQ(node_id, defaultGPUNode);

    EXPECT_SUCCESS(SVMRangeGetPrefetchNode(pBuf, BufSize, &node_id));
    EXPECT_EQ(node_id, 0xffffffff);
    delete sysBuffer;

    TEST_END
}

TEST_F(KFDSVMRangeTest, MigrateTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
        return;
    }

    if (!GetVramSize(defaultGPUNode)) {
        LOG() << "Skipping test: No VRAM found." << std::endl;
        return;
    }

    HSAuint32 migrateRepeat = 8;
    unsigned int BufferSize = 16 << 20;

    HsaSVMRange DataBuffer(BufferSize, defaultGPUNode);
    HSAuint32 *pData = DataBuffer.As<HSAuint32 *>();

    HsaSVMRange SysBuffer(BufferSize, defaultGPUNode);
    HSAuint32 *pBuf = SysBuffer.As<HSAuint32 *>();
    EXPECT_SUCCESS(SVMRangePrefetchToNode(pBuf, BufferSize, 0));

    HsaSVMRange SysBuffer2(BufferSize, defaultGPUNode);
    HSAuint32 *pBuf2 = SysBuffer2.As<HSAuint32 *>();
    EXPECT_SUCCESS(SVMRangePrefetchToNode(pBuf2, BufferSize, 0));

    SDMAQueue sdmaQueue;
    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));

    for (HSAuint32 i = 0; i < BufferSize / 4; i++)
        pData[i] = i;

    while (migrateRepeat--) {
        /* Migrate from ram to vram */
        EXPECT_SUCCESS(SVMRangePrefetchToNode(pBuf, BufferSize, defaultGPUNode));
        EXPECT_SUCCESS(SVMRangePrefetchToNode(pBuf2, BufferSize, defaultGPUNode));
        /* Update content in migrated buffer in vram */
        sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                    pBuf, pData, BufferSize));
        sdmaQueue.Wait4PacketConsumption();
        sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                    pBuf2, pData, BufferSize));
        sdmaQueue.Wait4PacketConsumption();

        /* Migrate from vram to ram
         * CPU access the buffer migrated to vram have page fault
         * page fault trigger migration from vram back to ram
         * so SysBuffer should have same value as in vram
         */
        for (HSAuint32 i = 0; i < BufferSize / 4; i++) {
            ASSERT_EQ(i, pBuf[i]);
            ASSERT_EQ(i, pBuf2[i]);
        }
   }

    /* If xnack off, after migrating back to ram, GPU mapping should be updated to ram
     * test if shade can read from ram
     * If xnack on, GPU mapping should be cleared, test if GPU vm fault can update
     * page table and shade can read from ram.
     */
    sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                pBuf, pData, BufferSize));
    sdmaQueue.Wait4PacketConsumption();
    for (HSAuint32 i = 0; i < BufferSize / 4; i++)
        ASSERT_EQ(i, pBuf[i]);

    TEST_END
}

/*
 * The test changes migration granularity, then trigger CPU page fault to migrate
 * the svm range from vram to ram.
 * Check the dmesg driver output to confirm the number of CPU page fault is correct
 * based on granularity.
 *
 * For example, this is BufferPages = 5, while granularity change from 2 to 0
 * [  292.623498] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597ee000
 * [  292.623727] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597f0000
 * [  292.724414] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597ee000
 * [  292.724824] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597f0000
 * [  292.725094] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597f2000
 * [  292.728186] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597ee000
 * [  292.729171] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597ef000
 * [  292.729576] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597f0000
 * [  292.730010] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597f1000
 * [  292.730931] amdgpu:svm_migrate_to_ram:744: CPU page fault address 0x7f22597f2000
 */
TEST_F(KFDSVMRangeTest, MigrateGranularityTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
            return;
        }

    if (!GetVramSize(defaultGPUNode)) {
        LOG() << "Skipping test: No VRAM found." << std::endl;
        return;
    }

    HSAuint64 BufferPages = 16384;
    HSAuint64 BufferSize = BufferPages * PAGE_SIZE;
    HsaSVMRange SysBuffer(BufferSize, defaultGPUNode);
    HSAint32 *pBuf = SysBuffer.As<HSAint32*>();

    HsaSVMRange SysBuffer2(BufferSize, defaultGPUNode);
    HSAint32 *pBuf2 = SysBuffer2.As<HSAint32*>();

    HSAint32 Granularity;

    SDMAQueue sdmaQueue;
    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));

    for (Granularity = 0; (1ULL << Granularity) <= BufferPages; Granularity++);
    for (HSAuint32 i = 0; i < BufferPages; i++)
        pBuf2[i * PAGE_SIZE / 4] = i;

    while (Granularity--) {
        /* Prefetch the entire range to vram */
        EXPECT_SUCCESS(SVMRangePrefetchToNode(pBuf, BufferSize, defaultGPUNode));
        EXPECT_SUCCESS(SVMRangSetGranularity(pBuf, BufferSize, Granularity));

        /* Change Buffer content in vram, then migrate it back to ram */
        sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                        pBuf, pBuf2, BufferSize));
        sdmaQueue.Wait4PacketConsumption();

        /* Migrate from vram to ram */
        for (HSAuint32 i = 0; i < BufferPages; i++)
            ASSERT_EQ(i, pBuf[i * PAGE_SIZE / 4]);
    }
    TEST_END
}

TEST_F(KFDSVMRangeTest, MigrateLargeBufTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    PM4Queue queue;
    HSAuint64 AlternateVAGPU;
    unsigned long BufferSize = 1L << 30;
    unsigned long maxSDMASize = 128L << 20;  /* IB size is 4K */
    unsigned long Size, i;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (!GetVramSize(defaultGPUNode)) {
        LOG() << "Skipping test: No VRAM found." << std::endl;
        return;
    }

    HsaSVMRange SysBuffer(BufferSize, defaultGPUNode);
    SysBuffer.Fill(0x1);

    HsaSVMRange SysBuffer2(BufferSize, defaultGPUNode);
    SysBuffer2.Fill(0x2);

    /* Migrate from ram to vram
     * using same address to register to GPU to trigger migration
     * so LocalBuffer will have same value as SysBuffer
     */
    HsaSVMRange LocalBuffer(SysBuffer.As<void*>(), BufferSize, defaultGPUNode, defaultGPUNode);

    SDMAQueue sdmaQueue;

    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));
    for (i = 0; i < BufferSize; i += Size) {
        Size = (BufferSize - i) > maxSDMASize ? maxSDMASize : (BufferSize - i);
        sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                    SysBuffer2.As<char*>() + i, LocalBuffer.As<char*>() + i, Size));
        sdmaQueue.Wait4PacketConsumption();
    }

    /* Check content in migrated buffer in vram */
    for (i = 0; i < BufferSize / 4; i += 1024)
        ASSERT_EQ(0x1, SysBuffer2.As<unsigned int*>()[i]);

    /* Change LocalBuffer content in vram, then migrate it back to ram */
    SysBuffer2.Fill(0x3);

    for (i = 0; i < BufferSize; i += Size) {
        Size = (BufferSize - i) > maxSDMASize ? maxSDMASize : (BufferSize - i);
        sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                    LocalBuffer.As<char*>() + i, SysBuffer2.As<char*>() + i, Size));
        sdmaQueue.Wait4PacketConsumption();
    }

    /* Migrate from vram to ram
     * CPU access the buffer migrated to vram have page fault
     * page fault trigger migration from vram back to ram
     * so SysBuffer should have same value as in LocalBuffer
     */
    EXPECT_SUCCESS(SVMRangSetGranularity(SysBuffer.As<unsigned int*>(), BufferSize, 30));
    for (i = 0; i < BufferSize / 4; i += 1024)
        ASSERT_EQ(0x3, SysBuffer.As<unsigned int*>()[i]);

    /* After migrating back to ram, GPU mapping should be updated to ram
     * test if shade can read from ram
     */
    SysBuffer.Fill(0x4);

    for (i = 0; i < BufferSize; i += Size) {
        Size = (BufferSize - i) > maxSDMASize ? maxSDMASize : (BufferSize - i);
        sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                    SysBuffer2.As<char*>() + i, LocalBuffer.As<char*>() + i, Size));
        sdmaQueue.Wait4PacketConsumption();
    }

    for (i = 0; i < BufferSize / 4; i += 1024)
        ASSERT_EQ(0x4, SysBuffer2.As<unsigned int*>()[i]);

    TEST_END
}

TEST_F(KFDSVMRangeTest, MigratePolicyTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
        return;
    }

    if (!GetVramSize(defaultGPUNode)) {
        LOG() << "Skipping test: No VRAM found." << std::endl;
        return;
    }

    unsigned long BufferSize = 1UL << 20;

    HsaSVMRange DataBuffer(BufferSize, defaultGPUNode);
    HSAuint64 *pData = DataBuffer.As<HSAuint64 *>();

    HsaSVMRange SysBuffer(BufferSize, defaultGPUNode);
    HSAuint64 *pBuf = SysBuffer.As<HSAuint64 *>();

    SDMAQueue sdmaQueue;
    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));

    for (HSAuint64 i = 0; i < BufferSize / 8; i++)
        pData[i] = i;

    /* Prefetch to migrate from ram to vram */
    EXPECT_SUCCESS(SVMRangePrefetchToNode(pBuf, BufferSize, defaultGPUNode));

    /* Update content in migrated buffer in vram */
    sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                pBuf, pData, BufferSize));
    sdmaQueue.Wait4PacketConsumption(NULL, HSA_EVENTTIMEOUT_INFINITE);

    /* Migrate from vram to ram
     * CPU access the buffer migrated to vram have page fault
     * page fault trigger migration from vram back to ram
     * so SysBuffer should have same value as in vram
     */
    for (HSAuint64 i = 0; i < BufferSize / 8; i++) {
        ASSERT_EQ(i, pBuf[i]);
        /* Update buf */
        pBuf[i] = i + 1;
    }

    /* Migrate from ram to vram if xnack on
     * If xnack off, after migrating back to ram, GPU mapping should be updated to ram
     * test if shade can read from ram
     * If xnack on, GPU mapping should be cleared, test if GPU vm fault can update
     * page table and shade can read from ram.
     */
//#define USE_PM4_QUEUE_TRIGGER_VM_FAULT
#ifdef USE_PM4_QUEUE_TRIGGER_VM_FAULT
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode);
    PM4Queue queue;
    m_pIsaGen->GetCopyDwordIsa(isaBuffer);
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    for (HSAuint64 i = 0; i < BufferSize / 8; i += 512) {
        Dispatch dispatch(isaBuffer);
        
        dispatch.SetArgs(pBuf + i, pData + i);
        dispatch.Submit(queue);
        dispatch.Sync(HSA_EVENTTIMEOUT_INFINITE);
    }
#else
    sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                pData, pBuf, BufferSize));
    sdmaQueue.Wait4PacketConsumption(NULL, HSA_EVENTTIMEOUT_INFINITE);
#endif

    for (HSAuint64 i = 0; i < BufferSize / 8; i += 512)
        ASSERT_EQ(i + 1, pData[i]);

    ASSERT_SUCCESS(sdmaQueue.Destroy());

    TEST_END
}

/* Multiple GPU migration test
 *
 * Steps:
 *     1. Prefetch pBuf, pData to all GPUs, to test migration from GPU to GPU
 *     2. Use sdma queue on all GPUs, to copy data from pBuf to pData
 *     3. Check pData data
 *
 * Notes:
 *     With xnack on, step 2 will have retry fault on pBuf, to migrate from GPU to GPU,
 *     retry fault on pData, to migrate from CPU to GPU
 *
 *     With xnack off, pBuf and pData should prefetch to CPU to ensure multiple GPU access
 *
 *     step3 migrate pData from GPU to CPU
 *
 * Test will skip if only one GPU found
 */
TEST_F(KFDSVMRangeTest, MultiGPUMigrationTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
        return;
    }

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    if (gpuNodes.size() < 2) {
        LOG() << "Skipping test: at least two GPUs needed." << std::endl;
        return;
    }

    unsigned long BufferSize = 1UL << 20;

    HsaSVMRange SysBuffer(BufferSize, defaultGPUNode);
    HSAuint64 *pBuf = SysBuffer.As<HSAuint64 *>();
    HsaSVMRange DataBuffer(BufferSize, defaultGPUNode);
    HSAuint64 *pData = DataBuffer.As<HSAuint64 *>();

    SDMAQueue sdmaQueue;

    for (HSAuint64 i = 0; i < BufferSize / 8; i++)
        pBuf[i] = i;

    for (HSAuint64 gpuidx = 0; gpuidx < gpuNodes.size(); gpuidx++) {
        EXPECT_SUCCESS(SVMRangeMapToNode(pBuf, BufferSize, gpuNodes.at(gpuidx)));
        EXPECT_SUCCESS(SVMRangePrefetchToNode(pBuf, BufferSize, gpuNodes.at(gpuidx)));

        EXPECT_SUCCESS(SVMRangeMapToNode(pData, BufferSize, gpuNodes.at(gpuidx)));
        EXPECT_SUCCESS(SVMRangePrefetchToNode(pData, BufferSize, gpuNodes.at(gpuidx)));
    }

    for (HSAuint64 gpuidx = 0; gpuidx < gpuNodes.size(); gpuidx++) {
        ASSERT_SUCCESS(sdmaQueue.Create(gpuNodes.at(gpuidx)));

        sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                    pData, pBuf, BufferSize));
        sdmaQueue.Wait4PacketConsumption();

        for (HSAuint64 i = 0; i < BufferSize / 8; i += 512)
            ASSERT_EQ(i, pData[i]);

        EXPECT_SUCCESS(sdmaQueue.Destroy());
    }

    TEST_END
}

/* Multiple GPU access in place test
 *
 * Steps:
 *     1. Prefetch pBuf, pData to all GPUs, with ACCESS_IN_PLACE on GPUs
 *     2. Use sdma queue on all GPUs, to copy data from pBuf to pData
 *     3. Prefetch pData to CPU, check pData data
 *
 * Notes:
 *     With xnack on, step 2 will have retry fault on pBuf, to migrate from GPU to GPU.
 *     If multiple GPU on xGMI same hive, there should not have retry fault on pBuf
 *     because mapping should update to another GPU vram through xGMI
 *
 *     With xnack off, pBuf and pData should prefetch to CPU to ensure multiple GPU access
 *
 *     step3 migrate pData from GPU to CPU, should not have retry fault on GPUs.
 *
 * Test will skip if only one GPU found
 */
TEST_F(KFDSVMRangeTest, MultiGPUAccessInPlaceTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
        return;
    }

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    if (gpuNodes.size() < 2) {
        LOG() << "Skipping test: at least two GPUs needed." << std::endl;
        return;
    }

    unsigned long BufferSize = 1UL << 20;

    HsaSVMRange SysBuffer(BufferSize, defaultGPUNode);
    HSAuint64 *pBuf = SysBuffer.As<HSAuint64 *>();
    HsaSVMRange DataBuffer(BufferSize, defaultGPUNode);
    HSAuint64 *pData = DataBuffer.As<HSAuint64 *>();

    SDMAQueue sdmaQueue;

    for (HSAuint64 i = 0; i < BufferSize / 8; i++)
        pBuf[i] = i;

    for (HSAuint64 gpuidx = 0; gpuidx < gpuNodes.size(); gpuidx++) {
        EXPECT_SUCCESS(SVMRangeMapInPlaceToNode(pBuf, BufferSize, gpuNodes.at(gpuidx)));
        EXPECT_SUCCESS(SVMRangePrefetchToNode(pBuf, BufferSize, gpuNodes.at(gpuidx)));

        EXPECT_SUCCESS(SVMRangeMapInPlaceToNode(pData, BufferSize, gpuNodes.at(gpuidx)));
        EXPECT_SUCCESS(SVMRangePrefetchToNode(pData, BufferSize, gpuNodes.at(gpuidx)));
    }

    for (HSAuint64 gpuidx = 0; gpuidx < gpuNodes.size(); gpuidx++) {
        ASSERT_SUCCESS(sdmaQueue.Create(gpuNodes.at(gpuidx)));

        sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                    pData, pBuf, BufferSize));
        sdmaQueue.Wait4PacketConsumption();

        for (HSAuint64 i = 0; i < BufferSize / 8; i += 512)
            ASSERT_EQ(i, pData[i]);

        EXPECT_SUCCESS(sdmaQueue.Destroy());
    }

    TEST_END
}

/* Multiple thread migration test
 *
 * 2 threads do migration at same time to test range migration race conditon handle.
 *
 * Steps:
 * 1. register 128MB range on system memory, don't map to GPU, 128MB is max size to put in
 *    sdma queue 4KB IB buffer.
 * 2. one thread prefetch range to GPU, another thread use sdma queue to access range at same
 *    time to generate retry vm fault to migrate range to GPU
 * 3. one thread prefetch range to CPU, another thread read range to generate CPU page fault
 *    to migrate range to CPU at same time
 * 4. loop test step 2 and 3 twice, to random CPU/GPU fault and prefetch migration order
 */
struct ReadThreadParams {
    HSAuint64* pBuf;
    HSAint64 BufferSize;
    int defaultGPUNode;
};

unsigned int CpuReadThread(void* p) {
    struct ReadThreadParams* pArgs = reinterpret_cast<struct ReadThreadParams*>(p);

    for (HSAuint64 i = 0; i < pArgs->BufferSize / 8; i += 512)
         EXPECT_EQ(i, pArgs->pBuf[i]);
    return 0;
}

unsigned int GpuReadThread(void* p) {
    struct ReadThreadParams* pArgs = reinterpret_cast<struct ReadThreadParams*>(p);

    EXPECT_SUCCESS(SVMRangePrefetchToNode(pArgs->pBuf, pArgs->BufferSize, pArgs->defaultGPUNode));
    return 0;
}

TEST_F(KFDSVMRangeTest, MultiThreadMigrationTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId < FAMILY_AI) {
        LOG() << std::hex << "Skipping test: No svm range support for family ID 0x" << m_FamilyId << "." << std::endl;
        return;
    }

    unsigned long test_loops = 2;
    unsigned long BufferSize = 1UL << 27;
    HsaSVMRange SysBuffer(BufferSize, defaultGPUNode);
    HSAuint64 *pBuf = SysBuffer.As<HSAuint64 *>();
    HsaSVMRange DataBuffer(BufferSize, defaultGPUNode);
    HSAuint64 *pData = DataBuffer.As<HSAuint64 *>();
    SDMAQueue sdmaQueue;
    uint64_t threadId;
    struct ReadThreadParams params;

    params.pBuf = pBuf;
    params.BufferSize = BufferSize;
    params.defaultGPUNode = defaultGPUNode;

    EXPECT_SUCCESS(sdmaQueue.Create(defaultGPUNode));

    for (HSAuint64 i = 0; i < BufferSize / 8; i++)
        pBuf[i] = i;

    for (HSAuint64 i = 0; i < test_loops; i++) {
        /* 2 threads migrate to GPU */
        sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(sdmaQueue.GetFamilyId(),
                    pData, pBuf, BufferSize));
        ASSERT_EQ(true, StartThread(&GpuReadThread, &params, threadId));
        sdmaQueue.Wait4PacketConsumption();
        WaitForThread(threadId);

        /* 2 threads migrate to cpu */
        ASSERT_EQ(true, StartThread(&CpuReadThread, &params, threadId));
        EXPECT_SUCCESS(SVMRangePrefetchToNode(pBuf, BufferSize, 0));
        WaitForThread(threadId);
    }

    EXPECT_SUCCESS(sdmaQueue.Destroy());

    TEST_END
}
