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

#ifndef HSA_RUNTIME_CORE_INC_AMD_HW_AQL_AIE_COMMAND_PROCESSOR_H_
#define HSA_RUNTIME_CORE_INC_AMD_HW_AQL_AIE_COMMAND_PROCESSOR_H_

#include "core/inc/amd_aie_agent.h"
#include "core/inc/queue.h"
#include "core/inc/runtime.h"
#include "core/inc/signal.h"
#include "core/util/locks.h"

namespace rocr {
namespace AMD {

/// @brief Encapsulates HW AIE AQL Command Processor functionality. It
/// provides the interface for things such as doorbells, queue read and
/// write pointers, and a buffer.
class AieAqlQueue : public core::Queue, public core::DoorbellSignal {
 public:
  static __forceinline bool IsType(core::Signal* signal) {
    return signal->IsType(&rtti_id_);
  }

  static __forceinline bool IsType(core::Queue* queue) {
    return queue->IsType(&rtti_id_);
  }

  AieAqlQueue() = delete;
  AieAqlQueue(AieAgent* agent, size_t req_size_pkts, uint32_t node_id);
  ~AieAqlQueue();

  hsa_status_t Inactivate() override;
  hsa_status_t SetPriority(HSA_QUEUE_PRIORITY priority) override;
  void Destroy() override;
  uint64_t LoadReadIndexRelaxed() override;
  uint64_t LoadReadIndexAcquire() override;
  uint64_t LoadWriteIndexRelaxed() override;
  uint64_t LoadWriteIndexAcquire() override;
  void StoreReadIndexRelaxed(uint64_t value) override { assert(false); }
  void StoreReadIndexRelease(uint64_t value) override { assert(false); }
  void StoreWriteIndexRelaxed(uint64_t value) override;
  void StoreWriteIndexRelease(uint64_t value) override;
  uint64_t CasWriteIndexRelaxed(uint64_t expected, uint64_t value) override;
  uint64_t CasWriteIndexAcquire(uint64_t expected, uint64_t value) override;
  uint64_t CasWriteIndexRelease(uint64_t expected, uint64_t value) override;
  uint64_t CasWriteIndexAcqRel(uint64_t expected, uint64_t value) override;
  uint64_t AddWriteIndexRelaxed(uint64_t value) override;
  uint64_t AddWriteIndexAcquire(uint64_t value) override;
  uint64_t AddWriteIndexRelease(uint64_t value) override;
  uint64_t AddWriteIndexAcqRel(uint64_t value) override;
  void StoreRelaxed(hsa_signal_value_t value) override;
  void StoreRelease(hsa_signal_value_t value) override;

  // GPU-specific queue functions are unsupported.
  hsa_status_t GetCUMasking(uint32_t num_cu_mask_count,
                            uint32_t* cu_mask) override;
  hsa_status_t SetCUMasking(uint32_t num_cu_mask_count,
                            const uint32_t* cu_mask) override;
  void ExecutePM4(uint32_t* cmd_data, size_t cmd_size_b) override;

  core::SharedQueue *shared_queue_;
  core::SharedSignal *shared_signal_;
  /// ID of the queue used in communication with the AMD AIR driver.
  uint32_t queue_id_;
  /// ID of the doorbell used in communication with the AMD AIR driver.
  uint32_t doorbell_id_;
  /// Pointer to the hardware doorbell for this queue.
  uint64_t *hardware_doorbell_ptr_;
  /// ID of AIE device on which this queue has been mapped.
  uint32_t node_id_;
  /// Queue size in bytes.
  uint32_t queue_size_bytes_;
  /// Maintain a heap in on-device DRAM and associate it with the queue.
  uint64_t dram_heap_vaddr_;

 protected:
  bool _IsA(Queue::rtti_t id) const override { return id == &rtti_id_; }

 private:
  core::SharedQueue* CreateSharedQueue(AieAgent* agent, size_t req_size_pkts, uint32_t node_id);
  core::SharedSignal* CreateSharedSignal(AieAgent* agent);

  AieAgent* owner_;
  /// Indicates if queue is active.
  std::atomic<bool> active_;
  static int rtti_id_;
};

} // namespace amd
} // namespace rocr

#endif  // header guard
