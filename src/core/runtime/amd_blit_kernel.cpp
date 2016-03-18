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

#include "core/inc/amd_blit_kernel.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "core/inc/amd_blit_kernel_kv.h"
#include "core/inc/amd_blit_kernel_vi.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/inc/hsa_internal.h"
#include "core/util/utils.h"

namespace amd {
const uint32_t BlitKernel::kGroupSize = 256;
const size_t BlitKernel::kMaxCopyCount = AlignDown(UINT32_MAX, kGroupSize);
const size_t BlitKernel::kMaxFillCount = AlignDown(UINT32_MAX, kGroupSize);

static const uint16_t kInvalidPacketHeader = HSA_PACKET_TYPE_INVALID;

BlitKernel::BlitKernel()
    : core::Blit(),
      copy_code_handle_(0),
      fill_code_handle_(0),
      queue_(NULL),
      cached_index_(0),
      kernarg_(NULL),
      kernarg_async_(NULL),
      kernarg_async_mask_(0),
      kernarg_async_counter_(0),
      code_arg_buffer_(NULL) {
  completion_signal_.handle = 0;
}

BlitKernel::~BlitKernel() {}

hsa_status_t BlitKernel::Initialize(const core::Agent& agent) {
  hsa_agent_t agent_handle = agent.public_handle();

  uint32_t features = 0;
  hsa_status_t status =
      HSA::hsa_agent_get_info(agent_handle, HSA_AGENT_INFO_FEATURE, &features);
  if (status != HSA_STATUS_SUCCESS) {
    return status;
  }

  if ((features & HSA_AGENT_FEATURE_KERNEL_DISPATCH) == 0) {
    return HSA_STATUS_ERROR;
  }

  // Need queue buffer that can cover the max size of local memory.
  const uint64_t kGpuVmVaSize = 1ULL << 40;
  const uint32_t kRequiredQueueSize = NextPow2(static_cast<uint32_t>(
      std::ceil(static_cast<double>(kGpuVmVaSize) / kMaxCopyCount)));

  uint32_t max_queue_size = 0;
  status = HSA::hsa_agent_get_info(agent_handle, HSA_AGENT_INFO_QUEUE_MAX_SIZE,
                                   &max_queue_size);

  if (HSA_STATUS_SUCCESS != status) {
    return status;
  }

  if (max_queue_size < kRequiredQueueSize) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  status =
      HSA::hsa_queue_create(agent_handle, kRequiredQueueSize,
                            HSA_QUEUE_TYPE_MULTI, NULL, NULL, 0, 0, &queue_);

  if (HSA_STATUS_SUCCESS != status) {
    return status;
  }

  queue_bitmask_ = queue_->size - 1;

  cached_index_ = 0;

  void* copy_raw_obj_mem = NULL;
  size_t copy_akc_size = 0;
  size_t copy_akc_offset = 0;

  void* copy_aligned_raw_obj_mem = NULL;
  size_t copy_aligned_akc_size = 0;
  size_t copy_aligned_akc_offset = 0;

  void* fill_raw_obj_mem = NULL;
  size_t fill_akc_size = 0;
  size_t fill_akc_offset = 0;

  switch (agent.isa()->GetMajorVersion()) {
    case 7:
      copy_raw_obj_mem = kVectorCopyKvObject;
      copy_akc_size = HSA_VECTOR_COPY_KV_AKC_SIZE;
      copy_akc_offset = HSA_VECTOR_COPY_KV_AKC_OFFSET;

      copy_aligned_raw_obj_mem = kVectorCopyAlignedKvObject;
      copy_aligned_akc_size = HSA_VECTOR_COPY_ALIGNED_KV_AKC_SIZE;
      copy_aligned_akc_offset = HSA_VECTOR_COPY_ALIGNED_KV_AKC_OFFSET;

      fill_raw_obj_mem = kFillMemoryKvObject;
      fill_akc_size = HSA_FILL_MEMORY_KV_AKC_SIZE;
      fill_akc_offset = HSA_FILL_MEMORY_KV_AKC_OFFSET;
      break;
    case 8:
      copy_raw_obj_mem = kVectorCopyViObject;
      copy_akc_size = HSA_VECTOR_COPY_VI_AKC_SIZE;
      copy_akc_offset = HSA_VECTOR_COPY_VI_AKC_OFFSET;

      copy_aligned_raw_obj_mem = kVectorCopyAlignedViObject;
      copy_aligned_akc_size = HSA_VECTOR_COPY_ALIGNED_VI_AKC_SIZE;
      copy_aligned_akc_offset = HSA_VECTOR_COPY_ALIGNED_VI_AKC_OFFSET;

      fill_raw_obj_mem = kFillMemoryViObject;
      fill_akc_size = HSA_FILL_MEMORY_VI_AKC_SIZE;
      fill_akc_offset = HSA_FILL_MEMORY_VI_AKC_OFFSET;
      break;
    default:
      assert(false && "Only gfx7 and gfx8 are supported");
      break;
  }

  static const size_t kKernArgSize =
      std::max(sizeof(KernelCopyArgs), sizeof(KernelFillArgs));
  const size_t total_alloc_size = AlignUp(
      AlignUp(copy_akc_size, 256) + AlignUp(copy_aligned_akc_size, 256) +
          AlignUp(fill_akc_size, 256) + AlignUp(kKernArgSize, 16),
      4096);

  amd_kernel_code_t *code_ptr = nullptr;
  code_arg_buffer_ = core::Runtime::runtime_singleton_->system_allocator()(
      total_alloc_size, 4096);

  char* akc_arg = reinterpret_cast<char*>(code_arg_buffer_);
  memcpy(akc_arg,
         reinterpret_cast<const char*>(copy_raw_obj_mem) + copy_akc_offset,
         copy_akc_size);
  copy_code_handle_ = reinterpret_cast<uint64_t>(akc_arg);
  code_ptr = (amd_kernel_code_t*)(copy_code_handle_);
  code_ptr->runtime_loader_kernel_symbol = 0;
  akc_arg += copy_akc_size;

  akc_arg = AlignUp(akc_arg, 256);
  memcpy(akc_arg, reinterpret_cast<const char*>(copy_aligned_raw_obj_mem) +
                      copy_aligned_akc_offset,
         copy_aligned_akc_size);
  copy_aligned_code_handle_ = reinterpret_cast<uint64_t>(akc_arg);
  code_ptr = (amd_kernel_code_t*)(copy_aligned_code_handle_);
  code_ptr->runtime_loader_kernel_symbol = 0;
  akc_arg += copy_aligned_akc_size;

  akc_arg = AlignUp(akc_arg, 256);
  memcpy(akc_arg,
         reinterpret_cast<const char*>(fill_raw_obj_mem) + fill_akc_offset,
         fill_akc_size);
  fill_code_handle_ = reinterpret_cast<uint64_t>(akc_arg);
  code_ptr = (amd_kernel_code_t*)(fill_code_handle_);
  code_ptr->runtime_loader_kernel_symbol = 0;
  akc_arg += fill_akc_size;

  akc_arg = AlignUp(akc_arg, 16);
  kernarg_ = akc_arg;

  status = HSA::hsa_signal_create(1, 0, NULL, &completion_signal_);
  if (HSA_STATUS_SUCCESS != status) {
    return status;
  }

  kernarg_async_ = reinterpret_cast<KernelCopyArgs*>(
      core::Runtime::runtime_singleton_->system_allocator()(
          kRequiredQueueSize * AlignUp(sizeof(KernelCopyArgs), 16), 16));

  kernarg_async_mask_ = kRequiredQueueSize - 1;

  // TODO(bwicakso): remove this code when execute permission level is not
  // mandatory.
  if (((amd::GpuAgent&)agent).profile() == HSA_PROFILE_FULL) {
#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
    DWORD old_protect = 0;
    const DWORD new_protect = PAGE_EXECUTE_READWRITE;
    if (!VirtualProtect(code_arg_buffer_, total_alloc_size, new_protect,
                        &old_protect)) {
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
#else
    if (0 != mprotect(code_arg_buffer_, total_alloc_size,
                      PROT_READ | PROT_WRITE | PROT_EXEC)) {
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
#endif
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t BlitKernel::Destroy(void) {
  std::lock_guard<std::mutex> guard(lock_);

  if (queue_ != NULL) {
    HSA::hsa_queue_destroy(queue_);
  }

  if (kernarg_async_ != NULL) {
    core::Runtime::runtime_singleton_->system_deallocator()(kernarg_async_);
  }

  if (code_arg_buffer_ != NULL) {
    core::Runtime::runtime_singleton_->system_deallocator()(code_arg_buffer_);
  }

  if (completion_signal_.handle != 0) {
    HSA::hsa_signal_destroy(completion_signal_);
  }

  return HSA_STATUS_SUCCESS;
}

static bool IsSystemMemory(void* address) {
  static const uint64_t kLimitSystem = 1ULL << 48;
  return (reinterpret_cast<uint64_t>(address) < kLimitSystem);
}

hsa_status_t BlitKernel::SubmitLinearCopyCommand(void* dst, const void* src,
                                                 size_t size) {
  assert(copy_code_handle_ != 0);

  std::lock_guard<std::mutex> guard(lock_);

  HSA::hsa_signal_store_relaxed(completion_signal_, 1);

  const size_t kAlignmentChar = 1;
  const size_t kAlignmentUin32 = 4;
  const size_t kAlignmentVec4 = 16;
  const size_t copy_granule =
      (IsMultipleOf(dst, kAlignmentVec4) && IsMultipleOf(src, kAlignmentVec4) &&
       IsMultipleOf(size, kAlignmentVec4))
          ? kAlignmentVec4
          : (IsMultipleOf(dst, kAlignmentUin32) &&
             IsMultipleOf(src, kAlignmentUin32) &&
             IsMultipleOf(size, kAlignmentUin32))
                ? kAlignmentUin32
                : kAlignmentChar;

  size = size / copy_granule;

  const uint32_t num_copy_packet = static_cast<uint32_t>(
      std::ceil(static_cast<double>(size) / kMaxCopyCount));

  // Reserve write index for copy + fence packet.
  uint64_t write_index = AcquireWriteIndex(num_copy_packet);

  const uint32_t last_copy_index = num_copy_packet - 1;
  size_t total_copy_count = 0;
  for (uint32_t i = 0; i < num_copy_packet; ++i) {
    // Setup arguments.
    const uint32_t copy_count = static_cast<uint32_t>(
        std::min((size - total_copy_count), kMaxCopyCount));

    void* cur_dst = static_cast<char*>(dst) + (total_copy_count * copy_granule);
    const void* cur_src =
        static_cast<const char*>(src) + (total_copy_count * copy_granule);

    KernelCopyArgs* args = ObtainAsyncKernelCopyArg();
    assert(args != NULL);
    assert(IsMultipleOf(args, 16));

    args->src = cur_src;
    args->dst = cur_dst;
    args->size = copy_count;
    args->use_vector = (copy_granule == kAlignmentVec4) ? 1 : 0;

    const uint32_t grid_size_x =
        AlignUp(static_cast<uint32_t>(copy_count), kGroupSize);

    // This assert to make sure kMaxCopySize is not changed to a number that
    // could cause overflow to packet.grid_size_x.
    assert(grid_size_x >= copy_count);

    hsa_signal_t signal = {(i == last_copy_index) ? completion_signal_.handle
                                                  : 0};
    PopulateQueue(write_index + i, ((copy_granule == kAlignmentChar)
                                        ? copy_code_handle_
                                        : copy_aligned_code_handle_),
                  args, grid_size_x, signal);

    total_copy_count += copy_count;
  }

  // Launch copy packet.
  ReleaseWriteIndex(write_index, num_copy_packet);

  // Wait for the packet to finish.
  if (HSA::hsa_signal_wait_acquire(completion_signal_, HSA_SIGNAL_CONDITION_LT,
                                   1, uint64_t(-1),
                                   HSA_WAIT_STATE_ACTIVE) != 0) {
    // Signal wait returned unexpected value.
    return HSA_STATUS_ERROR;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t BlitKernel::SubmitLinearCopyCommand(
    void* dst, const void* src, size_t size,
    std::vector<core::Signal*>& dep_signals, core::Signal& out_signal) {
  (copy_code_handle_ != 0);
  const size_t kAlignmentChar = 1;
  const size_t kAlignmentUin32 = 4;
  const size_t kAlignmentVec4 = 16;
  const size_t copy_granule =
      (IsMultipleOf(dst, kAlignmentVec4) && IsMultipleOf(src, kAlignmentVec4) &&
       IsMultipleOf(size, kAlignmentVec4))
          ? kAlignmentVec4
          : (IsMultipleOf(dst, kAlignmentUin32) &&
             IsMultipleOf(src, kAlignmentUin32) &&
             IsMultipleOf(size, kAlignmentUin32))
                ? kAlignmentUin32
                : kAlignmentChar;

  size = size / copy_granule;

  const uint32_t num_copy_packet = static_cast<uint32_t>(
      std::ceil(static_cast<double>(size) / kMaxCopyCount));

  const uint32_t num_barrier_packet =
      static_cast<uint32_t>(std::ceil(dep_signals.size() / 5.0f));

  // Reserve write index for copy + fence packet.
  const uint32_t total_num_packet = num_barrier_packet + num_copy_packet;

  uint64_t write_index = AcquireWriteIndex(total_num_packet);
  uint64_t write_index_temp = write_index;

  const uint16_t kBarrierPacketHeader =
      (HSA_PACKET_TYPE_BARRIER_AND << HSA_PACKET_HEADER_TYPE) |
      (1 << HSA_PACKET_HEADER_BARRIER) |
      (HSA_FENCE_SCOPE_NONE << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
      (HSA_FENCE_SCOPE_AGENT << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);

  hsa_barrier_and_packet_t barrier_packet = {0};
  barrier_packet.header = HSA_PACKET_TYPE_INVALID;

  hsa_barrier_and_packet_t* queue_buffer =
      reinterpret_cast<hsa_barrier_and_packet_t*>(queue_->base_address);

  const size_t dep_signal_count = dep_signals.size();
  for (size_t i = 0; i < dep_signal_count; ++i) {
    const size_t idx = i % 5;
    barrier_packet.dep_signal[idx] = core::Signal::Convert(dep_signals[i]);
    if (i == (dep_signal_count - 1) || idx == 4) {
      std::atomic_thread_fence(std::memory_order_acquire);
      queue_buffer[(write_index)&queue_bitmask_] = barrier_packet;
      std::atomic_thread_fence(std::memory_order_release);
      queue_buffer[(write_index)&queue_bitmask_].header = kBarrierPacketHeader;

      ++write_index;

      memset(&barrier_packet, 0, sizeof(hsa_barrier_and_packet_t));
      barrier_packet.header = HSA_PACKET_TYPE_INVALID;
    }
  }

  const uint32_t last_copy_index = num_copy_packet - 1;
  size_t total_copy_count = 0;
  for (uint32_t i = 0; i < num_copy_packet; ++i) {
    // Setup arguments.
    const uint32_t copy_count = static_cast<uint32_t>(
        std::min((size - total_copy_count), kMaxCopyCount));

    void* cur_dst = static_cast<char*>(dst) + (total_copy_count * copy_granule);
    const void* cur_src =
        static_cast<const char*>(src) + (total_copy_count * copy_granule);

    KernelCopyArgs* args = ObtainAsyncKernelCopyArg();
    assert(args != NULL);
    assert(IsMultipleOf(args, 16));

    args->src = cur_src;
    args->dst = cur_dst;
    args->size = copy_count;
    args->use_vector = (copy_granule == kAlignmentVec4) ? 1 : 0;

    const uint32_t grid_size_x =
        AlignUp(static_cast<uint32_t>(copy_count), kGroupSize);

    // This assert to make sure kMaxCopySize is not changed to a number that
    // could cause overflow to packet.grid_size_x.
    assert(grid_size_x >= copy_count);

    hsa_signal_t signal = {(i == last_copy_index)
                               ? (core::Signal::Convert(&out_signal)).handle
                               : 0};
    PopulateQueue(write_index, ((copy_granule == kAlignmentChar)
                                    ? copy_code_handle_
                                    : copy_aligned_code_handle_),
                  args, grid_size_x, signal);

    ++write_index;

    total_copy_count += copy_count;
  }

  // Launch copy packet.
  ReleaseWriteIndex(write_index_temp, total_num_packet);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t BlitKernel::SubmitLinearFillCommand(void* ptr, uint32_t value,
                                                 size_t num) {
  assert(fill_code_handle_ != 0);

  std::lock_guard<std::mutex> guard(lock_);

  HSA::hsa_signal_store_relaxed(completion_signal_, 1);

  const uint32_t num_fill_packet = static_cast<uint32_t>(
      std::ceil(static_cast<double>(num) / kMaxFillCount));

  // Reserve write index for copy + fence packet.
  uint64_t write_index = AcquireWriteIndex(num_fill_packet);

  KernelFillArgs* args = reinterpret_cast<KernelFillArgs*>(kernarg_);

  if (args == NULL) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  const uint32_t last_fill_index = num_fill_packet - 1;
  size_t total_fill_count = 0;
  for (uint32_t i = 0; i < num_fill_packet; ++i) {
    assert(IsMultipleOf(&args[i], 16));

    // Setup arguments.
    const uint32_t fill_count = static_cast<uint32_t>(
        std::min((num - total_fill_count), kMaxFillCount));
    void* cur_ptr = static_cast<char*>(ptr) + total_fill_count;

    args[i].ptr = cur_ptr;
    args[i].num = fill_count;
    args[i].value = value;

    const uint32_t grid_size_x =
        AlignUp(static_cast<uint32_t>(fill_count), kGroupSize);

    // This assert to make sure kMaxFillCount is not changed to a number that
    // could cause overflow to packet.grid_size_x.
    assert(grid_size_x >= fill_count);

    hsa_signal_t signal = {(i == last_fill_index) ? completion_signal_.handle
                                                  : 0};
    PopulateQueue(write_index + i, fill_code_handle_, &args[i], grid_size_x,
                  signal);

    total_fill_count += fill_count;
  }

  // Launch fill packet.
  // Launch copy packet.
  ReleaseWriteIndex(write_index, num_fill_packet);

  // Wait for the packet to finish.
  if (HSA::hsa_signal_wait_acquire(completion_signal_, HSA_SIGNAL_CONDITION_LT,
                                   1, uint64_t(-1),
                                   HSA_WAIT_STATE_ACTIVE) != 0) {
    // Signal wait returned unexpected value.
    return HSA_STATUS_ERROR;
  }

  return HSA_STATUS_SUCCESS;
}

uint64_t BlitKernel::AcquireWriteIndex(uint32_t num_packet) {
  assert(queue_->size >= num_packet);

  uint64_t write_index =
      HSA::hsa_queue_add_write_index_acq_rel(queue_, num_packet);

  while (true) {
    // Wait until we have room in the queue;
    const uint64_t read_index = HSA::hsa_queue_load_read_index_relaxed(queue_);
    if ((write_index - read_index) < queue_->size) {
      break;
    }
  }

  return write_index;
}

void BlitKernel::ReleaseWriteIndex(uint64_t write_index, uint32_t num_packet) {
  // Launch packet.
  while (true) {
    // Make sure that the address before ::current_offset is already released.
    // Otherwise the packet processor may read invalid packets.
    uint64_t expected_offset = write_index;
    if (atomic::Cas(&cached_index_, write_index + num_packet, expected_offset,
                    std::memory_order_release) == expected_offset) {
      // Update doorbel register with last packet id.
      HSA::hsa_signal_store_release(queue_->doorbell_signal,
                                    write_index + num_packet - 1);
      break;
    }
  }
}

hsa_status_t BlitKernel::FenceRelease(uint64_t write_index,
                                      uint32_t num_copy_packet,
                                      hsa_fence_scope_t fence) {
  // This function is not thread safe.

  const uint16_t kBarrierPacketHeader =
      (HSA_PACKET_TYPE_BARRIER_AND << HSA_PACKET_HEADER_TYPE) |
      (1 << HSA_PACKET_HEADER_BARRIER) |
      (HSA_FENCE_SCOPE_NONE << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
      (fence << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);

  hsa_barrier_and_packet_t packet = {0};
  packet.header = kInvalidPacketHeader;

  HSA::hsa_signal_store_relaxed(completion_signal_, 1);
  packet.completion_signal = completion_signal_;

  if (num_copy_packet == 0) {
    assert(write_index == 0);
    // Reserve write index.
    write_index = AcquireWriteIndex(1);
  }

  // Populate queue buffer with AQL packet.
  hsa_barrier_and_packet_t* queue_buffer =
      reinterpret_cast<hsa_barrier_and_packet_t*>(queue_->base_address);
  std::atomic_thread_fence(std::memory_order_acquire);
  queue_buffer[(write_index + num_copy_packet) & queue_bitmask_] = packet;
  std::atomic_thread_fence(std::memory_order_release);
  queue_buffer[(write_index + num_copy_packet) & queue_bitmask_].header =
      kBarrierPacketHeader;

  // Launch packet.
  ReleaseWriteIndex(write_index, num_copy_packet + 1);

  // Wait for the packet to finish.
  if (HSA::hsa_signal_wait_acquire(packet.completion_signal,
                                   HSA_SIGNAL_CONDITION_LT, 1, uint64_t(-1),
                                   HSA_WAIT_STATE_ACTIVE) != 0) {
    // Signal wait returned unexpected value.
    return HSA_STATUS_ERROR;
  }

  return HSA_STATUS_SUCCESS;
}

void BlitKernel::PopulateQueue(uint64_t index, uint64_t code_handle, void* args,
                               uint32_t grid_size_x,
                               hsa_signal_t completion_signal) {
  assert(IsMultipleOf(args, 16));

  hsa_kernel_dispatch_packet_t packet = {0};

  static const uint16_t kDispatchPacketHeader =
      (HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE) |
      (((completion_signal.handle != 0) ? 1 : 0) << HSA_PACKET_HEADER_BARRIER) |
      (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
      (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);

  packet.header = kInvalidPacketHeader;
  packet.kernel_object = code_handle;
  packet.kernarg_address = args;

  // Setup working size.
  const int kNumDimension = 1;
  packet.setup = kNumDimension << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
  packet.grid_size_x = AlignUp(static_cast<uint32_t>(grid_size_x), kGroupSize);
  packet.grid_size_y = packet.grid_size_z = 1;
  packet.workgroup_size_x = kGroupSize;
  packet.workgroup_size_y = packet.workgroup_size_z = 1;

  packet.completion_signal = completion_signal;

  // Populate queue buffer with AQL packet.
  hsa_kernel_dispatch_packet_t* queue_buffer =
      reinterpret_cast<hsa_kernel_dispatch_packet_t*>(queue_->base_address);
  std::atomic_thread_fence(std::memory_order_acquire);
  queue_buffer[index & queue_bitmask_] = packet;
  std::atomic_thread_fence(std::memory_order_release);
  queue_buffer[index & queue_bitmask_].header = kDispatchPacketHeader;
}

BlitKernel::KernelCopyArgs* BlitKernel::ObtainAsyncKernelCopyArg() {
  const uint32_t index =
      atomic::Add(&kernarg_async_counter_, 1U, std::memory_order_acquire);
  KernelCopyArgs* arg = &kernarg_async_[index & kernarg_async_mask_];
  assert(IsMultipleOf(arg, 16));
  return arg;
}

}  // namespace amd
