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

#include "KFDMemoryTest.hpp"
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <numa.h>
#include <vector>
#include "Dispatch.hpp"
#include "PM4Queue.hpp"
#include "PM4Packet.hpp"
#include "SDMAQueue.hpp"
#include "SDMAPacket.hpp"
#include "linux/kfd_ioctl.h"

const char* gfx8_ScratchCopyDword =
"\
shader ScratchCopyDword\n\
asic(VI)\n\
type(CS)\n\
/*copy the parameters from scalar registers to vector registers*/\n\
    v_mov_b32 v0, s0\n\
    v_mov_b32 v1, s1\n\
    v_mov_b32 v2, s2\n\
    v_mov_b32 v3, s3\n\
/*set up the scratch parameters. This assumes a single 16-reg block.*/\n\
    s_mov_b32 flat_scratch_lo, 8/*2 dwords of scratch per thread*/\n\
    s_mov_b32 flat_scratch_hi, 0/*offset in units of 256bytes*/\n\
/*copy a dword between the passed addresses*/\n\
    flat_load_dword v4, v[0:1] slc\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    flat_store_dword v[2:3], v4 slc\n\
    \n\
    s_endpgm\n\
    \n\
end\n\
";

const char* gfx9_ScratchCopyDword =
"\
shader ScratchCopyDword\n\
asic(GFX9)\n\
type(CS)\n\
/*copy the parameters from scalar registers to vector registers*/\n\
    v_mov_b32 v0, s0\n\
    v_mov_b32 v1, s1\n\
    v_mov_b32 v2, s2\n\
    v_mov_b32 v3, s3\n\
/*set up the scratch parameters. This assumes a single 16-reg block.*/\n\
    s_mov_b32 flat_scratch_lo, s4\n\
    s_mov_b32 flat_scratch_hi, s5\n\
/*copy a dword between the passed addresses*/\n\
    flat_load_dword v4, v[0:1] slc\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    flat_store_dword v[2:3], v4 slc\n\
    \n\
    s_endpgm\n\
    \n\
end\n\
";
const char* gfx10_ScratchCopyDword =
"\
shader ScratchCopyDword\n\
asic(GFX10)\n\
type(CS)\n\
wave_size(32)\n\
/*copy the parameters from scalar registers to vector registers*/\n\
    v_mov_b32 v0, s0\n\
    v_mov_b32 v1, s1\n\
    v_mov_b32 v2, s2\n\
    v_mov_b32 v3, s3\n\
/*set up the scratch parameters. This assumes a single 16-reg block.*/\n\
    s_setreg_b32 hwreg(HW_REG_SHADER_FLAT_SCRATCH_LO), s4\n\
    s_setreg_b32 hwreg(HW_REG_SHADER_FLAT_SCRATCH_HI), s5\n\
/*copy a dword between the passed addresses*/\n\
    flat_load_dword v4, v[0:1] slc\n\
    s_waitcnt vmcnt(0)&lgkmcnt(0)\n\
    flat_store_dword v[2:3], v4 slc\n\
    \n\
    s_endpgm\n\
    \n\
end\n\
";



/* Continuously poll src buffer and check buffer value
 * After src buffer is filled with specific value (0x5678,
 * by host program), fill dst buffer with specific
 * value(0x5678) and quit
 */
const char* gfx9_PollMemory =
"\
shader ReadMemory\n\
wave_size(32)\n\
type(CS)\n\
/* Assume src address in s0, s1 and dst address in s2, s3*/\n\
    s_movk_i32 s18, 0x5678\n\
    LOOP:\n\
    s_load_dword s16, s[0:1], 0x0 glc\n\
    s_cmp_eq_i32 s16, s18\n\
    s_cbranch_scc0   LOOP\n\
    s_store_dword s18, s[2:3], 0x0 glc\n\
    s_endpgm\n\
    end\n\
";

/* Input: A buffer of at least 3 dwords.
 * DW0: used as a signal. 0xcafe means it is signaled
 * DW1: Input buffer for device to read.
 * DW2: Output buffer for device to write.
 * Once receive signal, device will copy DW1 to DW2
 * This shader continously poll the signal buffer,
 * Once signal buffer is signaled, it copies input buffer
 * to output buffer
 */
const char* gfx9_CopyOnSignal =
"\
shader CopyOnSignal\n\
wave_size(32)\n\
type(CS)\n\
/* Assume input buffer in s0, s1 */\n\
    s_mov_b32 s18, 0xcafe\n\
POLLSIGNAL:\n\
    s_load_dword s16, s[0:1], 0x0 glc\n\
    s_cmp_eq_i32 s16, s18\n\
    s_cbranch_scc0   POLLSIGNAL\n\
    s_load_dword s17, s[0:1], 0x4 glc\n\
    s_waitcnt vmcnt(0) & lgkmcnt(0)\n\
    s_store_dword s17, s[0:1], 0x8 glc\n\
    s_waitcnt vmcnt(0) & lgkmcnt(0)\n\
    s_endpgm\n\
    end\n\
";

/* Input0: A buffer of at least 2 dwords.
 * DW0: used as a signal. Write 0xcafe to signal
 * DW1: Write to this buffer for other device to read.
 * Input1: mmio base address
 */
const char* gfx9_WriteAndSignal =
"\
shader WriteAndSignal\n\
wave_size(32)\n\
type(CS)\n\
/* Assume input buffer in s0, s1 */\n\
    s_mov_b32 s18, 0xbeef\n\
    s_store_dword s18, s[0:1], 0x4 glc\n\
    s_mov_b32 s18, 0x1\n\
    s_store_dword s18, s[2:3], 0 glc\n\
    s_mov_b32 s18, 0xcafe\n\
    s_store_dword s18, s[0:1], 0x0 glc\n\
    s_endpgm\n\
    end\n\
";

//These gfx9_PullMemory, gfx9_CopyOnSignal, gfx9_WriteAndSignal shaders can be used by both gfx9 and gfx10

void KFDMemoryTest::SetUp() {
    ROUTINE_START

    KFDBaseComponentTest::SetUp();

    m_pIsaGen = IsaGenerator::Create(m_FamilyId);

    ROUTINE_END
}

void KFDMemoryTest::TearDown() {
    ROUTINE_START

    if (m_pIsaGen)
        delete m_pIsaGen;
    m_pIsaGen = NULL;

    KFDBaseComponentTest::TearDown();

    ROUTINE_END
}

#include <sys/mman.h>
#define GB(x) ((x) << 30)

/*
 * Try to map as much as possible system memory to gpu
 * to see if KFD supports 1TB memory correctly or not.
 * After this test case, we can observe if there are any side effects.
 * NOTICE: There are memory usage limit checks in hsa/kfd according to the total
 * physical system memory.
 */
TEST_F(KFDMemoryTest, MMapLarge) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL)

    if (!is_dgpu()) {
        LOG() << "Skipping test: Test not supported on APU." << std::endl;
        return;
    }

    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    const HSAuint64 nObjects = 1<<14;
    HSAuint64 *AlternateVAGPU = new HSAuint64[nObjects];
    ASSERT_NE((HSAuint64)AlternateVAGPU, 0);
    HsaMemMapFlags mapFlags = {0};
    HSAuint64 s;
    char *addr;
    HSAuint64 flags = MAP_ANONYMOUS | MAP_PRIVATE;

    /* Test up to 1TB memory*/
    s = GB(1024ULL) / nObjects;
    addr = reinterpret_cast<char*>(mmap(0, s, PROT_READ | PROT_WRITE, flags, -1, 0));
    ASSERT_NE(addr, MAP_FAILED);
    memset(addr, 0, s);

    int i = 0;
    /* Allocate 1024GB, aka 1TB*/
    for (; i < nObjects; i++) {
        if (hsaKmtRegisterMemory(addr + i, s - i))
            break;
        if (hsaKmtMapMemoryToGPUNodes(addr + i, s - i,
                    &AlternateVAGPU[i], mapFlags, 1, reinterpret_cast<HSAuint32 *>(&defaultGPUNode))) {
            hsaKmtDeregisterMemory(addr + i);
            break;
        }
    }

    LOG() << "Successfully registered and mapped " << (i * s >> 30)
            << "GB system memory to gpu" << std::endl;

    RECORD(i * s >> 30) << "Mmap-SysMem-Size";

    while (i--) {
        EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(reinterpret_cast<void*>(AlternateVAGPU[i])));
        EXPECT_SUCCESS(hsaKmtDeregisterMemory(reinterpret_cast<void*>(AlternateVAGPU[i])));
    }

    munmap(addr, s);
    delete []AlternateVAGPU;

    TEST_END
}

/* Keep memory mapped to default node
 * Keep mapping/unmapping memory to/from non-default node
 * A shader running on default node consistantly accesses
 * memory - make sure memory is always accessible by default,
 * i.e. there is no gpu vm fault.
 * Synchronization b/t host program and shader:
 * 1. Host initializes src and dst buffer to 0
 * 2. Shader keeps reading src buffer and check value
 * 3. Host writes src buffer to 0x5678 to indicate quit, polling dst until it becomes 0x5678
 * 4. Shader write dst buffer to 0x5678 after src changes to 0x5678, then quits
 * 5. Host program quits after dst becomes 0x5678
 * Need at least two gpu nodes to run the test. The default node has to be a gfx9 node,
 * otherwise, test is skipped. Use kfdtest --node=$$ to specify the default node
 * This test case is introduced as a side-result of investigation of SWDEV-134798, which
 * is a gpu vm fault while running rocr conformance test. Here we try to simulate the
 * same test behaviour.
 */
TEST_F(KFDMemoryTest, MapUnmapToNodes) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId < FAMILY_AI) {
        LOG() << "Skipping test: Test requires gfx9 and later asics." << std::endl;
        return;
    }

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    if (gpuNodes.size() < 2) {
        LOG() << "Skipping test: At least two GPUs are required." << std::endl;
        return;
    }
    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    LOG() << "default GPU node" << defaultGPUNode << std::endl;
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HSAuint32 nondefaultNode;
    for (unsigned i = 0; i < gpuNodes.size(); i++) {
        if (gpuNodes.at(i) != defaultGPUNode) {
            nondefaultNode = gpuNodes.at(i);
            break;
        }
    }
    HSAuint32 mapNodes[2] = {defaultGPUNode, nondefaultNode};

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer srcBuffer(PAGE_SIZE, defaultGPUNode);
    HsaMemoryBuffer dstBuffer(PAGE_SIZE, defaultGPUNode);

    m_pIsaGen->CompileShader(gfx9_PollMemory, "ReadMemory", isaBuffer);

    PM4Queue pm4Queue;
    ASSERT_SUCCESS(pm4Queue.Create(defaultGPUNode));

    Dispatch dispatch0(isaBuffer);
    dispatch0.SetArgs(srcBuffer.As<void*>(), dstBuffer.As<void*>());
    dispatch0.Submit(pm4Queue);

    HsaMemMapFlags memFlags = {0};
    memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    memFlags.ui32.HostAccess = 1;

    for (unsigned i = 0; i < 1<<14; i ++) {
        hsaKmtMapMemoryToGPUNodes(srcBuffer.As<void*>(), PAGE_SIZE, NULL, memFlags, (i>>5)&1+1, mapNodes);
    }

    /* Fill src buffer so shader quits */
    srcBuffer.Fill(0x5678);
    WaitOnValue(dstBuffer.As<uint32_t *>(), 0x5678);
    EXPECT_EQ(*dstBuffer.As<uint32_t *>(), 0x5678);
    EXPECT_SUCCESS(pm4Queue.Destroy());
    TEST_END
}

// Basic test of hsaKmtMapMemoryToGPU and hsaKmtUnmapMemoryToGPU
TEST_F(KFDMemoryTest , MapMemoryToGPU) {
    TEST_START(TESTPROFILE_RUNALL)

    unsigned int *nullPtr = NULL;
    unsigned int* pDb = NULL;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    ASSERT_SUCCESS(hsaKmtAllocMemory(defaultGPUNode /* system */, PAGE_SIZE, m_MemoryFlags,
                   reinterpret_cast<void**>(&pDb)));
    // verify that pDb is not null before it's being used
    ASSERT_NE(nullPtr, pDb) << "hsaKmtAllocMemory returned a null pointer";
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(pDb, PAGE_SIZE, NULL));
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(pDb));
    // Release the buffers
    EXPECT_SUCCESS(hsaKmtFreeMemory(pDb, PAGE_SIZE));

    TEST_END
}

// Following tests are for hsaKmtAllocMemory with invalid params
TEST_F(KFDMemoryTest, InvalidMemoryPointerAlloc) {
    TEST_START(TESTPROFILE_RUNALL)

    m_MemoryFlags.ui32.NoNUMABind = 1;
    EXPECT_EQ(HSAKMT_STATUS_INVALID_PARAMETER, hsaKmtAllocMemory(0 /* system */, PAGE_SIZE, m_MemoryFlags, NULL));

    TEST_END
}

TEST_F(KFDMemoryTest, ZeroMemorySizeAlloc) {
    TEST_START(TESTPROFILE_RUNALL)

    unsigned int* pDb = NULL;
    EXPECT_EQ(HSAKMT_STATUS_INVALID_PARAMETER, hsaKmtAllocMemory(0 /* system */, 0, m_MemoryFlags,
              reinterpret_cast<void**>(&pDb)));

    TEST_END
}

// Basic test for hsaKmtAllocMemory
TEST_F(KFDMemoryTest, MemoryAlloc) {
    TEST_START(TESTPROFILE_RUNALL)

    unsigned int* pDb = NULL;
    m_MemoryFlags.ui32.NoNUMABind = 1;
    EXPECT_SUCCESS(hsaKmtAllocMemory(0 /* system */, PAGE_SIZE, m_MemoryFlags, reinterpret_cast<void**>(&pDb)));

    TEST_END
}

TEST_F(KFDMemoryTest, AccessPPRMem) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    if (is_dgpu()) {
        LOG() << "Skipping test: Test requires APU." << std::endl;
        return;
    }

    unsigned int *destBuf = (unsigned int *)VirtualAllocMemory(NULL, PAGE_SIZE,
                                            MEM_READ | MEM_WRITE);

    PM4Queue queue;

    ASSERT_SUCCESS(queue.Create(defaultGPUNode));

    queue.PlaceAndSubmitPacket(PM4WriteDataPacket(destBuf,
                                0xABCDEF09, 0x12345678));

    queue.Wait4PacketConsumption();

    WaitOnValue(destBuf, 0xABCDEF09);
    WaitOnValue(destBuf + 1, 0x12345678);

    EXPECT_SUCCESS(queue.Destroy());

    /* This sleep hides the dmesg PPR message storm on Raven, which happens
     * when the CPU buffer is freed before the excessive PPRs are all
     * consumed by IOMMU HW. Because of that, a kernel driver workaround
     * is put in place to address that, so we don't need to wait here.
     */
    // sleep(5);

    VirtualFreeMemory(destBuf, PAGE_SIZE);

    TEST_END
}

// Linux OS-specific Test for registering OS allocated memory
TEST_F(KFDMemoryTest, MemoryRegister) {
    const HsaNodeProperties *pNodeProperties = m_NodeInfo.HsaDefaultGPUNodeProperties();
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    /* Different unaligned memory locations to be mapped for GPU
     * access:
     *
     * - initialized data segment (file backed)
     * - stack (anonymous memory)
     *
     * Separate them enough so they are in different cache lines
     * (64-byte = 16-dword).
     */
    static volatile HSAuint32 globalData = 0xdeadbeef;
    volatile HSAuint32 stackData[17] = {0};
    const unsigned dstOffset = 0;
    const unsigned sdmaOffset = 16;

    HsaMemoryBuffer srcBuffer((void *)&globalData, sizeof(HSAuint32));
    HsaMemoryBuffer dstBuffer((void *)&stackData[dstOffset], sizeof(HSAuint32));
    HsaMemoryBuffer sdmaBuffer((void *)&stackData[sdmaOffset], sizeof(HSAuint32));

    /* Create PM4 and SDMA queues before fork+COW to test queue
     * eviction and restore
     */
    PM4Queue pm4Queue;
    SDMAQueue sdmaQueue;
    ASSERT_SUCCESS(pm4Queue.Create(defaultGPUNode));
    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->GetCopyDwordIsa(isaBuffer);

    /* First submit just so the queues are not empty, and to get the
     * TLB populated (in case we need to flush TLBs somewhere after
     * updating the page tables)
     */
    Dispatch dispatch0(isaBuffer);
    dispatch0.SetArgs(srcBuffer.As<void*>(), dstBuffer.As<void*>());
    dispatch0.Submit(pm4Queue);
    dispatch0.Sync(g_TestTimeOut);

    sdmaQueue.PlaceAndSubmitPacket(SDMAWriteDataPacket(sdmaQueue.GetFamilyId(), sdmaBuffer.As<HSAuint32 *>(), 0x12345678));
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
    globalData = 0xD00BED00;
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

    sdmaQueue.PlaceAndSubmitPacket(SDMAWriteDataPacket(sdmaQueue.GetFamilyId(), sdmaBuffer.As<HSAuint32 *>(), 0xD0BED0BE));
    sdmaQueue.Wait4PacketConsumption();

    EXPECT_SUCCESS(pm4Queue.Destroy());
    EXPECT_SUCCESS(sdmaQueue.Destroy());

    EXPECT_EQ(0xD00BED00, globalData);
    EXPECT_EQ(0xD00BED00, stackData[dstOffset]);
    EXPECT_EQ(0xD0BED0BE, stackData[sdmaOffset]);

    TEST_END
}

TEST_F(KFDMemoryTest, MemoryRegisterSamePtr) {
    if (!is_dgpu()) {
        LOG() << "Skipping test: Will run on APU once APU+dGPU supported." << std::endl;
        return;
    }

    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    HSAuint64 nGPU = gpuNodes.size();  // number of gpu nodes
    static volatile HSAuint32 mem[4];
    HSAuint64 gpuva1, gpuva2;

    /* Same address, different size */
    EXPECT_SUCCESS(hsaKmtRegisterMemory((void *)&mem[0], sizeof(HSAuint32)*2));
    EXPECT_SUCCESS(hsaKmtMapMemoryToGPU((void *)&mem[0], sizeof(HSAuint32)*2,
                                        &gpuva1));
    EXPECT_SUCCESS(hsaKmtRegisterMemory((void *)&mem[0], sizeof(HSAuint32)));
    EXPECT_SUCCESS(hsaKmtMapMemoryToGPU((void *)&mem[0], sizeof(HSAuint32),
                                        &gpuva2));
    EXPECT_TRUE(gpuva1 != gpuva2);
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(reinterpret_cast<void *>(gpuva1)));
    EXPECT_SUCCESS(hsaKmtDeregisterMemory(reinterpret_cast<void *>(gpuva1)));
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(reinterpret_cast<void *>(gpuva2)));
    EXPECT_SUCCESS(hsaKmtDeregisterMemory(reinterpret_cast<void *>(gpuva2)));

    /* Same address, same size */
    HsaMemMapFlags memFlags = {0};
    memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    memFlags.ui32.HostAccess = 1;

    HSAuint32 nodes[nGPU];
    for (unsigned int i = 0; i < nGPU; i++)
        nodes[i] = gpuNodes.at(i);
    EXPECT_SUCCESS(hsaKmtRegisterMemoryToNodes((void *)&mem[2],
                            sizeof(HSAuint32)*2, nGPU, nodes));
    EXPECT_SUCCESS(hsaKmtMapMemoryToGPUNodes((void *)&mem[2],
                                        sizeof(HSAuint32) * 2,
                                        &gpuva1, memFlags, nGPU, nodes));
    EXPECT_SUCCESS(hsaKmtRegisterMemoryToNodes((void *)&mem[2],
                                        sizeof(HSAuint32) * 2, nGPU, nodes));
    EXPECT_SUCCESS(hsaKmtMapMemoryToGPUNodes((void *)&mem[2],
                                        sizeof(HSAuint32) * 2,
                                        &gpuva2, memFlags, nGPU, nodes));
    EXPECT_EQ(gpuva1, gpuva2);
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(reinterpret_cast<void *>(gpuva1)));
    EXPECT_SUCCESS(hsaKmtDeregisterMemory(reinterpret_cast<void *>(gpuva1)));
    /* Confirm that we still have access to the memory, mem[2] */
    PM4Queue queue;
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    mem[2] = 0x0;
    queue.PlaceAndSubmitPacket(PM4WriteDataPacket(reinterpret_cast<unsigned int *>(gpuva2),
                                                  0xdeadbeef));
    queue.PlaceAndSubmitPacket(PM4ReleaseMemoryPacket(m_FamilyId, true, 0, 0));
    queue.Wait4PacketConsumption();
    EXPECT_EQ(true, WaitOnValue((unsigned int *)(&mem[2]), 0xdeadbeef));
    EXPECT_SUCCESS(queue.Destroy());
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(reinterpret_cast<void *>(gpuva2)));
    EXPECT_SUCCESS(hsaKmtDeregisterMemory(reinterpret_cast<void *>(gpuva2)));

    TEST_END
}

/* FlatScratchAccess
 * Since HsaMemoryBuffer has to be associated with a specific GPU node, this function in the current form
 * will not work for multiple GPU nodes. For now test only one default GPU node.
 * TODO: Generalize it to support multiple nodes
 */

#define SCRATCH_SLICE_SIZE 0x10000
#define SCRATCH_SLICE_NUM 3
#define SCRATCH_SIZE (SCRATCH_SLICE_NUM * SCRATCH_SLICE_SIZE)
#define SCRATCH_SLICE_OFFSET(i) ((i) * SCRATCH_SLICE_SIZE)

TEST_F(KFDMemoryTest, FlatScratchAccess) {
    TEST_START(TESTPROFILE_RUNALL)
    if (m_FamilyId == FAMILY_CI || m_FamilyId == FAMILY_KV) {
        LOG() << "Skipping test: VI-based shader not supported on other ASICs." << std::endl;
        return;
    }

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer scratchBuffer(SCRATCH_SIZE, defaultGPUNode, false/*zero*/, false/*local*/,
                                  false/*exec*/, true /*scratch*/);

    // Unmap scratch for sub-allocation mapping tests
    ASSERT_SUCCESS(hsaKmtUnmapMemoryToGPU(scratchBuffer.As<void*>()));

    // Map and unmap a few slices in different order: 2-0-1, 0-2-1
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(scratchBuffer.As<char*>() + SCRATCH_SLICE_OFFSET(2),
                                        SCRATCH_SLICE_SIZE, NULL));
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(scratchBuffer.As<char*>() + SCRATCH_SLICE_OFFSET(0),
                                        SCRATCH_SLICE_SIZE, NULL));
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(scratchBuffer.As<char*>() + SCRATCH_SLICE_OFFSET(1),
                                        SCRATCH_SLICE_SIZE, NULL));

    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(scratchBuffer.As<char*>() + SCRATCH_SLICE_OFFSET(1)));
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(scratchBuffer.As<char*>() + SCRATCH_SLICE_OFFSET(2)));
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(scratchBuffer.As<char*>() + SCRATCH_SLICE_OFFSET(0)));

    // Map everything for test below
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(scratchBuffer.As<char*>(), SCRATCH_SIZE, NULL));

    HsaMemoryBuffer srcMemBuffer(PAGE_SIZE, defaultGPUNode);
    HsaMemoryBuffer dstMemBuffer(PAGE_SIZE, defaultGPUNode);

    // Initialize the srcBuffer to some fixed value
    srcMemBuffer.Fill(0x01010101);

    const char *pScratchCopyDword;
    if (m_FamilyId < FAMILY_AI)
        pScratchCopyDword = gfx8_ScratchCopyDword;
    else if (m_FamilyId < FAMILY_NV)
        pScratchCopyDword = gfx9_ScratchCopyDword;
    else
        pScratchCopyDword = gfx10_ScratchCopyDword;
    m_pIsaGen->CompileShader(pScratchCopyDword, "ScratchCopyDword", isaBuffer);

    const HsaNodeProperties *pNodeProperties = m_NodeInfo.GetNodeProperties(defaultGPUNode);

    /* TODO: Add support to all GPU Nodes.
     * The loop over the system nodes is removed as the test can be executed only on GPU nodes. This
     * also requires changes to be made to all the HsaMemoryBuffer variables defined above, as
     * HsaMemoryBuffer is now associated with a Node.
     */
    if (pNodeProperties != NULL) {
        // Get the aperture of the scratch buffer
        HsaMemoryProperties *memoryProperties = new HsaMemoryProperties[pNodeProperties->NumMemoryBanks];
        EXPECT_SUCCESS(hsaKmtGetNodeMemoryProperties(defaultGPUNode, pNodeProperties->NumMemoryBanks,
                       memoryProperties));

        for (unsigned int bank = 0; bank < pNodeProperties->NumMemoryBanks; bank++) {
            if (memoryProperties[bank].HeapType == HSA_HEAPTYPE_GPU_SCRATCH) {
                int numWaves = pNodeProperties->NumShaderBanks;  // WAVES must be >= # SE
                int waveSize = 1;  // Amount of space used by each wave in units of 256 dwords

                PM4Queue queue;
                ASSERT_SUCCESS(queue.Create(defaultGPUNode));

                HSAuint64 scratchApertureAddr = memoryProperties[bank].VirtualBaseAddress;

                // Create a dispatch packet to copy
                Dispatch dispatchSrcToScratch(isaBuffer);

                // Setup the dispatch packet
                // Copying from the source Memory Buffer to the scratch buffer
                dispatchSrcToScratch.SetArgs(srcMemBuffer.As<void*>(), reinterpret_cast<void*>(scratchApertureAddr));
                dispatchSrcToScratch.SetDim(1, 1, 1);
                dispatchSrcToScratch.SetScratch(numWaves, waveSize, scratchBuffer.As<uint64_t>());
                // Submit the packet
                dispatchSrcToScratch.Submit(queue);
                dispatchSrcToScratch.Sync();

                // Create another dispatch packet to copy scratch buffer contents to destination buffer.
                Dispatch dispatchScratchToDst(isaBuffer);

                // Set the arguments to copy from the scratch buffer to the destination buffer
                dispatchScratchToDst.SetArgs(reinterpret_cast<void*>(scratchApertureAddr), dstMemBuffer.As<void*>());
                dispatchScratchToDst.SetDim(1, 1, 1);
                dispatchScratchToDst.SetScratch(numWaves, waveSize, scratchBuffer.As<uint64_t>());

                // Submit the packet
                dispatchScratchToDst.Submit(queue);
                dispatchScratchToDst.Sync();

                // Check that the scratch buffer contents were correctly copied over to the system memory buffer
                EXPECT_EQ(dstMemBuffer.As<unsigned int*>()[0], 0x01010101);
            }
        }

        delete [] memoryProperties;
    }

    TEST_END
}

TEST_F(KFDMemoryTest, GetTileConfigTest) {
    TEST_START(TESTPROFILE_RUNALL)

    HSAuint32 tile_config[32] = {0};
    HSAuint32 macro_tile_config[16] = {0};
    unsigned int i;
    HsaGpuTileConfig config = {0};

    config.TileConfig = tile_config;
    config.MacroTileConfig = macro_tile_config;
    config.NumTileConfigs = 32;
    config.NumMacroTileConfigs = 16;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();

    ASSERT_SUCCESS(hsaKmtGetTileConfig(defaultGPUNode, &config));

    LOG() << "tile_config:" << std::endl;
    for (i = 0; i < config.NumTileConfigs; i++)
        LOG() << "\t" << std::dec << i << ": 0x" << std::hex
                << tile_config[i] << std::endl;

    LOG() << "macro_tile_config:" << std::endl;
    for (i = 0; i < config.NumMacroTileConfigs; i++)
        LOG() << "\t" << std::dec << i << ": 0x" << std::hex
                << macro_tile_config[i] << std::endl;

    LOG() << "gb_addr_config: 0x" << std::hex << config.GbAddrConfig
            << std::endl;
    LOG() << "num_banks: 0x" << std::hex << config.NumBanks << std::endl;
    LOG() << "num_ranks: 0x" << std::hex << config.NumRanks << std::endl;

    TEST_END
}

void KFDMemoryTest::BigBufferSystemMemory(int defaultGPUNode, HSAuint64 granularityMB,
                                                HSAuint64 *lastSize) {
    HSAuint64 sysMemSizeMB;
    HsaMemMapFlags mapFlags = {0};
    HSAuint64 AlternateVAGPU;
    int ret;

    sysMemSizeMB = GetSysMemSize() >> 20;

    LOG() << "Found System Memory of " << std::dec << sysMemSizeMB
                << "MB" << std::endl;

    /* Testing big buffers in system memory */
    unsigned int * pDb = NULL;
    HSAuint64 lowMB = 0;
    HSAuint64 highMB = (sysMemSizeMB + granularityMB - 1) & ~(granularityMB - 1);

    HSAuint64 sizeMB;
    HSAuint64 size = 0;
    HSAuint64 lastTestedSize = 0;

    while (highMB - lowMB > granularityMB) {
        sizeMB = (lowMB + highMB) / 2;
        size = sizeMB * 1024 * 1024;
        ret = hsaKmtAllocMemory(0 /* system */, size, m_MemoryFlags,
                                reinterpret_cast<void**>(&pDb));
        if (ret) {
            highMB = sizeMB;
            continue;
        }

        ret = hsaKmtMapMemoryToGPUNodes(pDb, size, &AlternateVAGPU,
                        mapFlags, 1, reinterpret_cast<HSAuint32 *>(&defaultGPUNode));
        if (ret) {
            EXPECT_SUCCESS(hsaKmtFreeMemory(pDb, size));
            highMB = sizeMB;
            continue;
        }
        EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(pDb));
        EXPECT_SUCCESS(hsaKmtFreeMemory(pDb, size));

        lowMB = sizeMB;
        lastTestedSize = sizeMB;
    }

    /* Save the biggest allocated system buffer for signal handling test */
    LOG() << "The biggest allocated system buffer is " << std::dec
            << lastTestedSize << "MB" << std::endl;
    if (lastSize)
        *lastSize = lastTestedSize * 1024 *1024;
}

void KFDMemoryTest::BigBufferVRAM(int defaultGPUNode, HSAuint64 granularityMB,
                                        HSAuint64 *lastSize) {
    HSAuint64 AlternateVAGPU;
    int ret;
    HSAuint64 vramSizeMB;
    HsaMemFlags memFlags;
    HsaMemMapFlags mapFlags = {0};

    vramSizeMB = GetVramSize(defaultGPUNode) >> 20;

    LOG() << "Found VRAM of " << std::dec << vramSizeMB << "MB." << std::endl;

    /* Testing big buffers in VRAM */
    unsigned int * pDb = NULL;
    HSAuint64 lowMB = 0;
    HSAuint64 highMB = (vramSizeMB + granularityMB - 1) & ~(granularityMB - 1);

    HSAuint64 sizeMB;
    HSAuint64 size = 0;
    HSAuint64 lastTestedSize = 0;

    memset(&memFlags, 0, sizeof(memFlags));
    memFlags.ui32.HostAccess = 0;
    memFlags.ui32.NonPaged = 1;

    while (highMB - lowMB > granularityMB) {
        sizeMB = (lowMB + highMB) / 2;
        size = sizeMB * 1024 * 1024;
        ret = hsaKmtAllocMemory(defaultGPUNode, size, memFlags,
                                reinterpret_cast<void**>(&pDb));
        if (ret) {
            highMB = sizeMB;
            continue;
        }

        ret = hsaKmtMapMemoryToGPUNodes(pDb, size, &AlternateVAGPU,
                        mapFlags, 1, reinterpret_cast<HSAuint32 *>(&defaultGPUNode));
        if (ret) {
            EXPECT_SUCCESS(hsaKmtFreeMemory(pDb, size));
            highMB = sizeMB;
            continue;
        }
        EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(pDb));
        EXPECT_SUCCESS(hsaKmtFreeMemory(pDb, size));

        lowMB = sizeMB;
        lastTestedSize = sizeMB;
    }

    LOG() << "The biggest allocated VRAM buffer is " << std::dec
            << lastTestedSize << "MB" << std::endl;
    if (lastSize)
        *lastSize = lastTestedSize * 1024 * 1024;

    /* Make sure 3/4 vram can be allocated.*/
    EXPECT_GE(lastTestedSize * 4, vramSizeMB * 3);
    if (lastTestedSize * 16 < vramSizeMB * 15)
        WARN() << "The biggest allocated VRAM buffer size is smaller than the expected "
            << vramSizeMB * 15 / 16 << "MB" << std::endl;
}

/* BigBufferStressTest allocs, maps/unmaps, and frees the biggest possible system
 * buffers. Its size is found using binary search in the range (0, RAM SIZE) with
 * a granularity of 128M. Repeat the similar logic on local buffers (VRAM).
 * Finally, it allocs and maps 128M system buffers in a loop until it
 * fails, then unmaps and frees them afterwards.
 * Please note we limit the biggest possible system buffer to be smaller than
 * the RAM size. The reason is that the system buffer can make use of virtual
 * memory so that a system buffer could be very large even though the RAM size
 * is small. For example, on a typical Carrizo platform, the biggest allocated
 * system buffer could be more than 14G even though it only has 4G memory.
 * In that situation, it will take too much time to finish the test, because of
 * the onerous memory swap operation. So we limit the buffer size that way.
 */
TEST_F(KFDMemoryTest, BigBufferStressTest) {
    if (!is_dgpu()) {
        LOG() << "Skipping test: Running on APU fails and locks the system." << std::endl;
        return;
    }
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    HSAuint64 AlternateVAGPU;
    HsaMemMapFlags mapFlags = {0};
    int ret;

    HSAuint64 granularityMB = 128;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    BigBufferSystemMemory(defaultGPUNode, granularityMB, NULL);

    BigBufferVRAM(defaultGPUNode, granularityMB, NULL);

    /* Repeatedly allocate and map big buffers in system memory until it fails,
     * then unmap and free them.
     */
#define ARRAY_ENTRIES 2048

    int i = 0, allocationCount = 0;
    unsigned int* pDb_array[ARRAY_ENTRIES];
    HSAuint64 block_size_mb = 128;
    HSAuint64 block_size = block_size_mb * 1024 * 1024;

    /* Test 4 times to see if there is any memory leak.*/
    for (int repeat = 1; repeat < 5; repeat++) {

        for (i = 0; i < ARRAY_ENTRIES; i++) {
            ret = hsaKmtAllocMemory(0 /* system */, block_size, m_MemoryFlags,
                    reinterpret_cast<void**>(&pDb_array[i]));
            if (ret)
                break;

            ret = hsaKmtMapMemoryToGPUNodes(pDb_array[i], block_size,
                    &AlternateVAGPU, mapFlags, 1, reinterpret_cast<HSAuint32 *>(&defaultGPUNode));
            if (ret) {
                EXPECT_SUCCESS(hsaKmtFreeMemory(pDb_array[i], block_size));
                break;
            }
        }

        LOG() << "Allocated system buffers time " << std::dec << repeat << ": " << i << "x"
            << block_size_mb << "MB" << std::endl;

        if (allocationCount == 0)
            allocationCount = i;
        EXPECT_GE(i, allocationCount) << "There might be memory leak!" << std::endl;

        for (int j = 0; j < i; j++) {
            EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(pDb_array[j]));
            EXPECT_SUCCESS(hsaKmtFreeMemory(pDb_array[j], block_size));
        }
    }

    TEST_END
}

TEST_F(KFDMemoryTest, MMBench) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    unsigned testIndex, sizeIndex, memType, nMemTypes;
    const char *memTypeStrings[2] = {"SysMem", "VRAM"};
    const struct {
        unsigned size;
        unsigned num;
    } bufParams[] = {
        /* Buffer sizes in x16 increments. Limit memory usage to about
         * 1GB. For small sizes we use 1000 buffers, which means we
         * conveniently measure microseconds and report nanoseconds.
         */
        {PAGE_SIZE      , 1000},  /*  4KB */
        {PAGE_SIZE <<  4, 1000},  /* 64KB */
        {PAGE_SIZE <<  9,  500},  /*  2MB */
        {PAGE_SIZE << 13,   32},  /* 32MB */
        {PAGE_SIZE << 18,    1},  /*  1GB */
    };
    const unsigned nSizes = sizeof(bufParams) / sizeof(bufParams[0]);
    const unsigned nTests = nSizes << 2;
#define TEST_BUFSIZE(index) (bufParams[(index) % nSizes].size)
#define TEST_NBUFS(index)  (bufParams[(index) % nSizes].num)
#define TEST_MEMTYPE(index) ((index / nSizes) & 0x1)
#define TEST_SDMA(index)    (((index / nSizes) >> 1) & 0x1)

    void *bufs[1000];
    HSAuint64 start, end;
    unsigned i;
    HSAKMT_STATUS ret;
    HsaMemFlags memFlags = {0};
    HsaMemMapFlags mapFlags = {0};
    HSAuint64 altVa;

    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HSAuint64 vramSizeMB = GetVramSize(defaultGPUNode) >> 20;

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    bool is_all_large_bar = true;

    for (unsigned i = 0; i < gpuNodes.size(); i++) {
        if (!m_NodeInfo.IsGPUNodeLargeBar(gpuNodes.at(i))) {
                is_all_large_bar = false;
                break;
        }
    }

    LOG() << "Found VRAM of " << std::dec << vramSizeMB << "MB." << std::endl;

    if (vramSizeMB == 0)
        nMemTypes = 1;
    else
        nMemTypes = 2;

    /* Two SDMA queues to interleave user mode SDMA with memory
     * management on either SDMA engine. Make the queues long enough
     * to buffer at least nBufs x WriteData packets (7 dwords per
     * packet).
     */
    SDMAQueue sdmaQueue[2];
    ASSERT_SUCCESS(sdmaQueue[0].Create(defaultGPUNode, PAGE_SIZE*8));
    ASSERT_SUCCESS(sdmaQueue[1].Create(defaultGPUNode, PAGE_SIZE*8));
    HsaMemoryBuffer sdmaBuffer(PAGE_SIZE, 0); /* system memory */
#define INTERLEAVE_SDMA() do {                                          \
        if (interleaveSDMA) {                                           \
            sdmaQueue[0].PlaceAndSubmitPacket(                          \
                SDMAWriteDataPacket(sdmaQueue[0].GetFamilyId(), sdmaBuffer.As<HSAuint32 *>(),       \
                                    0x12345678));                       \
            sdmaQueue[1].PlaceAndSubmitPacket(                          \
                SDMAWriteDataPacket(sdmaQueue[1].GetFamilyId(), sdmaBuffer.As<HSAuint32 *>()+16,    \
                                    0x12345678));                       \
        }                                                               \
    } while (0)
#define IDLE_SDMA() do {                                                \
        if (interleaveSDMA) {                                           \
            sdmaQueue[0].Wait4PacketConsumption();                      \
            sdmaQueue[1].Wait4PacketConsumption();                      \
        }                                                               \
    } while (0)

    LOG() << "Test (avg. ns)\t    alloc   mapOne  umapOne   mapAll  umapAll     free" << std::endl;
    for (testIndex = 0; testIndex < nTests; testIndex++) {
        unsigned bufSize = TEST_BUFSIZE(testIndex);
        unsigned nBufs = TEST_NBUFS(testIndex);
        unsigned memType = TEST_MEMTYPE(testIndex);
        bool interleaveSDMA = TEST_SDMA(testIndex);
        HSAuint64 allocTime, map1Time, unmap1Time, mapAllTime, unmapAllTime, freeTime;
        HSAuint32 allocNode;

        if ((testIndex % nSizes) == 0)
            LOG() << "--------------------------------------------------------------------------" << std::endl;

        if (memType >= nMemTypes)
            continue;  // skip unsupported mem types

        if (memType == 0) {
            allocNode = 0;
            memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
            memFlags.ui32.HostAccess = 1;
            memFlags.ui32.NonPaged = 0;
            memFlags.ui32.NoNUMABind = 1;
        } else {
            allocNode = defaultGPUNode;
            memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
            memFlags.ui32.HostAccess = 0;
            memFlags.ui32.NonPaged = 1;
        }

        /* Allocation */
        start = GetSystemTickCountInMicroSec();
        for (i = 0; i < nBufs; i++) {
            ASSERT_SUCCESS(hsaKmtAllocMemory(allocNode, bufSize, memFlags,
                                             &bufs[i]));
            INTERLEAVE_SDMA();
        }
        allocTime = GetSystemTickCountInMicroSec() - start;
        IDLE_SDMA();

        /* Map to one GPU */
        start = GetSystemTickCountInMicroSec();
        for (i = 0; i < nBufs; i++) {
            ASSERT_SUCCESS(hsaKmtMapMemoryToGPUNodes(bufs[i], bufSize,
                                                     &altVa, mapFlags, 1,
                                                     &defaultGPUNode));
            INTERLEAVE_SDMA();
        }
        map1Time = GetSystemTickCountInMicroSec() - start;
        IDLE_SDMA();

        /* Unmap from GPU */
        start = GetSystemTickCountInMicroSec();
        for (i = 0; i < nBufs; i++) {
            EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(bufs[i]));
            INTERLEAVE_SDMA();
        }
        unmap1Time = GetSystemTickCountInMicroSec() - start;
        IDLE_SDMA();

        /* Map to all GPUs */
        if (is_all_large_bar) {
            start = GetSystemTickCountInMicroSec();
            for (i = 0; i < nBufs; i++) {
                ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(bufs[i], bufSize, &altVa));
                INTERLEAVE_SDMA();
            }
            mapAllTime = GetSystemTickCountInMicroSec() - start;
            IDLE_SDMA();

            /* Unmap from all GPUs */
            start = GetSystemTickCountInMicroSec();
            for (i = 0; i < nBufs; i++) {
                EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(bufs[i]));
                INTERLEAVE_SDMA();
            }
            unmapAllTime = GetSystemTickCountInMicroSec() - start;
            IDLE_SDMA();
        }

        /* Free */
        start = GetSystemTickCountInMicroSec();
        for (i = 0; i < nBufs; i++) {
            EXPECT_SUCCESS(hsaKmtFreeMemory(bufs[i], bufSize));
            INTERLEAVE_SDMA();
        }
        freeTime = GetSystemTickCountInMicroSec() - start;
        IDLE_SDMA();

        allocTime = allocTime * 1000 / nBufs;
        map1Time = map1Time * 1000 / nBufs;
        unmap1Time = unmap1Time * 1000 / nBufs;
        mapAllTime = mapAllTime * 1000 / nBufs;
        unmapAllTime = unmapAllTime * 1000 / nBufs;
        freeTime = freeTime * 1000 / nBufs;

        unsigned bufSizeLog;
        char bufSizeUnit;
        if (bufSize < (1 << 20)) {
            bufSizeLog = bufSize >> 10;
            bufSizeUnit = 'K';
        } else if (bufSize < (1 << 30)) {
            bufSizeLog = bufSize >> 20;
            bufSizeUnit = 'M';
        } else {
            bufSizeLog = bufSize >> 30;
            bufSizeUnit = 'G';
        }

        LOG() << std::dec << std::setiosflags(std::ios::right)
              << std::setw(3) << bufSizeLog << bufSizeUnit << "-"
              << memTypeStrings[memType] << "-"
              << (interleaveSDMA ? "SDMA\t" : "noSDMA\t")
              << std::setw(9) << allocTime
              << std::setw(9) << map1Time
              << std::setw(9) << unmap1Time
              << std::setw(9) << mapAllTime
              << std::setw(9) << unmapAllTime
              << std::setw(9) << freeTime << std::endl;

#define MMBENCH_KEY_PREFIX memTypeStrings[memType] << "-" \
                           << (interleaveSDMA ? "SDMA" : "noSDMA") << "-" \
                           << (bufSize >> 10) << "K-"
        RECORD(allocTime) << MMBENCH_KEY_PREFIX << "alloc";
        RECORD(map1Time) << MMBENCH_KEY_PREFIX << "mapOne";
        RECORD(unmap1Time) << MMBENCH_KEY_PREFIX << "unmapOne";
        RECORD(mapAllTime) << MMBENCH_KEY_PREFIX << "mapAll";
        RECORD(unmapAllTime) << MMBENCH_KEY_PREFIX << "unmapAll";
        RECORD(freeTime) << MMBENCH_KEY_PREFIX << "free";
    }

    TEST_END
}

TEST_F(KFDMemoryTest, QueryPointerInfo) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    unsigned int bufSize = PAGE_SIZE * 8;  // CZ and Tonga need 8 pages
    HsaPointerInfo ptrInfo;
    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    HSAuint64 nGPU = gpuNodes.size();  // number of gpu nodes

    /* GraphicHandle is tested at KFDGraphicsInterop.RegisterGraphicsHandle */

    /*** Memory allocated on CPU node ***/
    HsaMemoryBuffer hostBuffer(bufSize, 0/*node*/, false, false/*local*/);
    EXPECT_SUCCESS(hsaKmtQueryPointerInfo(hostBuffer.As<void*>(), &ptrInfo));
    EXPECT_EQ(ptrInfo.Type, HSA_POINTER_ALLOCATED);
    EXPECT_EQ(ptrInfo.Node, 0);
    EXPECT_EQ(ptrInfo.MemFlags.Value, hostBuffer.Flags().Value);
    EXPECT_EQ(ptrInfo.CPUAddress, hostBuffer.As<void*>());
    EXPECT_EQ(ptrInfo.GPUAddress, (HSAuint64)hostBuffer.As<void*>());
    EXPECT_EQ(ptrInfo.SizeInBytes, (HSAuint64)hostBuffer.Size());
    if (is_dgpu()) {
        EXPECT_EQ((HSAuint64)ptrInfo.NMappedNodes, nGPU);
        // Check NMappedNodes again after unmapping the memory
        hsaKmtUnmapMemoryToGPU(hostBuffer.As<void*>());
        hsaKmtQueryPointerInfo(hostBuffer.As<void*>(), &ptrInfo);
    }
    EXPECT_EQ((HSAuint64)ptrInfo.NMappedNodes, 0);

    /* Skip testing local memory if the platform does not have it */
    if (GetVramSize(defaultGPUNode)) {
        HsaMemoryBuffer localBuffer(bufSize, defaultGPUNode, false, true);
        EXPECT_SUCCESS(hsaKmtQueryPointerInfo(localBuffer.As<void*>(), &ptrInfo));
        EXPECT_EQ(ptrInfo.Type, HSA_POINTER_ALLOCATED);
        EXPECT_EQ(ptrInfo.Node, defaultGPUNode);
        EXPECT_EQ(ptrInfo.MemFlags.Value, localBuffer.Flags().Value);
        EXPECT_EQ(ptrInfo.CPUAddress, localBuffer.As<void*>());
        EXPECT_EQ(ptrInfo.GPUAddress, (HSAuint64)localBuffer.As<void*>());
        EXPECT_EQ(ptrInfo.SizeInBytes, (HSAuint64)localBuffer.Size());

        HSAuint32 *addr = localBuffer.As<HSAuint32 *>() + 4;
        EXPECT_SUCCESS(hsaKmtQueryPointerInfo(reinterpret_cast<void *>(addr), &ptrInfo));
        EXPECT_EQ(ptrInfo.GPUAddress, (HSAuint64)localBuffer.As<void*>());
    }

    /** Registered memory: user pointer */
    static volatile HSAuint32 mem[4];  // 8 bytes for register only and
                                       // 8 bytes for register to nodes
    HsaMemoryBuffer hsaBuffer((void *)(&mem[0]), sizeof(HSAuint32)*2);
    if (is_dgpu()) {  // APU doesn't use userptr
        EXPECT_SUCCESS(hsaKmtQueryPointerInfo((void *)(&mem[0]), &ptrInfo));
        EXPECT_EQ(ptrInfo.Type, HSA_POINTER_REGISTERED_USER);
        EXPECT_EQ(ptrInfo.CPUAddress, &mem[0]);
        EXPECT_EQ(ptrInfo.GPUAddress, (HSAuint64)hsaBuffer.As<void*>());
        EXPECT_EQ(ptrInfo.SizeInBytes, sizeof(HSAuint32)*2);
        EXPECT_EQ(ptrInfo.NRegisteredNodes, 0);
        EXPECT_EQ(ptrInfo.NMappedNodes, nGPU);
        // Register to nodes
        HSAuint32 nodes[nGPU];
        for (unsigned int i = 0; i < nGPU; i++)
            nodes[i] = gpuNodes.at(i);
        EXPECT_SUCCESS(hsaKmtRegisterMemoryToNodes((void *)(&mem[2]),
                                sizeof(HSAuint32)*2, nGPU, nodes));
        EXPECT_SUCCESS(hsaKmtQueryPointerInfo((void *)(&mem[2]), &ptrInfo));
        EXPECT_EQ(ptrInfo.NRegisteredNodes, nGPU);
        EXPECT_SUCCESS(hsaKmtDeregisterMemory((void *)(&mem[2])));
    }

    /* Not a starting address, but an address inside the memory range
     * should also get the memory information
     */
    HSAuint32 *address = hostBuffer.As<HSAuint32 *>() + 1;
    EXPECT_SUCCESS(hsaKmtQueryPointerInfo(reinterpret_cast<void *>(address), &ptrInfo));
    EXPECT_EQ(ptrInfo.Type, HSA_POINTER_ALLOCATED);
    EXPECT_EQ(ptrInfo.CPUAddress, hostBuffer.As<void*>());
    if (is_dgpu()) {
        EXPECT_SUCCESS(hsaKmtQueryPointerInfo((void *)(&mem[1]), &ptrInfo));
        EXPECT_EQ(ptrInfo.Type, HSA_POINTER_REGISTERED_USER);
        EXPECT_EQ(ptrInfo.CPUAddress, &mem[0]);
    }

    /*** Set user data ***/
    char userData[16] = "This is a test.";
    EXPECT_SUCCESS(hsaKmtSetMemoryUserData(hostBuffer.As<HSAuint32 *>(), reinterpret_cast<void *>(userData)));
    EXPECT_SUCCESS(hsaKmtQueryPointerInfo(hostBuffer.As<void*>(), &ptrInfo));
    EXPECT_EQ(ptrInfo.UserData, (void *)userData);

    TEST_END
}

/* Linux OS-specific test for a debugger accessing HSA memory in a
 * debugged process.
 *
 * Allocates a system memory and a visible local memory buffer (if
 * possible). Forks a child process that PTRACE_ATTACHes to the parent
 * to access its memory like a debugger would. Child copies data in
 * the parent process using PTRACE_PEEKDATA and PTRACE_POKEDATA. After
 * the child terminates, the parent checks that the copy was
 * successful.
 */
TEST_F(KFDMemoryTest, PtraceAccess) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemFlags memFlags = {0};
    memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    memFlags.ui32.HostAccess = 1;

    void *mem[2];
    unsigned i;

    /* Offset in the VRAM buffer to test crossing non-contiguous
     * buffer boundaries. The second access starting from offset
     * sizeof(HSAint64)+1 will cross a node boundary in a single access,
     * for node sizes of 4MB or smaller.
     */
    const HSAuint64 VRAM_OFFSET = (4 << 20) - 2 * sizeof(HSAint64);

    // Alloc system memory from node 0 and initialize it
    memFlags.ui32.NonPaged = 0;
    memFlags.ui32.NoNUMABind = 1;
    ASSERT_SUCCESS(hsaKmtAllocMemory(0, PAGE_SIZE*2, memFlags, &mem[0]));
    for (i = 0; i < 4*sizeof(HSAint64) + 4; i++) {
        (reinterpret_cast<HSAuint8 *>(mem[0]))[i] = i;            // source
        (reinterpret_cast<HSAuint8 *>(mem[0]))[PAGE_SIZE+i] = 0;  // destination
    }

    // Try to alloc local memory from GPU node
    memFlags.ui32.NonPaged = 1;
    if (m_NodeInfo.IsGPUNodeLargeBar(defaultGPUNode)) {
        EXPECT_SUCCESS(hsaKmtAllocMemory(defaultGPUNode, PAGE_SIZE*2 + (4 << 20),
                                            memFlags, &mem[1]));
        mem[1] = reinterpret_cast<void *>(reinterpret_cast<HSAuint8 *>(mem[1]) + VRAM_OFFSET);
        for (i = 0; i < 4*sizeof(HSAint64) + 4; i++) {
            (reinterpret_cast<HSAuint8 *>(mem[1]))[i] = i;
            (reinterpret_cast<HSAuint8 *>(mem[1]))[PAGE_SIZE+i] = 0;
        }
    } else {
        LOG() << "Not testing local memory, it's invisible" << std::endl;
        mem[1] = NULL;
    }

    /* Allow any process to trace this one. If kernel is built without
     * Yama, this is not needed, and this call will fail.
     */
#ifdef PR_SET_PTRACER
    prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif

    // Find current pid so the child can trace it
    pid_t tracePid = getpid();

    // Fork the child
    pid_t childPid = fork();
    ASSERT_GE(childPid, 0);
    if (childPid == 0) {
        int traceStatus;
        int err = 0, r;

        /* Child process: we catch any exceptions to make sure we detach
         * from the traced process, because terminating without detaching
         * leaves the traced process stopped.
         */
        r = ptrace(PTRACE_ATTACH, tracePid, NULL, NULL);
        if (r) {
            WARN() << "PTRACE_ATTACH failed: " << r << std::endl;
            exit(1);
        }
        try {
            do {
                waitpid(tracePid, &traceStatus, 0);
            } while (!WIFSTOPPED(traceStatus));

            for (i = 0; i < 4; i++) {
                // Test 4 different (mis-)alignments, leaving 1-byte gaps between longs
                HSAuint8 *addr = reinterpret_cast<HSAuint8 *>(reinterpret_cast<long *>(mem[0]) + i) + i;
                errno = 0;
                long data = ptrace(PTRACE_PEEKDATA, tracePid, addr, NULL);
                EXPECT_EQ(0, errno);
                EXPECT_EQ(0, ptrace(PTRACE_POKEDATA, tracePid, addr + PAGE_SIZE,
                                    reinterpret_cast<void *>(data)));

                if (mem[1] == NULL)
                    continue;

                addr = reinterpret_cast<HSAuint8 *>(reinterpret_cast<long *>(mem[1]) + i) + i;
                errno = 0;
                data = ptrace(PTRACE_PEEKDATA, tracePid, addr, NULL);
                EXPECT_EQ(0, errno);
                EXPECT_EQ(0, ptrace(PTRACE_POKEDATA, tracePid, addr + PAGE_SIZE,
                                reinterpret_cast<void *>(data)));
            }
        } catch (...) {
            err = 1;
        }
        r = ptrace(PTRACE_DETACH, tracePid, NULL, NULL);
        if (r) {
            WARN() << "PTRACE_DETACH failed: " << r << std::endl;
            exit(1);
        }
        exit(err);
    } else {
        int childStatus;

        // Parent process, just wait for the child to finish
        EXPECT_EQ(childPid, waitpid(childPid, &childStatus, 0));
        EXPECT_NE(0, WIFEXITED(childStatus));
        EXPECT_EQ(0, WEXITSTATUS(childStatus));
    }

    // Clear gaps in the source that should not have been copied
    (reinterpret_cast<uint8_t*>(mem[0]))[  sizeof(long)    ] = 0;
    (reinterpret_cast<uint8_t*>(mem[0]))[2*sizeof(long) + 1] = 0;
    (reinterpret_cast<uint8_t*>(mem[0]))[3*sizeof(long) + 2] = 0;
    (reinterpret_cast<uint8_t*>(mem[0]))[4*sizeof(long) + 3] = 0;
    // Check results
    EXPECT_EQ(0, memcmp(mem[0], reinterpret_cast<HSAuint8 *>(mem[0]) + PAGE_SIZE,
                        sizeof(long)*4 + 4));
    // Free memory
    EXPECT_SUCCESS(hsaKmtFreeMemory(mem[0], PAGE_SIZE*2));

    if (mem[1]) {
        (reinterpret_cast<uint8_t*>(mem[1]))[  sizeof(HSAint64)    ] = 0;
        (reinterpret_cast<uint8_t*>(mem[1]))[2*sizeof(HSAint64) + 1] = 0;
        (reinterpret_cast<uint8_t*>(mem[1]))[3*sizeof(HSAint64) + 2] = 0;
        (reinterpret_cast<uint8_t*>(mem[1]))[4*sizeof(HSAint64) + 3] = 0;
        EXPECT_EQ(0, memcmp(mem[1], reinterpret_cast<HSAuint8 *>(mem[1]) + PAGE_SIZE,
                            sizeof(HSAint64)*4 + 4));
        mem[1] = reinterpret_cast<void *>(reinterpret_cast<HSAuint8 *>(mem[1]) - VRAM_OFFSET);
        EXPECT_SUCCESS(hsaKmtFreeMemory(mem[1], PAGE_SIZE*2));
    }

    TEST_END
}

TEST_F(KFDMemoryTest, PtraceAccessInvisibleVram) {
    char *hsaDebug = getenv("HSA_DEBUG");

    if (!is_dgpu()) {
        LOG() << "Skipping test: There is no VRAM on APU." << std::endl;
        return;
    }

    if (!hsaDebug || !strcmp(hsaDebug, "0")) {
        LOG() << "Skipping test: HSA_DEBUG environment variable not set." << std::endl;
        return;
    }

    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HsaMemMapFlags mapFlags = {0};
    HsaMemFlags memFlags = {0};
    memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
    /* Allocate host not accessible vram */
    memFlags.ui32.HostAccess = 0;
    memFlags.ui32.NonPaged = 1;

    void *mem, *mem0, *mem1;
    unsigned size = PAGE_SIZE*2 + (4 << 20);
    HSAuint64 data[2] = {0xdeadbeefdeadbeef, 0xcafebabecafebabe};
    unsigned int data0[2] = {0xdeadbeef, 0xdeadbeef};
    unsigned int data1[2] = {0xcafebabe, 0xcafebabe};

    const HSAuint64 VRAM_OFFSET = (4 << 20) - sizeof(HSAuint64);

    ASSERT_SUCCESS(hsaKmtAllocMemory(defaultGPUNode, size, memFlags, &mem));
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPUNodes(mem, size, NULL,
                                mapFlags, 1, reinterpret_cast<HSAuint32 *>(&defaultGPUNode)));
    /* Set the word before 4M boundary to 0xdeadbeefdeadbeef
     * and the word after 4M boundary to 0xcafebabecafebabe
     */
    mem0 = reinterpret_cast<void *>(reinterpret_cast<HSAuint8 *>(mem) + VRAM_OFFSET);
    mem1 = reinterpret_cast<void *>(reinterpret_cast<HSAuint8 *>(mem) + VRAM_OFFSET + sizeof(HSAuint64));
    PM4Queue queue;
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    queue.PlaceAndSubmitPacket(PM4WriteDataPacket((unsigned int *)mem0,
                                                  data0[0], data0[1]));
    queue.PlaceAndSubmitPacket(PM4WriteDataPacket((unsigned int *)mem1,
                                                  data1[0], data1[1]));
    queue.PlaceAndSubmitPacket(PM4ReleaseMemoryPacket(m_FamilyId, true, 0, 0));
    queue.Wait4PacketConsumption();

    /* Allow any process to trace this one. If kernel is built without
     * Yama, this is not needed, and this call will fail.
     */
#ifdef PR_SET_PTRACER
    prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif

    // Find out my pid so the child can trace it
    pid_t tracePid = getpid();

    // Fork the child
    pid_t childPid = fork();
    ASSERT_GE(childPid, 0);
    if (childPid == 0) {
        int traceStatus;
        int err = 0, r;

        /* Child process: we catch any exceptions to make sure we detach
         * from the traced process, because terminating without detaching
         * leaves the traced process stopped.
         */
        r = ptrace(PTRACE_ATTACH, tracePid, NULL, NULL);
        if (r) {
            WARN() << "PTRACE_ATTACH failed: " << r << std::endl;
            exit(1);
        }
        try {
            do {
                waitpid(tracePid, &traceStatus, 0);
            } while (!WIFSTOPPED(traceStatus));

            /* Peek the memory */
            errno = 0;
            HSAint64 data0 = ptrace(PTRACE_PEEKDATA, tracePid, mem0, NULL);
            EXPECT_EQ(0, errno);
            EXPECT_EQ(data[0], data0);
            HSAint64 data1 = ptrace(PTRACE_PEEKDATA, tracePid, mem1, NULL);
            EXPECT_EQ(0, errno);
            EXPECT_EQ(data[1], data1);

            /* Swap mem0 and mem1 by poking */
            EXPECT_EQ(0, ptrace(PTRACE_POKEDATA, tracePid, mem0, reinterpret_cast<void *>(data[1])));
            EXPECT_EQ(0, errno);
            EXPECT_EQ(0, ptrace(PTRACE_POKEDATA, tracePid, mem1, reinterpret_cast<void *>(data[0])));
            EXPECT_EQ(0, errno);
        } catch (...) {
            err = 1;
        }
        r = ptrace(PTRACE_DETACH, tracePid, NULL, NULL);
        if (r) {
            WARN() << "PTRACE_DETACH failed: " << r << std::endl;
            exit(1);
        }
        exit(err);
    } else {
        int childStatus;

        // Parent process, just wait for the child to finish
        EXPECT_EQ(childPid, waitpid(childPid, &childStatus, 0));
        EXPECT_NE(0, WIFEXITED(childStatus));
        EXPECT_EQ(0, WEXITSTATUS(childStatus));
    }

    /* Use shader to read back data to check poke results */
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    // dstBuffer is cpu accessible gtt memory
    HsaMemoryBuffer dstBuffer(PAGE_SIZE, defaultGPUNode);

    const char *pScratchCopyDword;
    if (m_FamilyId < FAMILY_AI)
        pScratchCopyDword = gfx8_ScratchCopyDword;
    else if (m_FamilyId < FAMILY_NV)
        pScratchCopyDword = gfx9_ScratchCopyDword;
    else
        pScratchCopyDword = gfx10_ScratchCopyDword;

    m_pIsaGen->CompileShader(pScratchCopyDword, "ScratchCopyDword", isaBuffer);
    Dispatch dispatch0(isaBuffer);
    dispatch0.SetArgs(mem0, dstBuffer.As<void*>());
    dispatch0.Submit(queue);
    dispatch0.Sync();
    EXPECT_EQ(data1[0], dstBuffer.As<unsigned int*>()[0]);

    Dispatch dispatch1(isaBuffer);
    dispatch1.SetArgs(mem1, dstBuffer.As<int*>());
    dispatch1.Submit(queue);
    dispatch1.Sync();
    WaitOnValue(dstBuffer.As<uint32_t *>(), data0[0]);
    EXPECT_EQ(data0[0], dstBuffer.As<unsigned int*>()[0]);

    // Clean up
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(mem));
    EXPECT_SUCCESS(hsaKmtFreeMemory(mem, size));
    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

void CatchSignal(int IntrSignal) {
    LOG() << "Interrupt Signal " << std::dec << IntrSignal
          << " Received" << std::endl;
}

TEST_F(KFDMemoryTest, SignalHandling) {
    TEST_START(TESTPROFILE_RUNALL)

    if (!is_dgpu()) {
        LOG() << "Skipping test: Test not supported on APU." << std::endl;
        return;
    }

    unsigned int *nullPtr = NULL;
    unsigned int* pDb = NULL;
    struct sigaction sa;
    SDMAQueue queue;
    HSAuint64 size, sysMemSize;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    sa.sa_handler = CatchSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    pid_t ParentPid = getpid();
    EXPECT_EQ(0, sigaction(SIGUSR1, &sa, NULL)) << "An error occurred while setting a signal handler";

    sysMemSize = GetSysMemSize();

    /* System (kernel) memory are limited to 3/8th System RAM
     * Try to allocate 1/4th System RAM
     */
    size = (sysMemSize >> 2) & ~(HSAuint64)(PAGE_SIZE - 1);
    m_MemoryFlags.ui32.NoNUMABind = 1;
    ASSERT_SUCCESS(hsaKmtAllocMemory(0 /* system */, size, m_MemoryFlags, reinterpret_cast<void**>(&pDb)));
    // Verify that pDb is not null before it's being used
    EXPECT_NE(nullPtr, pDb) << "hsaKmtAllocMemory returned a null pointer";

    pid_t childPid = fork();
    ASSERT_GE(childPid, 0);
    if (childPid == 0) {
        EXPECT_EQ(0, kill(ParentPid, SIGUSR1));
        exit(0);
    } else {
        LOG() << "Start Memory Mapping..." << std::endl;
        ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(pDb, size, NULL));
        LOG() << "Mapping finished" << std::endl;
        int childStatus;

        // Parent process, just wait for the child to finish
        EXPECT_EQ(childPid, waitpid(childPid, &childStatus, 0));
        EXPECT_NE(0, WIFEXITED(childStatus));
        EXPECT_EQ(0, WEXITSTATUS(childStatus));
    }

    pDb[0] = 0x02020202;
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    queue.PlaceAndSubmitPacket(SDMAWriteDataPacket(queue.GetFamilyId(), pDb, 0x01010101) );
    queue.Wait4PacketConsumption();
    EXPECT_TRUE(WaitOnValue(pDb, 0x01010101));
    EXPECT_SUCCESS(queue.Destroy());

    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(pDb));
    // Release the buffers
    EXPECT_SUCCESS(hsaKmtFreeMemory(pDb, size));

    TEST_END
}

TEST_F(KFDMemoryTest, CheckZeroInitializationSysMem) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    int ret;

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HSAuint64 sysMemSizeMB = GetSysMemSize() >> 20;

    /* Testing system memory */
    HSAuint64 * pDb = NULL;

    HSAuint64 sysBufSizeMB = sysMemSizeMB >> 2;
    HSAuint64 sysBufSize = sysBufSizeMB * 1024 * 1024;

    int count = 5;

    LOG() << "Using " << std::dec << sysBufSizeMB
            << "MB system buffer to test " << std::dec << count
            << " times" << std::endl;

    unsigned int offset = 257;  // a constant offset, should be smaller than 512.
    unsigned int size = sysBufSize / sizeof(*pDb);

    m_MemoryFlags.ui32.NoNUMABind = 1;

    while (count--) {
        ret = hsaKmtAllocMemory(0 /* system */, sysBufSize, m_MemoryFlags,
                                reinterpret_cast<void**>(&pDb));
        if (ret) {
            LOG() << "Failed to allocate system buffer of" << std::dec << sysBufSizeMB
                    << "MB" << std::endl;
            return;
        }

        /* Check the first 64 bits */
        EXPECT_EQ(0, pDb[0]);
        pDb[0] = 1;

        for (HSAuint64 i = offset; i < size;) {
            EXPECT_EQ(0, pDb[i]);
            pDb[i] = i + 1;  // set it to non zero

            i += 4096 / sizeof(*pDb);
        }

        /* check the last 64 bit */
        EXPECT_EQ(0, pDb[size-1]);
        pDb[size-1] = size;

        EXPECT_SUCCESS(hsaKmtFreeMemory(pDb, sysBufSize));
    }

    TEST_END
}

static inline void access(volatile void *sd, int size, int rw) {
    /* Most likely sitting in cache*/
    static struct DUMMY {
        char dummy[1024];
    } dummy;

    while ((size -= sizeof(dummy)) >= 0) {
        if (rw == 0)
            dummy = *(struct DUMMY *)((char*)sd + size);
        else
            *(struct DUMMY *)((char*)sd + size) = dummy;
    }
}

/*
 * On large-bar system, test the visible vram access speed.
 * KFD is not allowed to alloc visible vram on non-largebar system.
 */
TEST_F(KFDMemoryTest, MMBandWidth) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    const unsigned nBufs = 1000; /* measure us, report ns */
    unsigned testIndex, sizeIndex, memType;
    const unsigned nMemTypes = 2;
    const char *memTypeStrings[nMemTypes] = {"SysMem", "VRAM"};
    const unsigned nSizes = 4;
    const unsigned bufSizes[nSizes] = {PAGE_SIZE, PAGE_SIZE*4, PAGE_SIZE*16, PAGE_SIZE*64};
    const unsigned nTests = nSizes * nMemTypes;
    const unsigned tmpBufferSize = PAGE_SIZE*64;
#define _TEST_BUFSIZE(index) (bufSizes[index % nSizes])
#define _TEST_MEMTYPE(index) ((index / nSizes) % nMemTypes)

    void *bufs[nBufs];
    HSAuint64 start;
    unsigned i;
    HSAKMT_STATUS ret;
    HsaMemFlags memFlags = {0};
    HsaMemMapFlags mapFlags = {0};

    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";

    HSAuint64 vramSizeMB = GetVramSize(defaultGPUNode) >> 20;

    LOG() << "Found VRAM of " << std::dec << vramSizeMB << "MB." << std::endl;

    if (!m_NodeInfo.IsGPUNodeLargeBar(defaultGPUNode) || !vramSizeMB) {
        LOG() << "Skipping test: Test requires a large bar GPU." << std::endl;
        return;
    }

    void *tmp = mmap(0,
            tmpBufferSize,
            PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_PRIVATE,
            -1,
            0);
    EXPECT_NE(tmp, MAP_FAILED);
    memset(tmp, 0, tmpBufferSize);

    LOG() << "Test (avg. ns)\t  memcpyRTime memcpyWTime accessRTime accessWTime" << std::endl;
    for (testIndex = 0; testIndex < nTests; testIndex++) {
        unsigned bufSize = _TEST_BUFSIZE(testIndex);
        unsigned memType = _TEST_MEMTYPE(testIndex);
        HSAuint64 mcpRTime, mcpWTime, accessRTime, accessWTime;
        HSAuint32 allocNode;

        if ((testIndex & (nSizes-1)) == 0)
            LOG() << "----------------------------------------------------------------------" << std::endl;

        if (memType == 0) {
            allocNode = 0;
            memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
            memFlags.ui32.HostAccess = 1;
            memFlags.ui32.NonPaged = 0;
            memFlags.ui32.NoNUMABind = 1;
        } else {
            /* Alloc visible vram*/
            allocNode = defaultGPUNode;
            memFlags.ui32.PageSize = HSA_PAGE_SIZE_4KB;
            memFlags.ui32.HostAccess = 1;
            memFlags.ui32.NonPaged = 1;
        }

        for (i = 0; i < nBufs; i++)
            ASSERT_SUCCESS(hsaKmtAllocMemory(allocNode, bufSize, memFlags,
                        &bufs[i]));

        start = GetSystemTickCountInMicroSec();
        for (i = 0; i < nBufs; i++) {
            memcpy(bufs[i], tmp, bufSize);
        }
        mcpWTime = GetSystemTickCountInMicroSec() - start;

        start = GetSystemTickCountInMicroSec();
        for (i = 0; i < nBufs; i++) {
            access(bufs[i], bufSize, 1);
        }
        accessWTime = GetSystemTickCountInMicroSec() - start;

        start = GetSystemTickCountInMicroSec();
        for (i = 0; i < nBufs; i++) {
            memcpy(tmp, bufs[i], bufSize);
        }
        mcpRTime = GetSystemTickCountInMicroSec() - start;

        start = GetSystemTickCountInMicroSec();
        for (i = 0; i < nBufs; i++) {
            access(bufs[i], bufSize, 0);
        }
        accessRTime = GetSystemTickCountInMicroSec() - start;

        for (i = 0; i < nBufs; i++)
            EXPECT_SUCCESS(hsaKmtFreeMemory(bufs[i], bufSize));

        LOG() << std::dec
            << std::right << std::setw(3) << (bufSize >> 10) << "K-"
            << std::left << std::setw(14) << memTypeStrings[memType]
            << std::right
            << std::setw(12) << mcpRTime
            << std::setw(12) << mcpWTime
            << std::setw(12) << accessRTime
            << std::setw(12) << accessWTime
            << std::endl;

#define MMBANDWIDTH_KEY_PREFIX memTypeStrings[memType] << "-" \
                               << (bufSize >> 10) << "K" << "-"
        RECORD(mcpRTime) << MMBANDWIDTH_KEY_PREFIX << "mcpRTime";
        RECORD(mcpWTime) << MMBANDWIDTH_KEY_PREFIX << "mcpWTime";
        RECORD(accessRTime) << MMBANDWIDTH_KEY_PREFIX << "accessRTime";
        RECORD(accessWTime) << MMBANDWIDTH_KEY_PREFIX << "accessWTime";
    }

    munmap(tmp, tmpBufferSize);

    TEST_END
}

/* For the purpose of testing HDP flush from CPU.
 * Use CPU to write to coherent vram and check
 * from shader.
 * Asic before gfx9 doesn't support user space
 * HDP flush so only run on vega10 and after.
 * This should only run on large bar system.
 */
TEST_F(KFDMemoryTest, HostHdpFlush) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    HsaMemFlags memoryFlags = m_MemoryFlags;
    /* buffer[0]: signal; buffer[1]: Input to shader; buffer[2]: Output to
     * shader
     */
    unsigned int *buffer = NULL;
    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    const HsaNodeProperties *pNodeProperties = m_NodeInfo.GetNodeProperties(defaultGPUNode);
    HSAuint32 *mmioBase = NULL;
    unsigned int *nullPtr = NULL;

    if (!pNodeProperties) {
        LOG() << "Failed to get gpu node properties." << std::endl;
        return;
    }

    if (m_FamilyId < FAMILY_AI) {
        LOG() << "Skipping test: Test requires gfx9 and later asics." << std::endl;
        return;
    }
    HSAuint64 vramSizeMB = GetVramSize(defaultGPUNode) >> 20;

    if (!m_NodeInfo.IsGPUNodeLargeBar(defaultGPUNode) || !vramSizeMB) {
        LOG() << "Skipping test: Test requires a large bar GPU." << std::endl;
        return;
    }

    HsaMemoryProperties *memoryProperties = new HsaMemoryProperties[pNodeProperties->NumMemoryBanks];
    EXPECT_SUCCESS(hsaKmtGetNodeMemoryProperties(defaultGPUNode, pNodeProperties->NumMemoryBanks,
                   memoryProperties));
    for (unsigned int bank = 0; bank < pNodeProperties->NumMemoryBanks; bank++) {
        if (memoryProperties[bank].HeapType == HSA_HEAPTYPE_MMIO_REMAP) {
            mmioBase = (unsigned int *)memoryProperties[bank].VirtualBaseAddress;
            break;
        }
    }
    ASSERT_NE(mmioBase, nullPtr) << "mmio base is NULL";

    memoryFlags.ui32.NonPaged = 1;
    memoryFlags.ui32.CoarseGrain = 0;
    ASSERT_SUCCESS(hsaKmtAllocMemory(defaultGPUNode, PAGE_SIZE, memoryFlags,
                   reinterpret_cast<void**>(&buffer)));
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(buffer, PAGE_SIZE, NULL));

    /* Signal is dead from the beginning*/
    buffer[0] = 0xdead;
    buffer[1] = 0xfeeb;
    buffer[2] = 0xfeed;
    /* Submit a shader to poll the signal*/
    PM4Queue queue;
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->CompileShader(gfx9_CopyOnSignal, "CopyOnSignal", isaBuffer);
    Dispatch dispatch0(isaBuffer);
    dispatch0.SetArgs(buffer, NULL);
    dispatch0.Submit(queue);

    buffer[1] = 0xbeef;
    /* Flush HDP */
    mmioBase[KFD_MMIO_REMAP_HDP_MEM_FLUSH_CNTL/4] = 0x1;
    buffer[0] = 0xcafe;

    /* Check test result*/
    dispatch0.Sync();
    mmioBase[KFD_MMIO_REMAP_HDP_MEM_FLUSH_CNTL/4] = 0x1;
    EXPECT_EQ(0xbeef, buffer[2]);

    // Clean up
    EXPECT_SUCCESS(queue.Destroy());
    delete [] memoryProperties;
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(buffer));
    EXPECT_SUCCESS(hsaKmtFreeMemory(buffer, PAGE_SIZE));

    TEST_END
}

/* Test HDP flush from device.
 * Use shader on device 1 to write vram of device 0
 * and flush HDP of device 0. Read vram from device 0
 * and write back to vram to check the result from CPU.
 * Asic before gfx9 doesn't support device HDP flush
 * so only run on vega10 and after.
 * This should only run on system with at least one
 * large bar node (which is used as device 0).
 */
TEST_F(KFDMemoryTest, DeviceHdpFlush) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    HsaMemFlags memoryFlags = m_MemoryFlags;
    /* buffer is physically on device 0.
     * buffer[0]: Use as signaling b/t devices;
     * buffer[1]: Device 1 write to buffer[1] and device 0 read it
     * buffer[2]: Device 0 copy buffer[1] to buffer[2] for CPU to check
     */
    unsigned int *buffer = NULL;
    const HsaNodeProperties *pNodeProperties;
    HSAuint32 *mmioBase = NULL;
    unsigned int *nullPtr = NULL;
    std::vector<HSAuint32> nodes;
    int numPeers;

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    if (gpuNodes.size() < 2) {
        LOG() << "Skipping test: At least two GPUs are required." << std::endl;
        return;
    }

     /* Users can use "--node=gpu1 --dst_node=gpu2" to specify devices */
    if (g_TestDstNodeId != -1 && g_TestNodeId != -1) {
        nodes.push_back(g_TestNodeId);
        nodes.push_back(g_TestDstNodeId);

        if (!m_NodeInfo.IsGPUNodeLargeBar(g_TestNodeId) &&
            !m_NodeInfo.AreGPUNodesXGMI(g_TestNodeId, g_TestDstNodeId)) {
            LOG() << "Skipping test: first GPU specified is not peer-accessible." << std::endl;
            return;
        }

        if (nodes[0] == nodes[1]) {
            LOG() << "Skipping test: Different GPUs must be specified (2 GPUs required)." << std::endl;
            return;
        }
    } else {
        HSAint32 defaultGPU = m_NodeInfo.HsaDefaultGPUNode();
        m_NodeInfo.FindAccessiblePeers(&nodes, defaultGPU, false);
        if (nodes.size() < 2) {
            LOG() << "Skipping test: Test requires at least one large bar GPU." << std::endl;
            LOG() << "               or two GPUs are XGMI connected." << std::endl;
            return;
        }
    }

    pNodeProperties = m_NodeInfo.GetNodeProperties(nodes[0]);
    if (!pNodeProperties) {
        LOG() << "Failed to get gpu node properties." << std::endl;
        return;
    }

    if (m_FamilyId < FAMILY_AI) {
        LOG() << "Skipping test: Test requires gfx9 and later asics." << std::endl;
        return;
    }

    HsaMemoryProperties *memoryProperties = new HsaMemoryProperties[pNodeProperties->NumMemoryBanks];
    EXPECT_SUCCESS(hsaKmtGetNodeMemoryProperties(nodes[0], pNodeProperties->NumMemoryBanks,
                   memoryProperties));
    for (unsigned int bank = 0; bank < pNodeProperties->NumMemoryBanks; bank++) {
        if (memoryProperties[bank].HeapType == HSA_HEAPTYPE_MMIO_REMAP) {
            mmioBase = (unsigned int *)memoryProperties[bank].VirtualBaseAddress;
            break;
        }
    }
    ASSERT_NE(mmioBase, nullPtr) << "mmio base is NULL";

    memoryFlags.ui32.NonPaged = 1;
    memoryFlags.ui32.CoarseGrain = 0;
    ASSERT_SUCCESS(hsaKmtAllocMemory(nodes[0], PAGE_SIZE, memoryFlags,
                   reinterpret_cast<void**>(&buffer)));
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(buffer, PAGE_SIZE, NULL));

    /* Signal is dead from the beginning*/
    buffer[0] = 0xdead;
    buffer[1] = 0xfeeb;
    buffer[2] = 0xfeeb;
    /* Submit shaders*/
    PM4Queue queue;
    ASSERT_SUCCESS(queue.Create(nodes[0]));
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, nodes[0], true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->CompileShader(gfx9_CopyOnSignal, "CopyOnSignal", isaBuffer);
    Dispatch dispatch(isaBuffer);
    dispatch.SetArgs(buffer, NULL);
    dispatch.Submit(queue);

    PM4Queue queue0;
    ASSERT_SUCCESS(queue0.Create(nodes[1]));
    HsaMemoryBuffer isaBuffer0(PAGE_SIZE, nodes[1], true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->CompileShader(gfx9_WriteAndSignal, "WriteAndSignal", isaBuffer0);
    Dispatch dispatch0(isaBuffer0);
    dispatch0.SetArgs(buffer, mmioBase);
    dispatch0.Submit(queue0);

    /* Check test result*/
    dispatch0.Sync();
    dispatch.Sync();
    EXPECT_EQ(0xbeef, buffer[2]);

    // Clean up
    EXPECT_SUCCESS(queue.Destroy());
    EXPECT_SUCCESS(queue0.Destroy());
    delete [] memoryProperties;
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(buffer));
    EXPECT_SUCCESS(hsaKmtFreeMemory(buffer, PAGE_SIZE));

    TEST_END
}

/* Test should only run on Arcturus series which has the new RW mtype
 * Map a local VRAM with RW mtype (coarse grain for upper layer),
 * read it locally to cache it and write with local SDMA, remote devices(
 * CPU or Remote GPU shader connected with PCIe or XGMI),
 * then read again. The second read should get back what SDMA wrote,
 * since the cache should be invalidated on write and second read
 * should go to physical VRAM instead of cache.
 */
TEST_F(KFDMemoryTest, CacheInvalidateOnSdmaWrite) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    HsaMemoryBuffer tmpBuffer(PAGE_SIZE, 0, true /* zero */);
    volatile HSAuint32 *tmp = tmpBuffer.As<volatile HSAuint32 *>();
    const int dwLocation = 100;

    if (m_FamilyId != FAMILY_AR) {
        LOG() << "Skipping test: Test requires arcturus series asics." << std::endl;
        return;
    }

    HsaMemoryBuffer buffer(PAGE_SIZE, defaultGPUNode, false/*zero*/, true/*local*/, false/*exec*/);
    SDMAQueue sdmaQueue;
    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));
    buffer.Fill(0, sdmaQueue, 0, PAGE_SIZE);
    sdmaQueue.PlacePacket(SDMAWriteDataPacket(sdmaQueue.GetFamilyId(), buffer.As<int*>(), 0x5678));

    /* Read buffer from shader to fill cache */
    PM4Queue queue;
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->CompileShader(gfx9_PollMemory, "ReadMemory", isaBuffer);
    Dispatch dispatch(isaBuffer);
    dispatch.SetArgs(buffer.As<int*>(), buffer.As<int*>()+dwLocation);
    dispatch.Submit(queue);

    /* Delay 100ms to make sure shader executed*/
    Delay(100);

    /* SDMA writes to buffer. Shader should get what sdma writes and quits*/
    sdmaQueue.SubmitPacket();
    sdmaQueue.Wait4PacketConsumption();

    /* Check test result*/
    dispatch.Sync();
    EXPECT_EQ(buffer.IsPattern(dwLocation*sizeof(int), 0x5678, sdmaQueue, tmp), true);

    // Clean up
    EXPECT_SUCCESS(queue.Destroy());
    EXPECT_SUCCESS(sdmaQueue.Destroy());

    TEST_END
}

TEST_F(KFDMemoryTest, CacheInvalidateOnCPUWrite) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();

    if (m_FamilyId != FAMILY_AR) {
        LOG() << "Skipping test: Test requires arcturus series asics." << std::endl;
        return;
    }

    if (!m_NodeInfo.IsGPUNodeLargeBar(defaultGPUNode)) {
        LOG() << "Skipping test: Test requires a large bar GPU." << std::endl;
        return;
    }

    int *buffer;
    HsaMemFlags memFlags = {0};
    /* Host accessible vram */
    memFlags.ui32.HostAccess = 1;
    memFlags.ui32.NonPaged = 1;
    memFlags.ui32.CoarseGrain = 1;
    ASSERT_SUCCESS(hsaKmtAllocMemory(defaultGPUNode, PAGE_SIZE, memFlags, reinterpret_cast<void**>(&buffer)));
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(buffer, PAGE_SIZE, NULL));
    *buffer = 0;

    /* Read buffer from shader to fill cache */
    PM4Queue queue;
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->CompileShader(gfx9_PollMemory, "ReadMemory", isaBuffer);
    Dispatch dispatch(isaBuffer);
    dispatch.SetArgs(buffer, buffer+100);
    dispatch.Submit(queue);

    /* Delay 100ms to make sure shader executed*/
    Delay(100);

    /* CPU writes to buffer. Shader should get what CPU writes and quits*/
    *buffer = 0x5678;

    /* Check test result*/
    dispatch.Sync();
    EXPECT_EQ(buffer[100], 0x5678);

    // Clean up
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(buffer));
    EXPECT_SUCCESS(hsaKmtFreeMemory(buffer, PAGE_SIZE));
    EXPECT_SUCCESS(queue.Destroy());

    TEST_END
}

TEST_F(KFDMemoryTest, CacheInvalidateOnRemoteWrite) {
    TEST_REQUIRE_ENV_CAPABILITIES(ENVCAPS_64BITLINUX);
    TEST_START(TESTPROFILE_RUNALL);

    HSAuint32 defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    HsaMemoryBuffer tmpBuffer(PAGE_SIZE, 0, true /* zero */);
    volatile HSAuint32 *tmp = tmpBuffer.As<volatile HSAuint32 *>();
    const int dwLocation = 100;
    const int dwLocation1 = 50;

    if (m_FamilyId != FAMILY_AR) {
        LOG() << "Skipping test: Test requires arcturus series asics." << std::endl;
        return;
    }

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    if (gpuNodes.size() < 2) {
        LOG() << "Skipping test: At least two GPUs are required." << std::endl;
        return;
    }

    HSAuint32 nondefaultNode;
    for (unsigned i = 0; i < gpuNodes.size(); i++) {
        if (gpuNodes.at(i) != defaultGPUNode) {
            nondefaultNode = gpuNodes.at(i);
            break;
        }
    }

    HsaMemoryBuffer buffer(PAGE_SIZE, defaultGPUNode, false/*zero*/, true/*local*/, false/*exec*/);
    buffer.MapMemToNodes(&nondefaultNode, 1);
    SDMAQueue sdmaQueue;
    ASSERT_SUCCESS(sdmaQueue.Create(defaultGPUNode));
    buffer.Fill(0, sdmaQueue, 0, PAGE_SIZE);

    /* Read buffer from shader to fill cache */
    PM4Queue queue;
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->CompileShader(gfx9_PollMemory, "ReadMemory", isaBuffer);
    Dispatch dispatch(isaBuffer);
    dispatch.SetArgs(buffer.As<int*>(), buffer.As<int*>()+dwLocation);
    dispatch.Submit(queue);

    /* Delay 100ms to make sure shader executed*/
    Delay(100);

    /* Using a remote shader to copy data from dwLocation1 to the beginning of the buffer.
     * Local shader should get what remote writes and quits
     */
    PM4Queue queue1;
    ASSERT_SUCCESS(queue1.Create(nondefaultNode));
    buffer.Fill(0x5678, sdmaQueue, dwLocation1*sizeof(int), 4);
    HsaMemoryBuffer isaBuffer1(PAGE_SIZE, nondefaultNode, true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->GetCopyDwordIsa(isaBuffer1);
    Dispatch dispatch1(isaBuffer1);
    dispatch1.SetArgs(buffer.As<int*>()+dwLocation1, buffer.As<int*>());
    dispatch1.Submit(queue1);
    dispatch1.Sync(g_TestTimeOut);

    /* Check test result*/
    dispatch.Sync();
    EXPECT_EQ(buffer.IsPattern(dwLocation*sizeof(int), 0x5678, sdmaQueue, tmp), true);

    // Clean up
    EXPECT_SUCCESS(queue.Destroy());
    EXPECT_SUCCESS(queue1.Destroy());
    EXPECT_SUCCESS(sdmaQueue.Destroy());

    TEST_END
}
