////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2023, Advanced Micro Devices, Inc. All rights reserved.
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
#include <unistd.h>
#include <elf.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <fstream>
#include <memory>
#include "core/util/utils.h"
#include "./amd_hsa_code_util.hpp"
#include "hsakmt/hsakmt.h"

constexpr char SNAPSHOT_INFO_ALIGNMENT = 0x8;

namespace rocr {
namespace amd {
namespace coredump {
/* Implementation details */
namespace impl {
class PackageBuilder {
 public:
  PackageBuilder() : st_(std::stringstream::out | std::stringstream::binary) {}
  size_t Size() const { return st_.str().size(); }
  template <typename T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
  void Write(const T& v) {
    st_.write((char*)&v, sizeof(T));
  }
  void Write(const std::vector<std::uint8_t>& v) { st_.write((const char*)v.data(), v.size()); }
  void Write(void* data, uint32_t size) { st_.write((const char*)data, size); }
  bool GetBuffer(void* out) {
    size_t sz = Size();

    if (!sz) return false;
    std::memcpy(out, st_.str().c_str(), sz);
    return true;
  }
  void Print(void* buf, uint64_t size) {
    int i;
    for (i = 0; i < size; i++) debug_print("%02x ", 0xFF & ((uint8_t*)buf)[i]);
    debug_print("\n");
  }
 private:
  std::stringstream st_;
};

enum SegmentType { LOAD, NOTE };
struct SegmentBuilder;

struct SegmentInfo {
  SegmentType stype;
  uint64_t vaddr = 0;
  uint64_t size = 0;
  uint32_t flags = 0;
  SegmentBuilder* builder;
};

using SegmentsInfo = std::vector<SegmentInfo>;
using rocr::amd::hsa::alignUp;
struct SegmentBuilder {
  virtual ~SegmentBuilder() = default;
  /* Find which segments needs to be created.  */
  virtual hsa_status_t Collect(SegmentsInfo& segments) = 0;
  /* Called to read a given SegmentInfo's data.  */
  virtual hsa_status_t Read(void* buf, size_t buf_size, off_t offset) = 0;
};

struct NoteSegmentBuilder : public SegmentBuilder {
  hsa_status_t Collect(SegmentsInfo& segments) override {
    void *runtime_ptr, *agents_ptr = NULL, *queues_ptr = NULL;
    uint32_t runtime_size, agents_size, queue_size, n_entries, entry_size;
    HsaVersionInfo versionInfo = {0};

    if (hsaKmtDbgEnable(&runtime_ptr, &runtime_size)) return HSA_STATUS_ERROR;
    std::unique_ptr<void, decltype(std::free) *> runtime_info(runtime_ptr, std::free);

    if (hsaKmtGetVersion(&versionInfo)) return HSA_STATUS_ERROR;
    /* Note version */
    note_package_builder_.Write<uint64_t>(1);
    /* Store version_major in PT_NOTE package */
    note_package_builder_.Write<uint32_t>(versionInfo.KernelInterfaceMajorVersion);
    /* Store version_minor in PT_NOTE package */
    note_package_builder_.Write<uint32_t>(versionInfo.KernelInterfaceMinorVersion);
    /* Store runtime_info_size in PT_NOTE package */
    note_package_builder_.Write<uint64_t>(runtime_size);

    if (hsaKmtDbgGetDeviceData(&agents_ptr, &n_entries, &entry_size))
       return HSA_STATUS_ERROR;
    agents_size = n_entries * entry_size;
    std::unique_ptr<void, decltype(std::free) *> agents_info(agents_ptr, std::free);
    /* Store n_agents in PT_NOTE package */
    note_package_builder_.Write<uint32_t>(n_entries);
    /* Store agent_info_entry_size in PT_NOTE package */
    note_package_builder_.Write<uint32_t>(entry_size);

    if (hsaKmtDbgGetQueueData(&queues_ptr, &n_entries, &entry_size, true))
       return HSA_STATUS_ERROR;
    queue_size = n_entries * entry_size;
    std::unique_ptr<void, decltype(std::free) *> queues_info(queues_ptr, std::free);
    /* Store n_queues in PT_NOTE package */
    note_package_builder_.Write<uint32_t>(n_entries);
    /* Store queue_info_entry_size in PT_NOTE package */
    note_package_builder_.Write<uint32_t>(entry_size);

    PushInfo(runtime_info.get(), runtime_size);
    PushInfo(agents_info.get(), agents_size);
    PushInfo(queues_info.get(), queue_size);
    if (hsaKmtDbgDisable()) return HSA_STATUS_ERROR;

    /* With note content, package this in the PT_NOTE.  */
    PackageBuilder noteHeaderBuilder;
    noteHeaderBuilder.Write<uint32_t> (7);  /* namesz */
    noteHeaderBuilder.Write<uint32_t> (note_package_builder_.Size());
    noteHeaderBuilder.Write<uint32_t> (NT_AMDGPU_CORE_STATE);  /* type.  */
    noteHeaderBuilder.Write<char[8]> ("AMDGPU\0");

    raw_.resize(noteHeaderBuilder.Size() + note_package_builder_.Size());
    if (!noteHeaderBuilder.GetBuffer(raw_.data()))
      return HSA_STATUS_ERROR;
    if (!note_package_builder_.GetBuffer(&raw_[noteHeaderBuilder.Size()]))
      return HSA_STATUS_ERROR;

    SegmentInfo s;
    s.stype = NOTE;
    s.vaddr = 0;
    s.size = raw_.size();
    s.flags = 0;
    s.builder = this;
    segments.push_back(s);

    return HSA_STATUS_SUCCESS;
  }

  hsa_status_t Read(void* buf, size_t buf_size, off_t offset) override {
    if (offset + buf_size >raw_.size ()) return HSA_STATUS_ERROR;
    memcpy(buf, raw_.data() + offset, buf_size);
    return HSA_STATUS_SUCCESS;
  }

 private:
  PackageBuilder note_package_builder_;
  std::vector<unsigned char> raw_;

  void PushInfo(void *data, uint32_t size) {
    note_package_builder_.Write(data, size);
    size = alignUp(size, SNAPSHOT_INFO_ALIGNMENT) - size;
    for (int i = 0; i < size; i++)
      note_package_builder_.Write<uint8_t>(0);
  }
};

struct LoadSegmentBuilder : public SegmentBuilder {
  LoadSegmentBuilder() : fd_(open("/proc/self/mem", O_RDONLY)) {}

  ~LoadSegmentBuilder() {
    if (fd_ != -1) close(fd_);
  }

  hsa_status_t Collect(SegmentsInfo& segments) override {
    const std::string maps_path = "/proc/self/maps";
    std::ifstream maps(maps_path);
    if (!maps.is_open()) {
      fprintf(stderr, "Could not open '%s'", maps_path.c_str());
      return HSA_STATUS_ERROR;
    }

    std::string line;
    while (std::getline(maps, line)) {
      std::istringstream isl{ line };
      std::string address, perms, offset, dev, inode, path;
      if (!(isl >> address >> perms >> offset >> dev >> inode)) {
        fprintf(stderr, "Failed to parse '%s'", maps_path.c_str());
        return HSA_STATUS_ERROR;
      }

      std::getline(isl >> std::ws, path);

      /* Look for the /dev/dri/renderD* files.  */
      if (path.rfind("/dev/dri/renderD", 0) == 0) {
        uint64_t start, end;
        if (sscanf(address.c_str(), "%lx-%lx", &start, &end) != 2) {
          fprintf(stderr, "Failed to parse '%s'", maps_path.c_str());
          return HSA_STATUS_ERROR;
        }
        uint32_t flags = SHF_ALLOC;
        flags |= (perms.find('w', 0) != std::string::npos) ? SHF_WRITE : 0;
        flags |= (perms.find('x', 0) != std::string::npos) ? SHF_EXECINSTR : 0;
        uint64_t size = end - start;

        debug_print("LOAD 0x%lx size: %ld\n", start, size);
        SegmentInfo s;
        s.stype = LOAD;
        s.vaddr = start;
        s.size = size;
        s.flags = flags;
        s.builder = this;
        segments.push_back(s);
       }
     }
     return HSA_STATUS_SUCCESS;
  }

  hsa_status_t Read(void* buf, size_t buf_size, off_t offset) override {
    if (fd_ == -1) return HSA_STATUS_ERROR;
    if (pread(fd_, buf, buf_size, offset) == -1) {
      perror("Failed to read GPU memory");
      return HSA_STATUS_ERROR;
    }
    return HSA_STATUS_SUCCESS;
  }

 private:
  int fd_ = -1;
};
}   //  namespace impl
}   //  namespace coredump
}   //  namespace amd
}   //  namespace rocr
