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
#include "core/inc/signal.h"

namespace amd {
// SDMA packet for VI device.
// Reference: http://people.freedesktop.org/~agd5f/dma_packets.txt

const unsigned int SDMA_OP_COPY = 1;
const unsigned int SDMA_OP_FENCE = 5;
const unsigned int SDMA_OP_TRAP = 6;
const unsigned int SDMA_OP_POLL_REGMEM = 8;
const unsigned int SDMA_OP_ATOMIC = 10;
const unsigned int SDMA_OP_CONST_FILL = 11;
const unsigned int SDMA_OP_TIMESTAMP = 13;
const unsigned int SDMA_SUBOP_COPY_LINEAR = 0;
const unsigned int SDMA_SUBOP_TIMESTAMP_GET_GLOBAL = 2;
const unsigned int SDMA_ATOMIC_ADD64 = 47;

typedef struct SDMA_PKT_COPY_LINEAR_TAG {
  union {
    struct {
      unsigned int op : 8;
      unsigned int sub_op : 8;
      unsigned int extra_info : 16;
    };
    unsigned int DW_0_DATA;
  } HEADER_UNION;

  union {
    struct {
      unsigned int count : 22;
      unsigned int reserved_0 : 10;
    };
    unsigned int DW_1_DATA;
  } COUNT_UNION;

  union {
    struct {
      unsigned int reserved_0 : 16;
      unsigned int dst_swap : 2;
      unsigned int reserved_1 : 6;
      unsigned int src_swap : 2;
      unsigned int reserved_2 : 6;
    };
    unsigned int DW_2_DATA;
  } PARAMETER_UNION;

  union {
    struct {
      unsigned int src_addr_31_0 : 32;
    };
    unsigned int DW_3_DATA;
  } SRC_ADDR_LO_UNION;

  union {
    struct {
      unsigned int src_addr_63_32 : 32;
    };
    unsigned int DW_4_DATA;
  } SRC_ADDR_HI_UNION;

  union {
    struct {
      unsigned int dst_addr_31_0 : 32;
    };
    unsigned int DW_5_DATA;
  } DST_ADDR_LO_UNION;

  union {
    struct {
      unsigned int dst_addr_63_32 : 32;
    };
    unsigned int DW_6_DATA;
  } DST_ADDR_HI_UNION;
} SDMA_PKT_COPY_LINEAR;

typedef struct SDMA_PKT_CONSTANT_FILL_TAG {
  union {
    struct {
      unsigned int op : 8;
      unsigned int sub_op : 8;
      unsigned int sw : 2;
      unsigned int reserved_0 : 12;
      unsigned int fillsize : 2;
    };
    unsigned int DW_0_DATA;
  } HEADER_UNION;

  union {
    struct {
      unsigned int dst_addr_31_0 : 32;
    };
    unsigned int DW_1_DATA;
  } DST_ADDR_LO_UNION;

  union {
    struct {
      unsigned int dst_addr_63_32 : 32;
    };
    unsigned int DW_2_DATA;
  } DST_ADDR_HI_UNION;

  union {
    struct {
      unsigned int src_data_31_0 : 32;
    };
    unsigned int DW_3_DATA;
  } DATA_UNION;

  union {
    struct {
      unsigned int count : 22;
      unsigned int reserved_0 : 10;
    };
    unsigned int DW_4_DATA;
  } COUNT_UNION;
} SDMA_PKT_CONSTANT_FILL;

typedef struct SDMA_PKT_FENCE_TAG {
  union {
    struct {
      unsigned int op : 8;
      unsigned int sub_op : 8;
      unsigned int reserved_0 : 16;
    };
    unsigned int DW_0_DATA;
  } HEADER_UNION;

  union {
    struct {
      unsigned int addr_31_0 : 32;
    };
    unsigned int DW_1_DATA;
  } ADDR_LO_UNION;

  union {
    struct {
      unsigned int addr_63_32 : 32;
    };
    unsigned int DW_2_DATA;
  } ADDR_HI_UNION;

  union {
    struct {
      unsigned int data : 32;
    };
    unsigned int DW_3_DATA;
  } DATA_UNION;
} SDMA_PKT_FENCE;

typedef struct SDMA_PKT_POLL_REGMEM_TAG {
  union {
    struct {
      unsigned int op : 8;
      unsigned int sub_op : 8;
      unsigned int reserved_0 : 10;
      unsigned int hdp_flush : 1;
      unsigned int reserved_1 : 1;
      unsigned int func : 3;
      unsigned int mem_poll : 1;
    };
    unsigned int DW_0_DATA;
  } HEADER_UNION;

  union {
    struct {
      unsigned int addr_31_0 : 32;
    };
    unsigned int DW_1_DATA;
  } ADDR_LO_UNION;

  union {
    struct {
      unsigned int addr_63_32 : 32;
    };
    unsigned int DW_2_DATA;
  } ADDR_HI_UNION;

  union {
    struct {
      unsigned int value : 32;
    };
    unsigned int DW_3_DATA;
  } VALUE_UNION;

  union {
    struct {
      unsigned int mask : 32;
    };
    unsigned int DW_4_DATA;
  } MASK_UNION;

  union {
    struct {
      unsigned int interval : 16;
      unsigned int retry_count : 12;
      unsigned int reserved_0 : 4;
    };
    unsigned int DW_5_DATA;
  } DW5_UNION;
} SDMA_PKT_POLL_REGMEM;

typedef struct SDMA_PKT_ATOMIC_TAG {
  union {
    struct {
      unsigned int op : 8;
      unsigned int sub_op : 8;
      unsigned int l : 1;
      unsigned int reserved_0 : 8;
      unsigned int operation : 7;
    };
    unsigned int DW_0_DATA;
  } HEADER_UNION;

  union {
    struct {
      unsigned int addr_31_0 : 32;
    };
    unsigned int DW_1_DATA;
  } ADDR_LO_UNION;

  union {
    struct {
      unsigned int addr_63_32 : 32;
    };
    unsigned int DW_2_DATA;
  } ADDR_HI_UNION;

  union {
    struct {
      unsigned int src_data_31_0 : 32;
    };
    unsigned int DW_3_DATA;
  } SRC_DATA_LO_UNION;

  union {
    struct {
      unsigned int src_data_63_32 : 32;
    };
    unsigned int DW_4_DATA;
  } SRC_DATA_HI_UNION;

  union {
    struct {
      unsigned int cmp_data_31_0 : 32;
    };
    unsigned int DW_5_DATA;
  } CMP_DATA_LO_UNION;

  union {
    struct {
      unsigned int cmp_data_63_32 : 32;
    };
    unsigned int DW_6_DATA;
  } CMP_DATA_HI_UNION;

  union {
    struct {
      unsigned int loop_interval : 13;
      unsigned int reserved_0 : 19;
    };
    unsigned int DW_7_DATA;
  } LOOP_UNION;
} SDMA_PKT_ATOMIC;

typedef struct SDMA_PKT_TIMESTAMP_TAG {
  union {
    struct {
      unsigned int op : 8;
      unsigned int sub_op : 8;
      unsigned int reserved_0 : 16;
    };
    unsigned int DW_0_DATA;
  } HEADER_UNION;

  union {
    struct {
      unsigned int addr_31_0 : 32;
    };
    unsigned int DW_1_DATA;
  } ADDR_LO_UNION;

  union {
    struct {
      unsigned int addr_63_32 : 32;
    };
    unsigned int DW_2_DATA;
  } ADDR_HI_UNION;

} SDMA_PKT_TIMESTAMP;

typedef struct SDMA_PKT_TRAP_TAG {
  union {
    struct {
      unsigned int op : 8;
      unsigned int sub_op : 8;
      unsigned int reserved_0 : 16;
    };
    unsigned int DW_0_DATA;
  } HEADER_UNION;

  union {
    struct {
      unsigned int int_ctx : 28;
      unsigned int reserved_1 : 4;
    };
    unsigned int DW_1_DATA;
  } INT_CONTEXT_UNION;
} SDMA_PKT_TRAP;

// Initialize Hdp flush packet for use on sDMA of devices
// from Gfx9 or new  family
static const SDMA_PKT_POLL_REGMEM hdp_flush_cmd_ {
                                        { SDMA_OP_POLL_REGMEM },
                                        { 0x00 },
                                        { 0x80000000 },
                                        { 0x00 },
                                        { 0x00 },
                                        { 0x00 },
};

// Version of sDMA microcode supporting Hdp flush
static const uint16_t sdma_version_ = 0x01A5;

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
const size_t BlitSdmaBase::kMaxSingleCopySize = 0x3fffe0;  // From HW documentation
const size_t BlitSdmaBase::kMaxSingleFillSize = 0x3fffe0;

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
      fence_base_addr_(NULL),
      fence_pool_size_(0),
      fence_pool_counter_(0),
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
  agent_ = reinterpret_cast<amd::GpuAgent*>(&const_cast<core::Agent&>(agent));

  if (queue_start_addr_ != NULL) {
    // Already initialized.
    return HSA_STATUS_SUCCESS;
  }

  if (agent.device_type() != core::Agent::kAmdGpuDevice) {
    return HSA_STATUS_ERROR;
  }

  const amd::GpuAgentInt& amd_gpu_agent =
      static_cast<const amd::GpuAgentInt&>(agent);

  if (HSA_PROFILE_FULL == amd_gpu_agent.profile()) {
    assert(false && "Only support SDMA for dgpu currently");
    return HSA_STATUS_ERROR;
  }

  if (amd_gpu_agent.isa()->version() == core::Isa::Version(7, 0, 1) ||
      amd_gpu_agent.isa()->GetMajorVersion() == 9) {
    platform_atomic_support_ = false;
  }

  // Determine if sDMA microcode supports HDP flush command
  if (agent_->GetSdmaMicrocodeVersion() >= sdma_version_) {
    hdp_flush_support_ = true;
  }

  // Allocate queue buffer.
  queue_start_addr_ = (char*)core::Runtime::runtime_singleton_->system_allocator()(
      kQueueSize, 0x1000, core::MemoryRegion::AllocateExecutable);

  if (queue_start_addr_ == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  std::memset(queue_start_addr_, 0, kQueueSize);

  // Access kernel driver to initialize the queue control block
  // This call binds user mode queue object to underlying compute
  // device.
  const HSA_QUEUE_TYPE kQueueType_ = HSA_QUEUE_SDMA;
  if (HSAKMT_STATUS_SUCCESS != hsaKmtCreateQueue(amd_gpu_agent.node_id(), kQueueType_, 100,
                                                 HSA_QUEUE_PRIORITY_MAXIMUM, queue_start_addr_,
                                                 kQueueSize, NULL, &queue_resource_)) {
    Destroy(agent);
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  cached_reserve_index_ = *reinterpret_cast<RingIndexTy*>(queue_resource_.Queue_write_ptr);
  cached_commit_index_ = cached_reserve_index_;

  fence_pool_size_ =
      static_cast<uint32_t>((kQueueSize + fence_command_size_ - 1) / fence_command_size_);

  fence_pool_mask_ = fence_pool_size_ - 1;

  fence_base_addr_ = reinterpret_cast<uint32_t*>(
      core::Runtime::runtime_singleton_->system_allocator()(
          fence_pool_size_ * sizeof(uint32_t), 256,
          core::MemoryRegion::AllocateNoFlags));

  if (fence_base_addr_ == NULL) {
    Destroy(agent);
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

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

  if (fence_base_addr_ != NULL) {
    core::Runtime::runtime_singleton_->system_deallocator()(fence_base_addr_);
  }

  queue_start_addr_ = NULL;
  cached_reserve_index_ = 0;
  cached_commit_index_ = 0;

  return HSA_STATUS_SUCCESS;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::SubmitLinearCopyCommand(
    void* dst, const void* src, size_t size) {
  // Break the copy into multiple copy operation incase the copy size exceeds
  // the SDMA linear copy limit.
  const uint32_t num_copy_command = (size + kMaxSingleCopySize - 1) / kMaxSingleCopySize;

  const uint32_t total_copy_command_size =
      num_copy_command * linear_copy_command_size_;

  // Add space for acquire or release Hdp flush command
  uint32_t flush_cmd_size = 0;
  if (core::Runtime::runtime_singleton_->flag().enable_sdma_hdp_flush()) {
    if ((HwIndexMonotonic) && (hdp_flush_support_)) {
      flush_cmd_size = flush_command_size_;
    }
  }

  const uint32_t total_command_size =
      total_copy_command_size + fence_command_size_ + flush_cmd_size;

  const uint32_t kFenceValue = 2015;
  uint32_t* fence_addr = ObtainFenceObject();
  *fence_addr = 0;

  RingIndexTy curr_index;
  char* command_addr = AcquireWriteAddress(total_command_size, curr_index);

  if (command_addr == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // Determine if a Hdp flush cmd is required at the top of cmd stream
  if (core::Runtime::runtime_singleton_->flag().enable_sdma_hdp_flush()) {
    if ((HwIndexMonotonic) && (hdp_flush_support_) && (sdma_h2d_ == false)) {
      BuildHdpFlushCommand(command_addr);
      command_addr += flush_command_size_;
    }
  }

  BuildCopyCommand(command_addr, num_copy_command, dst, src, size);
  command_addr += total_copy_command_size;

  // Determine if a Hdp flush cmd is required at the end of cmd stream
  if (core::Runtime::runtime_singleton_->flag().enable_sdma_hdp_flush()) {
    if ((HwIndexMonotonic) && (hdp_flush_support_) && (sdma_h2d_)) {
      BuildHdpFlushCommand(command_addr);
      command_addr += flush_command_size_;
    }
  }

  BuildFenceCommand(command_addr, fence_addr, kFenceValue);

  ReleaseWriteAddress(curr_index, total_command_size);

  WaitFence(fence_addr, kFenceValue);

  return HSA_STATUS_SUCCESS;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::SubmitLinearCopyCommand(
    void* dst, const void* src, size_t size, std::vector<core::Signal*>& dep_signals,
    core::Signal& out_signal) {
  // The signal is 64 bit value, and poll checks for 32 bit value. So we
  // need to use two poll operations per dependent signal.
  const uint32_t num_poll_command =
      static_cast<uint32_t>(2 * dep_signals.size());
  const uint32_t total_poll_command_size =
      (num_poll_command * poll_command_size_);

  // Break the copy into multiple copy operation incase the copy size exceeds
  // the SDMA linear copy limit.
  const uint32_t num_copy_command = (size + kMaxSingleCopySize - 1) / kMaxSingleCopySize;
  const uint32_t total_copy_command_size =
      num_copy_command * linear_copy_command_size_;

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

  const uint32_t total_command_size =
      total_poll_command_size + total_copy_command_size + sync_command_size +
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

  // Do the transfer after all polls are satisfied.
  BuildCopyCommand(command_addr, num_copy_command, dst, src, size);
  command_addr += total_copy_command_size;

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
    uint32_t* signal_value_location =
        reinterpret_cast<uint32_t*>(out_signal.ValueLocation());
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
    BuildFenceCommand(command_addr, reinterpret_cast<uint32_t*>(
                                        out_signal.signal_.event_mailbox_ptr),
                      static_cast<uint32_t>(out_signal.signal_.event_id));
    command_addr += fence_command_size_;

    BuildTrapCommand(command_addr);
  }

  ReleaseWriteAddress(curr_index, total_command_size);

  return HSA_STATUS_SUCCESS;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
hsa_status_t BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::SubmitLinearFillCommand(
    void* ptr, uint32_t value, size_t count) {
  const size_t size = count * sizeof(uint32_t);

  // Break the copy into multiple copy operation incase the copy size exceeds
  // the SDMA linear copy limit.
  const uint32_t num_fill_command = (size + kMaxSingleFillSize - 1) / kMaxSingleFillSize;

  const uint32_t total_fill_command_size =
      num_fill_command * fill_command_size_;

  // Add space for acquire or release Hdp flush command
  uint32_t flush_cmd_size = 0;
  if (core::Runtime::runtime_singleton_->flag().enable_sdma_hdp_flush()) {
    if ((HwIndexMonotonic) && (hdp_flush_support_)) {
      flush_cmd_size = flush_command_size_;
    }
  }

  const uint32_t total_command_size =
      total_fill_command_size + fence_command_size_ + flush_cmd_size;

  RingIndexTy curr_index;
  char* command_addr = AcquireWriteAddress(total_command_size, curr_index);

  if (command_addr == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  const uint32_t fill_command_size = fill_command_size_;
  size_t cur_size = 0;
  for (uint32_t i = 0; i < num_fill_command; ++i) {
    const uint32_t fill_size =
        static_cast<uint32_t>(std::min((size - cur_size), kMaxSingleFillSize));

    void* cur_ptr = static_cast<char*>(ptr) + cur_size;

    SDMA_PKT_CONSTANT_FILL* packet_addr =
        reinterpret_cast<SDMA_PKT_CONSTANT_FILL*>(command_addr);

    memset(packet_addr, 0, sizeof(SDMA_PKT_CONSTANT_FILL));

    packet_addr->HEADER_UNION.op = SDMA_OP_CONST_FILL;
    packet_addr->HEADER_UNION.fillsize = 2;  // DW fill

    packet_addr->DST_ADDR_LO_UNION.dst_addr_31_0 = ptrlow32(cur_ptr);
    packet_addr->DST_ADDR_HI_UNION.dst_addr_63_32 = ptrhigh32(cur_ptr);

    packet_addr->DATA_UNION.src_data_31_0 = value;

    packet_addr->COUNT_UNION.count = fill_size + SizeToCountOffset;

    command_addr += fill_command_size;
    cur_size += fill_size;
  }

  assert(cur_size == size);

  // Determine if a Hdp flush cmd is required at the end of cmd stream
  if (core::Runtime::runtime_singleton_->flag().enable_sdma_hdp_flush()) {
    if ((HwIndexMonotonic) && (hdp_flush_support_)) {
      BuildHdpFlushCommand(command_addr);
      command_addr += flush_command_size_;
    }
  }

  const uint32_t kFenceValue = 2015;
  uint32_t* fence_addr = ObtainFenceObject();
  *fence_addr = 0;

  BuildFenceCommand(command_addr, fence_addr, kFenceValue);

  ReleaseWriteAddress(curr_index, total_command_size);

  WaitFence(fence_addr, kFenceValue);

  return HSA_STATUS_SUCCESS;
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
uint32_t* BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::ObtainFenceObject() {
  const uint32_t fence_index =
      atomic::Add(&fence_pool_counter_, 1U, std::memory_order_acquire);
  uint32_t* fence_addr = &fence_base_addr_[fence_index & fence_pool_mask_];
  assert(IsMultipleOf(fence_addr, 4));
  return fence_addr;
}

template <typename RingIndexTy, bool HwIndexMonotonic, int SizeToCountOffset>
void BlitSdma<RingIndexTy, HwIndexMonotonic, SizeToCountOffset>::WaitFence(uint32_t* fence,
                                                                           uint32_t fence_value) {
  int spin_count = 51;
  while (atomic::Load(fence, std::memory_order_acquire) != fence_value) {
    if (--spin_count > 0) {
      continue;
    }
    os::YieldThread();
  }
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
  memcpy(addr, &hdp_flush_cmd_, flush_command_size_);
}

template class BlitSdma<uint32_t, false, 0>;
template class BlitSdma<uint64_t, true, -1>;

}  // namespace amd
