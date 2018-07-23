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

#ifndef _ISAGENERATOR_H_
#define _ISAGENERATOR_H_

#include "KFDTestUtil.hpp"

/* isa generation class - interface */
class IsaGenerator {

public:
    static IsaGenerator* Create(unsigned int familyId);

    virtual ~IsaGenerator() {}

    virtual void GetNoopIsa(HsaMemoryBuffer& rBuf) = 0;
    virtual void GetCopyDwordIsa(HsaMemoryBuffer& rBuf) = 0;
    virtual void GetInfiniteLoopIsa(HsaMemoryBuffer& rBuf) = 0;
    virtual void GetAtomicIncIsa(HsaMemoryBuffer& rBuf) = 0;
    virtual void GetCwsrTrapHandler(HsaMemoryBuffer& rBuf) {}
    virtual void GetAwTrapHandler(HsaMemoryBuffer& rBuf);

    void CompileShader(const char* shaderCode, const char* shaderName, HsaMemoryBuffer& rBuf);

protected:
    virtual const std::string& GetAsicName() = 0;

private:
    static const std::string ADDRESS_WATCH_SP3;
};

#endif //_ISAGENERATOR_H_
