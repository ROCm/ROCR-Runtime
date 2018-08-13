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

#include <stdlib.h>
#include <stdint.h>
#include <string>

#include "KFDTestFlags.hpp"
#include "hsakmt.h"

#ifndef __OS__WRAPPER__H__
#define __OS__WRAPPER__H__

#ifndef PAGE_SIZE
#define PAGE_SIZE   (1<<12)
#define PAGE_SHIFT  (12)
#endif

enum TEXTCOLOR {
    TEXTCOLOR_WHITE,
    TEXTCOLOR_GREEN,
    TEXTCOLOR_YELLOW
};

enum OS_PRIVILEGE {
    OS_DRIVER_OPERATIONS,
    OS_SUSPEND
};

enum CONFIG_VALUE {
    CONFIG_HWS
};

enum HwCapabilityStatus {
    HWCAP__FORCE_DISABLED,
    HWCAP__DEFAULT,
    HWCAP__FORCE_ENABLED
};

struct CommandLineArguments {
    HwCapabilityStatus HwsEnabled;
    TESTPROFILE TestProfile;
    bool ChildProcess;
    unsigned int TimeOut;
    int NodeId;
    int DstNodeId;
};

// It is either MEM_NONE or the bitwise OR of one or more of the following flags
#define MEM_NONE 0x00
#define MEM_READ 0x01
#define MEM_WRITE 0x02
#define MEM_EXECUTE 0x4



// @brief change console text color
void SetConsoleTextColor(TEXTCOLOR color);
// @params delayCount : delay time in milliseconds
void Delay(int delayCount);
// @brief replacement for windows VirtualAlloc func
void *VirtualAllocMemory(void *address, unsigned int size, int memProtection = MEM_READ | MEM_WRITE);
// @brief replacement for windows FreeVirtual func
bool VirtualFreeMemory(void *address, unsigned int size);
// @brief retrieve the last error number
HSAuint64 GetLastErrorNo();

HSAint64 AtomicInc(volatile HSAint64* pValue);

void MemoryBarrier();

// @brief: runs the selected test case number of times required, each in a separate process
// @params testToRun : can be a specific test testcase like TestCase.TestName or if you want
// to run all tests in a test case: TestCase.* and so on
// @params numOfProcesses : how many processes to run in parallel
// @params runsPerProcess : how many iteration a test should do per process, must be a positive number
bool MultiProcessTest(const char *testToRun, int numOfProcesses, int runsPerProcess = 1);

HSAuint64 GetSystemTickCountInMicroSec();

/**Put the system to S3/S4 power state and bring it back to S0.
@return 'true' on success, 'false' on failure.
*/
bool SuspendAndWakeUp();

void AcquirePrivilege(OS_PRIVILEGE priv);

void DisableKfd();
void EnableKfd();

bool ReadDriverConfigValue(CONFIG_VALUE config, unsigned int& rValue);

bool GetCommandLineArguments(int argc, char **argv, CommandLineArguments& rArgs);

void HWMemoryBarrier();
bool StartThread(unsigned int (*)(void*), void* pParam, uint64_t& threadId);
bool WaitForThread(uint64_t threadId);

#endif  // __OS__WRAPPER__H__
