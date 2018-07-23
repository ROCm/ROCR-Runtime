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

#include "KFDBaseComponentTest.hpp"
#include "KFDTestUtil.hpp"

void KFDBaseComponentTest::SetUpTestCase() {
}

void KFDBaseComponentTest::TearDownTestCase() {
}

void KFDBaseComponentTest::SetUp() {
    ROUTINE_START

    ASSERT_SUCCESS(hsaKmtOpenKFD());
    EXPECT_SUCCESS(hsaKmtGetVersion(&m_VersionInfo));
    memset( &m_SystemProperties, 0, sizeof(m_SystemProperties) );

    /** in order to be correctly testing the KFD interfaces and ensure
     *  that the KFD acknowledges relevant node parameters
     *  for the rest of the tests and used for more specific topology tests
     *  call to GetSystemProperties for a system snapshot of the topology here
     */
    ASSERT_SUCCESS(hsaKmtAcquireSystemProperties(&m_SystemProperties));
    ASSERT_GT(m_SystemProperties.NumNodes, HSAuint32(0)) << "HSA has no nodes.";

    m_NodeInfo.Init(m_SystemProperties.NumNodes);

    // setting memory flags with default values , can be modified according to needs
    m_MemoryFlags.ui32.NonPaged = 0;                         // Paged
    m_MemoryFlags.ui32.CachePolicy = HSA_CACHING_NONCACHED;  // Non cached
    m_MemoryFlags.ui32.ReadOnly = 0;                         // Read/Write
    m_MemoryFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;         // 4KB page
    m_MemoryFlags.ui32.HostAccess = 1;                       // host accessible
    m_MemoryFlags.ui32.NoSubstitute = 0;                     // fall back to node 0 if needed
    m_MemoryFlags.ui32.GDSMemory = 0;
    m_MemoryFlags.ui32.Scratch = 0;

    const HsaNodeProperties *nodeProperties = m_NodeInfo.HsaDefaultGPUNodeProperties();
    ASSERT_NOTNULL(nodeProperties) << "failed to get HSA default GPU Node properties";

    g_TestGPUFamilyId = FamilyIdFromNode(nodeProperties);

    m_FamilyId = g_TestGPUFamilyId;

    ROUTINE_END
}

void KFDBaseComponentTest::TearDown() {
    ROUTINE_START

    ASSERT_SUCCESS(hsaKmtReleaseSystemProperties());
    ASSERT_SUCCESS(hsaKmtCloseKFD());

    ROUTINE_END
}

HSAuint64 KFDBaseComponentTest::GetSysMemSize() {
    const HsaNodeProperties *nodeProps;
    HsaMemoryProperties cpuMemoryProps;
    HSAuint64 systemMemSize = 0;

    /* Find System Memory size */
    for (unsigned node = 0; node < m_SystemProperties.NumNodes; node++) {
        nodeProps = m_NodeInfo.GetNodeProperties(node);
        if (nodeProps != NULL && nodeProps->NumCPUCores > 0 && nodeProps->NumMemoryBanks > 0) {
            /* For NUMA nodes, memory is distributed among differnt nodes.
             * Compute total system memory size. KFD driver also computes
             * the system memory (si_meminfo) similarly
             */
            EXPECT_SUCCESS(hsaKmtGetNodeMemoryProperties(node, 1, &cpuMemoryProps));
            systemMemSize += cpuMemoryProps.SizeInBytes;
        }
    }

    return systemMemSize;
}

HSAuint64 KFDBaseComponentTest::GetVramSize(int defaultGPUNode) {
    const HsaNodeProperties *nodeProps;

    /* Find framebuffer size */
    nodeProps = m_NodeInfo.GetNodeProperties(defaultGPUNode);
    EXPECT_NE((const HsaNodeProperties *)NULL, nodeProps);
    HSAuint32 numBanks = nodeProps->NumMemoryBanks;
    HsaMemoryProperties memoryProps[numBanks];
    EXPECT_SUCCESS(hsaKmtGetNodeMemoryProperties(defaultGPUNode, numBanks, memoryProps));
    unsigned bank;
    for (bank = 0; bank < numBanks; bank++) {
        if (memoryProps[bank].HeapType == HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE
                || memoryProps[bank].HeapType == HSA_HEAPTYPE_FRAME_BUFFER_PUBLIC)
            return memoryProps[bank].SizeInBytes;
    }

    return 0;
}
