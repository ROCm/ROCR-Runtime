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

#include <limits>
#include <string>

#include "core/inc/memory_region.h"
#include "inc/hsa.h"
#include "inc/hsa_ext_amd.h"

namespace rocr {
namespace core {

class Queue;

struct DriverVersionInfo {
  uint32_t major;
  uint32_t minor;
};

enum class DriverQuery { GET_DRIVER_VERSION };

enum class DriverType { XDNA = 0, KFD, NUM_DRIVER_TYPES };

/// @brief Kernel driver interface.
///
/// @details A class used to provide an interface between the core runtime
/// and agent kernel drivers. It also maintains state associated with active
/// kernel drivers.
class Driver {
 public:
  Driver() = delete;
  Driver(DriverType kernel_driver_type, std::string devnode_name);
  virtual ~Driver() = default;

  /// @brief Initialize the driver's state after opening.
  virtual hsa_status_t Init() = 0;

  /// @brief Query the kernel-model driver.
  /// @retval HSA_STATUS_SUCCESS if the kernel-model driver query was
  /// successful.
  virtual hsa_status_t QueryKernelModeDriver(DriverQuery query) = 0;

  /// @brief Open a connection to the driver using name_.
  /// @retval HSA_STATUS_SUCCESS if the driver was opened successfully.
  hsa_status_t Open();

  /// @brief Close a connection to the open driver using fd_.
  /// @retval HSA_STATUS_SUCCESS if the driver was opened successfully.
  hsa_status_t Close();

  /// @brief Get driver version information.
  /// @retval DriverVersionInfo containing the driver's version information.
  const DriverVersionInfo &Version() const { return version_; }

  /// @brief Get the properties of a specific agent and initialize the agent
  /// object.
  /// @param agent Agent whose properties we're getting.
  /// @retval HSA_STATUS_SUCCESS if the driver successfully returns the agent's
  ///         properties.
  virtual hsa_status_t GetAgentProperties(Agent &agent) const = 0;

  /// @brief Get the memory properties of a specific node.
  /// @param node_id Node ID of the agent
  /// @param[in, out] mem_region MemoryRegion object whose properties will be
  /// retrieved.
  /// @retval HSA_STATUS_SUCCESS if the driver sucessfully returns the node's
  ///         memory properties.
  virtual hsa_status_t GetMemoryProperties(uint32_t node_id,
                                           MemoryRegion &mem_region) const = 0;

  /// @brief Allocate agent-accessible memory (system or agent-local memory).
  ///
  /// @param[out] pointer to newly allocated memory.
  ///
  /// @retval HSA_STATUS_SUCCESS if memory was successfully allocated or
  /// hsa_status_t error code if the memory allocation failed.
  virtual hsa_status_t AllocateMemory(const MemoryRegion &mem_region,
                                      MemoryRegion::AllocateFlags alloc_flags,
                                      void **mem, size_t size,
                                      uint32_t node_id) = 0;

  virtual hsa_status_t FreeMemory(void *mem, size_t size) = 0;

  virtual hsa_status_t CreateQueue(Queue &queue) const = 0;

  virtual hsa_status_t DestroyQueue(Queue &queue) const = 0;

  /// @brief Configure the hardware context for a queue.
  /// @param[in] queue The queue whose context is being configured.
  /// @param[in] config_type Type for the @p args argument. Tells the driver
  ///            how to interpret the args.
  /// @param[in] args Arguments for configuring the queue's hardware context.
  ///            @p config_type tells how to interpret args.
  virtual hsa_status_t
  ConfigHwCtx(Queue &queue, hsa_amd_queue_hw_ctx_config_param_t config_type,
              void *args) = 0;

  /// Unique identifier for supported kernel-mode drivers.
  const DriverType kernel_driver_type_;

protected:
  DriverVersionInfo version_{std::numeric_limits<uint32_t>::max(),
                             std::numeric_limits<uint32_t>::max()};

  const std::string devnode_name_;
  int fd_ = -1;
};

} // namespace core
} // namespace rocr

#endif // header guard
