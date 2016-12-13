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

#include <sstream>
#include <iostream>
#include <cassert>

#include "amdgpu_metadata.hpp"

namespace amd {
namespace hsa {
namespace code {

  template <typename T>
  bool Read(std::istream& in, T& v);

  template<>
  bool Read<uint32_t>(std::istream& in, uint32_t& v) {
    in.read((char *)&v, sizeof(v));
    return (in.tellg() != (std::streampos) -1 ) && !in.eof() && !in.fail() && !in.bad();
  }

  template<>
  bool Read<uint16_t>(std::istream& in, uint16_t& v) {
    in.read((char *)&v, sizeof(v));
    return !in.eof() && !in.fail() && !in.bad();
  }

  template<>
  bool Read<uint8_t>(std::istream& in, uint8_t& v) {
    in.read((char *)&v, sizeof(v));
    return !in.eof() && !in.fail() && !in.bad();
  }

  template<>
  bool Read<std::string>(std::istream& in, std::string& v) {
    uint32_t len;
    if (!Read(in, len)) { return false; }
    v.resize(len);
    if (!in.read(&v[0], len)) { return false; }
    return true;
  }

  template <typename T>
  bool Read3(std::istream& in, T* v) {
    for (size_t i = 0; i < 3; ++i) {
      if (!Read(in, v[i])) { return false; }
    }
    return true;
  }

  template<typename T1, typename T>
  bool ReadConvert(std::istream& in, T& v) {
    T1 v1;
    if (!Read<T1>(in, v1)) { return false; }
    v = static_cast<T>(v1);
    return true;
  }

  template<>
  bool Read<AMDGPU::RuntimeMD::Key>(std::istream& in, AMDGPU::RuntimeMD::Key& v) {
    return ReadConvert<uint8_t>(in, v);
  }

  template<>
  bool Read<AMDGPU::RuntimeMD::KernelArg::Kind>(std::istream& in, AMDGPU::RuntimeMD::KernelArg::Kind& v) {
    return ReadConvert<uint8_t>(in, v);
  }

  template<>
  bool Read<AMDGPU::RuntimeMD::KernelArg::ValueType>(std::istream& in, AMDGPU::RuntimeMD::KernelArg::ValueType& v) {
    return ReadConvert<uint16_t>(in, v);
  }

  template<>
  bool Read<AMDGPU::RuntimeMD::KernelArg::AccessQualifer>(std::istream& in, AMDGPU::RuntimeMD::KernelArg::AccessQualifer& v) {
    return ReadConvert<uint8_t>(in, v);
  }

  template<>
  bool Read<AMDGPU::RuntimeMD::Language>(std::istream& in, AMDGPU::RuntimeMD::Language& v) {
    return ReadConvert<uint8_t>(in, v);
  }

  namespace KernelArg {
    using namespace AMDGPU::RuntimeMD::KernelArg;
    Metadata::Metadata()
    : size(0), align(0), pointeeAlign(0), accQual(None),
      isConst(false), isRestrict(false), isVolatile(false), isPipe(false)
    {}

    static const char* KindToString(Kind kind) {
      switch (kind) {
      case ByValue: return "ByValue";
      case GlobalBuffer: return "GlobalBuffer";
      case DynamicSharedPointer: return "DynamicSharedPointer";
      case Image: return "Image";
      case Sampler: return "Sampler";
      case Pipe: return "Pipe";
      case Queue: return "Queue";
      case HiddenGlobalOffsetX: return "HiddenGlobalOffsetX";
      case HiddenGlobalOffsetY: return "HiddenGlobalOffsetY";
      case HiddenGlobalOffsetZ: return "HiddenGlobalOffsetZ";
      case HiddenPrintfBuffer: return "HiddenPrintfBuffer";
      case HiddenDefaultQueue: return "HiddenDefaultQueue";
      case HiddenCompletionAction: return "HiddenCompletionAction";
      case HiddenNone: return "HiddenNone";
      default: return "<UnknownType>";
      }
    }

    static const char* ValueTypeToString(ValueType valueType) {
      switch (valueType) {
      case Struct: return "Struct";
      case I8: return "I8";
      case U8: return "U8";
      case I16: return "I16";
      case U16: return "U16";
      case F16: return "F16";
      case I32: return "I32";
      case U32: return "U32";
      case F32: return "F32";
      case I64: return "I64";
      case U64: return "U64";
      case F64: return "F64";
      default: return "<UnknownValueType>";
      }
    }

    static const char* AccessQualToString(AccessQualifer accessQual) {
      switch (accessQual) {
      case None: return "None";
      case ReadOnly: return "ReadOnly";
      case WriteOnly: return "WriteOnly";
      case ReadWrite: return "ReadWrite";
      default: return "<UnknownTypeQual>";
      }
    }

    bool Metadata::ReadValue(std::istream& in, AMDGPU::RuntimeMD::Key key) {
      using namespace AMDGPU::RuntimeMD;

      switch (key) {
      case KeyArgSize: return Read(in, size);
      case KeyArgAlign: return Read(in, align);
      case KeyArgTypeName: return Read(in, typeName);
      case KeyArgName: return Read(in, name);
      case KeyArgKind: return Read(in, kind);
      case KeyArgValueType: return Read(in, valueType);
      case KeyArgPointeeAlign: return Read(in, pointeeAlign);
      case KeyArgAddrQual: return Read(in, addrQual);
      case KeyArgAccQual: return Read(in, accQual);
      case KeyArgIsConst: isConst = true; return true;
      case KeyArgIsRestrict: isRestrict = true; return true;
      case KeyArgIsVolatile: isVolatile = true; return true;
      case KeyArgIsPipe: isPipe = true; return true;
      default:
        return false;
      }
    }

    void Metadata::Print(std::ostream& out) {
      out
        << "Kind: " << KindToString(kind);
      if (kind == ByValue) {
        out << "  ValueType:" << ValueTypeToString(valueType);
      }
      if (isConst) { out << "  Const"; }
      if (isRestrict) { out << "  Restrict"; }
      if (isVolatile) { out << "  Volatile"; }
      if (isPipe) { out << "  Pipe"; }
      if (kind == Image || kind == Pipe) {
        out << "  Access: " << AccessQualToString(accQual);
      }
      if (kind == GlobalBuffer || kind == DynamicSharedPointer) {
        out
          << "  Address: " << (unsigned) addrQual;
      }
      out
        << "  Size: " << size
        << "  Align: " << align;
      if (kind == DynamicSharedPointer) {
        out << "  Pointee Align: " << pointeeAlign;
      }
      if (!typeName.empty()) {
        out << "  Type Name: \"" << typeName << "\"";
      }
      if (!name.empty()) {
        out << "  Name: \"" << name << "\"";
      }
    }

  }

  namespace Kernel {
    Metadata::Metadata()
      : mdVersion(UINT8_MAX), mdRevision(UINT8_MAX),
      language((AMDGPU::RuntimeMD::Language) UINT8_MAX), languageVersion(UINT16_MAX),
      hasRequiredWorkgroupSize(false),
      hasWorkgroupSizeHint(false),
      hasVectorTypeHint(false),
      hasKernelIndex(false),
      hasMinWavesPerSIMD(false), hasMaxWavesPerSIMD(false),
      hasFlatWorkgroupSizeLimits(false),
      hasMaxWorkgroupSize(false),
      isNoPartialWorkgroups(false)
    {}

    void Metadata::SetCommon(uint8_t mdVersion, uint8_t mdRevision,
      AMDGPU::RuntimeMD::Language language, uint16_t languageVersion) {
      this->mdVersion = mdVersion;
      this->mdRevision = mdRevision;
      this->language = language;
      this->languageVersion = languageVersion;
    }

    const KernelArg::Metadata& Metadata::GetKernelArgMetadata(size_t index) const {
      assert((index < args.size()) && "kernel argument index too big");
      return args[index];
    }

    bool Metadata::ReadValue(std::istream& in, AMDGPU::RuntimeMD::Key key) {
      using namespace AMDGPU::RuntimeMD;

      KernelArg::Metadata* arg = args.empty() ? nullptr : &args.back();

      switch (key) {
      case KeyKernelName:
        hasName = true;
        return Read(in, name);
      case KeyArgBegin:
        args.resize(args.size() + 1);
        break;
      case KeyArgEnd:
        // Verified in Program::Metadata::Read.
        break;
      case KeyArgSize:
      case KeyArgAlign:
      case KeyArgTypeName:
      case KeyArgName:
      case KeyArgKind:
      case KeyArgValueType:
      case KeyArgPointeeAlign:
      case KeyArgAddrQual:
      case KeyArgAccQual:
      case KeyArgIsConst:
      case KeyArgIsRestrict:
      case KeyArgIsVolatile:
      case KeyArgIsPipe:
        if (!arg) { return false; }
        if (!arg->ReadValue(in, key)) { return false; }
        break;
      case KeyReqdWorkGroupSize:
        hasRequiredWorkgroupSize = true;
        return Read3(in, requiredWorkgroupSize);
      case KeyWorkGroupSizeHint:
        hasWorkgroupSizeHint = true;
        return Read3(in, workgroupSizeHint);
      case KeyVecTypeHint:
        hasVectorTypeHint = true;
        return Read(in, vectorTypeHint);
      case KeyKernelIndex:
        hasKernelIndex = true;
        return Read(in, kernelIndex);
      case KeyMinWavesPerSIMD:
        hasMinWavesPerSIMD = true;
        return Read(in, minWavesPerSimd);
      case KeyMaxWavesPerSIMD:
        hasMaxWavesPerSIMD = true;
        return Read(in, maxWavesPerSimd);
      case KeyFlatWorkGroupSizeLimits:
        hasFlatWorkgroupSizeLimits = true;
        return
          Read(in, minFlatWorkgroupSize) &&
          Read(in, maxFlatWorkgroupSize);
      case KeyMaxWorkGroupSize:
        hasMaxWorkgroupSize = true;
        return Read3(in, maxWorkgroupSize);
      case KeyNoPartialWorkGroups:
        isNoPartialWorkgroups = true;
        return true;
      default:
        return false;
      }
      return true;
    }

    static const char* LanguageToString(AMDGPU::RuntimeMD::Language language) {
      using namespace AMDGPU::RuntimeMD;
      switch (language) {
      case OpenCL_C: return "OpenCL C";
      case HCC: return "HCC";
      case OpenMP: return "OpenMP";
      case OpenCL_CPP: return "OpenCL C++";
      default: return "<Unknown language>";
      }
    }

    void Metadata::Print(std::ostream& out) {
      using namespace metadata_output;

      out << "  Kernel";
      if (HasName()) {
        out << " " << name;
      }
      out <<
        " (" << LanguageToString(language) << ' ' << (int) languageVersion <<
        "), metadata " << (int) mdVersion << '.' << (int) mdRevision << std::endl;
      if (hasRequiredWorkgroupSize) {
        out << "    Required workgroup size: " << dim3(requiredWorkgroupSize) << std::endl;
      }
      if (hasWorkgroupSizeHint) {
        out << "    Workgroup size hint: " << dim3(workgroupSizeHint) << std::endl;
      }
      if (hasVectorTypeHint) {
        out << "    Vector type hint: " << vectorTypeHint << std::endl;
      }
      if (hasKernelIndex) {
        out << "    Kernel iIndex: " << kernelIndex << std::endl;
      }
      if (hasMinWavesPerSIMD) {
        out << "    Min waves per SIMD: " << minWavesPerSimd << std::endl;
      }
      if (hasMaxWavesPerSIMD) {
        out << "    Max waves per SIMD: " << maxWavesPerSimd << std::endl;
      }
      if (hasFlatWorkgroupSizeLimits) {
        out << "    Min flat workgroup size: " << minFlatWorkgroupSize << std::endl;
        out << "    Max flat workgroup size: " << maxFlatWorkgroupSize << std::endl;
      }
      if (isNoPartialWorkgroups) {
        out << "    No partial workgroups" << std::endl;
      }
      out << "    Arguments" << std::endl;
      for (uint32_t i = 0; i < args.size(); ++i) {
        out << "      " << i << ": ";
        args[i].Print(out);
        out << std::endl;
      }
    }
  }

  namespace Program {
    bool Metadata::ReadFrom(std::istream& in) {
      using namespace AMDGPU::RuntimeMD;
      Kernel::Metadata* kernel = nullptr;
      bool arg = false;
      uint8_t mdVersion = UINT8_MAX, mdRevision = UINT8_MAX;
      Language language = (Language) UINT8_MAX; uint16_t languageVersion = UINT16_MAX;
      while (in.tellg() != (std::streampos) -1 && !in.eof()) {
        Key key;
        if (!Read(in, key)) {
          if (in.eof()) { break; }
          return false;
        }
        switch (key) {
        case KeyNull: break; // Ignore
        case KeyMDVersion:
          if (!Read(in, mdRevision) ||
              !Read(in, mdVersion)) {
            return false;
          }
          break;
        case KeyLanguage:
          if (!Read(in, language)) { return false; }
          break;
        case KeyLanguageVersion:
          if (!Read(in, languageVersion)) { return false; }
          break;
        case KeyKernelBegin:
          if (kernel) { return false; }
          kernels.resize(kernels.size() + 1);
          kernel = &kernels.back();
          kernel->SetCommon(mdVersion, mdRevision, language, languageVersion);
          break;
        case KeyKernelEnd:
          if (!kernel) { return false; }
          kernel = nullptr;
          break;
        case KeyArgBegin:
          if (!kernel || arg) { return false; }
          arg = true;
          if (!kernel->ReadValue(in, key)) { return false; }
          break;
        case KeyArgEnd:
          if (!kernel || !arg) { return false; }
          arg = false;
          break;
        case KeyPrintfInfo: {
          std::string formatString;
          if (!Read(in, formatString)) { return false; }
          printfInfo.push_back(formatString);
          break;
        }
        case KeyKernelName:
        case KeyArgSize:
        case KeyArgAlign:
        case KeyArgTypeName:
        case KeyArgName:
        case KeyArgKind:
        case KeyArgValueType:
        case KeyArgPointeeAlign:
        case KeyArgAddrQual:
        case KeyArgAccQual:
        case KeyArgIsConst:
        case KeyArgIsRestrict:
        case KeyArgIsVolatile:
        case KeyArgIsPipe:
        case KeyReqdWorkGroupSize:
        case KeyWorkGroupSizeHint:
        case KeyVecTypeHint:
        case KeyKernelIndex:
        case KeyMinWavesPerSIMD:
        case KeyMaxWavesPerSIMD:
        case KeyFlatWorkGroupSizeLimits:
        case KeyMaxWorkGroupSize:
        case KeyNoPartialWorkGroups:
          if (!kernel) { return false; }
          if (!kernel->ReadValue(in, key)) { return false; }
          break;
        default:
          //out << "Unsupported metadata key: " << key << std::endl;
          return false;
        }
      }
      return true;
    }

    const Kernel::Metadata& Metadata::GetKernelMetadata(size_t index) const {
        assert(kernels.size() && "kernel metadata not found");
        assert((index < kernels.size()) && "kernel index too big");

        return kernels[index];
    }

    size_t Metadata::KernelIndexByName(const std::string& name) const {
        assert(kernels.size() && "kernel metadata not found");

        size_t idx = 0;
        for (auto kernel : kernels) {
            if (kernel.Name().compare(name) == 0) { return idx; }
            idx++;
        }
        return kernels.max_size();
    }

    bool Metadata::ReadFrom(const void* buffer, size_t size) {
      std::istringstream is(std::string(static_cast<const char*>(buffer), size));
      if (!ReadFrom(is)) { return false; }
      return true;
    }

    void Metadata::Print(std::ostream& out) {
      out << "AMDGPU runtime metadata (" << kernels.size() << " kernel";
      if (kernels.size() > 1) out << "s";
      if (printfInfo.size() > 0) {
        out << ", " << printfInfo.size() << " printf info string";
        if (printfInfo.size() > 1) out << "s";
      }
      out << "):" << std::endl;
      for (Kernel::Metadata& kernel : kernels) {
        kernel.Print(out);
      }
      for (auto str : printfInfo) {
        out << "  PrintfInfo \"" << str << "\"" << std::endl;
      }
    }
  }

  namespace metadata_output {
    std::ostream& operator<<(std::ostream& out, const dim3& d) {
      out << "(" << d.data[0] << ", " << d.data[1] << ", " << d.data[2] << ")";
      return out;
    }
  }

}
}
}
