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

#ifndef __GOOGLETEST_EXTENSION__H__
#define __GOOGLETEST_EXTENSION__H__

#include <gtest/gtest.h>
#include "hsakmt.h"
#include "KFDTestFlags.hpp"

enum LOGTYPE {
    LOGTYPE_INFO,            // msg header in green
    LOGTYPE_WARNING    // msg header in yellow
};

class KFDLog{};
std::ostream& operator << (KFDLog log ,LOGTYPE level);

// @brief  log additional details, to be displayed in the same format as other google test outputs
// currently not supported by google test
// should be used like cout: LOG() << "message" << value << std::endl;
#define LOG()      KFDLog() << LOGTYPE_INFO
#define WARN()     KFDLog() << LOGTYPE_WARNING

// all test MUST be in a try catch since google test flag to throw exception on any fatal fail is on
#define TEST_START(testProfile)   if (Ok2Run(testProfile)) try {
#define TEST_END       } catch (...) {}

// used to wrape setup and teardown functions, anything that is build-in gtest  and is not a test
#define ROUTINE_START   try {
#define ROUTINE_END       }catch(...) {}

#define TEST_REQUIRE_ENV_CAPABILITIES(envCaps)          if (!TestReqEnvCaps(envCaps))  return;
#define TEST_REQUIRE_NO_ENV_CAPABILITIES(envCaps)  if (!TestReqNoEnvCaps(envCaps))  return;

#define ASSERT_SUCCESS(_val) ASSERT_EQ(HSAKMT_STATUS_SUCCESS, (_val))
#define EXPECT_SUCCESS(_val) EXPECT_EQ(HSAKMT_STATUS_SUCCESS, (_val))

#define ASSERT_NOTNULL(_val) ASSERT_NE((void *)NULL, _val)
#define EXPECT_NOTNULL(_val) EXPECT_NE((void *)NULL, _val)

// @brief  determines if its ok to run a test given input flags
bool Ok2Run(unsigned int testProfile);

// @brief  checks if all HW capabilities needed for a test to run exist
bool TestReqEnvCaps(unsigned int hwCaps);

// @brief  checks if all HW capabilities that prevents a test from running are non existing
bool TestReqNoEnvCaps(unsigned int hwCaps);

#endif
