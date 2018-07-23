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

#include "gtest/gtest.h"
#include "KFDTestFlags.hpp"
#include "KFDTestUtil.hpp"
#include "GoogleTestExtension.hpp"
#include "OSWrapper.hpp"

#define KFD_TEST_DEFAULT_TIMEOUT 2000

std::ostream& operator << (std::ostream& out, TESTPROFILE profile) {
    switch (profile) {
    case TESTPROFILE_DEV:
        out << "Developer Test";
        break;
    case TESTPROFILE_PROMO:
        out << "Promotion Test";
        break;
    case TESTPROFILE_RUNALL:
        out << "Full Test";
        break;
    default:
        out << "INVALID";
    };

    return out;
}

unsigned int g_TestRunProfile;
unsigned int g_TestENVCaps;
unsigned int g_TestTimeOut;
int g_TestNodeId;
int g_TestDstNodeId;
bool g_IsChildProcess;
unsigned int g_TestGPUFamilyId;

GTEST_API_ int main(int argc, char **argv) {
    // default values for run parameters
    g_TestRunProfile = TESTPROFILE_RUNALL;
    g_TestENVCaps = ENVCAPS_NOADDEDCAPS | ENVCAPS_64BITLINUX;
    g_TestTimeOut = KFD_TEST_DEFAULT_TIMEOUT;

    // every fatal fail ( = assert that failed ) will throw an exption
    testing::GTEST_FLAG(throw_on_failure) = true;
    testing::InitGoogleTest(&argc, argv);

    CommandLineArguments args;
    memset(&args, 0, sizeof(args));

    bool success = GetCommandLineArguments(argc, argv, args);

    if (success) {
        if ((GetHwCapabilityHWS() || args.HwsEnabled == HWCAP__FORCE_ENABLED) && (args.HwsEnabled != HWCAP__FORCE_DISABLED))
            g_TestENVCaps |= ENVCAPS_HWSCHEDULING;

        g_TestRunProfile = args.TestProfile;
        g_IsChildProcess = args.ChildProcess;

        if ( args.TimeOut > 0 )
            g_TestTimeOut = args.TimeOut;

        // If --node is not specified, then args.NodeId == -1
        g_TestNodeId = args.NodeId;
        g_TestDstNodeId = args.DstNodeId;

        LOG() << "Profile: " << (TESTPROFILE)g_TestRunProfile << std::endl;
        LOG() << "HW capabilities: 0x" << std::hex << g_TestENVCaps << std::endl;

        return RUN_ALL_TESTS();
    }
}
