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
#ifndef __KFD_BASE_COMPONENT_TEST__H__
#define __KFD_BASE_COMPONENT_TEST__H__

#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include <sys/param.h>
#include "hsakmt.h"
#include "OSWrapper.hpp"
#include "KFDTestUtil.hpp"

//  @class KFDBaseComponentTest
class KFDBaseComponentTest : public testing::Test {
 public:
    KFDBaseComponentTest(void) { m_MemoryFlags.Value = 0; }
    ~KFDBaseComponentTest(void) {}

    HSAuint64 GetSysMemSize();
    HSAuint64 GetVramSize(int defaultGPUNode);
#define MAX_RENDER_NODES 64
    struct {
        int fd;
        uint32_t major_version;
        uint32_t minor_version;
        amdgpu_device_handle device_handle;
        uint32_t bdf;
    } m_RenderNodes[MAX_RENDER_NODES];

// @brief Finds DRM Render node corresponding to gpuNode
// @return DRM Render Node if successful or -1 on failure
    int FindDRMRenderNode(int gpuNode);

 protected:
    HsaVersionInfo  m_VersionInfo;
    HsaSystemProperties m_SystemProperties;
    unsigned int m_FamilyId;
    unsigned int m_numSdmaEngines;
    unsigned int m_numSdmaQueuesPerEngine;
    HsaMemFlags m_MemoryFlags;
    HsaNodeInfo m_NodeInfo;

    // @brief Executed before the first test that uses KFDOpenCloseKFDTest class and opens KFD.
    static  void SetUpTestCase();
    // @brief Executed after the last test from KFDOpenCloseKFDTest class and closes KFD.
    static  void TearDownTestCase();
    // @brief Executed before every test that uses KFDOpenCloseKFDTest class and sets all common settings for the tests.
    virtual void SetUp();
    // @brief Executed after every test that uses KFDOpenCloseKFDTest class.
    virtual void TearDown();
};

#endif  //  __KFD_BASE_COMPONENT_TEST__H__
