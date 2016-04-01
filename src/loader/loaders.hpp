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

#ifndef LOADERS_HPP_
#define LOADERS_HPP_

#include "amd_hsa_loader.hpp"
#include <set>
#include <iostream>

namespace amd {
namespace hsa {
namespace loader {

  class OfflineLoaderContext : public amd::hsa::loader::Context {
  private:
    hsa_isa_t invalid;
    hsa_isa_t gfx700, gfx701, gfx800, gfx801, gfx802, gfx803, gfx804, gfx810;
    hsa_isa_t reserved;
    std::ostream& out;
    typedef std::set<void*> PointerSet;
    PointerSet pointers;

  public:
    OfflineLoaderContext();

    hsa_isa_t IsaFromName(const char *name) override;

    bool IsaSupportedByAgent(hsa_agent_t agent, hsa_isa_t isa) override;

    void* SegmentAlloc(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, size_t size, size_t align, bool zero) override;

    bool SegmentCopy(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* dst, size_t offset, const void* src, size_t size) override;
    
    void SegmentFree(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t size = 0) override;

    void* SegmentAddress(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t offset) override;

    void* SegmentHostAddress(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t offset) override;

    bool SegmentFreeze(amdgpu_hsa_elf_segment_t segment, hsa_agent_t agent, void* seg, size_t size) override;

    bool ImageExtensionSupported();

    hsa_status_t ImageCreate(
      hsa_agent_t agent,
      hsa_access_permission_t image_permission,
      const hsa_ext_image_descriptor_t *image_descriptor,
      const void *image_data,
      hsa_ext_image_t *image_handle);

    hsa_status_t ImageDestroy(
      hsa_agent_t agent, hsa_ext_image_t image_handle);

    hsa_status_t SamplerCreate(
      hsa_agent_t agent,
      const hsa_ext_sampler_descriptor_t *sampler_descriptor,
      hsa_ext_sampler_t *sampler_handle);

    hsa_status_t SamplerDestroy(
      hsa_agent_t agent, hsa_ext_sampler_t sampler_handle);
  };
}
}
}

#endif // LOADERS_HPP_
