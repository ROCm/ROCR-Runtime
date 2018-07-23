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

#ifndef _ISAGENERATOR_GFX8_H_
#define _ISAGENERATOR_GFX8_H_

#include <string>
#include "IsaGenerator.hpp"

class IsaGenerator_Gfx8 : public IsaGenerator {
 public:
    virtual void GetNoopIsa(HsaMemoryBuffer& rBuf);
    virtual void GetCopyDwordIsa(HsaMemoryBuffer& rBuf);
    virtual void GetInfiniteLoopIsa(HsaMemoryBuffer& rBuf);
    virtual void GetAtomicIncIsa(HsaMemoryBuffer& rBuf);

 protected:
    virtual const std::string& GetAsicName();

 private:
    static const std::string ASIC_NAME;

    static const uint32_t NOOP_ISA[];
    static const uint32_t COPY_DWORD_ISA[];
    static const uint32_t INFINITE_LOOP_ISA[];
    static const uint32_t ATOMIC_ADD_ISA[];
};

#endif  // _ISAGENERATOR_GFX72_H_
