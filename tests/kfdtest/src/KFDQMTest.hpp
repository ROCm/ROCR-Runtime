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

#ifndef __KFD_QCM_TEST__H__
#define __KFD_QCM_TEST__H__

#include <gtest/gtest.h>

#include "PM4Queue.hpp"
#include "KFDBaseComponentTest.hpp"
#include "Dispatch.hpp"

class KFDQMTest : public KFDBaseComponentTest {
 public:
    KFDQMTest() {}

    ~KFDQMTest() {}

 protected:
    virtual void SetUp();
    virtual void TearDown();

    void SyncDispatch(const HsaMemoryBuffer& isaBuffer, void* pSrcBuf, void* pDstBuf, int node = -1);
    HSAint64 TimeConsumedwithCUMask(int node, uint32_t *mask, uint32_t mask_count);
    HSAint64 GetAverageTimeConsumedwithCUMask(int node, uint32_t *mask, uint32_t mask_count, int iterations);
 protected:  // Members
    /* Acceptable performance for CU Masking should be within 5% of linearly-predicted performance */
    const double CuVariance = 0.15;
    const double CuNegVariance = 1.0 - CuVariance;
    const double CuPosVariance = 1.0 + CuVariance;
};

#endif  // __KFD_QCM_TEST__H__
