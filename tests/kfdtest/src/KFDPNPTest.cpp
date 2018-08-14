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

#include "KFDPNPTest.hpp"
#include "KFDTestUtil.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "hsakmt.h"

bool KFDPNPTest::m_SetupSuccess = false;

void KFDPNPTest::SetUpTestCase() {
    ROUTINE_START

    AcquirePrivilege(OS_DRIVER_OPERATIONS);

    // If AcquirePrivilege fails, it will throw and we will not reach here.
    m_SetupSuccess = true;

    ROUTINE_END
}

void KFDPNPTest::TearDownTestCase() {
}


void KFDPNPTest::SetUp() {
    ROUTINE_START

    ASSERT_TRUE(m_SetupSuccess);

    KFDBaseComponentTest::SetUp();

    ROUTINE_END
}

void KFDPNPTest::TearDown() {
    ROUTINE_START

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

TEST_F(KFDPNPTest, DisableWithActiveProcess) {
    TEST_START(TESTPROFILE_RUNALL);

    DisableKfd();
    EnableKfd();

    TEST_END
}

TEST_F(KFDPNPTest, DisableAndCreateQueue) {
    TEST_START(TESTPROFILE_RUNALL);

    PM4Queue queue;
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer destBuffer(PAGE_SIZE, defaultGPUNode);

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    queue.PlaceAndSubmitPacket(PM4WriteDataPacket(destBuffer.As<unsigned int*>(), 0x1, 0x2));
    queue.Wait4PacketConsumption();

    WaitOnValue(&(destBuffer.As<unsigned int*>()[0]), 0x1);
    WaitOnValue(&(destBuffer.As<unsigned int*>()[1]), 0x2);

    ASSERT_SUCCESS(queue.Destroy());

    DisableKfd();
    EnableKfd();

    ASSERT_NE(HSAKMT_STATUS_SUCCESS, queue.Create(defaultGPUNode))
        << "Queue creation should fail after a topology change.";

    TEST_END
}
