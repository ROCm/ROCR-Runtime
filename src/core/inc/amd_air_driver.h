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

#ifndef HSA_RUNTME_CORE_DRIVER_INC_AIR_DRIVER_H_
#define HSA_RUNTME_CORE_DRIVER_INC_AIR_DRIVER_H_

#include <unordered_map>

#include "core/inc/driver.h"

namespace rocr {
namespace AMD {

enum AirMemFlags : uint32_t {
  MemFlagsNone = 0,
  MemFlagsHeapTypeDRAM = 1,
  MemFlagsHeapTypeBRAM = (1 << 1)
};

class AirDriver : public core::Driver {
 public:
  AirDriver() = delete;
  AirDriver(const std::string name,
            core::Agent::DeviceType agent_device_type);
  ~AirDriver();
  hsa_status_t GetMemoryProperties(uint32_t node_id, core::MemProperties &mprops) const override;
  hsa_status_t AllocateMemory(void** mem, size_t size, uint32_t node_id,
                              core::MemFlags flags) override;
  hsa_status_t FreeMemory(void *mem, uint32_t node_id) override;
  hsa_status_t CreateQueue(core::Queue& queue) override;
  hsa_status_t DestroyQueue(core::Queue& queue) const override;

 private:
  static constexpr size_t device_dram_heap_size_{8 * 1024 * 1024};
  static constexpr size_t air_page_size_{4096};
  uint64_t* process_doorbells_ = nullptr;
  std::unordered_map<void*, int> mem_allocations_;
};

} // namespace AMD
} // namespace rocr

#endif // header guard
