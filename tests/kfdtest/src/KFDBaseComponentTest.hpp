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

#include <gtest/gtest.h>
#include "hsakmt.h"
#include "OSWrapper.hpp"
#include "KFDTestUtil.hpp"

#ifndef __KFD_BASE_COMPONENT_TEST__H__
#define __KFD_BASE_COMPONENT_TEST__H__

//  @class KFDBaseComponentTest
class KFDBaseComponentTest : public testing::Test {
 public:
    KFDBaseComponentTest(void) { m_MemoryFlags.Value = 0; }
    ~KFDBaseComponentTest(void) {}

    HSAuint64 GetSysMemSize();
    HSAuint64 GetVramSize(int defaultGPUNode);

 protected:
    HsaVersionInfo  m_VersionInfo;
    HsaSystemProperties m_SystemProperties;
    unsigned int m_FamilyId;
    HsaMemFlags m_MemoryFlags;
    HsaNodeInfo m_NodeInfo;

    // @brief SetUpTestCase function run before the first test that uses KFDOpenCloseKFDTest class fixture, and opens KFD.
    static  void SetUpTestCase();
    // @brief TearDownTestCase function run after the last test from  KFDOpenCloseKFDTest class fixture and calls close KFD.
    static  void TearDownTestCase();
    // @brief SetUp function run before every test that uses KFDOpenCloseKFDTest class fixture, sets all common settings for the tests.
    virtual void SetUp();
    // @brief TearDown function run after every test that uses KFDOpenCloseKFDTest class fixture.
    virtual void TearDown();
};

#endif  //  __KFD_BASE_COMPONENT_TEST__H__
