/*
 * Copyright (C) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __KFD_MULTI_PROCESS_TEST__H__
#define __KFD_MULTI_PROCESS_TEST__H__

#include <string>
#include <vector>
#include "KFDBaseComponentTest.hpp"

// @class KFDMultiProcessTest
// Base class for tests forking multiple child processes
class KFDMultiProcessTest :  public KFDBaseComponentTest {
 public:
    KFDMultiProcessTest(void): m_ChildStatus(HSAKMT_STATUS_ERROR), m_IsParent(true) {}

    ~KFDMultiProcessTest(void) {
        if (!m_IsParent) {
            /* Child process has to exit
             * otherwise gtest will continue other tests
             */
            exit(m_ChildStatus);
        }

        try {
            WaitChildProcesses();
        } catch (...) {}
    }

 protected:
    void ForkChildProcesses(int nprocesses);
    void WaitChildProcesses();

 protected:  // Members
    std::string     m_psName;
    int             m_ProcessIndex;
    std::vector<pid_t> m_ChildPids;
    HSAKMT_STATUS   m_ChildStatus;
    bool            m_IsParent;
};

#endif  // __KFD_MULTI_PROCESS_TEST__H__
