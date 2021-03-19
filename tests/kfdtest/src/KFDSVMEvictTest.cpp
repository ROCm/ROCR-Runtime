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

#include "KFDSVMEvictTest.hpp"
#include <sys/mman.h>
#include <vector>
#include <string>
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "SDMAPacket.hpp"
#include "SDMAQueue.hpp"
#include "Dispatch.hpp"

#define N_PROCESSES             (4)     /* number of processes running in parallel, at least 2 */
#define ALLOCATE_BUF_SIZE_MB    (64)
#define ALLOCATE_RETRY_TIMES    (3)

HSAint32 KFDSVMEvictTest::GetBufferCounter(HSAuint64 vramSize, HSAuint64 vramBufSize) {
    HSAuint64 vramBufSizeInPages = vramBufSize >> PAGE_SHIFT;
    HSAuint64 sysMemSize = GetSysMemSize();
    HSAuint64 size, sizeInPages;
    HSAuint32 count;

    LOG() << "Found System RAM of " << std::dec << (sysMemSize >> 20) << "MB" << std::endl;

    /* use one third of total system memory for eviction buffer to test
     * limit max allocate size to duoble of vramSize
     * count is zero if not enough memory (sysMemSize/3 + vramSize) < (vramBufSize * N_PROCESSES)
     */
    size = sysMemSize / 3 + vramSize;
    size = size > vramSize << 1 ? vramSize << 1 : size;
    sizeInPages = size >> PAGE_SHIFT;
    count = sizeInPages / (vramBufSizeInPages * N_PROCESSES);

    return count;
}

void KFDSVMEvictTest::AllocBuffers(HSAuint32 defaultGPUNode, HSAuint32 count, HSAuint64 vramBufSize,
        std::vector<void *> &pBuffers) {
    HSAuint64   totalMB;

    totalMB = N_PROCESSES * count * (vramBufSize >> 20);
    if (m_IsParent) {
        LOG() << "Testing " << N_PROCESSES << "*" << count << "*" << (vramBufSize>>20) << "(="<< totalMB << ")MB" << std::endl;
    }
    HSAKMT_STATUS ret;
    HSAuint32 retry = 0;

    for (HSAuint32 i = 0; i < count; i++) {
        m_pBuf = mmap(0, vramBufSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        EXPECT_NOTNULL(m_pBuf);

        m_Flags = (HSA_SVM_FLAGS)0;
retry:
        ret = RegisterSVMRange(defaultGPUNode, m_pBuf, vramBufSize, defaultGPUNode, m_Flags);
        if (ret == HSAKMT_STATUS_SUCCESS) {
            pBuffers.push_back(m_pBuf);
            retry = 0;
        } else {
            if (retry++ > ALLOCATE_RETRY_TIMES) {
                munmap(m_pBuf, vramBufSize);
                break;
            }
            printf("retry %d allocate vram\n", retry);

            /* wait for 1 second to try allocate again */
            sleep(1);
            goto retry;
        }
    }
}

void KFDSVMEvictTest::FreeBuffers(std::vector<void *> &pBuffers, HSAuint64 vramBufSize) {
    for (HSAuint32 i = 0; i < pBuffers.size(); i++) {
        m_pBuf = pBuffers[i];
        if (m_pBuf != NULL)
            munmap(m_pBuf, vramBufSize);
    }
}

void KFDSVMEvictTest::ForkChildProcesses(int nprocesses) {
    int i;

    for (i = 0; i < nprocesses - 1; ++i) {
        pid_t pid = fork();
        ASSERT_GE(pid, 0);

        if (pid == 0) {
            /* Child process */
            /* Cleanup file descriptors copied from parent process
             * then call SetUp->hsaKmtOpenKFD to create new process
             */
            m_psName = "Test process " + std::to_string(i) + " ";
            TearDown();
            SetUp();
            m_ChildPids.clear();
            m_IsParent = false;
            return;
        }

        /* Parent process */
        m_ChildPids.push_back(pid);
    }

    m_psName = "Test process " + std::to_string(i) + " ";
}

void KFDSVMEvictTest::WaitChildProcesses() {
    if (m_IsParent) {
        /* only run by parent process */
        int childStatus;
        int childExitOkNum = 0;
        int size = m_ChildPids.size();

        for (HSAuint32 i = 0; i < size; i++) {
            pid_t pid = m_ChildPids.front();

            waitpid(pid, &childStatus, 0);
            if (WIFEXITED(childStatus) == 1 && WEXITSTATUS(childStatus) == 0)
                childExitOkNum++;

            m_ChildPids.erase(m_ChildPids.begin());
        }

        ASSERT_EQ(childExitOkNum, size);
    }

    /* child process or parent process finished successfullly */
    m_ChildStatus = HSAKMT_STATUS_SUCCESS;
}

/* Evict and restore procedure basic test
 *
 * Use N_PROCESSES processes to allocate vram buf size larger than total vram size
 *
 * ALLOCATE_BUF_SIZE_MB buf allocation size
 *
 * number of buf is equal to (vramSizeMB / (vramBufSizeMB * N_PROCESSES) ) + 8
 * Total vram all processes allocated: 8GB for 4GB Fiji, and 20GB for 16GB Vega10
 *
 * many times of eviction and restore will happen:
 * ttm will evict buffers of another process if not enough free vram
 * process restore will evict buffers of another process
 *
 * Sometimes the allocate may fail (maybe that is normal)
 * ALLOCATE_RETRY_TIMES max retry times to allocate
 *
 * This is basic test, no queue so vram are not used by GPU during test
 *
 * Todo:
 *    - Synchronization between the processes, so they know for sure when
 *        they are done allocating memory
 */
TEST_F(KFDSVMEvictTest, BasicTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    if (!SVMAPISupported())
        return;

    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    HSAuint64 vramBufSize = ALLOCATE_BUF_SIZE_MB * 1024 * 1024;

    HSAuint64 vramSize = GetVramSize(defaultGPUNode);

    if (!vramSize) {
        LOG() << "No VRAM found, skipping the test" << std::endl;
        return;
    } else {
        LOG() << "Found VRAM of " << std::dec << (vramSize >> 20) << "MB" << std::endl;
    }

    HSAuint32 count = GetBufferCounter(vramSize, vramBufSize);
    if (count == 0) {
        LOG() << "Not enough system memory, skipping the test" << std::endl;
        return;
    }

    /* Fork the child processes */
    ForkChildProcesses(N_PROCESSES);

    std::vector<void *> pBuffers;
    AllocBuffers(defaultGPUNode, count, vramBufSize, pBuffers);

    /* wait for other processes to finish allocation, then free buffer */
    sleep(ALLOCATE_RETRY_TIMES);

    LOG() << m_psName << "free buffer" << std::endl;
    FreeBuffers(pBuffers, vramBufSize);

    WaitChildProcesses();

    TEST_END
}

/* Shader to read local buffers using multiple wavefronts in parallel
 * until address buffer is filled with specific value 0x5678 by host program,
 * then each wavefront fills value 0x5678 at corresponding result buffer and quit
 *
 * initial state:
 *   s[0:1] - address buffer base address
 *   s[2:3] - result buffer base address
 *   s4 - workgroup id
 *   v0 - workitem id, always 0 because NUM_THREADS_X(number of threads) in workgroup set to 1
 * registers:
 *   v0 - calculated workitem id, v0 = v0 + s4 * NUM_THREADS_X
 *   v[2:3] - address of corresponding local buf address offset: s[0:1] + v0 * 8
 *   v[4:5] - corresponding output buf address: s[2:3] + v0 * 4
 *   v[6:7] - local buf address used for read test
 */
static const char* gfx9_ReadMemory =
"\
    shader ReadMemory\n\
    asic(GFX9)\n\
    type(CS)\n\
    \n\
    // compute address of corresponding output buffer\n\
    v_mov_b32       v0, s4                  // use workgroup id as index\n\
    v_lshlrev_b32   v0, 2, v0               // v0 *= 4\n\
    v_add_co_u32    v4, vcc, s2, v0         // v[4:5] = s[2:3] + v0 * 4\n\
    v_mov_b32       v5, s3\n\
    v_add_u32       v5, vcc_lo, v5\n\
    \n\
    // compute input buffer offset used to store corresponding local buffer address\n\
    v_lshlrev_b32   v0, 1, v0               // v0 *= 8\n\
    v_add_co_u32    v2, vcc, s0, v0         // v[2:3] = s[0:1] + v0 * 8\n\
    v_mov_b32       v3, s1\n\
    v_add_u32       v3, vcc_lo, v3\n\
    \n\
    // load 64bit local buffer address stored at v[2:3] to v[6:7]\n\
    flat_load_dwordx2   v[6:7], v[2:3] slc\n\
    s_waitcnt       vmcnt(0) & lgkmcnt(0)   // wait for memory reads to finish\n\
    \n\
    v_mov_b32       v8, 0x5678\n\
    s_movk_i32      s8, 0x5678\n\
L_REPEAT:\n\
    s_load_dword    s16, s[0:1], 0x0 glc\n\
    s_waitcnt       vmcnt(0) & lgkmcnt(0)   // wait for memory reads to finish\n\
    s_cmp_eq_i32    s16, s8\n\
    s_cbranch_scc1  L_QUIT                  // if notified to quit by host\n\
    // loop read 64M local buffer starting at v[6:7]\n\
    // every 4k page only read once\n\
    v_mov_b32       v9, 0\n\
    v_mov_b32       v10, 0x1000             // 4k page\n\
    v_mov_b32       v11, 0x4000000          // 64M size\n\
    v_mov_b32       v12, v6\n\
    v_mov_b32       v13, v7\n\
L_LOOP_READ:\n\
    flat_load_dwordx2   v[14:15], v[12:13] slc\n\
    v_add_u32       v9, v9, v10 \n\
    v_add_co_u32    v12, vcc, v12, v10\n\
    v_add_u32       v13, vcc_lo, v13\n\
    v_cmp_lt_u32    vcc, v9, v11\n\
    s_cbranch_vccnz L_LOOP_READ\n\
    s_branch        L_REPEAT\n\
L_QUIT:\n\
    flat_store_dword v[4:5], v8\n\
    s_waitcnt       vmcnt(0) & lgkmcnt(0)   // wait for memory writes to finish\n\
    s_endpgm\n\
    end\n\
";

static const char* gfx8_ReadMemory =
"\
    shader ReadMemory\n\
    asic(VI)\n\
    type(CS)\n\
    \n\
    // compute address of corresponding output buffer\n\
    v_mov_b32       v0, s4                  // use workgroup id as index\n\
    v_lshlrev_b32   v0, 2, v0               // v0 *= 4\n\
    v_add_u32       v4, vcc, s2, v0         // v[4:5] = s[2:3] + v0 * 4\n\
    v_mov_b32       v5, s3\n\
    v_addc_u32      v5, vcc, v5, 0, vcc\n\
    \n\
    // compute input buffer offset used to store corresponding local buffer address\n\
    v_lshlrev_b32   v0, 1, v0               // v0 *= 8\n\
    v_add_u32       v2, vcc, s0, v0         // v[2:3] = s[0:1] + v0 * 8\n\
    v_mov_b32       v3, s1\n\
    v_addc_u32      v3, vcc, v3, 0, vcc\n\
    \n\
    // load 64bit local buffer address stored at v[2:3] to v[6:7]\n\
    flat_load_dwordx2   v[6:7], v[2:3] slc\n\
    s_waitcnt       vmcnt(0) & lgkmcnt(0)   // wait for memory reads to finish\n\
    \n\
    v_mov_b32       v8, 0x5678\n\
    s_movk_i32      s8, 0x5678\n\
L_REPEAT:\n\
    s_load_dword    s16, s[0:1], 0x0 glc\n\
    s_waitcnt       vmcnt(0) & lgkmcnt(0)   // wait for memory reads to finish\n\
    s_cmp_eq_i32    s16, s8\n\
    s_cbranch_scc1  L_QUIT                  // if notified to quit by host\n\
    // loop read 64M local buffer starting at v[6:7]\n\
    // every 4k page only read once\n\
    v_mov_b32       v9, 0\n\
    v_mov_b32       v10, 0x1000             // 4k page\n\
    v_mov_b32       v11, 0x4000000          // 64M size\n\
    v_mov_b32       v12, v6\n\
    v_mov_b32       v13, v7\n\
L_LOOP_READ:\n\
    flat_load_dwordx2   v[14:15], v[12:13] slc\n\
    v_add_u32       v9, vcc, v9, v10 \n\
    v_add_u32       v12, vcc, v12, v10\n\
    v_addc_u32      v13, vcc, v13, 0, vcc\n\
    v_cmp_lt_u32    vcc, v9, v11\n\
    s_cbranch_vccnz L_LOOP_READ\n\
    s_branch        L_REPEAT\n\
L_QUIT:\n\
    flat_store_dword v[4:5], v8\n\
    s_waitcnt       vmcnt(0) & lgkmcnt(0)   // wait for memory writes to finish\n\
    s_endpgm\n\
    end\n\
";

std::string KFDSVMEvictTest::CreateShader() {
    if (m_FamilyId >= FAMILY_AI)
        return gfx9_ReadMemory;
    else
        return gfx8_ReadMemory;
}

/* Evict and restore queue test
 *
 * N_PROCESSES processes read all local buffers in parallel while buffers are evicted and restored
 * If GPU vm page fault happens, then test shader will stop and failed to write specific value
 * at dest buffer. Test will report failed.
 *
 * Steps:
 *    - fork N_PROCESSES processes, each process does the same below
 *    - allocate local buffers, each buffer size is 64MB
 *    - allocate zero initialized host access address buffer and result buffer
 *        address buffer to pass address of local buffers to shader
 *        result buffer to store shader output result
 *    - submit queue to run ReadMemory shader
 *    - shader start m_DimX wavefronts, each wavefront keep reading one local buffer
 *    - notify shader to quit
 *    - check result buffer with specific value to confirm all wavefronts quit normally
 */
TEST_F(KFDSVMEvictTest, QueueTest) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL)

    if (!SVMAPISupported())
        return;

    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    HSAuint64 vramBufSize = ALLOCATE_BUF_SIZE_MB * 1024 * 1024;

    const HsaNodeProperties *pNodeProperties = m_NodeInfo.HsaDefaultGPUNodeProperties();

    /* Skip test for chip it doesn't have CWSR, which the test depends on */
    if (m_FamilyId < FAMILY_VI || isTonga(pNodeProperties)) {
        LOG() << std::hex << "Test is skipped for family ID 0x" << m_FamilyId << std::endl;
        return;
    }

    HSAuint32 i;
    HSAuint64 vramSize = GetVramSize(defaultGPUNode);

    if (!vramSize) {
        LOG() << "No VRAM found, skipping the test" << std::endl;
        return;
    } else {
        LOG() << "Found VRAM of " << std::dec << (vramSize >> 20) << "MB." << std::endl;
    }

    HSAuint32 count = GetBufferCounter(vramSize, vramBufSize);
    if (count == 0) {
        LOG() << "Not enough system memory, skipping the test" << std::endl;
        return;
    }
    /* assert all buffer address can be stored within one page
     * because only one page host memory srcBuf is allocated
     */
    ASSERT_LE(count, PAGE_SIZE/sizeof(unsigned int *));

    /* Fork the child processes */
    ForkChildProcesses(N_PROCESSES);

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer addrBuffer(PAGE_SIZE, defaultGPUNode);
    HsaMemoryBuffer resultBuffer(PAGE_SIZE, defaultGPUNode);

    std::vector<void *> pBuffers;
    AllocBuffers(defaultGPUNode, count, vramBufSize, pBuffers);

    unsigned int wavefront_num = pBuffers.size();
    LOG() << m_psName << "wavefront number " << wavefront_num << std::endl;

    void **localBufAddr = addrBuffer.As<void **>();
    unsigned int *result = resultBuffer.As<uint32_t *>();

    for (i = 0; i < wavefront_num; i++)
        *(localBufAddr + i) = pBuffers[i];

    m_pIsaGen->CompileShader(CreateShader().c_str(), "ReadMemory", isaBuffer);

    PM4Queue pm4Queue;
    ASSERT_SUCCESS(pm4Queue.Create(defaultGPUNode));

    Dispatch dispatch0(isaBuffer);
    dispatch0.SetArgs(localBufAddr, result);
    dispatch0.SetDim(wavefront_num, 1, 1);
    /* submit the packet and start shader */
    dispatch0.Submit(pm4Queue);

    /* doing evict/restore queue test for 5 seconds while queue is running */
    sleep(5);

    /* LOG() << m_psName << "notify shader to quit" << std::endl; */
    /* fill address buffer so shader quits */
    addrBuffer.Fill(0x5678);

    /* wait for shader to finish or timeout if shade has vm page fault */
    dispatch0.SyncWithStatus(120000);

    ASSERT_SUCCESS(pm4Queue.Destroy());
    /* LOG() << m_psName << "free buffer" << std::endl; */
    /* cleanup */
    FreeBuffers(pBuffers, vramBufSize);

    /* check if all wavefronts finish successfully */
    for (i = 0; i < wavefront_num; i++)
        ASSERT_EQ(0x5678, *(result + i));

    WaitChildProcesses();

    TEST_END
}

