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

#include <cstring>
#include <cassert>
#include "loaders.hpp"

namespace amd {
namespace hsa {
namespace loader {

  // Helper function that allocates an aligned memory.
  static inline void*
  alignedMalloc(size_t size, size_t alignment)
  {
  #if defined(_WIN32)
    return ::_aligned_malloc(size, alignment);
  #else
    void * ptr = NULL;
    alignment = (std::max)(alignment, sizeof(void*));
    if (0 == ::posix_memalign(&ptr, alignment, size)) {
      return ptr;
    }
    return NULL;
  #endif
  }

  // Helper function that frees an aligned memory.
  static inline void
  alignedFree(void *ptr)
  {
  #if defined(_WIN32)
    ::_aligned_free(ptr);
  #else
    free(ptr);
  #endif
  }

  OfflineLoaderContext::OfflineLoaderContext()
    : out(std::cout)
  {
    invalid.handle = 0;
    gfx700.handle = 700;
    gfx701.handle = 701;
    gfx800.handle = 800;
    gfx801.handle = 801;
    gfx802.handle = 802;
    gfx803.handle = 803;
    gfx804.handle = 804;
    gfx810.handle = 810;
    gfx900.handle = 900;
    gfx901.handle = 901;
    gfx902.handle = 902;
    gfx903.handle = 903;
  }

  hsa_isa_t OfflineLoaderContext::IsaFromName(const char *name)
  {
    std::string sname(name);
    if (sname == "AMD:AMDGPU:7:0:0") {
      return gfx700;
    } else if (sname == "AMD:AMDGPU:7:0:1") {
      return gfx701;
    } else if (sname == "AMD:AMDGPU:8:0:0") {
      return gfx800;
    } else if (sname == "AMD:AMDGPU:8:0:1") {
      return gfx801;
    } else if (sname == "AMD:AMDGPU:8:0:2") {
      return gfx802;
    } else if (sname == "AMD:AMDGPU:8:0:3") {
      return gfx803;
    } else if (sname == "AMD:AMDGPU:8:0:4") {
      return gfx804;
    } else if (sname == "AMD:AMDGPU:8:1:0") {
      return gfx810;
    } else if (sname == "AMD:AMDGPU:9:0:0") {
      return gfx900;
    } else if (sname == "AMD:AMDGPU:9:0:1") {
      return gfx901;
    } else if (sname == "AMD:AMDGPU:9:0:2") {
      return gfx902;
    } else if (sname == "AMD:AMDGPU:9:0:3") {
      return gfx903;
    }

    assert(0);
    return invalid;
  }

  bool OfflineLoaderContext::IsaSupportedByAgent(hsa_agent_t agent, hsa_isa_t isa)
  {
    return true;
  }

  void* OfflineLoaderContext::SegmentAlloc(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, size_t size, size_t align, bool zero)
  {
    void* ptr = alignedMalloc(size, align);
    if (zero) { memset(ptr, 0, size); }
    out << "SegmentAlloc: " << segment << ": " << "size=" << size << " align=" << align << " zero=" << zero << " result=" << ptr << std::endl;
    pointers.insert(ptr);
    return ptr;
  }

  bool OfflineLoaderContext::SegmentCopy(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* dst, size_t offset, const void* src, size_t size)
  {
    out << "SegmentCopy: " << segment << ": " << "dst=" << dst << " offset=" << offset << " src=" << src << " size=" << size << std::endl;
    if (!dst || !src || dst == src) {
      return false;
    }
    if (0 == size) {
      return true;
    }
    memcpy((char *) dst + offset, src, size);
    return true;
  }

  void OfflineLoaderContext::SegmentFree(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t size)
  {
    out << "SegmentFree: " << segment << ": " << " ptr=" << seg << " size=" << size << std::endl;
    pointers.erase(seg);
    alignedFree(seg);
  }

  void* OfflineLoaderContext::SegmentAddress(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t offset)
  {
      out << "SegmentAddress: " << segment << ": " << " ptr=" << seg << " offset=" << offset << std::endl;
      return (char*) seg + offset;
  }

  void* OfflineLoaderContext::SegmentHostAddress(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t offset)
  {
      out << "SegmentHostAddress: " << segment << ": " << " ptr=" << seg << " offset=" << offset << std::endl;
      return (char*) seg + offset;
  }

  bool OfflineLoaderContext::SegmentFreeze(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t size)
  {
    out << "SegmentFreeze: " << segment << ": " << " ptr=" << seg << " size=" << size << std::endl;
    return true;
  }

  bool OfflineLoaderContext::ImageExtensionSupported()
  {
    return true;
  }

  hsa_status_t OfflineLoaderContext::ImageCreate(
    hsa_agent_t agent,
    hsa_access_permission_t image_permission,
    const hsa_ext_image_descriptor_t *image_descriptor,
    const void *image_data,
    hsa_ext_image_t *image_handle)
  {
    void* ptr = alignedMalloc(256, 8);
    out << "ImageCreate" << ":" <<
      " permission=" << image_permission <<
      " geometry=" << image_descriptor->geometry <<
      " width=" << image_descriptor->width <<
      " height=" << image_descriptor->height <<
      " depth=" << image_descriptor->depth <<
      " array_size=" << image_descriptor->array_size <<
      " channel_type=" << image_descriptor->format.channel_type <<
      " channel_order=" << image_descriptor->format.channel_order<<
      " data=" << image_data <<
      std::endl;
    pointers.insert(ptr);
    image_handle->handle = reinterpret_cast<uint64_t>(ptr);
    return HSA_STATUS_SUCCESS;
  }

  hsa_status_t OfflineLoaderContext::ImageDestroy(
    hsa_agent_t agent, hsa_ext_image_t image_handle)
  {
    void* ptr = reinterpret_cast<void*>(image_handle.handle);
    pointers.erase(ptr);
    alignedFree(ptr);
    return HSA_STATUS_SUCCESS;
  }

  hsa_status_t OfflineLoaderContext::SamplerCreate(
    hsa_agent_t agent,
    const hsa_ext_sampler_descriptor_t *sampler_descriptor,
    hsa_ext_sampler_t *sampler_handle)
  {
    void* ptr = alignedMalloc(256, 8);
    out << "SamplerCreate" << ":" <<
      " coordinate_mode=" << sampler_descriptor->coordinate_mode <<
      " filter_mode=" << sampler_descriptor->filter_mode <<
      " address_mode=" << sampler_descriptor->address_mode <<
      std::endl;
    pointers.insert(ptr);
    sampler_handle->handle = reinterpret_cast<uint64_t>(ptr);
    return HSA_STATUS_SUCCESS;
  }

  hsa_status_t OfflineLoaderContext::SamplerDestroy(
    hsa_agent_t agent, hsa_ext_sampler_t sampler_handle)
  {
    void* ptr = reinterpret_cast<void*>(sampler_handle.handle);
    pointers.erase(ptr);
    alignedFree(ptr);
    return HSA_STATUS_SUCCESS;
  }

}
}
}
