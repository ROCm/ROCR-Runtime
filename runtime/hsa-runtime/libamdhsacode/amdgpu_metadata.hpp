////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2016, Advanced Micro Devices, Inc. All rights reserved.
//
// Developed by:
//
//                 AMD Research and AMD HSA Software Development
//
//                 Advanced Micro Devices, Inc.
//
//                 www.amd.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef AMDGPU_METADATA_HPP_
#define AMDGPU_METADATA_HPP_

#include <string>
#include <cstdint>
#include <vector>
#include <istream>
#include <ostream>

#undef None
#include "AMDGPURuntimeMetadata.h"

namespace amd {
namespace hsa {
namespace code {

  namespace KernelArg {
    class Metadata {
    private:
      uint32_t size;
      uint32_t align;
      uint32_t pointeeAlign;
      std::string typeName;
      std::string name;
      AMDGPU::RuntimeMD::KernelArg::Kind kind;
      AMDGPU::RuntimeMD::KernelArg::ValueType valueType;
      uint8_t addrQual;
      AMDGPU::RuntimeMD::KernelArg::AccessQualifer accQual;
      bool isConst, isRestrict, isVolatile, isPipe;

    public:
      Metadata();
      uint32_t Size() const { return size; }
      uint32_t Align() const { return align; }
      uint32_t PointeeAlign() const { return pointeeAlign; }
      const std::string& TypeName() const { return typeName; }
      const std::string& Name() const { return name; }
      AMDGPU::RuntimeMD::KernelArg::Kind Kind() const { return kind; }
      AMDGPU::RuntimeMD::KernelArg::ValueType ValueType() const { return valueType; }
      uint8_t AddrQual() const { return addrQual; }
      AMDGPU::RuntimeMD::KernelArg::AccessQualifer AccQual() const { return accQual; }
      bool IsConst() const { return isConst;  }
      bool IsRestrict() const { return isRestrict; }
      bool IsVolatile() const { return isVolatile; }
      bool IsPipe() const { return isPipe; }

      bool ReadValue(std::istream& in, AMDGPU::RuntimeMD::Key key);
      void Print(std::ostream& out);
    };
  }

  namespace Kernel {
    class Metadata {
    private:
      uint8_t mdVersion, mdRevision;
      AMDGPU::RuntimeMD::Language language;
      uint16_t languageVersion;
      std::vector<KernelArg::Metadata> args;

      unsigned hasName : 1;
      unsigned hasRequiredWorkgroupSize : 1;
      unsigned hasWorkgroupSizeHint : 1;
      unsigned hasVectorTypeHint : 1;
      unsigned hasKernelIndex : 1;
      unsigned hasMinWavesPerSIMD : 1, hasMaxWavesPerSIMD : 1;
      unsigned hasFlatWorkgroupSizeLimits : 1;
      unsigned hasMaxWorkgroupSize : 1;
      unsigned isNoPartialWorkgroups : 1;

      std::string name;
      uint32_t requiredWorkgroupSize[3];
      uint32_t workgroupSizeHint[3];
      std::string vectorTypeHint;

      uint32_t kernelIndex;
      uint32_t numSgprs, numVgprs;
      uint32_t minWavesPerSimd, maxWavesPerSimd;
      uint32_t minFlatWorkgroupSize, maxFlatWorkgroupSize;
      uint32_t maxWorkgroupSize[3];

    public:
      Metadata();

      bool HasName() const { return hasName; }
      bool HasRequiredWorkgroupSize() const { return hasRequiredWorkgroupSize; }
      bool HasWorkgroupSizeHint() const { return hasWorkgroupSizeHint; }
      bool HasVecTypeHint() const { return hasVectorTypeHint; }
      bool HasKernelIndex() const { return hasKernelIndex; }
      bool HasMinWavesPerSIMD() const { return hasMinWavesPerSIMD; }
      bool HasMaxWavesPerSIMD() const { return hasMaxWavesPerSIMD; }
      bool HasFlatWorkgroupSizeLimits() const { return hasFlatWorkgroupSizeLimits; }
      bool HasMaxWorkgroupSize() const { return hasMaxWorkgroupSize; }

      size_t KernelArgCount() const { return args.size(); }
      const KernelArg::Metadata& GetKernelArgMetadata(size_t index) const;

      const std::string& Name() const { return name; }
      const uint32_t* RequiredWorkgroupSize() const { return hasRequiredWorkgroupSize ? requiredWorkgroupSize : nullptr; }
      const uint32_t* WorkgroupSizeHint() const { return hasWorkgroupSizeHint ? workgroupSizeHint : nullptr; }
      const std::string& VecTypeHint() const { return vectorTypeHint; }
      uint32_t KernelIndex() const { return hasKernelIndex ? kernelIndex : UINT32_MAX; }
      uint32_t MinWavesPerSIMD() const { return hasMinWavesPerSIMD ? minWavesPerSimd : UINT32_MAX; }
      uint32_t MaxWavesPerSIMD() const { return hasMaxWavesPerSIMD ? maxWavesPerSimd : UINT32_MAX; }
      uint32_t MinFlatWorkgroupSize() const { return hasFlatWorkgroupSizeLimits ? minFlatWorkgroupSize : UINT32_MAX; }
      uint32_t MaxFlatWorkgroupSize() const { return hasFlatWorkgroupSizeLimits ? maxFlatWorkgroupSize : UINT32_MAX; }
      const uint32_t* MaxWorkgroupSize() const { return hasMaxWorkgroupSize ? maxWorkgroupSize : 0; }
      bool IsNoPartialWorkgroups() const { return isNoPartialWorkgroups; }

      void SetCommon(uint8_t mdVersion, uint8_t mdRevision, AMDGPU::RuntimeMD::Language language, uint16_t languageVersion);
      bool ReadValue(std::istream& in, AMDGPU::RuntimeMD::Key key);
      void Print(std::ostream& out);
    };
  }

  namespace Program {
    class Metadata {
    private:
      uint16_t version;
      std::vector<Kernel::Metadata> kernels;
      std::vector<std::string> printfInfo;

    public:
      size_t KernelCount() const { return kernels.size(); }
      const Kernel::Metadata& GetKernelMetadata(size_t index) const;
      size_t KernelIndexByName(const std::string& name) const;
      const std::vector<std::string>& PrintfInfo() const { return printfInfo; }

      bool ReadFrom(std::istream& in);
      bool ReadFrom(const void* buffer, size_t size);
      void Print(std::ostream& out);
    };
  }

  namespace metadata_output {

    struct dim3 {
      uint32_t* data;

      dim3(uint32_t* data_)
        : data(data_) {}
    };

    std::ostream& operator<<(std::ostream& out, const dim3& d);
  }

}
}
}

#endif // AMDGPU_METADATA_HPP_
