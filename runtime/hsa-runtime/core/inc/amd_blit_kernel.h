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

#ifndef HSA_RUNTIME_CORE_INC_AMD_BLIT_KERNEL_H_
#define HSA_RUNTIME_CORE_INC_AMD_BLIT_KERNEL_H_

#include <stdint.h>

#include "core/inc/blit.h"

namespace amd {
class BlitKernel : public core::Blit {
 public:
  explicit BlitKernel();
  virtual ~BlitKernel() override;

  /// @brief Initialize a blit kernel object.
  ///
  /// @param agent Pointer to the agent that will execute the AQL packets.
  ///
  /// @return hsa_status_t
  virtual hsa_status_t Initialize(const core::Agent& agent) override;

  /// @brief Marks the blit kernel object as invalid and uncouples its link with
  /// the underlying AQL kernel queue. Use of the blit object
  /// once it has been release is illegal and any behavior is indeterminate
  ///
  /// @note: The call will block until all AQL packets have been executed.
  ///
  /// @return hsa_status_t
  virtual hsa_status_t Destroy() override;

  /// @brief Submit an AQL packet to perform vector copy. The call is blocking
  /// until the command execution is finished.
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

  /// @brief Submit an AQL packet to perform memory fill. The call is blocking
  /// until the command execution is finished.
  ///
  /// @param ptr Memory address of the fill destination.
  /// @param value Value to be set.
  /// @param count Number of uint32_t element to be set to the value.
  virtual hsa_status_t SubmitLinearFillCommand(void* ptr, uint32_t value,
                                               size_t count) override;

 private:
  union KernelArgs {
    struct __ALIGNED__(16) KernelCopyArgs {
      const void* src;
      void* dst;
      uint64_t size;
      uint32_t use_vector;
    } copy;

    struct __ALIGNED__(16) KernelFillArgs {
      void* ptr;
      uint64_t num;
      uint32_t value;
    } fill;
  };

  /// Reserve a slot in the queue buffer. The call will wait until the queue
  /// buffer has a room.
  uint64_t AcquireWriteIndex(uint32_t num_packet);

  /// Update the queue doorbell register with ::write_index. This
  /// function also serializes concurrent doorbell update to ensure that the
  /// packet processor doesn't get invalid packet.
  void ReleaseWriteIndex(uint64_t write_index, uint32_t num_packet);

  /// Wait until all packets are finished.
  hsa_status_t FenceRelease(uint64_t write_index, uint32_t num_copy_packet,
                            hsa_fence_scope_t fence);

  void PopulateQueue(uint64_t index, uint64_t code_handle, void* args,
                     uint32_t grid_size_x, hsa_signal_t completion_signal);

  KernelArgs* ObtainAsyncKernelCopyArg();

  /// Handles to the vector copy kernel.
  uint64_t copy_code_handle_;

  /// Handles to the vector copy aligned kernel.
  uint64_t copy_aligned_code_handle_;

  /// Handles to the fill memory kernel.
  uint64_t fill_code_handle_;

  /// AQL queue for submitting the vector copy kernel.
  hsa_queue_t* queue_;
  uint32_t queue_bitmask_;

  /// Index to track concurrent kernel launch.
  volatile uint64_t cached_index_;

  /// Pointer to the kernel argument buffer.
  KernelArgs* kernarg_async_;
  uint32_t kernarg_async_mask_;
  volatile uint32_t kernarg_async_counter_;

  /// Completion signal for every kernel dispatched.
  hsa_signal_t completion_signal_;

  /// Lock to synchronize access to kernarg_ and completion_signal_
  std::mutex lock_;

  /// Pointer to memory containing the ISA and argument buffer.
  void* code_arg_buffer_;

  static const size_t kMaxCopyCount;
  static const size_t kMaxFillCount;
  static const uint32_t kGroupSize;
};
}  // namespace amd

#endif  // header guard
