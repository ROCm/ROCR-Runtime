////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
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

#include "core/inc/intercept_queue.h"
#include "core/util/utils.h"

namespace rocr {
namespace core {

struct InterceptFrame {
  InterceptQueue* queue;
  uint64_t pkt_index;
  size_t interceptor_index;
};

static thread_local InterceptFrame Cursor = {nullptr, 0, 0};

static const uint16_t kInvalidHeader = (HSA_PACKET_TYPE_INVALID << HSA_PACKET_HEADER_TYPE) |
    (1 << HSA_PACKET_HEADER_BARRIER) |
    (HSA_FENCE_SCOPE_NONE << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
    (HSA_FENCE_SCOPE_NONE << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);

static const uint16_t kBarrierHeader = (HSA_PACKET_TYPE_BARRIER_AND << HSA_PACKET_HEADER_TYPE) |
    (1 << HSA_PACKET_HEADER_BARRIER) |
    (HSA_FENCE_SCOPE_NONE << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
    (HSA_FENCE_SCOPE_NONE << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);

static const hsa_barrier_and_packet_t kBarrierPacket = {kInvalidHeader, 0, 0, {}, 0, {}};

int InterceptQueue::rtti_id_ = 0;

InterceptQueue::InterceptQueue(std::unique_ptr<Queue> queue)
    : QueueProxy(std::move(queue)),
      LocalSignal(0, false),
      DoorbellSignal(signal()),
      next_packet_(0),
      retry_index_(0),
      quit_(false),
      active_(true) {
  buffer_ = SharedArray<AqlPacket, 4096>(wrapped->amd_queue_.hsa_queue.size);
  amd_queue_.hsa_queue.base_address = reinterpret_cast<void*>(&buffer_[0]);

  // Fill the ring buffer with invalid packet headers.
  // Leave packet content uninitialized to help trigger application errors.
  for (uint32_t pkt_id = 0; pkt_id < wrapped->amd_queue_.hsa_queue.size; ++pkt_id) {
    buffer_[pkt_id].dispatch.header = HSA_PACKET_TYPE_INVALID;
  }

  // Match the queue's signal ABI block to async_doorbell_'s
  // This allows us to use the queue's signal ABI block from devices to trigger async_doorbell while
  // host side use jumps directly to the queue's signal implementation.
  async_doorbell_ = new InterruptSignal(DOORBELL_MAX);
  MAKE_NAMED_SCOPE_GUARD(sigGuard, [&]() { async_doorbell_->DestroySignal(); });
  this->signal_ = async_doorbell_->signal_;
  amd_queue_.hsa_queue.doorbell_signal = Signal::Convert(this);

  // Install an async handler for device side dispatches.
  auto err = Runtime::runtime_singleton_->SetAsyncSignalHandler(
      core::Signal::Convert(async_doorbell_), HSA_SIGNAL_CONDITION_NE,
      async_doorbell_->LoadRelaxed(), HandleAsyncDoorbell, this);
  if (err != HSA_STATUS_SUCCESS)
    throw AMD::hsa_exception(err, "Doorbell handler registration failed.\n");

  // Install copy submission interceptor.
  AddInterceptor(Submit, this);

  sigGuard.Dismiss();
}

InterceptQueue::~InterceptQueue() {
  active_ = false;

  // Kill the async doorbell handler
  // Doorbell may not be used during or after queue destroy, however an interrupt may be in flight.
  // Ensure doorbell value is not 0, mark for exit, wake handler and wait for termination value.
  async_doorbell_->StoreRelaxed(DOORBELL_MAX);
  quit_ = true;
  hsa_signal_value_t val = async_doorbell_->ExchRelaxed(1);
  if (val != 0)
    async_doorbell_->WaitRelaxed(HSA_SIGNAL_CONDITION_EQ, 0, -1, HSA_WAIT_STATE_BLOCKED);
  async_doorbell_->DestroySignal();
}

bool InterceptQueue::HandleAsyncDoorbell(hsa_signal_value_t value, void* arg) {
  InterceptQueue* queue = reinterpret_cast<InterceptQueue*>(arg);
  if (queue->quit_) {
    queue->async_doorbell_->StoreRelaxed(0);
    return false;
  }
  queue->async_doorbell_->StoreRelaxed(DOORBELL_MAX);
  queue->StoreRelease(value);
  return true;
}

void InterceptQueue::PacketWriter(const void* pkts, uint64_t pkt_count) {
  Cursor.interceptor_index--;
  auto& handler = Cursor.queue->interceptors[Cursor.interceptor_index];
  handler.first(pkts, pkt_count, Cursor.pkt_index, handler.second, PacketWriter);
}

void InterceptQueue::Submit(const void* pkts, uint64_t pkt_count, uint64_t user_pkt_index,
                            void* data, hsa_amd_queue_intercept_packet_writer writer) {
  InterceptQueue* queue = reinterpret_cast<InterceptQueue*>(data);
  const AqlPacket* packets = (const AqlPacket*)pkts;

  // Submit final packet transform to hardware.
  if (queue->Submit(packets, pkt_count)) return;

  // Could not submit final packets, stash for later.
  assert(queue->overflow_.empty() && "Packet intercept error: overflow buffer not empty.\n");
  for (uint64_t i = 0; i < pkt_count; i++) queue->overflow_.push_back(packets[i]);
}

bool InterceptQueue::Submit(const AqlPacket* packets, uint64_t count) {
  if (count == 0) return true;

  AqlPacket* ring = reinterpret_cast<AqlPacket*>(wrapped->amd_queue_.hsa_queue.base_address);
  uint64_t mask = wrapped->amd_queue_.hsa_queue.size - 1;

  while (true) {
    uint64_t write = wrapped->LoadWriteIndexRelaxed();
    uint64_t read = wrapped->LoadReadIndexRelaxed();
    uint64_t free_slots = wrapped->amd_queue_.hsa_queue.size - (write - read);

    // If out of space defer packet insertion.
    if (free_slots <= count) {
      // If there is not already a pending retry point add one.
      if (retry_index_ <= read) {
        // Reserve and wait for one slot.
        write = wrapped->AddWriteIndexRelaxed(1);
        read = write - wrapped->amd_queue_.hsa_queue.size + 1;
        while (wrapped->LoadReadIndexRelaxed() < read) os::YieldThread();

        // Submit barrer which will wake async queue processing.
        ring[write & mask].barrier_and = kBarrierPacket;
        ring[write & mask].barrier_and.completion_signal = Signal::Convert(async_doorbell_);
        atomic::Store(&ring[write & mask].barrier_and.header, kBarrierHeader,
                      std::memory_order_release);
        HSA::hsa_signal_store_screlease(wrapped->amd_queue_.hsa_queue.doorbell_signal, write);

        // Record the retry point
        retry_index_ = write;
      }
      return false;
    }

    // Attempt to reserve useable queue space
    uint64_t new_write = wrapped->CasWriteIndexRelaxed(write, write + count);
    if (new_write == write) {
      AqlPacket first = packets[0];
      uint16_t header = first.dispatch.header;
      first.dispatch.header = kInvalidHeader;

      ring[write & mask] = first;
      for (uint64_t i = 1; i < count; i++) ring[(write + i) & mask] = packets[i];
      atomic::Store(&ring[write & mask].dispatch.header, header, std::memory_order_release);
      HSA::hsa_signal_store_screlease(wrapped->amd_queue_.hsa_queue.doorbell_signal,
                                      write + count - 1);

      return true;
    }
  }
}

void InterceptQueue::StoreRelaxed(hsa_signal_value_t value) {
  if (!active_) return;

  // If called recursively defer to async doorbell thread.
  if (Cursor.queue != nullptr) {
    debug_print("Likely incorrect queue use observed in an interceptor.\n");
    async_doorbell_->StoreRelaxed(value);
    return;
  }

  ScopedAcquire<KernelMutex> lock(&lock_);

  // Submit overflow packets.
  if (!overflow_.empty()) {
    if (!Submit(&overflow_[0], overflow_.size())) return;
    overflow_.clear();
  }

  Cursor.queue = this;

  AqlPacket* ring = reinterpret_cast<AqlPacket*>(amd_queue_.hsa_queue.base_address);
  uint64_t mask = wrapped->amd_queue_.hsa_queue.size - 1;

  // Loop over valid packets and process.
  uint64_t end = LoadWriteIndexAcquire();
  uint64_t i;
  for (i = next_packet_; i < end; i++) {
    if (!ring[i & mask].IsValid()) break;

    // Process callbacks.
    Cursor.interceptor_index = interceptors.size() - 1;
    Cursor.pkt_index = i;
    auto& handler = interceptors[Cursor.interceptor_index];
    handler.first(&ring[i & mask], 1, i, handler.second, PacketWriter);

    // Invalidate consumed packet
    atomic::Store(&ring[i & mask].dispatch.header, kInvalidHeader, std::memory_order_release);
  }

  next_packet_ = i;
  Cursor.queue = nullptr;
  atomic::Store(&amd_queue_.read_dispatch_id, next_packet_, std::memory_order_release);
}

}  // namespace core
}  // namespace rocr
