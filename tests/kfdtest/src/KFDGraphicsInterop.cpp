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

#include "Dispatch.hpp"
#include "PM4Queue.hpp"

TEST_F(KFDGraphicsInterop, RegisterGraphicsHandle) {
    TEST_START(TESTPROFILE_RUNALL)

    int defaultGPUNode = m_NodeInfo.HsaDefaultGPUNode();
    ASSERT_GE(defaultGPUNode, 0) << "failed to get default GPU Node";
    const HsaNodeProperties *pNodeProps =
        m_NodeInfo.GetNodeProperties(defaultGPUNode);
    const HSAuint32 familyID = FamilyIdFromNode(pNodeProps);

    if (isTonga(pNodeProps)) {
        LOG() << "Skipping test: Tonga workaround in thunk returns incorrect allocation size." << std::endl;
        return;
    }

    HSAuint32 nodes[1] = {(uint32_t)defaultGPUNode};

    const char metadata[] = "This data is really meta.";
    unsigned metadata_size = strlen(metadata)+1;
    int rn = FindDRMRenderNode(defaultGPUNode);

    if (rn < 0) {
        LOG() << "Skipping test: Could not find render node for default GPU node." << std::endl;
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
    EXPECT_EQ(0, amdgpu_bo_cpu_unmap(handle));

    struct amdgpu_bo_metadata meta;
    meta.flags = 0;
    meta.tiling_info = 0;
    meta.size_metadata = metadata_size;
    memcpy(meta.umd_metadata, metadata, metadata_size);
    EXPECT_EQ(0, amdgpu_bo_set_metadata(handle, &meta));

    uint32_t dmabufFd;
    EXPECT_EQ(0, amdgpu_bo_export(handle, amdgpu_bo_handle_type_dma_buf_fd, &dmabufFd));

    // Register it with HSA
    HsaGraphicsResourceInfo info;
    ASSERT_SUCCESS(hsaKmtRegisterGraphicsHandleToNodes(dmabufFd, &info,
                                                       1, nodes));

    /* DMA buffer handle and GEM handle are no longer needed, KFD
     * should have taken a reference to the BO
     */
    EXPECT_EQ(0, close(dmabufFd));
    EXPECT_EQ(0, amdgpu_bo_free(handle));

    // Check that buffer size and metadata match
    EXPECT_EQ(info.SizeInBytes, alloc.alloc_size);
    EXPECT_EQ(info.MetadataSizeInBytes, metadata_size);
    EXPECT_EQ(0, strcmp(metadata, (const char *)info.Metadata));

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

    EXPECT_SUCCESS(queue.Destroy());

    EXPECT_EQ(dstBuffer.As<unsigned int *>()[0], 0xaaaaaaaa);

    // Test QueryMem before the cleanup
    HsaPointerInfo ptrInfo;
    EXPECT_SUCCESS(hsaKmtQueryPointerInfo((const void *)info.MemoryAddress, &ptrInfo));
    EXPECT_EQ(ptrInfo.Type, HSA_POINTER_REGISTERED_GRAPHICS);
    EXPECT_EQ(ptrInfo.Node, (HSAuint32)defaultGPUNode);
    EXPECT_EQ(ptrInfo.GPUAddress, (HSAuint64)info.MemoryAddress);
    EXPECT_EQ(ptrInfo.SizeInBytes, alloc.alloc_size);
    EXPECT_EQ(ptrInfo.MemFlags.ui32.CoarseGrain, 1);

    // Cleanup
    EXPECT_SUCCESS(hsaKmtUnmapMemoryToGPU(info.MemoryAddress));
    EXPECT_SUCCESS(hsaKmtDeregisterMemory(info.MemoryAddress));

    TEST_END
}

#if 0
/* This test isn't testing things the way we wanted it to. It is flaky and
 * will end up failing if the memory is evicted, which isn't possible for what 
 * it is intended to test. It needs a rework
 */

/* Third-party device memory can be registered for GPU access in
 * ROCm stack. Test this feature. Third party device is mimicked
 * in multi-GPU system using Graphics stack (libdrm). CPU accessible
 * device memory is allocated using Graphics stack on gpuNode2 and
 * this memory will be registered on gpuNode1 for GPU access.
 */
TEST_F(KFDGraphicsInterop, RegisterForeignDeviceMem) {
    TEST_START(TESTPROFILE_RUNALL)

    if (!is_dgpu()) {
        LOG() << "Skipping test: Only supported on multi-dGPU system." << std::endl;
        return;
    }

    const std::vector<int> gpuNodes = m_NodeInfo.GetNodesWithGPU();
    if (gpuNodes.size() < 2) {
        LOG() << "Skipping test: At least two GPUs are required." << std::endl;
        return;
    }

    /* gpuNode2 must have public memory (large bar) to allocate CPU accessible
     * device memory.
     */
    HSAint32 gpuNode1 = m_NodeInfo.HsaDefaultGPUNode(), gpuNode2 = 0;
    const HsaNodeProperties *pNodeProperties;

    gpuNode2 = m_NodeInfo.FindLargeBarGPUNode();
    if (gpuNode2 < 0) {
        LOG() << "Skipping test: At least one large bar GPU is required." << std::endl;
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
        LOG() << "Skipping test: Cound not find render node for 2nd GPU." << std::endl;
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

    EXPECT_SUCCESS(queue.Destroy());
    EXPECT_EQ(dstBuffer.As<HSAuint32*>()[0], 0xAAAAAAAA);

    EXPECT_EQ(0, amdgpu_bo_cpu_unmap(handle));
    EXPECT_EQ(0, amdgpu_bo_free(handle));

    TEST_END
}
#endif
