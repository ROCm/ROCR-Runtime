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

#include "core/inc/amd_blit_sdma.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>

#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/runtime.h"
#include "core/inc/sdma_registers.h"
#include "core/inc/signal.h"
#include "core/inc/interrupt_signal.h"

namespace amd {

inline uint32_t ptrlow32(const void* p) {
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

inline uint32_t ptrhigh32(const void* p) {
#if defined(HSA_LARGE_MODEL)
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) >> 32);
#else
  return 0;
#endif
}

const size_t BlitSdmaBase::kQueueSize = 1024 * 1024;
const size_t BlitSdmaBase::kCopyPacketSize = sizeof(SDMA_PKT_COPY_LINEAR);
const size_t BlitSdmaBase::kMaxSingleCopySize = SDMA_PKT_COPY_LINEAR::kMaxSize_;
const size_t BlitSdmaBase::kMaxSingleFillSize = SDMA_PKT_CONSTANT_FILL::kMaxSize_;

// Initialize size of various sDMA commands use by this module
template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
const uint32_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::linear_copy_command_size_ = sizeof(SDMA_PKT_COPY_LINEAR);

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
const uint32_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::fill_command_size_ = sizeof(SDMA_PKT_CONSTANT_FILL);

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
const uint32_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::fence_command_size_ = sizeof(SDMA_PKT_FENCE);

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
const uint32_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::poll_command_size_ = sizeof(SDMA_PKT_POLL_REGMEM);

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
const uint32_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::flush_command_size_ = sizeof(SDMA_PKT_POLL_REGMEM);

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
const uint32_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::atomic_command_size_ = sizeof(SDMA_PKT_ATOMIC);

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
const uint32_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::timestamp_command_size_ = sizeof(SDMA_PKT_TIMESTAMP);

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
const uint32_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::trap_command_size_ = sizeof(SDMA_PKT_TRAP);

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BlitSdma(bool copy_direction)
    : agent_(NULL),
      queue_start_addr_(NULL),
      parity_(false),
      cached_reserve_index_(0),
      cached_commit_index_(0),
      sdma_h2d_(copy_direction),
      platform_atomic_support_(true),
      hdp_flush_support_(false) {
  std::memset(&queue_resource_, 0, sizeof(queue_resource_));
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::~BlitSdma() {}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::Initialize(
    const core::Agent& agent) {
  if (queue_start_addr_ != NULL) {
    // Already initialized.
    return HSA_STATUS_SUCCESS;
  }

  if (agent.device_type() != core::Agent::kAmdGpuDevice) {
    return HSA_STATUS_ERROR;
  }

  agent_ = reinterpret_cast<amd::GpuAgent*>(&const_cast<core::Agent&>(agent));

  if (HSA_PROFILE_FULL == agent_->profile()) {
    assert(false && "Only support SDMA for dgpu currently");
    return HSA_STATUS_ERROR;
  }

  if (agent_->isa()->version() == core::Isa::Version(7, 0, 1)) {
    platform_atomic_support_ = false;
  } else {
    const core::Runtime::LinkInfo& link = core::Runtime::runtime_singleton_->GetLinkInfo(
        agent_->node_id(), core::Runtime::runtime_singleton_->cpu_agents()[0]->node_id());
    platform_atomic_support_ = link.info.atomic_support_64bit;
  }

  // Determine if sDMA microcode supports HDP flush command
  if (agent_->GetSdmaMicrocodeVersion() >= SDMA_PKT_HDP_FLUSH::kMinVersion_) {
    hdp_flush_support_ = true;
  }

  // Allocate queue buffer.
  queue_start_addr_ = (char*)core::Runtime::runtime_singleton_->system_allocator()(
      kQueueSize, 0x1000, core::MemoryRegion::AllocateExecutable);

  if (queue_start_addr_ == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }
  MAKE_NAMED_SCOPE_GUARD(cleanupOnException, [&]() { Destroy(agent); };);
  std::memset(queue_start_addr_, 0, kQueueSize);

  // Access kernel driver to initialize the queue control block
  // This call binds user mode queue object to underlying compute
  // device.
  const HSA_QUEUE_TYPE kQueueType_ = HSA_QUEUE_SDMA;
  if (HSAKMT_STATUS_SUCCESS != hsaKmtCreateQueue(agent_->node_id(), kQueueType_, 100,
                                                 HSA_QUEUE_PRIORITY_MAXIMUM, queue_start_addr_,
                                                 kQueueSize, NULL, &queue_resource_)) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  cached_reserve_index_ = *reinterpret_cast<RingIndexTy*>(queue_resource_.Queue_write_ptr);
  cached_commit_index_ = cached_reserve_index_;

  signals_[0].reset(new core::InterruptSignal(0));
  signals_[1].reset(new core::InterruptSignal(0));

  cleanupOnException.Dismiss();
  return HSA_STATUS_SUCCESS;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::Destroy(
    const core::Agent& agent) {
  // Release all allocated resources and reset them to zero.

  if (queue_resource_.QueueId != 0) {
    // Release queue resources from the kernel
    auto err = hsaKmtDestroyQueue(queue_resource_.QueueId);
    assert(err == HSAKMT_STATUS_SUCCESS);
    memset(&queue_resource_, 0, sizeof(queue_resource_));
  }

  if (queue_start_addr_ != NULL) {
    // Release queue buffer.
    core::Runtime::runtime_singleton_->system_deallocator()(queue_start_addr_);
  }

  queue_start_addr_ = NULL;
  cached_reserve_index_ = 0;
  cached_commit_index_ = 0;

  signals_[0].reset();
  signals_[1].reset();

  return HSA_STATUS_SUCCESS;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::SubmitBlockingCommand(
    const void* cmd, size_t cmd_size) {
  ScopedAcquire<KernelMutex> lock(&lock_);

  // Alternate between completion signals
  // Using two allows overlapping command writing and copies
  core::Signal* completionSignal;
  if (parity_)
    completionSignal = signals_[0].get();
  else
    completionSignal = signals_[1].get();
  parity_ ^= true;

  // Wait for prior operation with this signal to complete
  completionSignal->WaitRelaxed(HSA_SIGNAL_CONDITION_EQ, 0, -1, HSA_WAIT_STATE_BLOCKED);

  // Mark signal as in use, guard against exception leaving the signal in an unusable state.
  completionSignal->StoreRelaxed(2);
  MAKE_SCOPE_GUARD([&]() { completionSignal->StoreRelaxed(0); });
  lock.Release();

  // Submit command and wait for completion
  hsa_status_t ret = SubmitCommand(cmd, cmd_size, std::vector<core::Signal*>(), *completionSignal);
  completionSignal->WaitRelaxed(HSA_SIGNAL_CONDITION_EQ, 1, -1, HSA_WAIT_STATE_BLOCKED);
  return ret;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::SubmitCommand(
    const void* cmd, size_t cmd_size, const std::vector<core::Signal*>& dep_signals,
    core::Signal& out_signal) {
  // The signal is 64 bit value, and poll checks for 32 bit value. So we
  // need to use two poll operations per dependent signal.
  const uint32_t num_poll_command =
      static_cast<uint32_t>(2 * dep_signals.size());
  const uint32_t total_poll_command_size =
      (num_poll_command * poll_command_size_);

  // Load the profiling state early in case the user disable or enable the
  // profiling in the middle of the call.
  const bool profiling_enabled = agent_->profiling_enabled();

  uint64_t* end_ts_addr = NULL;
  uint32_t total_timestamp_command_size = 0;

  if (profiling_enabled) {
    // SDMA timestamp packet requires 32 byte of aligned memory, but
    // amd_signal_t::end_ts is not 32 byte aligned. So an extra copy packet to
    // read from a 32 byte aligned bounce buffer is required to avoid changing
    // the amd_signal_t ABI.

    end_ts_addr = agent_->ObtainEndTsObject();
    if (end_ts_addr == NULL) {
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }

    total_timestamp_command_size =
        (2 * timestamp_command_size_) + linear_copy_command_size_;
  }

  // On agent that does not support platform atomic, we replace it with
  // one or two fence packet(s) to update the signal value. The reason fence
  // is used and not write packet is because the SDMA engine may overlap a
  // serial copy/write packets.
  const uint64_t completion_signal_value =
      static_cast<uint64_t>(out_signal.LoadRelaxed() - 1);
  const size_t sync_command_size = (platform_atomic_support_)
                                       ? atomic_command_size_
                                       : (completion_signal_value > UINT32_MAX)
                                             ? 2 * fence_command_size_
                                             : fence_command_size_;

  // If the signal is an interrupt signal, we also need to make SDMA engine to
  // send interrupt packet to IH.
  const size_t interrupt_command_size =
      (out_signal.signal_.event_mailbox_ptr != 0)
          ? (fence_command_size_ + trap_command_size_)
          : 0;

  // Add space for acquire or release Hdp flush command
  uint32_t flush_cmd_size = 0;
  if (core::Runtime::runtime_singleton_->flag().enable_sdma_hdp_flush()) {
    if ((HwIndexMonotonic) && (hdp_flush_support_)) {
      flush_cmd_size = flush_command_size_;
    }
  }

  const uint32_t total_command_size = total_poll_command_size + cmd_size + sync_command_size +
      total_timestamp_command_size + interrupt_command_size + flush_cmd_size;

  RingIndexTy curr_index;
  char* command_addr = AcquireWriteAddress(total_command_size, curr_index);

  if (command_addr == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  for (size_t i = 0; i < dep_signals.size(); ++i) {
    uint32_t* signal_addr =
        reinterpret_cast<uint32_t*>(dep_signals[i]->ValueLocation());
    // Wait for the higher 64 bit to 0.
    BuildPollCommand(command_addr, &signal_addr[1], 0);
    command_addr += poll_command_size_;
    // Then wait for the lower 64 bit to 0.
    BuildPollCommand(command_addr, &signal_addr[0], 0);
    command_addr += poll_command_size_;
  }

  if (profiling_enabled) {
    BuildGetGlobalTimestampCommand(
        command_addr, reinterpret_cast<void*>(&out_signal.signal_.start_ts));
    command_addr += timestamp_command_size_;
  }

  // Determine if a Hdp flush cmd is required at the top of cmd stream
  if (core::Runtime::runtime_singleton_->flag().enable_sdma_hdp_flush()) {
    if ((HwIndexMonotonic) && (hdp_flush_support_) && (sdma_h2d_ == false)) {
      BuildHdpFlushCommand(command_addr);
      command_addr += flush_command_size_;
    }
  }

  // Do the command after all polls are satisfied.
  memcpy(command_addr, cmd, cmd_size);
  command_addr += cmd_size;

  // Determine if a Hdp flush cmd is required at the end of cmd stream
  if (core::Runtime::runtime_singleton_->flag().enable_sdma_hdp_flush()) {
    if ((HwIndexMonotonic) && (hdp_flush_support_) && (sdma_h2d_)) {
      BuildHdpFlushCommand(command_addr);
      command_addr += flush_command_size_;
    }
  }

  if (profiling_enabled) {
    assert(IsMultipleOf(end_ts_addr, 32));
    BuildGetGlobalTimestampCommand(command_addr,
                                   reinterpret_cast<void*>(end_ts_addr));
    command_addr += timestamp_command_size_;

    BuildCopyCommand(command_addr, 1,
                     reinterpret_cast<void*>(&out_signal.signal_.end_ts),
                     reinterpret_cast<void*>(end_ts_addr), sizeof(uint64_t));
    command_addr += linear_copy_command_size_;
  }

  // After transfer is completed, decrement the signal value.
  if (platform_atomic_support_) {
    BuildAtomicDecrementCommand(command_addr, out_signal.ValueLocation());
    command_addr += atomic_command_size_;

  } else {
    uint32_t* signal_value_location = reinterpret_cast<uint32_t*>(out_signal.ValueLocation());
    if (completion_signal_value > UINT32_MAX) {
      BuildFenceCommand(command_addr, signal_value_location + 1,
                        static_cast<uint32_t>(completion_signal_value >> 32));
      command_addr += fence_command_size_;
    }

    BuildFenceCommand(command_addr, signal_value_location,
                      static_cast<uint32_t>(completion_signal_value));

    command_addr += fence_command_size_;
  }

  // Update mailbox event and send interrupt to IH.
  if (out_signal.signal_.event_mailbox_ptr != 0) {
    BuildFenceCommand(command_addr,
                      reinterpret_cast<uint32_t*>(out_signal.signal_.event_mailbox_ptr),
                      static_cast<uint32_t>(out_signal.signal_.event_id));
    command_addr += fence_command_size_;

    BuildTrapCommand(command_addr);
  }

  ReleaseWriteAddress(curr_index, total_command_size);

  return HSA_STATUS_SUCCESS;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::SubmitLinearCopyCommand(
    void* dst, const void* src, size_t size) {
  // Break the copy into multiple copy operation incase the copy size exceeds
  // the SDMA linear copy limit.
  const uint32_t num_copy_command = (size + kMaxSingleCopySize - 1) / kMaxSingleCopySize;

  std::vector<SDMA_PKT_COPY_LINEAR> buff(num_copy_command);
  BuildCopyCommand(reinterpret_cast<char*>(&buff[0]), num_copy_command, dst, src, size);

  return SubmitBlockingCommand(&buff[0], buff.size() * sizeof(SDMA_PKT_COPY_LINEAR));
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::SubmitLinearCopyCommand(
    void* dst, const void* src, size_t size, std::vector<core::Signal*>& dep_signals,
    core::Signal& out_signal) {
  // Break the copy into multiple copy operations when the copy size exceeds
  // the SDMA linear copy limit.
  const uint32_t num_copy_command = (size + kMaxSingleCopySize - 1) / kMaxSingleCopySize;

  // Assemble copy packets.
  std::vector<SDMA_PKT_COPY_LINEAR> buff(num_copy_command);
  BuildCopyCommand(reinterpret_cast<char*>(&buff[0]), num_copy_command, dst, src, size);

  return SubmitCommand(&buff[0], buff.size() * sizeof(SDMA_PKT_COPY_LINEAR), dep_signals,
                       out_signal);
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::SubmitCopyRectCommand(
    const hsa_pitched_ptr_t* dst, const hsa_dim3_t* dst_offset, const hsa_pitched_ptr_t* src,
    const hsa_dim3_t* src_offset, const hsa_dim3_t* range, std::vector<core::Signal*>& dep_signals,
    core::Signal& out_signal) {
  // Hardware requires DWORD alignment for base address, pitches
  // Also confirm that we have a geometric rect (copied block does not wrap an edge).
  if (((uintptr_t)dst->base) % 4 != 0 || ((uintptr_t)src->base) % 4 != 0)
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT,
                             "Copy rect base address not aligned.");
  if (((uintptr_t)dst->pitch) % 4 != 0 || ((uintptr_t)src->pitch) % 4 != 0)
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT, "Copy rect pitch not aligned.");
  if (((uintptr_t)dst->slice) % 4 != 0 || ((uintptr_t)src->slice) % 4 != 0)
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT, "Copy rect slice not aligned.");
  if (uint64_t(src_offset->x) + range->x > src->pitch ||
      uint64_t(dst_offset->x) + range->x > dst->pitch)
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT, "Copy rect width out of range.");
  if ((src->slice != 0) && (uint64_t(src_offset->y) + range->y) > src->slice / src->pitch)
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT, "Copy rect height out of range.");
  if ((dst->slice != 0) && (uint64_t(dst_offset->y) + range->y) > dst->slice / dst->pitch)
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT, "Copy rect height out of range.");
  if (range->z > 1 && (src->slice == 0 || dst->slice == 0))
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT, "Copy rect slice needed.");

  const uint max_pitch = 1 << SDMA_PKT_COPY_LINEAR_RECT::pitch_bits;

  std::vector<SDMA_PKT_COPY_LINEAR_RECT> pkts;
  auto append = [&](size_t size) {
    assert(size == sizeof(SDMA_PKT_COPY_LINEAR_RECT) && "SDMA packet size missmatch");
    pkts.emplace_back(SDMA_PKT_COPY_LINEAR_RECT());
    return &pkts.back();
  };

  // Do wide pitch 2D copies along X-Z
  if (range->z == 1 && (src->pitch > max_pitch || dst->pitch > max_pitch)) {
    hsa_pitched_ptr_t Src = *src;
    hsa_pitched_ptr_t Dst = *dst;
    hsa_dim3_t Soff = *src_offset;
    hsa_dim3_t Doff = *dst_offset;
    hsa_dim3_t Range = *range;

    Src.base =
      static_cast<char*>(Src.base) + Soff.z * Src.slice + Soff.y * Src.pitch;
    Dst.base =
      static_cast<char*>(Dst.base) + Doff.z * Dst.slice + Doff.y * Dst.pitch;
    Soff.y = Soff.z = 0;
    Doff.y = Doff.z = 0;

    Src.slice = Src.pitch;
    Src.pitch = 0;
    Dst.slice = Dst.pitch;
    Dst.pitch = 0;

    Range.z = Range.y;
    Range.y = 1;

    BuildCopyRectCommand(append, &Dst, &Doff, &Src, &Soff, &Range);
  } else {
    BuildCopyRectCommand(append, dst, dst_offset, src, src_offset, range);
  }

  return SubmitCommand(&pkts[0], pkts.size() * sizeof(SDMA_PKT_COPY_LINEAR_RECT), dep_signals,
                       out_signal);
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::SubmitLinearFillCommand(
    void* ptr, uint32_t value, size_t count) {
  const size_t size = count * sizeof(uint32_t);

  const uint32_t num_fill_command = (size + kMaxSingleFillSize - 1) / kMaxSingleFillSize;

  std::vector<SDMA_PKT_CONSTANT_FILL> buff(num_fill_command);
  BuildFillCommand(reinterpret_cast<char*>(&buff[0]), num_fill_command, ptr, value, count);

  return SubmitBlockingCommand(&buff[0], buff.size() * sizeof(SDMA_PKT_CONSTANT_FILL));
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::EnableProfiling(
    bool enable) {
  return HSA_STATUS_SUCCESS;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
char* BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::AcquireWriteAddress(
    uint32_t cmd_size, RingIndexTy& curr_index) {
  // Ring is full when all but one byte is written.
  if (cmd_size >= kQueueSize) {
    return NULL;
  }

  while (true) {
    curr_index = atomic::Load(&cached_reserve_index_, std::memory_order_acquire);

    // Check whether a linear region of the requested size is available.
    // If == cmd_size: region is at beginning of ring.
    // If < cmd_size: region intersects end of ring, pad with no-ops and retry.
    if (WrapIntoRing(curr_index + cmd_size) < cmd_size) {
      PadRingToEnd(curr_index);
      continue;
    }

    // Check whether the engine has finished using this region.
    const RingIndexTy new_index = curr_index + cmd_size;

    if (CanWriteUpto(new_index) == false) {
      // Wait for read index to move and try again.
      os::YieldThread();
      continue;
    }

    // Try to reserve this part of the ring.
    if (atomic::Cas(&cached_reserve_index_, new_index, curr_index, std::memory_order_release) ==
        curr_index) {
      return queue_start_addr_ + WrapIntoRing(curr_index);
    }

    // Another thread reserved curr_index, try again.
    os::YieldThread();
  }

  return NULL;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::UpdateWriteAndDoorbellRegister(
    RingIndexTy curr_index, RingIndexTy new_index) {
  while (true) {
    // Make sure that the address before ::curr_index is already released.
    // Otherwise the CP may read invalid packets.
    if (atomic::Load(&cached_commit_index_, std::memory_order_acquire) == curr_index) {
      if (core::Runtime::runtime_singleton_->flag().sdma_wait_idle()) {
        // TODO: remove when sdma wpointer issue is resolved.
        // Wait until the SDMA engine finish processing all packets before
        // updating the wptr and doorbell.
        while (WrapIntoRing(*reinterpret_cast<RingIndexTy*>(queue_resource_.Queue_read_ptr)) !=
               WrapIntoRing(curr_index)) {
          os::YieldThread();
        }
      }

      // Update write pointer and doorbel register.
      *reinterpret_cast<RingIndexTy*>(queue_resource_.Queue_write_ptr) =
          (HwIndexMonotonic ? new_index : WrapIntoRing(new_index));

      // Ensure write pointer is visible to GPU before doorbell.
      std::atomic_thread_fence(std::memory_order_release);

      *reinterpret_cast<RingIndexTy*>(queue_resource_.Queue_DoorBell) =
          (HwIndexMonotonic ? new_index : WrapIntoRing(new_index));

      atomic::Store(&cached_commit_index_, new_index, std::memory_order_release);
      break;
    }

    // Waiting for another thread to submit preceding commands first.
    os::YieldThread();
  }
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::ReleaseWriteAddress(
    RingIndexTy curr_index, uint32_t cmd_size) {
  if (cmd_size > kQueueSize) {
    assert(false && "cmd_addr is outside the queue buffer range");
    return;
  }

  UpdateWriteAndDoorbellRegister(curr_index, curr_index + cmd_size);
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::PadRingToEnd(
    RingIndexTy curr_index) {
  // Reserve region from here to the end of the ring.
  RingIndexTy new_index = curr_index + (kQueueSize - WrapIntoRing(curr_index));

  // Check whether the engine has finished using this region.
  if (CanWriteUpto(new_index) == false) {
    // Wait for read index to move and try again.
    return;
  }

  if (atomic::Cas(&cached_reserve_index_, new_index, curr_index, std::memory_order_release) ==
      curr_index) {
    // Write and submit NOP commands in reserved region.
    char* nop_address = queue_start_addr_ + WrapIntoRing(curr_index);
    memset(nop_address, 0, new_index - curr_index);

    UpdateWriteAndDoorbellRegister(curr_index, new_index);
  }
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
uint32_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::WrapIntoRing(
    RingIndexTy index) {
  return index & (kQueueSize - 1);
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
bool BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::CanWriteUpto(
    RingIndexTy upto_index) {
  // Get/calculate the monotonic read index.
  RingIndexTy hw_read_index = *reinterpret_cast<RingIndexTy*>(queue_resource_.Queue_read_ptr);
  RingIndexTy read_index;

  if (HwIndexMonotonic) {
    read_index = hw_read_index;
  } else {
    // Calculate distance from commit index to HW read index.
    // Commit index is always < kQueueSize away from HW read index.
    RingIndexTy commit_index = atomic::Load(&cached_commit_index_, std::memory_order_relaxed);
    RingIndexTy dist_to_read_index = WrapIntoRing(commit_index - hw_read_index);
    read_index = commit_index - dist_to_read_index;
  }

  // Check whether the read pointer has passed the given index.
  // At most we can submit (kQueueSize - 1) bytes at a time.
  return (upto_index - read_index) < kQueueSize;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BuildFenceCommand(
    char* fence_command_addr, uint32_t* fence, uint32_t fence_value) {
  assert(fence_command_addr != NULL);
  SDMA_PKT_FENCE* packet_addr =
      reinterpret_cast<SDMA_PKT_FENCE*>(fence_command_addr);

  memset(packet_addr, 0, sizeof(SDMA_PKT_FENCE));

  packet_addr->HEADER_UNION.op = SDMA_OP_FENCE;

  packet_addr->ADDR_LO_UNION.addr_31_0 = ptrlow32(fence);

  packet_addr->ADDR_HI_UNION.addr_63_32 = ptrhigh32(fence);

  packet_addr->DATA_UNION.data = fence_value;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BuildCopyCommand(
    char* cmd_addr, uint32_t num_copy_command, void* dst, const void* src, size_t size) {
  size_t cur_size = 0;
  for (uint32_t i = 0; i < num_copy_command; ++i) {
    const uint32_t copy_size =
        static_cast<uint32_t>(std::min((size - cur_size), kMaxSingleCopySize));

    void* cur_dst = static_cast<char*>(dst) + cur_size;
    const void* cur_src = static_cast<const char*>(src) + cur_size;

    SDMA_PKT_COPY_LINEAR* packet_addr =
        reinterpret_cast<SDMA_PKT_COPY_LINEAR*>(cmd_addr);

    memset(packet_addr, 0, sizeof(SDMA_PKT_COPY_LINEAR));

    packet_addr->HEADER_UNION.op = SDMA_OP_COPY;
    packet_addr->HEADER_UNION.sub_op = SDMA_SUBOP_COPY_LINEAR;

    packet_addr->COUNT_UNION.count = copy_size + SizeToCountOffset;

    packet_addr->SRC_ADDR_LO_UNION.src_addr_31_0 = ptrlow32(cur_src);
    packet_addr->SRC_ADDR_HI_UNION.src_addr_63_32 = ptrhigh32(cur_src);

    packet_addr->DST_ADDR_LO_UNION.dst_addr_31_0 = ptrlow32(cur_dst);
    packet_addr->DST_ADDR_HI_UNION.dst_addr_63_32 = ptrhigh32(cur_dst);

    cmd_addr += linear_copy_command_size_;
    cur_size += copy_size;
  }

  assert(cur_size == size);
}

/*
Copies are done in terms of elements (1, 2, 4, 8, or 16 bytes) and have alignment restrictions.
Elements are coded by the log2 of the element size in bytes (ie. element 0=1 byte, 4=16 byte).
This routine breaks a large rect into tiles that can be handled by hardware.  Pitches and offsets
must be representable in terms of elements in all tiles of the copy.
*/
template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BuildCopyRectCommand(
    const std::function<void*(size_t)>& append, const hsa_pitched_ptr_t* dst,
    const hsa_dim3_t* dst_offset, const hsa_pitched_ptr_t* src, const hsa_dim3_t* src_offset,
    const hsa_dim3_t* range) {
  // Returns the index of the first set bit (ie log2 of the largest power of 2 that evenly divides
  // width), the largest element that perfectly covers width.
  // width | 16 ensures that we don't return a higher element than is supported and avoids
  // issues with 0.
  auto maxAlignedElement = [](size_t width) {
    return __builtin_ctz(width | 16);
  };

  // Limits in terms of element count
  const uint max_pitch = 1 << SDMA_PKT_COPY_LINEAR_RECT::pitch_bits;
  const uint max_slice = 1 << SDMA_PKT_COPY_LINEAR_RECT::slice_bits;
  const uint max_x = 1 << SDMA_PKT_COPY_LINEAR_RECT::rect_xy_bits;
  const uint max_y = 1 << SDMA_PKT_COPY_LINEAR_RECT::rect_xy_bits;
  const uint max_z = 1 << SDMA_PKT_COPY_LINEAR_RECT::rect_z_bits;

  // Find maximum element that describes the pitch and slice.
  // Pitch and slice must both be represented in units of elements.  No element larger than this
  // may be used in any tile as the pitches would not be exactly represented.
  int max_ele = Min(maxAlignedElement(src->pitch), maxAlignedElement(dst->pitch));
  if (range->z != 1)  // Only need to consider slice if HW will copy along Z.
    max_ele = Min(max_ele, maxAlignedElement(src->slice), maxAlignedElement(dst->slice));

  /*
  Find the minimum element size that will be needed for any tile.

  No subdivision of a range admits a larger element size for the smallest element in any subdivision
  than the element size that covers the whole range, though some can be worse (this is easily model
  checked).  Subdividing with any element larger than the covering element won't change the covering
  element of the remainder
  ( Range%Element = (Range-N*LargerElement)%Element since LargerElement%Element=0 ).
    Ex. range->x=71, assume max range is 16 elements:  We can break at 64 giving tiles:
    [0,63], [64-70] (width 64 & 7).  64 is covered by element 4 (16B) and 7 is covered by element 0
    (1B).  Exactly covering 71 requires using element 0.

  Base addresses in each tile must be DWORD aligned, if not then the offset from an aligned address
  must be represented in elements.  This may reduce the size of the element, but since elements are
  integer multiples of each other this is harmless.

  src and dst base has already been checked for DWORD alignment so we only need to consider the
  offset here.
  */
  int min_ele = Min(max_ele, maxAlignedElement(range->x), maxAlignedElement(src_offset->x % 4),
                    maxAlignedElement(dst_offset->x % 4));

  // Check that pitch and slice can be represented in the tile with the smallest element
  if ((src->pitch >> min_ele) > max_pitch || (dst->pitch >> min_ele) > max_pitch)
    throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT, "Copy rect pitch out of limits.\n");
  if (range->z != 1) {  // Only need to consider slice if HW will copy along Z.
    if ((src->slice >> min_ele) > max_slice || (dst->slice >> min_ele) > max_slice)
      throw AMD::hsa_exception(HSA_STATUS_ERROR_INVALID_ARGUMENT,
                               "Copy rect slice out of limits.\n");
  }

  // Break copy into tiles
  for (uint64_t z = 0; z < range->z; z += max_z) {
    for (uint64_t y = 0; y < range->y; y += max_y) {
      uint64_t x = 0;
      while (x < range->x) {
        uint64_t width = range->x - x;

        // Get largest element which describes the start of this tile after its base address has
        // been aligned.  Base addresses must be DWORD (4 byte) aligned.
        int aligned_ele = Min(maxAlignedElement((src_offset->x + x) % 4),
                              maxAlignedElement((dst_offset->x + x) % 4), max_ele);

        // Get largest permissible element which exactly covers width
        int element = Min(maxAlignedElement(width), aligned_ele);
        int xcount = width >> element;

        // If width is too large then width is at least max_x bytes (bigger than any element) so
        // drop the width restriction and clip element count to max_x.
        if (xcount > max_x) {
          element = aligned_ele;
          xcount = Min(width >> element, max_x);
        }

        // Get base addresses and offsets for this tile.
        uintptr_t sbase = (uintptr_t)src->base + src_offset->x + x +
            (src_offset->y + y) * src->pitch + (src_offset->z + z) * src->slice;
        uintptr_t dbase = (uintptr_t)dst->base + dst_offset->x + x +
            (dst_offset->y + y) * dst->pitch + (dst_offset->z + z) * dst->slice;
        uint soff = (sbase % 4) >> element;
        uint doff = (dbase % 4) >> element;
        sbase &= ~3ull;
        dbase &= ~3ull;

        x += xcount << element;

        SDMA_PKT_COPY_LINEAR_RECT* pkt =
            (SDMA_PKT_COPY_LINEAR_RECT*)append(sizeof(SDMA_PKT_COPY_LINEAR_RECT));
        *pkt = {};
        pkt->HEADER_UNION.op = SDMA_OP_COPY;
        pkt->HEADER_UNION.sub_op = SDMA_SUBOP_COPY_LINEAR_RECT;
        pkt->HEADER_UNION.element = element;
        pkt->SRC_ADDR_LO_UNION.src_addr_31_0 = sbase;
        pkt->SRC_ADDR_HI_UNION.src_addr_63_32 = sbase >> 32;
        pkt->SRC_PARAMETER_1_UNION.src_offset_x = soff;
        pkt->SRC_PARAMETER_2_UNION.src_pitch = (src->pitch >> element) - 1;
        pkt->SRC_PARAMETER_3_UNION.src_slice_pitch =
            (range->z == 1) ? 0 : (src->slice >> element) - 1;
        pkt->DST_ADDR_LO_UNION.dst_addr_31_0 = dbase;
        pkt->DST_ADDR_HI_UNION.dst_addr_63_32 = dbase >> 32;
        pkt->DST_PARAMETER_1_UNION.dst_offset_x = doff;
        pkt->DST_PARAMETER_2_UNION.dst_pitch = (dst->pitch >> element) - 1;
        pkt->DST_PARAMETER_3_UNION.dst_slice_pitch =
            (range->z == 1) ? 0 : (dst->slice >> element) - 1;
        pkt->RECT_PARAMETER_1_UNION.rect_x = xcount - 1;
        pkt->RECT_PARAMETER_1_UNION.rect_y = Min(range->y - y, max_y) - 1;
        pkt->RECT_PARAMETER_2_UNION.rect_z = Min(range->z - z, max_z) - 1;
      }
    }
  }
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BuildFillCommand(
    char* cmd_addr, uint32_t num_fill_command, void* ptr, uint32_t value, size_t count) {
  char* cur_ptr = reinterpret_cast<char*>(ptr);
  const uint32_t maxDwordCount = kMaxSingleFillSize / sizeof(uint32_t);
  SDMA_PKT_CONSTANT_FILL* packet_addr = reinterpret_cast<SDMA_PKT_CONSTANT_FILL*>(cmd_addr);

  for (uint32_t i = 0; i < num_fill_command; i++) {
    assert(count != 0 && "SDMA fill command count error.");
    const uint32_t fill_count = Min(count, maxDwordCount);

    memset(packet_addr, 0, sizeof(SDMA_PKT_CONSTANT_FILL));

    packet_addr->HEADER_UNION.op = SDMA_OP_CONST_FILL;
    packet_addr->HEADER_UNION.fillsize = 2;  // DW fill

    packet_addr->DST_ADDR_LO_UNION.dst_addr_31_0 = ptrlow32(cur_ptr);
    packet_addr->DST_ADDR_HI_UNION.dst_addr_63_32 = ptrhigh32(cur_ptr);

    packet_addr->DATA_UNION.src_data_31_0 = value;

    packet_addr->COUNT_UNION.count = (fill_count + SizeToCountOffset) * sizeof(uint32_t);

    packet_addr++;
    cur_ptr += fill_count * sizeof(uint32_t);
    count -= fill_count;
  }
  assert(count == 0 && "SDMA fill command count error.");
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BuildPollCommand(
    char* cmd_addr, void* addr, uint32_t reference) {
  SDMA_PKT_POLL_REGMEM* packet_addr =
      reinterpret_cast<SDMA_PKT_POLL_REGMEM*>(cmd_addr);

  memset(packet_addr, 0, sizeof(SDMA_PKT_POLL_REGMEM));

  packet_addr->HEADER_UNION.op = SDMA_OP_POLL_REGMEM;
  packet_addr->HEADER_UNION.mem_poll = 1;
  packet_addr->HEADER_UNION.func = 0x3;  // IsEqual.
  packet_addr->ADDR_LO_UNION.addr_31_0 = ptrlow32(addr);
  packet_addr->ADDR_HI_UNION.addr_63_32 = ptrhigh32(addr);

  packet_addr->VALUE_UNION.value = reference;

  packet_addr->MASK_UNION.mask = 0xffffffff;  // Compare the whole content.

  packet_addr->DW5_UNION.interval = 0x04;
  packet_addr->DW5_UNION.retry_count = 0xfff;  // Retry forever.
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BuildAtomicDecrementCommand(
    char* cmd_addr, void* addr) {
  SDMA_PKT_ATOMIC* packet_addr = reinterpret_cast<SDMA_PKT_ATOMIC*>(cmd_addr);

  memset(packet_addr, 0, sizeof(SDMA_PKT_ATOMIC));

  packet_addr->HEADER_UNION.op = SDMA_OP_ATOMIC;
  packet_addr->HEADER_UNION.operation = SDMA_ATOMIC_ADD64;

  packet_addr->ADDR_LO_UNION.addr_31_0 = ptrlow32(addr);
  packet_addr->ADDR_HI_UNION.addr_63_32 = ptrhigh32(addr);

  packet_addr->SRC_DATA_LO_UNION.src_data_31_0 = 0xffffffff;
  packet_addr->SRC_DATA_HI_UNION.src_data_63_32 = 0xffffffff;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BuildGetGlobalTimestampCommand(
    char* cmd_addr, void* write_address) {
  SDMA_PKT_TIMESTAMP* packet_addr =
      reinterpret_cast<SDMA_PKT_TIMESTAMP*>(cmd_addr);

  memset(packet_addr, 0, sizeof(SDMA_PKT_TIMESTAMP));

  packet_addr->HEADER_UNION.op = SDMA_OP_TIMESTAMP;
  packet_addr->HEADER_UNION.sub_op = SDMA_SUBOP_TIMESTAMP_GET_GLOBAL;

  packet_addr->ADDR_LO_UNION.addr_31_0 = ptrlow32(write_address);
  packet_addr->ADDR_HI_UNION.addr_63_32 = ptrhigh32(write_address);
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BuildTrapCommand(char* cmd_addr) {
  SDMA_PKT_TRAP* packet_addr =
      reinterpret_cast<SDMA_PKT_TRAP*>(cmd_addr);

  memset(packet_addr, 0, sizeof(SDMA_PKT_TRAP));

  packet_addr->HEADER_UNION.op = SDMA_OP_TRAP;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::BuildHdpFlushCommand(
    char* cmd_addr) {
  assert(cmd_addr != NULL);
  SDMA_PKT_POLL_REGMEM* addr = reinterpret_cast<SDMA_PKT_POLL_REGMEM*>(cmd_addr);
  memcpy(addr, &hdp_flush_cmd, flush_command_size_);
}

template class BlitSdma<uint32_t, false, 0>;
template class BlitSdma<uint64_t, true, -1>;

}  // namespace amd
