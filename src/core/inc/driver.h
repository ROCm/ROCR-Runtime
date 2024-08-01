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

#ifndef HSA_RUNTME_CORE_INC_DRIVER_H_
#define HSA_RUNTME_CORE_INC_DRIVER_H_

#include <string>

#include "core/inc/agent.h"
#include "core/inc/memory_region.h"
#include "inc/hsa.h"

namespace rocr {
namespace core {

using MemFlags = uint32_t;

struct MemProperties {
  MemFlags flags_;
  size_t size_bytes_;
  uint64_t virtual_base_addr_;
};

/// @brief Kernel driver interface.
///
/// @details A class used to provide an interface between the core runtime
/// and agent kernel drivers. It also maintains state associated with active
/// kernel drivers.
class Driver {
 public:
  Driver() = delete;
  Driver(const std::string devnode_name, Agent::DeviceType agent_device_type);
  virtual ~Driver() {}

  /// @brief Open a connection to the driver using name_.
  /// @retval HSA_STATUS_SUCCESS if the driver was opened successfully.
  hsa_status_t Open();
  /// @brief Close a connection to the open driver using fd_.
  /// @retval HSA_STATUS_SUCCESS if the driver was opened successfully.
  hsa_status_t Close();

  virtual hsa_status_t GetMemoryProperties(uint32_t node_id, MemProperties &mprops) const = 0;

  /// @brief Allocate agent-accessible memory (system or agent-local memory).
  ///
  /// @param[out] pointer to newly allocated memory.
  ///
  /// @retval HSA_STATUS_SUCCESS if memory was successfully allocated or
  /// hsa_status_t error code if the memory allocation failed.
  virtual hsa_status_t AllocateMemory(void** mem, size_t size, uint32_t node_id,
                                      MemFlags flags) = 0;

  virtual hsa_status_t FreeMemory(void* mem, uint32_t node_id) = 0;

  virtual hsa_status_t CreateQueue(Queue &queue) = 0;

  virtual hsa_status_t DestroyQueue(Queue &queue) const = 0;

  /// Specify the agent device type this driver is for.
  const Agent::DeviceType agent_device_type_;

 protected:
  const std::string devnode_name_;
  int fd_ = -1;
};

} // namespace core
} // namespace rocr

#endif // header guard
