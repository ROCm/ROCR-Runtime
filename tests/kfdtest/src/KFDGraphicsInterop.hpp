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

#include "KFDMemoryTest.hpp"
extern "C" {
#include <amdgpu.h>
}

#ifndef __KFD_GRAPHICS_INTEROP_TEST__H__
#define __KFD_GRAPHICS_INTEROP_TEST__H__

// @class KFDGraphicsInteropTest
// Adds access to graphics device for interoperability testing
class KFDGraphicsInterop :  public KFDMemoryTest
{
public:
    KFDGraphicsInterop(void) {};
    ~KFDGraphicsInterop(void) {};
protected:
    virtual void SetUp();
    virtual void TearDown();

protected:
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
};

#endif
