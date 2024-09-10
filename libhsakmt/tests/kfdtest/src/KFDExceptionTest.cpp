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
#include <sys/mman.h>

void KFDExceptionTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    LOG() << "This Exception test might cause expected page fault "
             "error logs at kernel level." << std::endl;

    ROUTINE_END
}

void KFDExceptionTest::TearDown() {
    ROUTINE_START

    KFDBaseComponentTest::TearDown();

    // WORKAROUND: This needs to be fixed in the kernel
    // Wait 500ms for the kernel to process any fault storms before the
    // next test to avoid reporting incorrect faults in the next test.
    Delay(500);

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

    ASSERT_SUCCESS(m_pAsm->RunAssembleBuf(CopyDwordIsa, isaBuffer.As<char*>()));

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

void KFDExceptionTest::TestSdmaException(int defaultGPUNode, void *pDst) {
    SDMAQueue queue;
    HsaEvent *vmFaultEvent;
    HSAuint64 faultAddress, page_mask = ~((HSAuint64)PAGE_SIZE - 1);


    HsaEventDescriptor eventDesc;
    eventDesc.EventType = HSA_EVENTTYPE_MEMORY;
    eventDesc.NodeId = defaultGPUNode;
    eventDesc.SyncVar.SyncVar.UserData = NULL;
    eventDesc.SyncVar.SyncVarSize = 0;

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

    queue.PlaceAndSubmitPacket(SDMAWriteDataPacket(queue.GetFamilyId(),
                                                   reinterpret_cast<void *>(pDst),
                                                   0x02020202));

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
    if (faultAddress != ((HSAuint64)pDst & page_mask) ) {
        WARN() << "Unexpected Fault Address " << faultAddress
               << " expected " << ((HSAuint64)pDst & page_mask) << std::endl;
        m_ChildStatus = HSAKMT_STATUS_ERROR;
    }

eventfail:
    hsaKmtDestroyEvent(vmFaultEvent);
queuefail:
    queue.Destroy();
}

/* Test Bad Address access in a child process */
TEST_F(KFDExceptionTest, AddressFault) {
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
        KFDBaseComponentTest::TearDown();
        KFDBaseComponentTest::SetUp();

        HsaMemoryBuffer srcBuffer(PAGE_SIZE, defaultGPUNode, false);

        srcBuffer.Fill(0xAA55AA55);
        TestMemoryException(defaultGPUNode, srcBuffer.As<HSAuint64>(),
                            0x12345678ULL);
    } else {
        int childStatus;

        waitpid(m_ChildPid, &childStatus, 0);
        if (hsakmt_is_dgpu()) {
            EXPECT_EQ(WIFEXITED(childStatus), true);
            EXPECT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        } else {
            EXPECT_EQ(WIFSIGNALED(childStatus), true);
            EXPECT_EQ(WTERMSIG(childStatus), SIGSEGV);
        }
    }

    TEST_END
}

/* Allocate Read Only buffer. Test Memory Exception failure by
 * attempting to write to that buffer in the child process.
 */
TEST_F(KFDExceptionTest, PermissionFault) {
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
        KFDBaseComponentTest::TearDown();
        KFDBaseComponentTest::SetUp();

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
        if (hsakmt_is_dgpu()) {
            EXPECT_EQ(WIFEXITED(childStatus), true);
            EXPECT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        } else {
            EXPECT_EQ(WIFSIGNALED(childStatus), true);
            EXPECT_EQ(WTERMSIG(childStatus), SIGSEGV);
        }
    }

    TEST_END
}

/* Allocate Read Only user pointer buffer. Test Memory Exception failure by
 * attempting to write to that buffer in the child process.
 */
TEST_F(KFDExceptionTest, PermissionFaultUserPointer) {
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
        KFDBaseComponentTest::TearDown();
        KFDBaseComponentTest::SetUp();

	void *pBuf = mmap(NULL, PAGE_SIZE, PROT_READ,
			  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	ASSERT_NE(pBuf, MAP_FAILED);
	EXPECT_SUCCESS(hsaKmtRegisterMemory(pBuf, PAGE_SIZE));
	EXPECT_SUCCESS(hsaKmtMapMemoryToGPU(pBuf, PAGE_SIZE, NULL));
        HsaMemoryBuffer srcSysBuffer(PAGE_SIZE, defaultGPUNode, false);

        srcSysBuffer.Fill(0xAA55AA55);

        TestMemoryException(defaultGPUNode, srcSysBuffer.As<HSAuint64>(),
                            (HSAuint64)pBuf);
    } else {
        int childStatus;

        waitpid(m_ChildPid, &childStatus, 0);
        if (hsakmt_is_dgpu()) {
            EXPECT_EQ(WIFEXITED(childStatus), true);
            EXPECT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        } else {
            EXPECT_EQ(WIFSIGNALED(childStatus), true);
            EXPECT_EQ(WTERMSIG(childStatus), SIGSEGV);
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
        KFDBaseComponentTest::TearDown();
        KFDBaseComponentTest::SetUp();

        TestMemoryException(defaultGPUNode, 0x12345678, 0x76543210, 1024, 1024, 1);
    } else {
        int childStatus;

        waitpid(m_ChildPid, &childStatus, 0);
        if (hsakmt_is_dgpu()) {
            EXPECT_EQ(WIFEXITED(childStatus), true);
            EXPECT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        } else {
            EXPECT_EQ(WIFSIGNALED(childStatus), true);
            EXPECT_EQ(WTERMSIG(childStatus), SIGSEGV);
        }
    }

    TEST_END
}

/*
 */
TEST_F(KFDExceptionTest, SdmaQueueException) {
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
	unsigned int* pDb = NULL;
	unsigned int *nullPtr = NULL;

        KFDBaseComponentTest::TearDown();
        KFDBaseComponentTest::SetUp();

	m_MemoryFlags.ui32.NonPaged = 1;
	m_MemoryFlags.ui32.HostAccess = 0;
	ASSERT_SUCCESS(hsaKmtAllocMemory(defaultGPUNode, PAGE_SIZE, m_MemoryFlags,
				reinterpret_cast<void**>(&pDb)));
	// verify that pDb is not null before it's being used
	ASSERT_NE(nullPtr, pDb) << "hsaKmtAllocMemory returned a null pointer";
	ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(pDb, PAGE_SIZE, NULL));
	EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(pDb));

	TestSdmaException(defaultGPUNode, pDb);
	EXPECT_SUCCESS(hsaKmtFreeMemory(pDb, PAGE_SIZE));
    } else {
        int childStatus;

        waitpid(m_ChildPid, &childStatus, 0);
        if (hsakmt_is_dgpu()) {
            EXPECT_EQ(WIFEXITED(childStatus), true);
            EXPECT_EQ(WEXITSTATUS(childStatus), HSAKMT_STATUS_SUCCESS);
        } else {
            EXPECT_EQ(WIFSIGNALED(childStatus), true);
            EXPECT_EQ(WTERMSIG(childStatus), SIGSEGV);
        }
    }

    TEST_END
}
