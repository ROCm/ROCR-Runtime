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

#ifndef HSA_RUNTIME_CORE_INC_AMD_BLIT_SDMA_H_
#define HSA_RUNTIME_CORE_INC_AMD_BLIT_SDMA_H_

#include <mutex>
#include <stdint.h>

#include "hsakmt.h"

#include "core/inc/amd_gpu_agent.h"
#include "core/inc/blit.h"
#include "core/inc/runtime.h"
#include "core/inc/signal.h"
#include "core/util/utils.h"

namespace amd {
class BlitSdma : public core::Blit {
 public:
  explicit BlitSdma();

  virtual ~BlitSdma() override;

  /// @brief Initialize a User Mode SDMA Queue object. Input parameters specify
  /// properties of queue being created.
  ///
  /// @param agent Pointer to the agent that will execute the PM4 commands.
  ///
  /// @return hsa_status_t
  virtual hsa_status_t Initialize(const core::Agent& agent) override;

  /// @brief Marks the queue object as invalid and uncouples its link with
  /// the underlying compute device's control block. Use of queue object
  /// once it has been release is illegal and any behavior is indeterminate
  ///
  /// @note: The call will block until all packets have executed.
  ///
  /// @param agent Agent passed to Initialize.
  ///
  /// @return hsa_status_t
  virtual hsa_status_t Destroy(const core::Agent& agent) override;

  /// @brief Submit a linear copy command to the queue buffer.
  ///
  /// @param dst Memory address of the copy destination.
  /// @param src Memory address of the copy source.
  /// @param size Size of the data to be copied.
  virtual hsa_status_t SubmitLinearCopyCommand(void* dst, const void* src,
                                               size_t size) override;

  /// @brief Submit a linear copy command to the the underlying compute device's
  /// control block. The call is non blocking. The memory transfer will start
  /// after all dependent signals are satisfied. After the transfer is
  /// completed, the out signal will be decremented.
  ///
  /// @param dst Memory address of the copy destination.
  /// @param src Memory address of the copy source.
  /// @param size Size of the data to be copied.
  /// @param dep_signals Arrays of dependent signal.
  /// @param out_signal Output signal.
  virtual hsa_status_t SubmitLinearCopyCommand(
      void* dst, const void* src, size_t size,
      std::vector<core::Signal*>& dep_signals,
      core::Signal& out_signal) override;

  /// @brief Submit a linear fill command to the queue buffer
  ///
  /// @param ptr Memory address of the fill destination.
  /// @param value Value to be set.
  /// @param count Number of uint32_t element to be set to the value.
  virtual hsa_status_t SubmitLinearFillCommand(void* ptr, uint32_t value,
                                               size_t count) override;

  virtual hsa_status_t EnableProfiling(bool enable) override;

  static const size_t kQueueSize;

  static const size_t kCopyPacketSize;

 protected:
  /// @brief Acquires the address into queue buffer where a new command
  /// packet of specified size could be written. The address that is
  /// returned is guaranteed to be unique even in a multi-threaded access
  /// scenario. This function is guaranteed to return a pointer for writing
  /// data into the queue buffer.
  ///
  /// @param cmd_size Command packet size in bytes.
  ///
  /// @return pointer into the queue buffer where a PM4 packet of specified size
  /// could be written. NULL if input size is greater than the size of queue
  /// buffer.
  char* AcquireWriteAddress(uint32_t cmd_size);

  void UpdateWriteAndDoorbellRegister(uint32_t current_offset,
                                      uint32_t new_offset);

  /// @brief Updates the Write Register of compute device to the end of
  /// SDMA packet written into queue buffer. The update to Write Register
  /// will be safe under multi-threaded usage scenario. Furthermore, updates
  /// to Write Register are blocking until all prior updates are completed
  /// i.e. if two threads T1 & T2 were to call release, then updates by T2
  /// will block until T1 has completed its update (assumes T1 acquired the
  /// write address first).
  ///
  /// @param cmd_addr pointer into the queue buffer where a PM4 packet was
  /// written.
  ///
  /// @param cmd_size Command packet size in bytes.
  void ReleaseWriteAddress(char* cmd_addr, uint32_t cmd_size);

  /// @brief Writes NO-OP words into queue buffer in case writing a command
  /// causes the queue buffer to wrap.
  ///
  /// @param cmd_size Size in bytes of command causing queue buffer to wrap.
  void WrapQueue(uint32_t cmd_size);

  /// @brief Build fence command
  void BuildFenceCommand(char* fence_command_addr, uint32_t* fence,
                         uint32_t fence_value);

  uint32_t* ObtainFenceObject();

  void WaitFence(uint32_t* fence, uint32_t fence_value);

  void BuildCopyCommand(char* cmd_addr, uint32_t num_copy_command, void* dst,
                        const void* src, size_t size);

  void BuildPollCommand(char* cmd_addr, void* addr, uint32_t reference);

  void BuildAtomicDecrementCommand(char* cmd_addr, void* addr);

  void BuildGetGlobalTimestampCommand(char* cmd_addr, void* write_address);

  // Agent object owning the SDMA engine.
  GpuAgent* agent_;

  /// Indicates size of Queue buffer in bytes.
  uint32_t queue_size_;

  /// Base address of the Queue buffer at construction time.
  char* queue_start_addr_;

  uint32_t* fence_base_addr_;
  uint32_t fence_pool_size_;
  uint32_t fence_pool_mask_;
  volatile uint32_t fence_pool_counter_;

  /// Queue resource descriptor for doorbell, read
  /// and write indices
  HsaQueueResource queue_resource_;

  /// @brief Current address of execution in Queue buffer.
  ///
  /// @note: The value of address is obtained by reading
  /// the value of Write Register of the compute device.
  /// Users should write to the Queue buffer at the current
  /// address, else it will lead to execution error and potentially
  /// a hang.
  ///
  /// @note: The value of Write Register does not always begin
  /// with Zero after a Queue has been created. This needs to be
  /// understood better. This means that current address number of
  /// words of Queue buffer is unavailable for use.
  volatile uint32_t cached_reserve_offset_;
  volatile uint32_t cached_commit_offset_;

  uint32_t linear_copy_command_size_;

  uint32_t fill_command_size_;

  uint32_t fence_command_size_;

  uint32_t poll_command_size_;

  uint32_t atomic_command_size_;

  uint32_t timestamp_command_size_;

  // Max copy size of a single linear copy command packet.
  size_t max_single_linear_copy_size_;

  /// Max total copy size supported by the queue.
  size_t max_total_linear_copy_size_;

  /// Max count of uint32_t of a single fill command packet.
  size_t max_single_fill_size_;

  /// Max total fill count supported by the queue.
  size_t max_total_fill_size_;
};
}  // namespace amd

#endif  // header guard
