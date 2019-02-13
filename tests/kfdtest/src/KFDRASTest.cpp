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

#include <math.h>
#include <limits.h>

#include "linux/kfd_ioctl.h"
#include "KFDRASTest.hpp"
#include "PM4Queue.hpp"

#define AMDGPU_DEBUGFS_NODES "/sys/kernel/debug/dri/"
#define RAS_SDMA_ERR_INJECTION "ras/sdma_err_inject"

void KFDRASTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    char path[256];
    int renderNode;
    uint32_t rasFeatures = 0;
    HsaEventDescriptor eventDesc;

    m_pRasEvent = NULL;
    m_setupStatus = false;

    m_defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();

    renderNode = KFDBaseComponentTest::FindDRMRenderNode(m_defaultGPUNode);
    if (renderNode < 0) {
        LOG() << "Skipping test: Could not find render node for default GPU." << std::endl;
        return;
    }

    amdgpu_query_info(m_RenderNodes[renderNode].device_handle,
                AMDGPU_INFO_RAS_ENABLED_FEATURES,
                sizeof(uint32_t), &rasFeatures);
    if (!(rasFeatures &
            (AMDGPU_INFO_RAS_ENABLED_SDMA ||
             AMDGPU_INFO_RAS_ENABLED_UMC ||
             AMDGPU_INFO_RAS_ENABLED_GFX))) {
        LOG() << "Skipping test: GPU doesn't support RAS features!" << std::endl;
        return;
    }

    snprintf(path, sizeof(path), "%s/%d/%s", AMDGPU_DEBUGFS_NODES, renderNode, RAS_SDMA_ERR_INJECTION);

    m_pFile = fopen(path, "w");
    if (!m_pFile) {
        LOG() << "Skipping test: RAS error injection requires root access!" << std::endl;
        return;
    }

    eventDesc.EventType = HSA_EVENTTYPE_MEMORY;
    eventDesc.NodeId = m_defaultGPUNode;
    eventDesc.SyncVar.SyncVar.UserData = NULL;
    eventDesc.SyncVar.SyncVarSize = 0;

    ASSERT_SUCCESS(hsaKmtCreateEvent(&eventDesc, true, false, &m_pRasEvent));

    m_setupStatus = true;

    ROUTINE_END
}

void KFDRASTest::TearDown() {
    ROUTINE_START

    if (m_pRasEvent != NULL) {
        EXPECT_SUCCESS(hsaKmtDestroyEvent(m_pRasEvent));
    }

    fclose(m_pFile);

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

TEST_F(KFDRASTest, BasicTest) {
    TEST_START(TESTPROFILE_RUNALL);

    if (!m_setupStatus) {
        return;
    }

    // write an uncorrectable error injection at address 1 as value 1
    ASSERT_SUCCESS(fwrite("ue 1 1", sizeof(char), 6, m_pFile));

    EXPECT_SUCCESS(hsaKmtWaitOnEvent(m_pRasEvent, g_TestTimeOut));

    EXPECT_EQ(1, m_pRasEvent->EventData.EventData.MemoryAccessFault.Failure.ErrorType);

    TEST_END;
}

TEST_F(KFDRASTest, MixEventsTest) {
    TEST_START(TESTPROFILE_RUNALL);

    if (!m_setupStatus) {
        return;
    }

    PM4Queue queue;
    HsaEvent* pHsaEvent;

    ASSERT_SUCCESS(CreateQueueTypeEvent(false, false, m_defaultGPUNode, &pHsaEvent));
    ASSERT_NE(0, pHsaEvent->EventData.HWData2);

    ASSERT_SUCCESS(queue.Create(m_defaultGPUNode));

    queue.PlaceAndSubmitPacket(PM4ReleaseMemoryPacket(false,
            pHsaEvent->EventData.HWData2, pHsaEvent->EventId));

    queue.Wait4PacketConsumption();

    EXPECT_SUCCESS(hsaKmtWaitOnEvent(pHsaEvent, g_TestTimeOut));

    ASSERT_SUCCESS(fwrite("ue 1 1", sizeof(char), 6, m_pFile));

    EXPECT_SUCCESS(hsaKmtWaitOnEvent(m_pRasEvent, g_TestTimeOut));

    EXPECT_EQ(1, m_pRasEvent->EventData.EventData.MemoryAccessFault.Failure.ErrorType);

    EXPECT_SUCCESS(queue.Destroy());
    EXPECT_SUCCESS(hsaKmtDestroyEvent(pHsaEvent));

    TEST_END;
}
