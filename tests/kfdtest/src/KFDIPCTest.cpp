/*
 * Copyright (C) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "KFDIPCTest.hpp"
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "SDMAQueue.hpp"
#include "SDMAPacket.hpp"

void KFDIPCTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    ROUTINE_END
}

void KFDIPCTest::TearDown() {
    ROUTINE_START

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

KFDIPCTest::~KFDIPCTest(void) {
    /* exit() is necessary for the child process. Otherwise when the
     * child process finishes, gtest assumes the test has finished and
     * starts the next test while the parent is still active.
     */
    if (m_ChildPid == 0)
        exit(0);
}

/* Import shared Local Memory from parent process. Check for the pattern
 * filled in by the parent process. Then fill a new pattern.
 */
void KFDIPCTest::BasicTestChildProcess(int defaultGPUNode, int *pipefd) {
    /* Open KFD device for child process. This needs to called before
     * any memory definitions
     */
    if (HSAKMT_STATUS_SUCCESS != hsaKmtOpenKFD())
        exit(1);

    SDMAQueue sdmaQueue;
    HsaSharedMemoryHandle sharedHandleLM;
    HSAuint64 size = PAGE_SIZE, sharedSize;
    HsaMemoryBuffer tempSysBuffer(size, defaultGPUNode, false);
    HSAuint32 *sharedLocalBuffer = NULL;

    /* Read from Pipe the shared Handle. Import shared Local Memory */
    ASSERT_GE(read(pipefd[0], reinterpret_cast<void*>(&sharedHandleLM), sizeof(sharedHandleLM)), 0);

    ASSERT_SUCCESS(hsaKmtRegisterSharedHandle(&sharedHandleLM,
                  reinterpret_cast<void**>(&sharedLocalBuffer), &sharedSize));
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(sharedLocalBuffer, sharedSize, NULL));

    /* Check for pattern in the shared Local Memory */
    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));
    size = size < sharedSize ? size : sharedSize;
    sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(tempSysBuffer.As<HSAuint32*>(),
        sharedLocalBuffer, size));
    sdmaQueue.Wait4PacketConsumption();
    ASSERT_TRUE(WaitOnValue(tempSysBuffer.As<HSAuint32*>(), 0xAAAAAAAA));

    /* Fill in the Local Memory with different pattern */
    sdmaQueue.PlaceAndSubmitPacket(SDMAWriteDataPacket(sharedLocalBuffer, 0xBBBBBBBB));
    sdmaQueue.Wait4PacketConsumption();

    /* Clean up */
    ASSERT_SUCCESS(sdmaQueue.Destroy());
    ASSERT_SUCCESS(hsaKmtUnmapMemoryToGPU(sharedLocalBuffer));
    ASSERT_SUCCESS(hsaKmtDeregisterMemory(sharedLocalBuffer));
}

/* Fill a pattern into Local Memory and share with the child process.
 * Then wait until Child process to exit and check for the new pattern
 * filled in by the child process.
 */

void KFDIPCTest::BasicTestParentProcess(int defaultGPUNode, pid_t cpid, int *pipefd) {
    HSAuint64 size = PAGE_SIZE, sharedSize;
    int status;
    HSAuint64 AlternateVAGPU;
    HsaMemoryBuffer toShareLocalBuffer(size, defaultGPUNode, false, true);
    HsaMemoryBuffer tempSysBuffer(PAGE_SIZE, defaultGPUNode, false);
    SDMAQueue sdmaQueue;
    HsaSharedMemoryHandle sharedHandleLM;

    /* Fill a Local Buffer with a pattern */
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(toShareLocalBuffer.As<void*>(), toShareLocalBuffer.Size(), &AlternateVAGPU));
    tempSysBuffer.Fill(0xAAAAAAAA);

    /* Copy pattern in Local Memory before sharing it */
    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));
    sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(toShareLocalBuffer.As<HSAuint32*>(),
        tempSysBuffer.As<HSAuint32*>(), size));
    sdmaQueue.Wait4PacketConsumption();

    /* Share it with the child process */
    ASSERT_SUCCESS(hsaKmtShareMemory(toShareLocalBuffer.As<void*>(), size, &sharedHandleLM));

    ASSERT_GE(write(pipefd[1], reinterpret_cast<void*>(&sharedHandleLM), sizeof(sharedHandleLM)), 0);

    /* Wait for the child to finish */
    waitpid(cpid, &status, 0);

    ASSERT_EQ(WIFEXITED(status), 1);
    ASSERT_EQ(WEXITSTATUS(status), 0);

    /* Check for the new pattern filled in by child process */
    sdmaQueue.PlaceAndSubmitPacket(SDMACopyDataPacket(tempSysBuffer.As<HSAuint32*>(),
        toShareLocalBuffer.As<HSAuint32*>(), size));
    sdmaQueue.Wait4PacketConsumption();
    ASSERT_TRUE(WaitOnValue(tempSysBuffer.As<HSAuint32*>(), 0xBBBBBBBB));

    /* Clean up */
    ASSERT_SUCCESS(hsaKmtUnmapMemoryToGPU(toShareLocalBuffer.As<void*>()));
    ASSERT_SUCCESS(sdmaQueue.Destroy());
}

/* Test IPC memory.
 * 1. Parent Process [Create/Fill] LocalMemory (LM) --share--> Child Process
 * 2. Child Process import LM and check for the pattern.
 * 3. Child Process fill in a new pattern and quit.
 * 4. Parent Process wait for the Child process to finish and then check for
 * the new pattern in LM
 *
 * IPC support is limited to Local Memory.
 */

TEST_F(KFDIPCTest, BasicTest) {
    TEST_START(TESTPROFILE_RUNALL)

    const std::vector<int>& GpuNodes = m_NodeInfo.GetNodesWithGPU();
    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    int pipefd[2];

    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (!GetVramSize(defaultGPUNode)) {
        LOG() << "Skipping test: No VRAM found." << std::endl;
        return;
    }

    /* Test libhsakmt fork() clean up by defining some buffers. These
     * buffers gets duplicated in the child process but not are not valid
     * as it doesn't have proper mapping in GPU. The clean up code in libhsakmt
     * should handle it
     */
    volatile HSAuint32 stackData[1];
    HsaMemoryBuffer tmpSysBuffer(PAGE_SIZE, defaultGPUNode, false);
    HsaMemoryBuffer tmpUserptrBuffer((void *)&stackData[0], sizeof(HSAuint32));

    /* Create Pipes for communicating shared handles */
    ASSERT_EQ(pipe(pipefd), 0);

    /* Create a child process and share the above Local Memory with it */

    m_ChildPid = fork();
    if (m_ChildPid == 0)
        BasicTestChildProcess(defaultGPUNode, pipefd); /* Child Process */
    else
        BasicTestParentProcess(defaultGPUNode, m_ChildPid, pipefd); /* Parent proces */

    /* Code path executed by both parent and child with respective fds */
    close(pipefd[1]);
    close(pipefd[0]);

    TEST_END
}

/* Cross Memory Attach Test. Memory Descriptor Array.
 *  The following 2 2D-arrays describe the source and destination memory arrays used
 *  by CMA test. The entry is only valid if Size != 0. Each of these buffers will be
 *  filled intially with "FillPattern". After the test the srcRange is still expected
 *  to have the same pattern. The dstRange is expected to have srcRange pattern.
 *
 *  For e.g. for TEST_COUNT = 1,
 *      srcRange has 2 buffers of size 0x1800. Buf1 filled with 0xA5A5A5A5 and Buf2
 *          filled with 0xAAAAAAAA
 *      dstRange has 3 buffers of size 0x1000. All of them filled 0xFFFFFFFF.
 *      After Copy: dstBuf1[0-0x1000] is expected to be 0xA5A5A5A5
 *                  dstBuf2[0-0x800] is expected to be 0xA5A5A5A5
 *                  dstBuf3[0x800-0x1000] is expected to be 0xAAAAAAAA
 *              and dstBuf4[0x0-0x1000] is expected to be 0xAAAAAAAA
 *
 * For this CMA test, after copying only the first and the last of dstBuf is checked
 */

static testMemoryDescriptor srcRange[CMA_TEST_COUNT][CMA_MEMORY_TEST_ARRAY_SIZE] = {
    {   /* Memory Type          Size    FillPattern FirstItem   Last item  */
        { CMA_MEM_TYPE_USERPTR, 0x801800, 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_USERPTR, 0x1800, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA },
        { CMA_MEM_TYPE_USERPTR, 0x0,    0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_USERPTR, 0x0,    0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5 },
    },
    {
        { CMA_MEM_TYPE_SYSTEM, 0x208000, 0xDEADBEEF, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_SYSTEM, 0x4000, 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_SYSTEM, 0x6000, 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_SYSTEM, 0x2000, 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5 },
    },
    {
        { CMA_MEM_TYPE_LOCAL_MEM, 0x800000, 0xDEADBEEF, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_LOCAL_MEM, 0x1000, 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_LOCAL_MEM, 0x1000, 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_LOCAL_MEM, 0x1000, 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5 },
    }
};

static testMemoryDescriptor dstRange[CMA_TEST_COUNT][CMA_MEMORY_TEST_ARRAY_SIZE] = {
    {
        /* Memory Type          Size    FillPattern FirstItem   Last item  */
        { CMA_MEM_TYPE_USERPTR, 0x801000, 0xFFFFFFFF, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_USERPTR, 0x1000, 0xFFFFFFFF, 0xA5A5A5A5, 0xAAAAAAAA },
        { CMA_MEM_TYPE_USERPTR, 0x1000, 0xFFFFFFFF, 0xAAAAAAAA, 0xAAAAAAAA },
        { CMA_MEM_TYPE_USERPTR, 0x0,    0xFFFFFFFF, 0xA5A5A5A5, 0xA5A5A5A5 },
    },
    {
        { CMA_MEM_TYPE_SYSTEM, 0x202000, 0xFFFFFFFF, 0xDEADBEEF, 0xDEADBEEF },
        { CMA_MEM_TYPE_SYSTEM, 0x4000, 0xFFFFFFFF, 0xDEADBEEF, 0xDEADBEEF },
        { CMA_MEM_TYPE_SYSTEM, 0x8000, 0xFFFFFFFF, 0xDEADBEEF, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_SYSTEM, 0x6000, 0xFFFFFFFF, 0xA5A5A5A5, 0xA5A5A5A5 },
    },
    {
        { CMA_MEM_TYPE_LOCAL_MEM, 0x800000, 0xFFFFFFFF, 0xDEADBEEF, 0xDEADBEEF },
        { CMA_MEM_TYPE_LOCAL_MEM, 0x1000, 0xFFFFFFFF, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_LOCAL_MEM, 0x1000, 0xFFFFFFFF, 0xA5A5A5A5, 0xA5A5A5A5 },
        { CMA_MEM_TYPE_LOCAL_MEM, 0x1000, 0xFFFFFFFF, 0xA5A5A5A5, 0xA5A5A5A5 },
    }
};

KFDCMAArray::KFDCMAArray() : m_ValidCount(0), m_QueueArray(HSA_QUEUE_SDMA) {
    memset(m_MemArray, 0, sizeof(m_MemArray));
    memset(m_HsaMemoryRange, 0, sizeof(m_HsaMemoryRange));
}

CMA_TEST_STATUS KFDCMAArray::Destroy() {
    for (int i = 0; i < m_ValidCount; i++) {
        if (m_MemArray[i]) {
            void *userPtr;

            userPtr = m_MemArray[i]->GetUserPtr();
            delete m_MemArray[i];

            if (userPtr)
                free(userPtr);
        }
    }

    memset(m_MemArray, 0, sizeof(m_MemArray));
    memset(m_HsaMemoryRange, 0, sizeof(m_HsaMemoryRange));
    m_ValidCount = 0;

    return CMA_TEST_SUCCESS;
}

/* Initialize KFDCMAArray based on array of testMemoryDescriptor. Usually testMemoryDescriptor[] is 
 * statically defined array by the user. Only items with non-zero size are considered valid
 */
CMA_TEST_STATUS KFDCMAArray::Init(testMemoryDescriptor(*memDescriptor)[CMA_MEMORY_TEST_ARRAY_SIZE], int node) {
    CMA_TEST_STATUS err = CMA_TEST_SUCCESS;
    memset(m_MemArray, 0, sizeof(m_MemArray));
    memset(m_HsaMemoryRange, 0, sizeof(m_HsaMemoryRange));

    m_ValidCount = 0;
    for (int i = 0; i < CMA_MEMORY_TEST_ARRAY_SIZE; i++) {
        if ((*memDescriptor)[i].m_MemSize == 0)
            continue;

        switch ((*memDescriptor)[i].m_MemType) {
        case CMA_MEM_TYPE_SYSTEM:
            m_MemArray[i] = new HsaMemoryBuffer((*memDescriptor)[i].m_MemSize, node);
            break;

        case CMA_MEM_TYPE_USERPTR:
        {
            void *userPtr = malloc((*memDescriptor)[i].m_MemSize);
            m_MemArray[i] = new HsaMemoryBuffer(userPtr, (*memDescriptor)[i].m_MemSize);
            break;
        }

        case CMA_MEM_TYPE_LOCAL_MEM:
            m_MemArray[i] = new HsaMemoryBuffer((*memDescriptor)[i].m_MemSize, node, false, true);
            break;
        }

        if (m_MemArray[i]) {
            m_HsaMemoryRange[i].MemoryAddress = m_MemArray[i]->As<void*>();
            m_HsaMemoryRange[i].SizeInBytes = m_MemArray[i]->Size();
            m_ValidCount++;
        } else {
            err = CMA_TEST_NOMEM;
            break;
        }
    }

    return err;
}

/* Fill each buffer of KFDCMAArray with the pattern described by testMemoryDescriptor[] */
void KFDCMAArray::FillPattern(testMemoryDescriptor(*memDescriptor)[CMA_MEMORY_TEST_ARRAY_SIZE]) {
    SDMAQueue sdmaQueue;
    bool queueCreated = false;
    unsigned int queueNode;

    for (int i = 0; i < m_ValidCount; i++) {
        if (m_MemArray[i]->isLocal())
            m_MemArray[i]->Fill((*memDescriptor)[i].m_FillPattern, *m_QueueArray.GetQueue(m_MemArray[i]->Node()));
        else
            m_MemArray[i]->Fill((*memDescriptor)[i].m_FillPattern);
    }
}

/* Check the first and last item of each buffer in KFDCMAArray with the pattern described by
 * testMemoryDescriptor[]. Return 0 on success.
 */
CMA_TEST_STATUS KFDCMAArray::checkPattern(testMemoryDescriptor(*memDescriptor)[CMA_MEMORY_TEST_ARRAY_SIZE]) {
    HSAuint64 lastItem;
    CMA_TEST_STATUS ret = CMA_TEST_SUCCESS;
    unsigned int queueNode = 0;
    bool queueCreated = false;
    HsaMemoryBuffer tmpBuffer(PAGE_SIZE, 0, true /* zero */);
    volatile HSAuint32 *tmp = tmpBuffer.As<volatile HSAuint32 *>();

    for (int i = 0; i < m_ValidCount; i++) {
        lastItem = m_MemArray[i]->Size();
        lastItem -= sizeof(HSAuint32);

        if (m_MemArray[i]->isLocal()) {
            BaseQueue *sdmaQueue = m_QueueArray.GetQueue(m_MemArray[i]->Node());

            if (!m_MemArray[i]->IsPattern(0, (*memDescriptor)[i].m_CheckFirstWordPattern, *sdmaQueue, tmp) ||
                !m_MemArray[i]->IsPattern(lastItem, (*memDescriptor)[i].m_CheckLastWordPattern, *sdmaQueue, tmp)) {
                ret = CMA_CHECK_PATTERN_ERROR;
                break;
            }

        } else {
            if (!m_MemArray[i]->IsPattern(0, (*memDescriptor)[i].m_CheckFirstWordPattern) ||
                !m_MemArray[i]->IsPattern(lastItem, (*memDescriptor)[i].m_CheckLastWordPattern)) {
                ret = CMA_CHECK_PATTERN_ERROR;
                break;
            }
        }
    }

    return ret;
}


/* Non-blocking read and write to avoid Test from hanging (block indefinitely)
 * if either server or client process exits due to assert failure
 */
static int write_non_block(int fd, const void *buf, int size) {
    int total_bytes = 0, cur_bytes = 0;
    int retries = 5;
    struct timespec tm = { 0, 10000000ULL };
    const char *ptr = (const char *)buf;

    do {
        cur_bytes = write(fd, ptr, (size - total_bytes));

        if (cur_bytes < 0 && errno != EAGAIN)
                return cur_bytes;

        if (cur_bytes > 0) {
            total_bytes += cur_bytes;
            ptr += cur_bytes;
        }

        if (total_bytes < size)
            nanosleep(&tm, NULL);
    } while (total_bytes < size && retries--);

    /* Check for overflow */
    if (total_bytes > size)
        return -1;

    return total_bytes;
}

static int read_non_block(int fd, void *buf, int size) {
    int total_bytes = 0, cur_bytes = 0;
    int retries = 5;
    struct timespec tm = { 0, 100000000ULL };
    char *ptr = reinterpret_cast<char *>(buf);

    do {
        cur_bytes = read(fd, ptr, (size - total_bytes));

        if (cur_bytes < 0 && errno != EAGAIN)
            return cur_bytes;

        if (cur_bytes > 0) {
            total_bytes += cur_bytes;
            ptr += cur_bytes;
        }

        if (total_bytes < size)
            nanosleep(&tm, NULL);
    } while (total_bytes < size && retries--);

    if (total_bytes > size)
        return -1;

    return total_bytes;
}


/* Send HsaMemoryRange to another process that is connected via writePipe */
CMA_TEST_STATUS KFDCMAArray::sendCMAArray(int writePipe) {
    if (write_non_block(writePipe, reinterpret_cast<void*>(&m_HsaMemoryRange), sizeof(m_HsaMemoryRange)) !=
                sizeof(m_HsaMemoryRange))
        return CMA_IPC_PIPE_ERROR;
    return CMA_TEST_SUCCESS;
}

/* Send HsaMemoryRange from another process and initialize KFDCMAArray */
CMA_TEST_STATUS KFDCMAArray::recvCMAArray(int readPipe) {
    int i;

    if (read_non_block(readPipe, reinterpret_cast<void*>(&m_HsaMemoryRange), sizeof(m_HsaMemoryRange)) !=
                sizeof(m_HsaMemoryRange))
        return CMA_IPC_PIPE_ERROR;

    for (i = 0; i < CMA_MEMORY_TEST_ARRAY_SIZE; i++) {
        if (m_HsaMemoryRange[i].SizeInBytes)
                m_ValidCount++;
    }
    return CMA_TEST_SUCCESS;
}


CMA_TEST_STATUS KFDIPCTest::CrossMemoryAttachChildProcess(int defaultGPUNode, int writePipe,
                                                          int readPipe, CMA_TEST_TYPE testType) {
    KFDCMAArray cmaLocalArray;
    char msg[16];
    int testNo;
    CMA_TEST_STATUS status;

    /* Initialize and fill Local Buffer Array with a pattern.
    *  READ_TEST: Send the Array to parent process. Wait for the parent
    *   to finish reading and checking. Then move to next text case or
    *   quit if last one.
    *  WRITE_TEST: Send Local Buffer Array to parent process and and wait
    *   for parent to write to it. Check for new pattern. Then move to next
    *   case or quit if last one.
    */
    for (testNo = 0; testNo < CMA_TEST_COUNT; testNo++) {
        if (testType == CMA_READ_TEST) {
            cmaLocalArray.Init(&srcRange[testNo], defaultGPUNode);
            cmaLocalArray.FillPattern(&srcRange[testNo]);
        } else {
            cmaLocalArray.Init(&dstRange[testNo], defaultGPUNode);
            cmaLocalArray.FillPattern(&dstRange[testNo]);
        }

        if (cmaLocalArray.sendCMAArray(writePipe) < 0) {
            status = CMA_IPC_PIPE_ERROR;
            break;
        }

        /* Wait until the test is over */
        memset(msg, 0, sizeof(msg));
        if (read_non_block(readPipe, msg, 4) < 0) {
            status = CMA_IPC_PIPE_ERROR;
            break;
        }

        if (!strcmp(msg, "CHCK"))
            status = cmaLocalArray.checkPattern(&dstRange[testNo]);
        else if (!strcmp(msg, "NEXT"))
            status = CMA_TEST_SUCCESS;
        else if (!strcmp(msg, "EXIT"))
            status = CMA_TEST_ABORT;
        else
            status = CMA_PARENT_FAIL;

        cmaLocalArray.Destroy();
        if (status != CMA_TEST_SUCCESS)
            break;
    }

    return status;
}


CMA_TEST_STATUS KFDIPCTest::CrossMemoryAttachParentProcess(int defaultGPUNode, pid_t cid,
                                                           int writePipe, int readPipe,
                                                           CMA_TEST_TYPE testType) {
    KFDCMAArray cmaLocalArray, cmaRemoteArray;
    HSAuint64 copied = 0;
    int testNo;
    CMA_TEST_STATUS status;

    /* Receive buffer array from child and then initialize and fill in Local Buffer Array.
     * READ_TEST: Copy remote buffer array into Local Buffer Array and then check
     *              for the new pattern.
     * WRITE_TEST: Write Local Buffer Array into remote buffer array. Notify child to
     *              to check for the new pattern.
     */
    for (testNo = 0; testNo < CMA_TEST_COUNT; testNo++) {
        status = cmaRemoteArray.recvCMAArray(readPipe);
        if (status != CMA_TEST_SUCCESS)
            break;

        if (testType == CMA_READ_TEST) {
            status = cmaLocalArray.Init(&dstRange[testNo], defaultGPUNode);
            if (status != CMA_TEST_SUCCESS)
                break;
            cmaLocalArray.FillPattern(&dstRange[testNo]);

            if (hsaKmtProcessVMRead(cid, cmaLocalArray.getMemoryRange(),
                                cmaLocalArray.getValidRangeCount(),
                                cmaRemoteArray.getMemoryRange(),
                                cmaRemoteArray.getValidRangeCount(),
                                &copied) != HSAKMT_STATUS_SUCCESS) {
                status = CMA_TEST_HSA_READ_FAIL;
                break;
            }

            status = cmaLocalArray.checkPattern(&dstRange[testNo]);
            if (status != CMA_TEST_SUCCESS)
                break;

            cmaLocalArray.Destroy();
            cmaRemoteArray.Destroy();

            if (write_non_block(writePipe, "NEXT", 4) < 0) {
                status = CMA_IPC_PIPE_ERROR;
                break;
            }
        } else {
            status = cmaLocalArray.Init(&srcRange[testNo], defaultGPUNode);
            if (status != CMA_TEST_SUCCESS)
                break;
            cmaLocalArray.FillPattern(&srcRange[testNo]);

            if (hsaKmtProcessVMWrite(cid, cmaLocalArray.getMemoryRange(),
                                    cmaLocalArray.getValidRangeCount(),
                                    cmaRemoteArray.getMemoryRange(),
                                    cmaRemoteArray.getValidRangeCount(),
                                    &copied) != HSAKMT_STATUS_SUCCESS) {
                status = CMA_TEST_HSA_WRITE_FAIL;
                break;
            }

            cmaLocalArray.Destroy();
            cmaRemoteArray.Destroy();
            if (write_non_block(writePipe, "CHCK", 4) < 0) {
                status = CMA_IPC_PIPE_ERROR;
                break;
            }
        }
    } /* for loop */

    return status;
}

/* Test Cross Memory Attach
 * hsaKmtProcessVMRead and hsaKmtProcessVMWrite are GPU address equivalent to
 * process_vm_readv and process_vm_writev. These calls transfer data between
 * the address space of the calling process ("the local process") and the process
 * identified by pid ("the remote process").
 *
 * In the tests parent process will be the local process and child will be
 * the remote.
 */
TEST_F(KFDIPCTest, CrossMemoryAttachTest) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    int pipeCtoP[2], pipePtoC[2];
    int status;

    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (!GetVramSize(defaultGPUNode)) {
        LOG() << "Skipping test: No VRAM found." << std::endl;
        return;
    }

    /* Create Pipes for communicating shared handles */
    ASSERT_EQ(pipe2(pipeCtoP, O_NONBLOCK), 0);
    ASSERT_EQ(pipe2(pipePtoC, O_NONBLOCK), 0);

    /* Create a child process and share the above Local Memory with it */
    m_ChildPid = fork();
    if (m_ChildPid == 0 && hsaKmtOpenKFD() == HSAKMT_STATUS_SUCCESS) {
        /* Child Process */
        status = CrossMemoryAttachChildProcess(defaultGPUNode, pipeCtoP[1],
            pipePtoC[0], CMA_READ_TEST);
        ASSERT_EQ(status, CMA_TEST_SUCCESS) << "Child: Read Test Fail";
        status = CrossMemoryAttachChildProcess(defaultGPUNode, pipeCtoP[1],
                pipePtoC[0], CMA_WRITE_TEST);
        ASSERT_EQ(status, CMA_TEST_SUCCESS) << "Child: Write Test Fail";
    } else {
        int childStatus;

        status = CrossMemoryAttachParentProcess(defaultGPUNode, m_ChildPid,
                                pipePtoC[1], pipeCtoP[0], CMA_READ_TEST); /* Parent proces */
        ASSERT_EQ(status, CMA_TEST_SUCCESS) << "Parent: Read Test Fail";
        status = CrossMemoryAttachParentProcess(defaultGPUNode, m_ChildPid,
                                pipePtoC[1], pipeCtoP[0], CMA_WRITE_TEST);
        ASSERT_EQ(status, CMA_TEST_SUCCESS) << "Parent: Write Test Fail";

        waitpid(m_ChildPid, &childStatus, 0);
        ASSERT_EQ(WIFEXITED(childStatus), true);
        ASSERT_EQ(WEXITSTATUS(childStatus), 0);
    }

    /* Code path executed by both parent and child with respective fds */
    close(pipeCtoP[1]);
    close(pipeCtoP[0]);
    close(pipePtoC[1]);
    close(pipePtoC[0]);
    TEST_END
}

/* Test Cross Memory Attach
 *
 * hsaKmtProcessVMRead and hsaKmtProcessVMWrite are GPU address equivalent to
 * process_vm_readv and process_vm_writev. These calls are used to transfer data
 * between the address space of the calling process ("the local process") and the process
 * identified by pid ("the remote process"). However, these functions should also work
 * with a single process and single BO.
 */
TEST_F(KFDIPCTest, CMABasicTest) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    HSAuint64 size = PAGE_SIZE;
    SDMAQueue sdmaQueue;
    HsaMemoryRange srcRange, dstRange;
    HSAuint64 copied;
    const int PATTERN1 = 0xA5A5A5A5, PATTERN2 = 0xFFFFFFFF;
    HSAKMT_STATUS status;

    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (!GetVramSize(defaultGPUNode)) {
        LOG() << "Skipping test: No VRAM found." << std::endl;
        return;
    }

    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));
    HsaMemoryBuffer tmpBuffer(PAGE_SIZE, 0, true /* zero */);
    volatile HSAuint32 *tmp = tmpBuffer.As<volatile HSAuint32 *>();

    /* Initialize test buffer. Fill first half and second half with
     * different pattern
     */
    HsaMemoryBuffer testLocalBuffer(size, defaultGPUNode, false, true);
    testLocalBuffer.Fill(PATTERN1, sdmaQueue, 0, size/2);
    testLocalBuffer.Fill(PATTERN2, sdmaQueue, size/2, size/2);

    /* Test1. Copy (or overwrite) buffer onto itself */
    srcRange.MemoryAddress = testLocalBuffer.As<void*>();
    srcRange.SizeInBytes = size;
    dstRange.MemoryAddress = testLocalBuffer.As<void*>();
    dstRange.SizeInBytes = size;
    ASSERT_SUCCESS(hsaKmtProcessVMRead(getpid(), &dstRange, 1, &srcRange, 1, &copied));
    ASSERT_EQ(copied, size);

    ASSERT_TRUE(testLocalBuffer.IsPattern(0, PATTERN1, sdmaQueue, tmp));
    ASSERT_TRUE(testLocalBuffer.IsPattern(size - 4, PATTERN2, sdmaQueue, tmp));


    /* Test2. Test unaligned byte copy. Write 3 bytes to an unaligned destination address */
    const int unaligned_offset = 1;
    const int unaligned_size = 3;
    const int unaligned_mask = (((1 << (unaligned_size * 8)) - 1) << (unaligned_offset * 8));
    HSAuint32 expected_pattern;

    srcRange.MemoryAddress = testLocalBuffer.As<void*>();

    /* Deliberately set to value > unaligned_size. Only unaligned_size
     * should be copied since dstRange.SizeInBytes == unaligned_size
     */
    srcRange.SizeInBytes = size;

    dstRange.MemoryAddress = reinterpret_cast<void *>(testLocalBuffer.As<char*>() + (size / 2) + unaligned_offset);
    dstRange.SizeInBytes = unaligned_size;
    ASSERT_SUCCESS(hsaKmtProcessVMRead(getpid(), &dstRange, 1, &srcRange, 1, &copied));
    ASSERT_EQ(copied, unaligned_size);

    expected_pattern = (PATTERN2 & ~unaligned_mask | (PATTERN1 & unaligned_mask));
    ASSERT_TRUE(testLocalBuffer.IsPattern(size/2, expected_pattern, sdmaQueue, tmp));


    /* Test3. Test overflow and expect failure */
    srcRange.MemoryAddress = testLocalBuffer.As<void*>();
    srcRange.SizeInBytes = size;
    dstRange.MemoryAddress = reinterpret_cast<void *>(testLocalBuffer.As<char*>() + 4);
    dstRange.SizeInBytes = size; /* This should overflow since offset is VA + 4 */
    status = hsaKmtProcessVMRead(getpid(), &dstRange, 1, &srcRange, 1, &copied);
    EXPECT_NE(status, HSAKMT_STATUS_SUCCESS);
    EXPECT_LE(copied, (size - 4));

    ASSERT_SUCCESS(sdmaQueue.Destroy());

    TEST_END
}
