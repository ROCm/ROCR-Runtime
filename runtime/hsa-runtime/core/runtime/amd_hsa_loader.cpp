////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2015, Advanced Micro Devices, Inc. All rights reserved.
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

#include "core/inc/amd_hsa_loader.hpp"

#include <linux/limits.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

std::string EncodePathname(const char *Pathname) {
  std::ostringstream ss;
  unsigned char c;

  ss.fill('0');
  ss << "file://";

  while ((c = *Pathname++) != '\0') {
    if (isalnum(c) || c == '/' || c == '-' ||
        c == '_' || c == '.' || c == '~') {
      ss << c;
    } else {
      ss << std::uppercase;
      ss << '%' << std::hex << std::setw(2) << static_cast<int>(c);
      ss << std::nouppercase;
    }
  }

  return ss.str();
}

}  // namespace

namespace amd {
namespace hsa {
namespace loader {

std::string CodeObjectReaderWrapper::GetUriFromFile(
    int Fd, size_t Offset, size_t Size) const {
#if !defined(_WIN32) && !defined(_WIN64)
  std::ostringstream ProcFdPath;
  ProcFdPath << "/proc/self/fd/" << Fd;

  char FdPath[PATH_MAX];
  memset(FdPath, 0, PATH_MAX);

  if (readlink(ProcFdPath.str().c_str(), FdPath, PATH_MAX) == -1) {
    return std::string();
  }

  std::ostringstream UriStream;
  UriStream << EncodePathname(FdPath);
  if (!is_complete_file) {
    UriStream << "#offset=" << Offset;
    UriStream << "&size=" << Size;
  }
  return UriStream.str();
#else
  return std::string();
#endif  // !defined(_WIN32) && !defined(_WIN64)
}

std::string CodeObjectReaderWrapper::GetUriFromMemoryBasic(
    const void *Mem, size_t Size) const {
  pid_t PID = getpid();
  std::ostringstream UriStream;
  UriStream << "memory://" << PID
            << "#offset=0x" << std::hex << (uint64_t)Mem << std::dec
            << "&size=" << Size;
  return UriStream.str();
}

std::string CodeObjectReaderWrapper::GetUriFromMemory(
    const void *Mem, size_t Size) const {
#if !defined(_WIN32) && !defined(_WIN64)
  std::ostringstream ProcMapsPath;
  ProcMapsPath << "/proc/self/maps";

  std::ifstream ProcMapsFile;
  ProcMapsFile.open(ProcMapsPath.str(), std::ifstream::in);
  if (!ProcMapsFile.is_open() || !ProcMapsFile.good()) {
    return GetUriFromMemoryBasic(Mem, Size);
  }

  std::string ProcMapsLine;
  while (std::getline(ProcMapsFile, ProcMapsLine)) {
    std::stringstream TokenStream(ProcMapsLine);

    uint64_t LowAddress, HighAddress;
    char Dash;
    TokenStream >> std::hex >> LowAddress >> std::dec
                >> Dash
                >> std::hex >> HighAddress >> std::dec;
    if (Dash != '-') {
      continue;
    }

    uint64_t MyAddress = reinterpret_cast<uint64_t>(Mem);
    if (!(MyAddress >= LowAddress && (MyAddress + Size) <= HighAddress)) {
      continue;
    }

    std::string Perms, Dev, Pathname;
    uint64_t Offset, INode;
    TokenStream >> Perms
                >> std::hex >> Offset >> std::dec
                >> Dev
                >> INode
                >> Pathname;

    if (INode == 0 || Pathname.empty()) {
      return GetUriFromMemoryBasic(Mem, Size);
    }

    uint64_t UriOffset = Offset + MyAddress - LowAddress;

    std::ostringstream UriStream;
    UriStream << EncodePathname(Pathname.c_str());
    UriStream << "#offset=" << UriOffset;
    if (Size) {
      UriStream << "&size=" << Size;
    }
    return UriStream.str();
  }

#endif  // !defined(_WIN32) && !defined(_WIN64)
  return GetUriFromMemoryBasic(Mem, Size);
}

}  // namespace loader
}  // namespace hsa
}  // namespace amd
