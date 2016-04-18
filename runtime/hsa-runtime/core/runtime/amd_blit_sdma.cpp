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

#include "core/inc/amd_gpu_agent.h"
#include "core/inc/runtime.h"
#include "core/inc/signal.h"

namespace amd {
// SDMA packet for VI device.
// Reference: http://people.freedesktop.org/~agd5f/dma_packets.txt

const unsigned int SDMA_OP_COPY = 1;
const unsigned int SDMA_OP_FENCE = 5;
const unsigned int SDMA_OP_POLL_REGMEM = 8;
const unsigned int SDMA_OP_ATOMIC = 10;
const unsigned int SDMA_OP_CONST_FILL = 11;
const unsigned int SDMA_SUBOP_COPY_LINEAR = 0;
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

BlitSdma::BlitSdma()
    : core::Blit(),
      queue_size_(0),
      queue_start_addr_(NULL),
      fence_base_addr_(NULL),
      fence_pool_size_(0),
      fence_pool_counter_(0),
      cached_reserve_offset_(0),
      cached_commit_offset_(0) {
  std::memset(&queue_resource_, 0, sizeof(queue_resource_));
}

BlitSdma::~BlitSdma() {}

hsa_status_t BlitSdma::Initialize(const core::Agent& agent) {
  if (queue_start_addr_ != NULL && queue_size_ != 0) {
    // Already initialized.
    return HSA_STATUS_SUCCESS;
  }

  if (agent.device_type() != core::Agent::kAmdGpuDevice) {
    return HSA_STATUS_ERROR;
  }

  linear_copy_command_size_ = sizeof(SDMA_PKT_COPY_LINEAR);
  fill_command_size_ = sizeof(SDMA_PKT_CONSTANT_FILL);
  fence_command_size_ = sizeof(SDMA_PKT_FENCE);
  poll_command_size_ = sizeof(SDMA_PKT_POLL_REGMEM);
  atomic_command_size_ = sizeof(SDMA_PKT_ATOMIC);

  const uint32_t sync_command_size = fence_command_size_;
  const uint32_t max_num_copy_command =
      std::floor((static_cast<uint32_t>(queue_size_) - sync_command_size) /
                 linear_copy_command_size_);
  const uint32_t max_num_fill_command =
      std::floor((static_cast<uint32_t>(queue_size_) - sync_command_size) /
                 fill_command_size_);

  max_single_linear_copy_size_ = 0x3fffe0;
  max_total_linear_copy_size_ = static_cast<size_t>(
      std::min(static_cast<uint64_t>(SIZE_MAX),
               static_cast<uint64_t>(max_num_copy_command) *
                   static_cast<uint64_t>(max_single_linear_copy_size_)));

  max_single_fill_size_ = (1 << 22) - sizeof(uint32_t);
  max_total_fill_size_ = static_cast<size_t>(
      std::min(static_cast<uint64_t>(SIZE_MAX),
               static_cast<uint64_t>(max_num_fill_command) *
                   static_cast<uint64_t>(max_single_fill_size_)));

  const amd::GpuAgent& amd_gpu_agent = static_cast<const amd::GpuAgent&>(agent);

  if (amd_gpu_agent.isa()->version() != core::Isa::Version(8, 0, 3)) {
    assert(false && "Only for Fiji currently");
    return HSA_STATUS_ERROR;
  }

  // Allocate queue buffer.
  const size_t kPageSize = 4096;
  const size_t kSdmaQueueSize = 1024 * 1024;

  queue_size_ = kSdmaQueueSize;

  HsaMemFlags flags;
  flags.Value = 0;
  flags.ui32.HostAccess = 1;
  flags.ui32.AtomicAccessPartial = 1;
  flags.ui32.ExecuteAccess = 1;

  auto err = hsaKmtAllocMemory(amd_gpu_agent.node_id(), queue_size_, flags,
                               reinterpret_cast<void**>(&queue_start_addr_));

  if (err != HSAKMT_STATUS_SUCCESS) {
    assert(false && "SDMA queue memory allocation failure.");
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  HSAuint64 alternate_va;
  err = hsaKmtMapMemoryToGPU(queue_start_addr_, queue_size_, &alternate_va);

  if (err != HSAKMT_STATUS_SUCCESS) {
    assert(false && "AQL queue memory map failure.");
    Destroy();
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  std::memset(queue_start_addr_, 0, queue_size_);

  // Access kernel driver to initialize the queue control block
  // This call binds user mode queue object to underlying compute
  // device.
  const GpuAgent& gpu_agent = reinterpret_cast<const GpuAgent&>(agent);
  const HSA_QUEUE_TYPE kQueueType_ = HSA_QUEUE_SDMA;
  if (HSAKMT_STATUS_SUCCESS !=
      hsaKmtCreateQueue(gpu_agent.node_id(), kQueueType_, 100,
                        HSA_QUEUE_PRIORITY_MAXIMUM, queue_start_addr_,
                        queue_size_, NULL, &queue_resource_)) {
    Destroy();
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  cached_reserve_offset_ = *(queue_resource_.Queue_write_ptr);
  cached_commit_offset_ = cached_reserve_offset_;

  fence_pool_size_ =
      static_cast<uint32_t>(std::ceil(kSdmaQueueSize / fence_command_size_));

  fence_pool_mask_ = fence_pool_size_ - 1;

  fence_base_addr_ = reinterpret_cast<uint32_t*>(
      core::Runtime::runtime_singleton_->system_allocator()(
          fence_pool_size_ * sizeof(uint32_t), 256));

  if (fence_base_addr_ == NULL) {
    Destroy();
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t BlitSdma::Destroy(void) {
  // Release all allocated resources and reset them to zero.

  if (queue_resource_.QueueId != 0) {
    // Release queue resources from the kernel
    auto err = hsaKmtDestroyQueue(queue_resource_.QueueId);
    assert(err == HSAKMT_STATUS_SUCCESS);
    memset(&queue_resource_, 0, sizeof(queue_resource_));
  }

  if (queue_start_addr_ != NULL && queue_size_ != 0) {
    // Release queue buffer.
    hsaKmtUnmapMemoryToGPU(queue_start_addr_);
    hsaKmtFreeMemory(queue_start_addr_, queue_size_);
  }

  if (fence_base_addr_ != NULL) {
    core::Runtime::runtime_singleton_->system_deallocator()(fence_base_addr_);
  }

  queue_size_ = 0;
  queue_start_addr_ = NULL;
  cached_reserve_offset_ = 0;
  cached_commit_offset_ = 0;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t BlitSdma::SubmitLinearCopyCommand(void* dst, const void* src,
                                               size_t size) {
  if (size > max_total_linear_copy_size_) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // Break the copy into multiple copy operation incase the copy size exceeds
  // the SDMA linear copy limit.
  const uint32_t num_copy_command = static_cast<uint32_t>(
      std::ceil(static_cast<double>(size) / max_single_linear_copy_size_));

  const uint32_t total_copy_command_size =
      num_copy_command * linear_copy_command_size_;

  const uint32_t total_command_size =
      total_copy_command_size + fence_command_size_;

  const uint32_t kFenceValue = 2015;
  uint32_t* fence_addr = ObtainFenceObject();
  *fence_addr = 0;

  char* command_addr = AcquireWriteAddress(total_command_size);
  char* const command_addr_temp = command_addr;

  if (command_addr == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  BuildCopyCommand(command_addr, num_copy_command, dst, src, size);

  command_addr += total_copy_command_size;

  BuildFenceCommand(command_addr, fence_addr, kFenceValue);

  ReleaseWriteAddress(command_addr_temp, total_command_size);

  WaitFence(fence_addr, kFenceValue);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t BlitSdma::SubmitLinearCopyCommand(
    void* dst, const void* src, size_t size,
    std::vector<core::Signal*>& dep_signals, core::Signal& out_signal) {
  if (size > max_total_linear_copy_size_) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // The signal is 64 bit value, and poll checks for 32 bit value. So we
  // need to use two poll operations per dependent signal.
  const uint32_t num_poll_command =
      static_cast<uint32_t>(2 * dep_signals.size());
  const uint32_t total_poll_command_size =
      (num_poll_command * poll_command_size_);

  // Break the copy into multiple copy operation incase the copy size exceeds
  // the SDMA linear copy limit.
  const uint32_t num_copy_command = static_cast<uint32_t>(
      std::ceil(static_cast<double>(size) / max_single_linear_copy_size_));
  const uint32_t total_copy_command_size =
      num_copy_command * linear_copy_command_size_;

  const uint32_t total_command_size =
      total_poll_command_size + total_copy_command_size + atomic_command_size_ +
      fence_command_size_;

  const uint32_t kFenceValue = 2015;
  uint32_t* fence_addr = ObtainFenceObject();
  *fence_addr = 0;

  char* command_addr = AcquireWriteAddress(total_command_size);
  char* const command_addr_temp = command_addr;

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

  // Do the transfer after all polls are satisfied.
  BuildCopyCommand(command_addr, num_copy_command, dst, src, size);

  command_addr += total_copy_command_size;

  // After transfer is completed, decrement the signal.
  BuildAtomicDecrementCommand(command_addr, out_signal.ValueLocation());

  command_addr += atomic_command_size_;

  BuildFenceCommand(command_addr, fence_addr, kFenceValue);

  ReleaseWriteAddress(command_addr_temp, total_command_size);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t BlitSdma::SubmitLinearFillCommand(void* ptr, uint32_t value,
                                               size_t count) {
  const size_t size = count * sizeof(uint32_t);

  if (size > max_total_fill_size_) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // Break the copy into multiple copy operation incase the copy size exceeds
  // the SDMA linear copy limit.
  const uint32_t num_fill_command = static_cast<uint32_t>(
      std::ceil(static_cast<double>(size) / max_single_fill_size_));

  const uint32_t total_fill_command_size =
      num_fill_command * fill_command_size_;

  const uint32_t total_command_size =
      total_fill_command_size + fence_command_size_;

  char* command_addr = AcquireWriteAddress(total_command_size);
  char* const command_addr_temp = command_addr;

  if (command_addr == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  const uint32_t fill_command_size = fill_command_size_;
  size_t cur_size = 0;
  for (uint32_t i = 0; i < num_fill_command; ++i) {
    const uint32_t fill_size = static_cast<uint32_t>(
        std::min((size - cur_size), max_single_fill_size_));

    void* cur_ptr = static_cast<char*>(ptr) + cur_size;

    SDMA_PKT_CONSTANT_FILL* packet_addr =
        reinterpret_cast<SDMA_PKT_CONSTANT_FILL*>(command_addr);

    memset(packet_addr, 0, sizeof(SDMA_PKT_CONSTANT_FILL));

    packet_addr->HEADER_UNION.op = SDMA_OP_CONST_FILL;
    packet_addr->HEADER_UNION.fillsize = 2;  // DW fill

    packet_addr->DST_ADDR_LO_UNION.dst_addr_31_0 = ptrlow32(cur_ptr);
    packet_addr->DST_ADDR_HI_UNION.dst_addr_63_32 = ptrhigh32(cur_ptr);

    packet_addr->DATA_UNION.src_data_31_0 = value;

    packet_addr->COUNT_UNION.count = fill_size;

    command_addr += fill_command_size;
    cur_size += fill_size;
  }

  assert(cur_size == size);

  const uint32_t kFenceValue = 2015;
  uint32_t* fence_addr = ObtainFenceObject();
  *fence_addr = 0;

  BuildFenceCommand(command_addr, fence_addr, kFenceValue);

  ReleaseWriteAddress(command_addr_temp, total_command_size);

  WaitFence(fence_addr, kFenceValue);

  return HSA_STATUS_SUCCESS;
}

char* BlitSdma::AcquireWriteAddress(uint32_t cmd_size) {
  if (cmd_size > queue_size_) {
    return NULL;
  }

  while (true) {
    const uint32_t curr_offset =
        atomic::Load(&cached_reserve_offset_, std::memory_order_acquire);
    const uint32_t end_offset = curr_offset + cmd_size;

    if (end_offset >= queue_size_) {
      // Queue buffer is not enough to contain the new command.
      WrapQueue(cmd_size);
      continue;
    }

    const uint32_t curr_read_ptr_val =
        atomic::Load(queue_resource_.Queue_read_ptr, std::memory_order_acquire);
    if (curr_offset < curr_read_ptr_val && end_offset > curr_read_ptr_val) {
      // Queue is wrapping and there is not enough space to recycle.
      continue;
    }

    if (atomic::Cas(&cached_reserve_offset_, end_offset, curr_offset,
                    std::memory_order_release) == curr_offset) {
      return queue_start_addr_ + curr_offset;
    }
  }

  return NULL;
}

void BlitSdma::UpdateWriteAndDoorbellRegister(uint32_t current_offset,
                                              uint32_t new_offset) {
  while (true) {
    // Make sure that the address before ::current_offset is already released.
    // Otherwise the CP may read invalid packets.
    if (atomic::Load(&cached_commit_offset_, std::memory_order_acquire) ==
        current_offset) {
      if (core::Runtime::runtime_singleton_->flag().sdma_wait_idle()) {
        // TODO: remove when sdma wpointer issue is resolved.
        // Wait until the SDMA engine finish processing all packets before
        // updating the wptr and doorbell.
        while (atomic::Load(queue_resource_.Queue_read_ptr,
                            std::memory_order_acquire) != current_offset) {
          os::YieldThread();
        }
      }

      // Update write pointer and doorbel register.
      atomic::Store(queue_resource_.Queue_write_ptr, new_offset);

      std::atomic_thread_fence(std::memory_order_release);

      atomic::Store(queue_resource_.Queue_DoorBell, new_offset);

      std::atomic_thread_fence(std::memory_order_release);

      atomic::Store(&cached_commit_offset_, new_offset);
      break;
    }
  }
}

void BlitSdma::ReleaseWriteAddress(char* cmd_addr, uint32_t cmd_size) {
  assert(cmd_addr != NULL);
  assert(cmd_addr >= queue_start_addr_);

  if (cmd_size > queue_size_) {
    assert(false && "cmd_addr is outside the queue buffer range");
    return;
  }

  // Update write register.
  const uint32_t curent_offset = cmd_addr - queue_start_addr_;
  const uint32_t new_offset = curent_offset + cmd_size;
  UpdateWriteAndDoorbellRegister(curent_offset, new_offset);
}

void BlitSdma::WrapQueue(uint32_t cmd_size) {
  // Re-determine the offset into queue buffer where NOOP instructions
  // should be written.
  while (true) {
    const uint32_t full_offset = queue_size_ + 1;

    uint32_t curent_offset =
        atomic::Load(&cached_reserve_offset_, std::memory_order_acquire);
    const uint32_t end_offset = curent_offset + cmd_size;
    if (end_offset < queue_size_) {
      return;
    }

    if (curent_offset == full_offset) {
      // Another thread is already wrapping the queue.
      continue;
    }

    // Close reservation to queue temporarily by "making" it full.
    if (atomic::Cas(&cached_reserve_offset_, full_offset, curent_offset,
                    std::memory_order_release) == curent_offset) {
      // Wait till all reserved packets are commited.
      while (atomic::Load(&cached_commit_offset_, std::memory_order_acquire) !=
             curent_offset) {
        os::YieldThread();
      }

      // Fill the remainder of the queue with NOOP commands.
      char* noop_address = queue_start_addr_ + curent_offset;
      const size_t noop_commands_size = queue_size_ - curent_offset;
      memset(noop_address, 0, noop_commands_size);

      // Update write and doorbell registers to execute NOOP instructions.
      UpdateWriteAndDoorbellRegister(curent_offset, 0);

      // Open access to queue.
      atomic::Store(&cached_reserve_offset_, 0U, std::memory_order_release);
    }
  }
}

void BlitSdma::BuildFenceCommand(char* fence_command_addr, uint32_t* fence,
                                 uint32_t fence_value) {
  assert(fence_command_addr != NULL);
  SDMA_PKT_FENCE* packet_addr =
      reinterpret_cast<SDMA_PKT_FENCE*>(fence_command_addr);

  memset(packet_addr, 0, sizeof(SDMA_PKT_FENCE));

  packet_addr->HEADER_UNION.op = SDMA_OP_FENCE;

  packet_addr->ADDR_LO_UNION.addr_31_0 = ptrlow32(fence);

  packet_addr->ADDR_HI_UNION.addr_63_32 = ptrhigh32(fence);

  packet_addr->DATA_UNION.data = fence_value;
}

uint32_t* BlitSdma::ObtainFenceObject() {
  const uint32_t fence_index =
      atomic::Add(&fence_pool_counter_, 1U, std::memory_order_acquire);
  uint32_t* fence_addr = &fence_base_addr_[fence_index & fence_pool_mask_];
  assert(IsMultipleOf(fence_addr, 4));
  return fence_addr;
}

void BlitSdma::WaitFence(uint32_t* fence, uint32_t fence_value) {
  int spin_count = 51;
  while (atomic::Load(fence, std::memory_order_acquire) != fence_value) {
    if (--spin_count > 0) {
      continue;
    }
    os::YieldThread();
  }
}

void BlitSdma::BuildCopyCommand(char* cmd_addr, uint32_t num_copy_command,
                                void* dst, const void* src, size_t size) {
  size_t cur_size = 0;
  for (uint32_t i = 0; i < num_copy_command; ++i) {
    const uint32_t copy_size = static_cast<uint32_t>(
        std::min((size - cur_size), max_single_linear_copy_size_));

    void* cur_dst = static_cast<char*>(dst) + cur_size;
    const void* cur_src = static_cast<const char*>(src) + cur_size;

    SDMA_PKT_COPY_LINEAR* packet_addr =
        reinterpret_cast<SDMA_PKT_COPY_LINEAR*>(cmd_addr);

    memset(packet_addr, 0, sizeof(SDMA_PKT_COPY_LINEAR));

    packet_addr->HEADER_UNION.op = SDMA_OP_COPY;
    packet_addr->HEADER_UNION.sub_op = SDMA_SUBOP_COPY_LINEAR;

    packet_addr->COUNT_UNION.count = copy_size;

    packet_addr->SRC_ADDR_LO_UNION.src_addr_31_0 = ptrlow32(cur_src);
    packet_addr->SRC_ADDR_HI_UNION.src_addr_63_32 = ptrhigh32(cur_src);

    packet_addr->DST_ADDR_LO_UNION.dst_addr_31_0 = ptrlow32(cur_dst);
    packet_addr->DST_ADDR_HI_UNION.dst_addr_63_32 = ptrhigh32(cur_dst);

    cmd_addr += linear_copy_command_size_;
    cur_size += copy_size;
  }

  assert(cur_size == size);
}

void BlitSdma::BuildPollCommand(char* cmd_addr, void* addr,
                                uint32_t reference) {
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

void BlitSdma::BuildAtomicDecrementCommand(char* cmd_addr, void* addr) {
  SDMA_PKT_ATOMIC* packet_addr = reinterpret_cast<SDMA_PKT_ATOMIC*>(cmd_addr);

  memset(packet_addr, 0, sizeof(SDMA_PKT_ATOMIC));

  packet_addr->HEADER_UNION.op = SDMA_OP_ATOMIC;
  packet_addr->HEADER_UNION.operation = SDMA_ATOMIC_ADD64;

  packet_addr->ADDR_LO_UNION.addr_31_0 = ptrlow32(addr);
  packet_addr->ADDR_HI_UNION.addr_63_32 = ptrhigh32(addr);

  packet_addr->SRC_DATA_LO_UNION.src_data_31_0 = 0xffffffff;
  packet_addr->SRC_DATA_HI_UNION.src_data_63_32 = 0xffffffff;
}
}  // namespace amd
