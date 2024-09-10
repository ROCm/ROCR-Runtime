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

#include "core/inc/amd_aie_aql_queue.h"

#ifdef __linux__
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

#include <stdio.h>
#include <string.h>
#include <thread>

#include "core/inc/queue.h"
#include "core/inc/runtime.h"
#include "core/inc/signal.h"
#include "core/util/utils.h"

namespace rocr {
namespace AMD {

int AieAqlQueue::rtti_id_ = 0;

AieAqlQueue::AieAqlQueue(AieAgent *agent, size_t req_size_pkts,
                         uint32_t node_id)
    : Queue(0, 0), LocalSignal(0, false), DoorbellSignal(signal()),
      agent_(*agent), active_(false) {
  amd_queue_.hsa_queue.doorbell_signal = Signal::Convert(this);
  amd_queue_.hsa_queue.size = 0x40;

  signal_.hardware_doorbell_ptr =
      reinterpret_cast<volatile uint64_t *>(hardware_doorbell_ptr_);
  signal_.kind = AMD_SIGNAL_KIND_DOORBELL;
  signal_.queue_ptr = &amd_queue_;
  active_ = true;

  core::Runtime::runtime_singleton_->AgentDriver(agent_.driver_type)
      .CreateQueue(*this);
}

AieAqlQueue::~AieAqlQueue() { Inactivate(); }

hsa_status_t AieAqlQueue::Inactivate() {
  bool active(active_.exchange(false, std::memory_order_relaxed));
  hsa_status_t status(HSA_STATUS_SUCCESS);

  if (active) {
    status = core::Runtime::runtime_singleton_->AgentDriver(agent_.driver_type)
                 .DestroyQueue(*this);
    hw_ctx_handle_ = std::numeric_limits<uint32_t>::max();
  }

  return status;
}

hsa_status_t AieAqlQueue::SetPriority(HSA_QUEUE_PRIORITY priority) {
  return HSA_STATUS_SUCCESS;
}

void AieAqlQueue::Destroy() { delete this; }

// Atomic Reads/Writes
uint64_t AieAqlQueue::LoadReadIndexRelaxed() {
  return atomic::Load(&amd_queue_.read_dispatch_id, std::memory_order_relaxed);
}

uint64_t AieAqlQueue::LoadReadIndexAcquire() {
  return atomic::Load(&amd_queue_.read_dispatch_id, std::memory_order_acquire);
}

uint64_t AieAqlQueue::LoadWriteIndexRelaxed() {
  return atomic::Load(&amd_queue_.write_dispatch_id, std::memory_order_relaxed);
}

uint64_t AieAqlQueue::LoadWriteIndexAcquire() {
  return atomic::Load(&amd_queue_.write_dispatch_id, std::memory_order_acquire);
}

void AieAqlQueue::StoreWriteIndexRelaxed(uint64_t value) {
  atomic::Store(&amd_queue_.write_dispatch_id, value,
                std::memory_order_relaxed);
}

void AieAqlQueue::StoreWriteIndexRelease(uint64_t value) {
  atomic::Store(&amd_queue_.write_dispatch_id, value,
                std::memory_order_release);
}

uint64_t AieAqlQueue::CasWriteIndexRelaxed(uint64_t expected, uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_relaxed);
}

uint64_t AieAqlQueue::CasWriteIndexAcquire(uint64_t expected, uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_acquire);
}

uint64_t AieAqlQueue::CasWriteIndexRelease(uint64_t expected, uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_release);
}

uint64_t AieAqlQueue::CasWriteIndexAcqRel(uint64_t expected, uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_acq_rel);
}

uint64_t AieAqlQueue::AddWriteIndexRelaxed(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_relaxed);
}

uint64_t AieAqlQueue::AddWriteIndexAcquire(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_acquire);
}

uint64_t AieAqlQueue::AddWriteIndexRelease(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_release);
}

uint64_t AieAqlQueue::AddWriteIndexAcqRel(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_acq_rel);
}

void AieAqlQueue::StoreRelaxed(hsa_signal_value_t value) {
  atomic::Store(signal_.hardware_doorbell_ptr, uint64_t(value),
                std::memory_order_release);
}

void AieAqlQueue::StoreRelease(hsa_signal_value_t value) {
  std::atomic_thread_fence(std::memory_order_release);
  StoreRelaxed(value);
}

hsa_status_t AieAqlQueue::GetInfo(hsa_queue_info_attribute_t attribute,
                                  void *value) {
  switch (attribute) {
  case HSA_AMD_QUEUE_INFO_AGENT:
    *(reinterpret_cast<hsa_agent_t *>(value)) = agent_.public_handle();
    break;
  case HSA_AMD_QUEUE_INFO_DOORBELL_ID:
    // Hardware doorbell supports AQL semantics.
    *(reinterpret_cast<uint64_t *>(value)) =
        reinterpret_cast<uint64_t>(signal_.hardware_doorbell_ptr);
    break;
  default:
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
}

core::SharedQueue *AieAqlQueue::CreateSharedQueue(AieAgent *agent,
                                                  size_t req_size_pkts,
                                                  uint32_t node_id) {
  queue_size_bytes_ = req_size_pkts * sizeof(core::AqlPacket);

  if (!IsPowerOfTwo(queue_size_bytes_)) {
    throw AMD::hsa_exception(
        HSA_STATUS_ERROR_INVALID_QUEUE_CREATION,
        "Requested queue with non-power of two packet capacity.\n");
  }

  node_id_ = node_id;

  return nullptr;
}

core::SharedSignal *AieAqlQueue::CreateSharedSignal(AieAgent *agent) {
  return nullptr;
}

hsa_status_t AieAqlQueue::GetCUMasking(uint32_t num_cu_mask_count,
                                       uint32_t *cu_mask) {
  assert(false && "AIE AQL queue does not support CU masking.");
  return HSA_STATUS_ERROR;
}

hsa_status_t AieAqlQueue::SetCUMasking(uint32_t num_cu_mask_count,
                                       const uint32_t *cu_mask) {
  assert(false && "AIE AQL queue does not support CU masking.");
  return HSA_STATUS_ERROR;
}

void AieAqlQueue::ExecutePM4(uint32_t *cmd_data, size_t cmd_size_b,
                             hsa_fence_scope_t acquireFence,
                             hsa_fence_scope_t releaseFence,
                             hsa_signal_t *signal) {
  assert(false && "AIE AQL queue does not support PM4 packets.");
}

} // namespace AMD
} // namespace rocr
