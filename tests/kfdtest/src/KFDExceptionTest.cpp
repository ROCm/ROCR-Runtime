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

#include "KFDExceptionTest.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "SDMAPacket.hpp"
#include "SDMAQueue.hpp"
#include "Dispatch.hpp"

void KFDExceptionTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);

    ROUTINE_END
}

void KFDExceptionTest::TearDown() {
    ROUTINE_START

    if (m_pIsaGen)
        delete m_pIsaGen;
    m_pIsaGen = NULL;

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

/* Test for memory exception. The function expects a Memory Fault to be
 * triggered by the GPU when it tries to copy dword from pSrc to pDst.
 * Should be called from a Child Process since the Memory Fault causes
 * all the queues to be halted.
*/
void KFDExceptionTest::TestMemoryException(int defaultGPUNode, HSAuint64 pSrc,
                                           HSAuint64 pDst, unsigned int dimX,
                                           unsigned int dimY, unsigned int dimZ) {
    PM4Queue queue;
    HsaEvent *vmFaultEvent;
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    HSAuint64 faultAddress, page_mask = ~((HSAuint64)PAGE_SIZE - 1);
    Dispatch dispatch(isaBuffer, false);

    HsaEventDescriptor eventDesc;
    eventDesc.EventType = HSA_EVENTTYPE_MEMORY;
    eventDesc.NodeId = defaultGPUNode;
    eventDesc.SyncVar.SyncVar.UserData = NULL;
    eventDesc.SyncVar.SyncVarSize = 0;

    m_pIsaGen->GetCopyDwordIsa(isaBuffer);
    m_ChildStatus = queue.Create(defaultGPUNode);
    if (m_ChildStatus != HSAKMT_STATUS_SUCCESS) {
        WARN() << "Queue create failed" << std::endl;
        return;
    }
    m_ChildStatus = hsaKmtCreateEvent(&eventDesc, true, false, &vmFaultEvent);
    if (m_ChildStatus != HSAKMT_STATUS_SUCCESS) {
        WARN() << "Event create failed" << std::endl;
        goto queuefail;
    }

    dispatch.SetDim(dimX, dimY, dimZ);
    dispatch.SetArgs(reinterpret_cast<void *>(pSrc), reinterpret_cast<void *>(pDst));
    dispatch.Submit(queue);

    m_ChildStatus = hsaKmtWaitOnEvent(vmFaultEvent, g_TestTimeOut);
    if (m_ChildStatus != HSAKMT_STATUS_SUCCESS) {
        WARN() << "Wait failed. No Exception triggered" << std::endl;
        goto eventfail;
    }

    if (vmFaultEvent->EventData.EventType != HSA_EVENTTYPE_MEMORY) {
        WARN() << "Unexpected Event Received " << vmFaultEvent->EventData.EventType
               << std::endl;
        m_ChildStatus = HSAKMT_STATUS_ERROR;
        goto eventfail;
    }
    faultAddress = vmFaultEvent->EventData.EventData.MemoryAccessFault.VirtualAddress;
    if (faultAddress != (pSrc & page_mask) &&
        faultAddress != (pDst & page_mask) ) {
        WARN() << "Unexpected Fault Address " << faultAddress
               << " expected " << (pSrc & page_mask) << " or "
               << (pDst & page_mask) << std::endl;
        m_ChildStatus = HSAKMT_STATUS_ERROR;
    }

eventfail:
    hsaKmtDestroyEvent(vmFaultEvent);
queuefail:
    queue.Destroy();
}

/* Test Bad Address access in a child process */
TEST_F(KFDExceptionTest, InvalidBadAddress) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId == FAMILY_RV) {
        LOG() << "Skipping test: IOMMU issues on Raven." << std::endl;
        return;
    }

    m_ChildPid = fork();
    if (m_ChildPid == 0) {
        m_ChildStatus = hsaKmtOpenKFD();
        if (m_ChildStatus != HSAKMT_STATUS_SUCCESS) {
            WARN() << "KFD open failed in child process" << std::endl;
            return;
        }

        HsaMemoryBuffer srcBuffer(PAGE_SIZE, defaultGPUNode, false);

        srcBuffer.Fill(0xAA55AA55);
        TestMemoryException(defaultGPUNode, srcBuffer.As<HSAuint64>(),
                            0x12345678ULL);
    } else {
        int childStatus;

        waitpid(m_ChildPid, &childStatus, 0);
        if (is_dgpu()) {
            ASSERT_EQ(WIFEXITED(childStatus), true);
            ASSERT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        } else {
            ASSERT_EQ(WIFSIGNALED(childStatus), true);
            ASSERT_EQ(WTERMSIG(childStatus), SIGSEGV);
        }
    }

    TEST_END
}

/* TODO: Check why this test doesn't behave as expected.
 * Expected behaviour: Since pDst is not mapped in GPU VM, an attempt
 *  to write to pDst should trigger a Memory Fault.
 *  However, If pDst > 40bits (MC Size) GPU does not create Memory Fault.
 *  An EOP interrupt is generated however as expected no data is copied
 *  into pDst.
 */
TEST_F(KFDExceptionTest, InvalidPPRWriteProtection) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (is_dgpu()) {
        LOG() << "Skipping test: Test requires APU." << std::endl;
        return;
    }

    if (m_FamilyId == FAMILY_RV) {
        LOG() << "Skipping test: IOMMU issues on Raven." << std::endl;
        return;
    }

    m_ChildPid = fork();
    if (m_ChildPid == 0) {
        m_ChildStatus = hsaKmtOpenKFD();
        if (m_ChildStatus != HSAKMT_STATUS_SUCCESS) {
            WARN() << "KFD open failed in child process" << std::endl;
            return;
        }

        HsaMemoryBuffer srcBuffer(PAGE_SIZE, defaultGPUNode, false);
        void* pDst = VirtualAllocMemory(NULL, PAGE_SIZE, MEM_READ);

        LOG() << "PPR dst address is " << pDst << std::endl;

        srcBuffer.Fill(0xAA55AA55);
        TestMemoryException(defaultGPUNode, srcBuffer.As<HSAuint64>(),
                            (HSAuint64)pDst);

        /* Wait for enough time here to ensure this process got killed by kernel
         * due to PPR exception.
         */
        sleep(5);
    } else {
        int childStatus;

        waitpid(m_ChildPid, &childStatus, 0);
        ASSERT_EQ(WIFSIGNALED(childStatus), true);
        ASSERT_EQ(WTERMSIG(childStatus), SIGSEGV);
    }

    TEST_END
}

/* TODO: Same as previous test InvalidPPRWriteProtection */
TEST_F(KFDExceptionTest, InvalidPPRReadProtection) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (is_dgpu()) {
        LOG() << "Skipping test: Test requires APU." << std::endl;
        return;
    }

    if (m_FamilyId == FAMILY_RV) {
        LOG() << "Skipping test: IOMMU issues on Raven." << std::endl;
        return;
    }

    m_ChildPid = fork();
    if (m_ChildPid == 0) {
        m_ChildStatus = hsaKmtOpenKFD();
        if (m_ChildStatus != HSAKMT_STATUS_SUCCESS) {
            WARN() << "KFD open failed in child process" << std::endl;
            return;
        }

        HsaMemoryBuffer dstBuffer(PAGE_SIZE, defaultGPUNode, false);
        void* pSrc = VirtualAllocMemory(NULL, PAGE_SIZE, MEM_NONE);

        LOG() << "PPR src address is " << pSrc << std::endl;

        dstBuffer.Fill(0xAA55AA55);
        TestMemoryException(defaultGPUNode, (HSAuint64)pSrc,
                            dstBuffer.As<HSAuint64>());

        /* Wait for enough time here to ensure this process got killed by kernel
         * due to PPR exception.
         */
        sleep(5);
    } else {
        int childStatus;

        waitpid(m_ChildPid, &childStatus, 0);
        ASSERT_EQ(WIFSIGNALED(childStatus), true);
        ASSERT_EQ(WTERMSIG(childStatus), SIGSEGV);
    }

    TEST_END
}

/* Allocate Read Only buffer. Test Memory Exception failure by
 * attempting to write to that buffer in the child process.
 */
TEST_F(KFDExceptionTest, InvalidWriteAddress) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId == FAMILY_RV) {
        LOG() << "Skipping test: IOMMU issues on Raven." << std::endl;
        return;
    }

    m_ChildPid = fork();
    if (m_ChildPid == 0) {
        m_ChildStatus = hsaKmtOpenKFD();
        if (m_ChildStatus != HSAKMT_STATUS_SUCCESS) {
            WARN() << "KFD open failed in child process" << std::endl;
            return;
        }

        HsaMemoryBuffer readOnlyBuffer(PAGE_SIZE, defaultGPUNode, false /*zero*/,
                                       false /*isLocal*/, true /*isExec*/,
                                       false /*isScratch*/, true /*isReadOnly*/);
        HsaMemoryBuffer srcSysBuffer(PAGE_SIZE, defaultGPUNode, false);

        srcSysBuffer.Fill(0xAA55AA55);

        TestMemoryException(defaultGPUNode, srcSysBuffer.As<HSAuint64>(),
                            readOnlyBuffer.As<HSAuint64>());
    } else {
        int childStatus;

        waitpid(m_ChildPid, &childStatus, 0);
        if (is_dgpu()) {
            ASSERT_EQ(WIFEXITED(childStatus), true);
            ASSERT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        } else {
            ASSERT_EQ(WIFSIGNALED(childStatus), true);
            ASSERT_EQ(WTERMSIG(childStatus), SIGSEGV);
        }
    }

    TEST_END
}

/* Test VM fault storm handling by copying to/from invalid pointers
 * with lots of work items at the same time
 */
TEST_F(KFDExceptionTest, FaultStorm) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (m_FamilyId == FAMILY_RV) {
        LOG() << "Skipping test: IOMMU issues on Raven." << std::endl;
        return;
    }

    HSAKMT_STATUS status;

    m_ChildPid = fork();
    if (m_ChildPid == 0) {
        m_ChildStatus = hsaKmtOpenKFD();
        if (m_ChildStatus != HSAKMT_STATUS_SUCCESS) {
            WARN() << "KFD open failed in child process" << std::endl;
            return;
        }

        TestMemoryException(defaultGPUNode, 0x12345678, 0x76543210, 1024, 1024, 1);
    } else {
        int childStatus;

        waitpid(m_ChildPid, &childStatus, 0);
        if (is_dgpu()) {
            ASSERT_EQ(WIFEXITED(childStatus), true);
            ASSERT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        } else {
            ASSERT_EQ(WIFSIGNALED(childStatus), true);
            ASSERT_EQ(WTERMSIG(childStatus), SIGSEGV);
        }
    }

    TEST_END
}
