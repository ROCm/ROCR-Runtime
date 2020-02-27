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

#ifndef HSA_RUNTIME_CORE_INC_AMD_HW_AQL_COMMAND_PROCESSOR_H_
#define HSA_RUNTIME_CORE_INC_AMD_HW_AQL_COMMAND_PROCESSOR_H_

#include "core/inc/runtime.h"
#include "core/inc/signal.h"
#include "core/inc/queue.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/util/locks.h"

namespace amd {
/// @brief Encapsulates HW Aql Command Processor functionality. It
/// provide the interface for things such as Doorbell register, read,
/// write pointers and a buffer.
class AqlQueue : public core::Queue, private core::LocalSignal, public core::DoorbellSignal {
 public:
  static __forceinline bool IsType(core::Signal* signal) {
    return signal->IsType(&rtti_id_);
  }

  static __forceinline bool IsType(core::Queue* queue) { return queue->IsType(&rtti_id_); }

  // Acquires/releases queue resources and requests HW schedule/deschedule.
  AqlQueue(GpuAgent* agent, size_t req_size_pkts, HSAuint32 node_id,
           ScratchInfo& scratch, core::HsaEventCallback callback,
           void* err_data, bool is_kv = false);

  ~AqlQueue();

  /// @brief Queue interfaces
  hsa_status_t Inactivate() override;

  /// @brief Change the scheduling priority of the queue
  hsa_status_t SetPriority(HSA_QUEUE_PRIORITY priority) override;

  /// @brief Destroy ref counted queue
  void Destroy() override;

  /// @brief Atomically reads the Read index of with Acquire semantics
  ///
  /// @return uint64_t Value of read index
  uint64_t LoadReadIndexAcquire() override;

  /// @brief Atomically reads the Read index of with Relaxed semantics
  ///
  /// @return uint64_t Value of read index
  uint64_t LoadReadIndexRelaxed() override;

  /// @brief Atomically reads the Write index of with Acquire semantics
  ///
  /// @return uint64_t Value of write index
  uint64_t LoadWriteIndexAcquire() override;

  /// @brief Atomically reads the Write index of with Relaxed semantics
  ///
  /// @return uint64_t Value of write index
  uint64_t LoadWriteIndexRelaxed() override;

  /// @brief This operation is illegal
  void StoreReadIndexRelaxed(uint64_t value) override { assert(false); }

  /// @brief This operation is illegal
  void StoreReadIndexRelease(uint64_t value) override { assert(false); }

  /// @brief Atomically writes the Write index of with Relaxed semantics
  ///
  /// @param value New value of write index to update with
  void StoreWriteIndexRelaxed(uint64_t value) override;

  /// @brief Atomically writes the Write index of with Release semantics
  ///
  /// @param value New value of write index to update with
  void StoreWriteIndexRelease(uint64_t value) override;

  /// @brief Compares and swaps Write index using Acquire and Release semantics
  ///
  /// @param expected Current value of write index
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t CasWriteIndexAcqRel(uint64_t expected, uint64_t value) override;

  /// @brief Compares and swaps Write index using Acquire semantics
  ///
  /// @param expected Current value of write index
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t CasWriteIndexAcquire(uint64_t expected, uint64_t value) override;

  /// @brief Compares and swaps Write index using Relaxed semantics
  ///
  /// @param expected Current value of write index
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t CasWriteIndexRelaxed(uint64_t expected, uint64_t value) override;

  /// @brief Compares and swaps Write index using Release semantics
  ///
  /// @param expected Current value of write index
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t CasWriteIndexRelease(uint64_t expected, uint64_t value) override;

  /// @brief Updates the Write index using Acquire and Release semantics
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t AddWriteIndexAcqRel(uint64_t value) override;

  /// @brief Updates the Write index using Acquire semantics
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t AddWriteIndexAcquire(uint64_t value) override;

  /// @brief Updates the Write index using Relaxed semantics
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t AddWriteIndexRelaxed(uint64_t value) override;

  /// @brief Updates the Write index using Release semantics
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t AddWriteIndexRelease(uint64_t value) override;

  /// @brief Set CU Masking
  ///
  /// @param num_cu_mask_count size of mask bit array
  ///
  /// @param cu_mask pointer to cu mask
  ///
  /// @return hsa_status_t
  hsa_status_t SetCUMasking(const uint32_t num_cu_mask_count, const uint32_t* cu_mask) override;

  // @brief Submits a block of PM4 and waits until it has been executed.
  void ExecutePM4(uint32_t* cmd_data, size_t cmd_size_b) override;

  /// @brief Update signal value using Relaxed semantics
  void StoreRelaxed(hsa_signal_value_t value) override;

  /// @brief Update signal value using Release semantics
  void StoreRelease(hsa_signal_value_t value) override;

 protected:
  bool _IsA(Queue::rtti_t id) const override { return id == &rtti_id_; }

 private:
  uint32_t ComputeRingBufferMinPkts();
  uint32_t ComputeRingBufferMaxPkts();

  // (De)allocates and (de)registers ring_buf_.
  void AllocRegisteredRingBuffer(uint32_t queue_size_pkts);
  void FreeRegisteredRingBuffer();

  /// @brief Abstracts the file handle use for double mapping queues.
  void CloseRingBufferFD(const char* ring_buf_shm_path, int fd) const;
  int CreateRingBufferFD(const char* ring_buf_shm_path, uint32_t ring_buf_phys_size_bytes) const;

  /// @brief Define the Scratch Buffer Descriptor and related parameters
  /// that enable kernel access scratch memory
  void InitScratchSRD();

  /// @brief Halt the queue without destroying it or fencing memory.
  void Suspend();

  /// @brief Handler for hardware queue events.
  static bool DynamicScratchHandler(hsa_signal_value_t error_code, void* arg);

  // AQL packet ring buffer
  void* ring_buf_;

  // Size of ring_buf_ allocation.
  // This may be larger than (amd_queue_.hsa_queue.size * sizeof(AqlPacket)).
  uint32_t ring_buf_alloc_bytes_;

  // Id of the Queue used in communication with thunk
  HSA_QUEUEID queue_id_;

  // Indicates if queue is active
  std::atomic<bool> active_;

  // Cached value of HsaNodeProperties.HSA_CAPABILITY.DoorbellType
  int doorbell_type_;

  // Handle of agent, which queue is attached to
  GpuAgent* agent_;

  uint32_t queue_full_workaround_;

  // Handle of scratch memory descriptor
  ScratchInfo queue_scratch_;

  AMD::callback_t<core::HsaEventCallback> errors_callback_;

  void* errors_data_;

  // Is KV device queue
  bool is_kv_queue_;

  // GPU-visible indirect buffer holding PM4 commands.
  void* pm4_ib_buf_;
  uint32_t pm4_ib_size_b_;
  KernelMutex pm4_ib_mutex_;

  // Error handler control variable.
  std::atomic<uint32_t> dynamicScratchState;
  enum { ERROR_HANDLER_DONE = 1, ERROR_HANDLER_TERMINATE = 2, ERROR_HANDLER_SCRATCH_RETRY = 4 };

  // Queue currently suspended or scheduled
  bool suspended_;

  // Thunk dispatch and wavefront scheduling priority
  HSA_QUEUE_PRIORITY priority_;

  // Shared event used for queue errors
  static HsaEvent* queue_event_;

  // Queue count - used to ref count queue_event_
  static std::atomic<uint32_t> queue_count_;

  // Mutex for queue_event_ manipulation
  static KernelMutex queue_lock_;

  static int rtti_id_;

  // Forbid copying and moving of this object
  DISALLOW_COPY_AND_ASSIGN(AqlQueue);
};

}  // namespace amd

#endif  // header guard
