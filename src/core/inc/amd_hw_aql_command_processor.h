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

namespace amd {
/// @brief Encapsulates HW Aql Command Processor functionality. It
/// provide the interface for things such as Doorbell register, read,
/// write pointers and a buffer.
class HwAqlCommandProcessor : public core::Queue, public core::Signal {
 public:
  // Acquires/releases queue resources and requests HW schedule/deschedule.
  HwAqlCommandProcessor(GpuAgent* agent, size_t req_size_pkts,
                        HSAuint32 node_id, ScratchInfo& scratch,
                        core::HsaEventCallback callback, void* err_data,
                        bool is_kv = false);

  ~HwAqlCommandProcessor();

  /// @brief Indicates if queue is valid or not
  bool IsValid() const { return valid_; }

  /// @brief Queue interfaces
  hsa_status_t Inactivate();

  /// @brief Atomically reads the Read index of with Acquire semantics
  ///
  /// @return uint64_t Value of read index
  uint64_t LoadReadIndexAcquire();

  /// @brief Atomically reads the Read index of with Relaxed semantics
  ///
  /// @return uint64_t Value of read index
  uint64_t LoadReadIndexRelaxed();

  /// @brief Atomically reads the Write index of with Acquire semantics
  ///
  /// @return uint64_t Value of write index
  uint64_t LoadWriteIndexAcquire();

  /// @brief Atomically reads the Write index of with Relaxed semantics
  ///
  /// @return uint64_t Value of write index
  uint64_t LoadWriteIndexRelaxed();

  /// @brief This operation is illegal
  void StoreReadIndexRelaxed(uint64_t value) { assert(false); }

  /// @brief This operation is illegal
  void StoreReadIndexRelease(uint64_t value) { assert(false); }

  /// @brief Atomically writes the Write index of with Relaxed semantics
  ///
  /// @param value New value of write index to update with
  void StoreWriteIndexRelaxed(uint64_t value);

  /// @brief Atomically writes the Write index of with Release semantics
  ///
  /// @param value New value of write index to update with
  void StoreWriteIndexRelease(uint64_t value);

  /// @brief Compares and swaps Write index using Acquire and Release semantics
  ///
  /// @param expected Current value of write index
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t CasWriteIndexAcqRel(uint64_t expected, uint64_t value);

  /// @brief Compares and swaps Write index using Acquire semantics
  ///
  /// @param expected Current value of write index
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t CasWriteIndexAcquire(uint64_t expected, uint64_t value);

  /// @brief Compares and swaps Write index using Relaxed semantics
  ///
  /// @param expected Current value of write index
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t CasWriteIndexRelaxed(uint64_t expected, uint64_t value);

  /// @brief Compares and swaps Write index using Release semantics
  ///
  /// @param expected Current value of write index
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t CasWriteIndexRelease(uint64_t expected, uint64_t value);

  /// @brief Updates the Write index using Acquire and Release semantics
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t AddWriteIndexAcqRel(uint64_t value);

  /// @brief Updates the Write index using Acquire semantics
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t AddWriteIndexAcquire(uint64_t value);

  /// @brief Updates the Write index using Relaxed semantics
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t AddWriteIndexRelaxed(uint64_t value);

  /// @brief Updates the Write index using Release semantics
  ///
  /// @param value Value of new write index
  ///
  /// @return uint64_t Value of write index before the update
  uint64_t AddWriteIndexRelease(uint64_t value);

  /// @brief Set CU Masking
  ///
  /// @param num_cu_mask_count size of mask bit array
  ///
  /// @param cu_mask pointer to cu mask
  ///
  /// @return hsa_status_t
  hsa_status_t SetCUMasking(const uint32_t num_cu_mask_count,
                            const uint32_t* cu_mask);

  /// @brief This operation is illegal
  hsa_signal_value_t LoadRelaxed() {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t LoadAcquire() {
    assert(false);
    return 0;
  }

  /// @brief Update signal value using Relaxed semantics
  void StoreRelaxed(hsa_signal_value_t value);

  /// @brief Update signal value using Release semantics
  void StoreRelease(hsa_signal_value_t value);

  /// @brief This operation is illegal
  hsa_signal_value_t WaitRelaxed(hsa_signal_condition_t condition,
                                 hsa_signal_value_t compare_value,
                                 uint64_t timeout, hsa_wait_state_t wait_hint) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t WaitAcquire(hsa_signal_condition_t condition,
                                 hsa_signal_value_t compare_value,
                                 uint64_t timeout, hsa_wait_state_t wait_hint) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  void AndRelaxed(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void AndAcquire(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void AndRelease(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void AndAcqRel(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void OrRelaxed(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void OrAcquire(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void OrRelease(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void OrAcqRel(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void XorRelaxed(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void XorAcquire(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void XorRelease(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void XorAcqRel(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void AddRelaxed(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void AddAcquire(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void AddRelease(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void AddAcqRel(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void SubRelaxed(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void SubAcquire(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void SubRelease(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  void SubAcqRel(hsa_signal_value_t value) { assert(false); }

  /// @brief This operation is illegal
  hsa_signal_value_t ExchRelaxed(hsa_signal_value_t value) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t ExchAcquire(hsa_signal_value_t value) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t ExchRelease(hsa_signal_value_t value) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t ExchAcqRel(hsa_signal_value_t value) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t CasRelaxed(hsa_signal_value_t expected,
                                hsa_signal_value_t value) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t CasAcquire(hsa_signal_value_t expected,
                                hsa_signal_value_t value) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t CasRelease(hsa_signal_value_t expected,
                                hsa_signal_value_t value) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t CasAcqRel(hsa_signal_value_t expected,
                               hsa_signal_value_t value) {
    assert(false);
    return 0;
  }

  /// @brief This operation is illegal
  hsa_signal_value_t* ValueLocation() const {
    assert(false);
    return NULL;
  }

  /// @brief This operation is illegal
  HsaEvent* EopEvent() {
    assert(false);
    return NULL;
  }

  // 64 byte-aligned allocation and release, for Queue::amd_queue_.
  void* operator new(size_t size);
  void* operator new(size_t size, void* ptr) { return ptr; }
  void operator delete(void* ptr);
  void operator delete(void*, void*) {}

 private:
  uint32_t ComputeRingBufferMinPkts();
  uint32_t ComputeRingBufferMaxPkts();

  // (De)allocates and (de)registers ring_buf_.
  void AllocRegisteredRingBuffer(uint32_t queue_size_pkts);
  void FreeRegisteredRingBuffer();

  static bool DynamicScratchHandler(hsa_signal_value_t error_code, void* arg);

  // AQL packet ring buffer
  void* ring_buf_;

  // Size of ring_buf_ allocation.
  // This may be larger than (amd_queue_.hsa_queue.size * sizeof(AqlPacket)).
  uint32_t ring_buf_alloc_bytes_;

  // Id of the Queue used in communication with thunk
  HSA_QUEUEID queue_id_;

  // Indicates is queue is valid
  bool valid_;

  // Indicates if queue is inactive
  int32_t active_;

  // Cached value of HsaNodeProperties.HSA_CAPABILITY.DoorbellType
  int doorbell_type_;

  // Handle of agent, which queue is attached to
  GpuAgent* agent_;

  hsa_profile_t agent_profile_;

  uint32_t queue_full_workaround_;

  // Handle of scratch memory descriptor
  ScratchInfo queue_scratch_;

  core::HsaEventCallback errors_callback_;

  void* errors_data_;

  // Is KV device queue
  bool is_kv_queue_;

  // Shared event used for queue errors
  static HsaEvent* queue_event_;

  // Queue count - used to ref count queue_event_
  static volatile uint32_t queue_count_;

  // Mutex for queue_event_ manipulation
  static KernelMutex queue_lock_;

  // Forbid copying and moving of this object
  DISALLOW_COPY_AND_ASSIGN(HwAqlCommandProcessor);
};
}  // namespace amd
#endif  // header guard
