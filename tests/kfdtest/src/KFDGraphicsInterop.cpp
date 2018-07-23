/*
 * Copyright (C) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "KFDGraphicsInterop.hpp"

extern "C" {
#include <xf86drm.h>
#include <amdgpu.h>
#include <amdgpu_drm.h>
}
#include <unistd.h>
#include <sys/param.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include "Dispatch.hpp"
#include "PM4Queue.hpp"

void KFDGraphicsInterop::SetUp() {
    ROUTINE_START

    KFDMemoryTest::SetUp();

    // Try to open and initialize a render node for each device
    for (int i = 0; i < MAX_RENDER_NODES; i++) {
        m_RenderNodes[i].fd = drmOpenRender(i + 128);

        if (m_RenderNodes[i].fd <= 0)
            continue;

        if (amdgpu_device_initialize(m_RenderNodes[i].fd,
                                     &m_RenderNodes[i].major_version,
                                     &m_RenderNodes[i].minor_version,
                                     &m_RenderNodes[i].device_handle) != 0) {
            drmClose(m_RenderNodes[i].fd);
            m_RenderNodes[i].fd = 0;
        }

        // Try to determine the bus-ID from sysfs
        char path[PATH_MAX], link[PATH_MAX];
        snprintf(path, PATH_MAX, "/sys/class/drm/renderD%d", i+128);
        if (readlink(path, link, PATH_MAX) < 0) {
            LOG() << "Failed to read sysfs link " << path
                  << ", can't determine bus ID." << std::endl;
            continue;
        }

        char *state = NULL;
        char *prev = NULL;
        char *tok = strtok_r(link, "/", &state);
        while (tok && strcmp(tok, "drm")) {
            prev = tok;
            tok = strtok_r(NULL, "/", &state);
        }
        unsigned domain, bus, device, func;
        if (!prev ||
            sscanf(prev, "%04x:%02x:%02x.%1x", &domain, &bus, &device, &func)
            != 4) {
            LOG() << "Failed to parse sysfs link " << path
                  << ", can't determine bus ID." << std::endl;
            continue;
        }

        m_RenderNodes[i].bdf = (bus << 8) | (device << 3) | func;
    }

    ROUTINE_END
}

void KFDGraphicsInterop::TearDown() {
    ROUTINE_START

    for (int i = 0; i < MAX_RENDER_NODES; i++) {
        if (m_RenderNodes[i].fd <= 0)
            continue;

        EXPECT_EQ(0, amdgpu_device_deinitialize(m_RenderNodes[i].device_handle));
        EXPECT_EQ(0, drmClose(m_RenderNodes[i].fd));
    }

    KFDMemoryTest::TearDown();

    ROUTINE_END
}

int KFDGraphicsInterop::FindDRMRenderNode(int gpuNode) {
    int rn;
    const HsaNodeProperties *pNodeProps =
                m_NodeInfo.GetNodeProperties(gpuNode);

    for (rn = 0; rn < MAX_RENDER_NODES; rn++) {
        if (m_RenderNodes[rn].fd <= 0)
            continue;
        if (m_RenderNodes[rn].bdf == pNodeProps->LocationId)
            return rn;
    }
    LOG() << "Found no render node corresponding to GPU node "
          << gpuNode << std::endl;
    LOG() << "Check your device permissions" << std::endl;
    return -1;
}

TEST_F(KFDGraphicsInterop, RegisterGraphicsHandle) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    const HsaNodeProperties *pNodeProps =
        m_NodeInfo.GetNodeProperties(defaultGPUNode);
    const HSAuint32 familyID = FamilyIdFromNode(pNodeProps);

    if (isTonga(pNodeProps)) {
        LOG() << "Skipping test: Tonga workaround in thunk returns incorrect allocation size" << std::endl;
        return;
    }

    HSAuint32 nodes[1] = {(uint32_t)defaultGPUNode};

    const char metadata[] = "This data is really meta.";
    unsigned metadata_size = strlen(metadata)+1;
    int rn = FindDRMRenderNode(defaultGPUNode);

    if (rn < 0) {
        LOG() << "Skipping test" << std::endl;
        return;
    }

    // Create the buffer with metadata and get a dmabuf handle to it
    struct amdgpu_bo_alloc_request alloc;
    amdgpu_bo_handle handle;
    if (familyID == FAMILY_CZ || isTonga(pNodeProps))
        alloc.alloc_size = PAGE_SIZE * 8;
    else
        alloc.alloc_size = PAGE_SIZE;
    alloc.phys_alignment = PAGE_SIZE;
    alloc.preferred_heap = AMDGPU_GEM_DOMAIN_VRAM;
    alloc.flags = AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
    ASSERT_EQ(0, amdgpu_bo_alloc(m_RenderNodes[rn].device_handle, &alloc, &handle));

    void *pCpuMap;
    ASSERT_EQ(0, amdgpu_bo_cpu_map(handle, &pCpuMap));
    memset(pCpuMap, 0xaa, PAGE_SIZE);
    ASSERT_EQ(0, amdgpu_bo_cpu_unmap(handle));

    struct amdgpu_bo_metadata meta;
    meta.flags = 0;
    meta.tiling_info = 0;
    meta.size_metadata = metadata_size;
    memcpy(meta.umd_metadata, metadata, metadata_size);
    ASSERT_EQ(0, amdgpu_bo_set_metadata(handle, &meta));

    uint32_t dmabufFd;
    ASSERT_EQ(0, amdgpu_bo_export(handle, amdgpu_bo_handle_type_dma_buf_fd, &dmabufFd));

    // Register it with HSA
    HsaGraphicsResourceInfo info;
    ASSERT_SUCCESS(hsaKmtRegisterGraphicsHandleToNodes(dmabufFd, &info,
                                                       1, nodes));

    // DMA buffer handle and GEM handle are no longer needed, KFD
    // should have taken a reference to the BO
    ASSERT_EQ(0, close(dmabufFd));
    ASSERT_EQ(0, amdgpu_bo_free(handle));

    // Check that buffer size and metadata match
    ASSERT_EQ(info.SizeInBytes, alloc.alloc_size);
    ASSERT_EQ(info.MetadataSizeInBytes, metadata_size);
    ASSERT_EQ(0, strcmp(metadata, (const char *)info.Metadata));

    // Map the buffer
    ASSERT_SUCCESS(hsaKmtMapMemoryToGPU(info.MemoryAddress,
                                        info.SizeInBytes,
                                        NULL));

    // Copy contents to a system memory buffer for comparison
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/, false/*local*/, true/*exec*/);
    m_pIsaGen->GetCopyDwordIsa(isaBuffer);

    HsaMemoryBuffer dstBuffer(PAGE_SIZE, defaultGPUNode, true/*zero*/);

    PM4Queue queue;
    ASSERT_SUCCESS(queue.Create(defaultGPUNode));
    Dispatch dispatch(isaBuffer);

    dispatch.SetArgs(info.MemoryAddress, dstBuffer.As<void*>());
    dispatch.Submit(queue);
    dispatch.Sync(g_TestTimeOut);

    ASSERT_SUCCESS(queue.Destroy());

    ASSERT_EQ(dstBuffer.As<unsigned int *>()[0], 0xaaaaaaaa);

    // Test QueryMem before the cleanup
    HsaPointerInfo ptrInfo;
    EXPECT_SUCCESS(hsaKmtQueryPointerInfo((const void *)info.MemoryAddress, &ptrInfo));
    EXPECT_EQ(ptrInfo.Type, HSA_POINTER_REGISTERED_GRAPHICS);
    EXPECT_EQ(ptrInfo.Node, (HSAuint32)defaultGPUNode);
    EXPECT_EQ(ptrInfo.GPUAddress, (HSAuint64)info.MemoryAddress);
    EXPECT_EQ(ptrInfo.SizeInBytes, alloc.alloc_size);

    // Cleanup
    ASSERT_SUCCESS(hsaKmtUnmapMemoryToGPU(info.MemoryAddress));
    ASSERT_SUCCESS(hsaKmtDeregisterMemory(info.MemoryAddress));

    TEST_END
}

/* Third-party device memory can be registered for GPU access in
 * ROCm stack. Test this feature. Third party device is mimicked
 * in multi-GPU system using Graphics stack (libdrm). CPU accessible
 * device memory is allocated using Graphics stack on gpuNode2 and
 * this memory will be registered on gpuNode1 for GPU access.
 */
TEST_F(KFDGraphicsInterop, RegisterForeignDeviceMem) {
    TEST_START(TESTPROFILE_RUNALL)

    if (!is_dgpu()) {
        LOG() << "Skipping test: Supports only multi-dGPU system" << std::endl;
        return;
    }

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    if (gpuNodes.size() < 2) {
        LOG() << "Skipping test: Need at least two GPUs" << std::endl;
        return;
    }

    /* gpuNode2 must have public memory (large bar) to allocate CPU accessible
     * device memory.
     */
    HSAint32 gpuNode1 = m_NodeInfo.HsaDefaultGPUNode(), gpuNode2 = 0;
    const HsaNodeProperties *pNodeProperties;

    gpuNode2 = m_NodeInfo.FindLargeBarGPUNode();
    if (gpuNode2 < 0) {
        LOG() << "Skipping test: Need at least one large bar GPU" << std::endl;
        return;
    }
    if (gpuNode1 == gpuNode2) {
        for (unsigned i = 0; i < gpuNodes.size(); i++) {
            if (gpuNodes.at(i) != gpuNode2) {
                gpuNode1 = gpuNodes.at(i);
                break;
            }
        }
    }

    const HsaNodeProperties *pNodeProps =
        m_NodeInfo.GetNodeProperties(gpuNode2);
    const HSAuint32 familyID = FamilyIdFromNode(pNodeProps);

    int rn = FindDRMRenderNode(gpuNode2);
    if (rn < 0) {
        LOG() << "Skipping test" << std::endl;
        return;
    }

    // Allocate CPU accessible device memory on gpuNode2
    struct amdgpu_bo_alloc_request alloc;
    amdgpu_bo_handle handle;
    if (familyID == FAMILY_CZ || isTonga(pNodeProps))
        alloc.alloc_size = PAGE_SIZE * 8;
    else
        alloc.alloc_size = PAGE_SIZE;
    alloc.phys_alignment = PAGE_SIZE;
    alloc.preferred_heap = AMDGPU_GEM_DOMAIN_VRAM;
    alloc.flags = AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
    ASSERT_EQ(0, amdgpu_bo_alloc(m_RenderNodes[rn].device_handle, &alloc, &handle));

    void *pCpuMap;
    ASSERT_EQ(0, amdgpu_bo_cpu_map(handle, &pCpuMap));
    memset(pCpuMap, 0xAA, PAGE_SIZE);

    /* Register third-party device memory in KFD. Test GPU access
     * by carrying out a simple copy test
     */
    HsaMemoryBuffer lockDeviceMemory(pCpuMap, PAGE_SIZE);
    HsaMemoryBuffer isaBuffer(PAGE_SIZE, gpuNode1, true/*zero*/, false/*local*/, true/*exec*/);
    HsaMemoryBuffer dstBuffer(PAGE_SIZE, gpuNode1, true/*zero*/);
    PM4Queue queue;
    Dispatch dispatch(isaBuffer);

    m_pIsaGen->GetCopyDwordIsa(isaBuffer);
    ASSERT_SUCCESS(queue.Create(gpuNode1));

    dispatch.SetArgs(lockDeviceMemory.As<void*>(), dstBuffer.As<void*>());
    dispatch.Submit(queue);
    dispatch.Sync(g_TestTimeOut);

    ASSERT_SUCCESS(queue.Destroy());
    ASSERT_EQ(dstBuffer.As<HSAuint32*>()[0], 0xAAAAAAAA);

    ASSERT_EQ(0, amdgpu_bo_cpu_unmap(handle));
    ASSERT_EQ(0, amdgpu_bo_free(handle));

    TEST_END
}
