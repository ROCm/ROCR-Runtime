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

#include "core/inc/amd_hw_aql_command_processor.h"

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

#include "core/inc/runtime.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/signal.h"
#include "core/inc/queue.h"
#include "core/util/utils.h"
#include "core/inc/registers.h"
#include "core/inc/interrupt_signal.h"

namespace amd {
// Queue::amd_queue_ is cache-aligned for performance.
const uint32_t kAmdQueueAlignBytes = 0x40;

HsaEvent* HwAqlCommandProcessor::queue_event_ = NULL;
volatile uint32_t HwAqlCommandProcessor::queue_count_ = 0;
KernelMutex HwAqlCommandProcessor::queue_lock_;
int HwAqlCommandProcessor::rtti_id_;

void* HwAqlCommandProcessor::operator new(size_t size) {
  // Align base to 64B to enforce amd_queue_ member alignment.
  return _aligned_malloc(size, kAmdQueueAlignBytes);
}

void HwAqlCommandProcessor::operator delete(void* ptr) { _aligned_free(ptr); }

HwAqlCommandProcessor::HwAqlCommandProcessor(GpuAgent* agent,
                                             size_t req_size_pkts,
                                             HSAuint32 node_id,
                                             ScratchInfo& scratch,
                                             core::HsaEventCallback callback,
                                             void* err_data, bool is_kv)
    : Queue(),
      Signal(0),
      ring_buf_(NULL),
      ring_buf_alloc_bytes_(0),
      queue_id_(HSA_QUEUEID(-1)),
      valid_(false),
      agent_(agent),
      queue_scratch_(scratch),
      errors_callback_(callback),
      errors_data_(err_data),
      is_kv_queue_(is_kv) {
  if (!Queue::Shared::IsSharedObjectAllocationValid()) {
    return;
  }

  hsa_status_t stat = agent_->GetInfo(HSA_AGENT_INFO_PROFILE, &agent_profile_);
  assert(stat == HSA_STATUS_SUCCESS);

  const core::ComputeCapability& compute_cap = agent_->compute_capability();

  // When queue_full_workaround_ is set to 1, the ring buffer is internally
  // doubled in size. Virtual addresses in the upper half of the ring allocation
  // are mapped to the same set of pages backing the lower half.
  // Values written to the HW doorbell are modulo the doubled size.
  // This allows the HW to accept (doorbell == last_doorbell + queue_size).
  // This workaround is required for GFXIP 7 and GFXIP 8 ASICs.
  queue_full_workaround_ =
      (compute_cap.version_major() == 7 || compute_cap.version_major() == 8)
          ? 1
          : 0;

  // Identify doorbell semantics for this agent.
  doorbell_type_ = agent->properties().Capability.ui32.DoorbellType;

  // Queue size is a function of several restrictions.
  const uint32_t min_pkts = ComputeRingBufferMinPkts();
  const uint32_t max_pkts = ComputeRingBufferMaxPkts();

  // Apply sizing constraints to the ring buffer.
  uint32_t queue_size_pkts = uint32_t(req_size_pkts);
  queue_size_pkts = Min(queue_size_pkts, max_pkts);
  queue_size_pkts = Max(queue_size_pkts, min_pkts);

  uint32_t queue_size_bytes = queue_size_pkts * sizeof(core::AqlPacket);
  if ((queue_size_bytes & (queue_size_bytes - 1)) != 0) return;

  // Allocate the AQL packet ring buffer.
  AllocRegisteredRingBuffer(queue_size_pkts);
  if (ring_buf_ == NULL) return;
  MAKE_NAMED_SCOPE_GUARD(RingGuard, [&]() { FreeRegisteredRingBuffer(); });

  // Fill the ring buffer with ALWAYS_RESERVED packet headers.
  // Leave packet content uninitialized to help track errors.
  for (uint32_t pkt_id = 0; pkt_id < queue_size_pkts; ++pkt_id) {
    ((uint32_t*)ring_buf_)[16 * pkt_id] = HSA_PACKET_TYPE_INVALID;
  }

  // Zero the amd_queue_ structure to clear RPTR/WPTR before queue attach.
  memset(&amd_queue_, 0, sizeof(amd_queue_));

  // Initialize and map a HW AQL queue.
  HsaQueueResource queue_rsrc = {0};
  queue_rsrc.Queue_read_ptr_aql = (uint64_t*)&amd_queue_.read_dispatch_id;
  queue_rsrc.Queue_write_ptr_aql =
      (uint64_t*)&amd_queue_.max_legacy_doorbell_dispatch_id_plus_1;

  HSAKMT_STATUS kmt_status;
  kmt_status = hsaKmtCreateQueue(node_id, HSA_QUEUE_COMPUTE_AQL, 100,
                                 HSA_QUEUE_PRIORITY_NORMAL, ring_buf_,
                                 ring_buf_alloc_bytes_, NULL, &queue_rsrc);
  if (kmt_status != HSAKMT_STATUS_SUCCESS) return;
  queue_id_ = queue_rsrc.QueueId;
  MAKE_NAMED_SCOPE_GUARD(QueueGuard, [&]() { hsaKmtDestroyQueue(queue_id_); });

  // Populate doorbell signal structure.
  memset(&signal_, 0, sizeof(signal_));
  signal_.kind = AMD_SIGNAL_KIND_LEGACY_DOORBELL;
  signal_.legacy_hardware_doorbell_ptr =
      (volatile uint32_t*)queue_rsrc.Queue_DoorBell;
  signal_.queue_ptr = &amd_queue_;

  // Populate amd_queue_ structure.
  amd_queue_.hsa_queue.type = HSA_QUEUE_TYPE_MULTI;
  amd_queue_.hsa_queue.features = HSA_QUEUE_FEATURE_KERNEL_DISPATCH;
  amd_queue_.hsa_queue.base_address = ring_buf_;
  amd_queue_.hsa_queue.doorbell_signal = Signal::Convert(this);
  amd_queue_.hsa_queue.size = queue_size_pkts;
  amd_queue_.hsa_queue.id = core::Runtime::runtime_singleton_->GetQueueId();
  amd_queue_.read_dispatch_id_field_base_byte_offset = uint32_t(
      uintptr_t(&amd_queue_.read_dispatch_id) - uintptr_t(&amd_queue_));

  const auto& props = agent->properties();
  amd_queue_.max_cu_id = (props.NumFComputeCores / props.NumSIMDPerCU) - 1;
  amd_queue_.max_wave_id = props.MaxWavesPerSIMD - 1;

#ifdef HSA_LARGE_MODEL
  AMD_HSA_BITS_SET(amd_queue_.queue_properties, AMD_QUEUE_PROPERTIES_IS_PTR64,
                   1);
#else
  AMD_HSA_BITS_SET(amd_queue_.queue_properties, AMD_QUEUE_PROPERTIES_IS_PTR64,
                   0);
#endif

  // Populate scratch resource descriptor in amd_queue_.
  SQ_BUF_RSRC_WORD0 srd0;
  SQ_BUF_RSRC_WORD1 srd1;
  SQ_BUF_RSRC_WORD2 srd2;
  SQ_BUF_RSRC_WORD3 srd3;
  uintptr_t scratch_base = uintptr_t(queue_scratch_.queue_base);
  uint32_t scratch_base_hi = 0;

#ifdef HSA_LARGE_MODEL
  scratch_base_hi = uint32_t(scratch_base >> 32);
#endif

  srd0.bits.BASE_ADDRESS = uint32_t(scratch_base);
  srd1.bits.BASE_ADDRESS_HI = scratch_base_hi;
  srd1.bits.STRIDE = 0;
  srd1.bits.CACHE_SWIZZLE = 0;
  srd1.bits.SWIZZLE_ENABLE = 1;
  srd2.bits.NUM_RECORDS = uint32_t(queue_scratch_.size);
  srd3.bits.DST_SEL_X = SQ_SEL_X;
  srd3.bits.DST_SEL_Y = SQ_SEL_Y;
  srd3.bits.DST_SEL_Z = SQ_SEL_Z;
  srd3.bits.DST_SEL_W = SQ_SEL_W;
  srd3.bits.NUM_FORMAT = BUF_NUM_FORMAT_UINT;
  srd3.bits.DATA_FORMAT = BUF_DATA_FORMAT_32;
  srd3.bits.ELEMENT_SIZE = 1;  // 4
  srd3.bits.INDEX_STRIDE = 3;  // 64
  srd3.bits.ADD_TID_ENABLE = 1;
  srd3.bits.ATC__CI__VI = (agent_profile_ == HSA_PROFILE_FULL) ? 1 : 0;
  srd3.bits.HASH_ENABLE = 0;
  srd3.bits.HEAP = 0;
  srd3.bits.MTYPE__CI__VI = 0;
  srd3.bits.TYPE = SQ_RSRC_BUF;

  amd_queue_.scratch_resource_descriptor[0] = srd0.u32All;
  amd_queue_.scratch_resource_descriptor[1] = srd1.u32All;
  amd_queue_.scratch_resource_descriptor[2] = srd2.u32All;
  amd_queue_.scratch_resource_descriptor[3] = srd3.u32All;

  // Populate flat scratch parameters in amd_queue_.
  amd_queue_.scratch_backing_memory_location =
      queue_scratch_.queue_process_offset;
  amd_queue_.scratch_backing_memory_byte_size = queue_scratch_.size;
  amd_queue_.scratch_workitem_byte_size =
      uint32_t(queue_scratch_.size_per_thread);

  // Set concurrent wavefront limits when scratch is being used.
  COMPUTE_TMPRING_SIZE tmpring_size = {0};

  if (queue_scratch_.size != 0) {
    tmpring_size.bits.WAVES =
        (queue_scratch_.size / queue_scratch_.size_per_thread / 64);
    tmpring_size.bits.WAVESIZE =
        (((64 * queue_scratch_.size_per_thread) + 1023) / 1024);
  }

  amd_queue_.compute_tmpring_size = tmpring_size.u32All;

  // Set group and private memory apertures in amd_queue_.
  auto& regions = agent->regions();

  for (int i = 0; i < regions.size(); i++) {
    const MemoryRegion* amdregion;
    amdregion = static_cast<const MemoryRegion*>(regions[i]);
    uint64_t base = amdregion->GetBaseAddress();

    if (amdregion->IsLDS()) {
#ifdef HSA_LARGE_MODEL
      amd_queue_.group_segment_aperture_base_hi =
          uint32_t(uintptr_t(base) >> 32);
#else
      amd_queue_.group_segment_aperture_base_hi = uint32_t(base);
#endif
    }

    if (amdregion->IsScratch()) {
#ifdef HSA_LARGE_MODEL
      amd_queue_.private_segment_aperture_base_hi =
          uint32_t(uintptr_t(base) >> 32);
#else
      amd_queue_.private_segment_aperture_base_hi = uint32_t(base);
#endif
    }
  }

  assert(amd_queue_.group_segment_aperture_base_hi != NULL &&
         "No group region found.");

  if (os::GetEnvVar("HSA_CHECK_FLAT_SCRATCH") == "1") {
    assert(amd_queue_.private_segment_aperture_base_hi != NULL &&
           "No private region found.");
  }

  MAKE_NAMED_SCOPE_GUARD(EventGuard, [&]() {
    ScopedAcquire<KernelMutex> _lock(&queue_lock_);
    queue_count_--;
    if (queue_count_ == 0) {
      core::InterruptSignal::DestroyEvent(queue_event_);
      queue_event_ = NULL;
    }
  });

  MAKE_NAMED_SCOPE_GUARD(SignalGuard, [&]() {
    HSA::hsa_signal_destroy(amd_queue_.queue_inactive_signal);
  });
#if defined(HSA_LARGE_MODEL) && defined(__linux__)
  if (core::g_use_interrupt_wait) {
    {
      ScopedAcquire<KernelMutex> _lock(&queue_lock_);
      queue_count_++;
      if (queue_event_ == NULL) {
        assert(queue_count_ == 1 &&
               "Inconsistency in queue event reference counting found.\n");
        queue_event_ = core::InterruptSignal::CreateEvent();
        if (queue_event_ == NULL) return;
      }
    }
    auto signal = new core::InterruptSignal(0, queue_event_);
    amd_queue_.queue_inactive_signal = core::InterruptSignal::Convert(signal);
    if (hsa_amd_signal_async_handler(
            amd_queue_.queue_inactive_signal, HSA_SIGNAL_CONDITION_NE, 0,
            DynamicScratchHandler, this) != HSA_STATUS_SUCCESS)
      return;
  } else {
    EventGuard.Dismiss();
    SignalGuard.Dismiss();
  }
#else
  EventGuard.Dismiss();
  SignalGuard.Dismiss();
#endif

  valid_ = true;
  active_ = 1;

  RingGuard.Dismiss();
  QueueGuard.Dismiss();
  EventGuard.Dismiss();
  SignalGuard.Dismiss();
}

HwAqlCommandProcessor::~HwAqlCommandProcessor() {
  if (!IsValid()) {
    return;
  }

  if (active_ == 1) hsaKmtDestroyQueue(queue_id_);

  FreeRegisteredRingBuffer();
  agent_->ReleaseQueueScratch(queue_scratch_.queue_base);
  HSA::hsa_signal_destroy(amd_queue_.queue_inactive_signal);
#if defined(HSA_LARGE_MODEL) && defined(__linux__)
  if (core::g_use_interrupt_wait) {
    ScopedAcquire<KernelMutex> lock(&queue_lock_);
    queue_count_--;
    if (queue_count_ == 0) {
      core::InterruptSignal::DestroyEvent(queue_event_);
      queue_event_ = NULL;
    }
  }
#endif
}

uint64_t HwAqlCommandProcessor::LoadReadIndexAcquire() {
  return atomic::Load(&amd_queue_.read_dispatch_id, std::memory_order_acquire);
}

uint64_t HwAqlCommandProcessor::LoadReadIndexRelaxed() {
  return atomic::Load(&amd_queue_.read_dispatch_id, std::memory_order_relaxed);
}

uint64_t HwAqlCommandProcessor::LoadWriteIndexAcquire() {
  return atomic::Load(&amd_queue_.write_dispatch_id, std::memory_order_acquire);
}

uint64_t HwAqlCommandProcessor::LoadWriteIndexRelaxed() {
  return atomic::Load(&amd_queue_.write_dispatch_id, std::memory_order_relaxed);
}

void HwAqlCommandProcessor::StoreWriteIndexRelaxed(uint64_t value) {
  atomic::Store(&amd_queue_.write_dispatch_id, value,
                std::memory_order_relaxed);
}

void HwAqlCommandProcessor::StoreWriteIndexRelease(uint64_t value) {
  atomic::Store(&amd_queue_.write_dispatch_id, value,
                std::memory_order_release);
}

uint64_t HwAqlCommandProcessor::CasWriteIndexAcqRel(uint64_t expected,
                                                    uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_acq_rel);
}
uint64_t HwAqlCommandProcessor::CasWriteIndexAcquire(uint64_t expected,
                                                     uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_acquire);
}
uint64_t HwAqlCommandProcessor::CasWriteIndexRelaxed(uint64_t expected,
                                                     uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_relaxed);
}
uint64_t HwAqlCommandProcessor::CasWriteIndexRelease(uint64_t expected,
                                                     uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_release);
}

uint64_t HwAqlCommandProcessor::AddWriteIndexAcqRel(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_acq_rel);
}

uint64_t HwAqlCommandProcessor::AddWriteIndexAcquire(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_acquire);
}

uint64_t HwAqlCommandProcessor::AddWriteIndexRelaxed(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_relaxed);
}

uint64_t HwAqlCommandProcessor::AddWriteIndexRelease(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_release);
}

void HwAqlCommandProcessor::StoreRelaxed(hsa_signal_value_t value) {
  // Acquire spinlock protecting the legacy doorbell.
  while (atomic::Cas(&amd_queue_.legacy_doorbell_lock, 1U, 0U,
                     std::memory_order_acquire) != 0) {
    os::YieldThread();
  }

#ifdef HSA_LARGE_MODEL
  // AMD hardware convention expects the packet index to point beyond
  // the last packet to be processed. Packet indices written to the
  // max_legacy_doorbell_dispatch_id_plus_1 field must conform to this
  // expectation, since this field is used as the HW-visible write index.
  uint64_t legacy_dispatch_id = value + 1;
#else
  // In the small machine model it is difficult to distinguish packet index
  // wrap at 2^32 packets from a backwards doorbell. Instead, ignore the
  // doorbell value and submit the write index instead. It is OK to issue
  // a doorbell for packets in the INVALID or ALWAYS_RESERVED state.
  // The HW will stall on these packets until they enter a valid state.
  uint64_t legacy_dispatch_id = amd_queue_.write_dispatch_id;

  // The write index may extend more than a full queue of packets beyond
  // the read index. The hardware can process at most a full queue of packets
  // at a time. Clamp the write index appropriately. A doorbell for the
  // remaining packets is guaranteed to be sent at a later time.
  legacy_dispatch_id =
      Min(legacy_dispatch_id,
          uint64_t(amd_queue_.read_dispatch_id) + amd_queue_.hsa_queue.size);
#endif

  // Discard backwards and duplicate doorbells.
  if (legacy_dispatch_id > amd_queue_.max_legacy_doorbell_dispatch_id_plus_1) {
    // Record the most recent packet index used in a doorbell submission.
    // This field will be interpreted as a write index upon HW queue connect.
    // Must be visible to the HW before sending the doorbell to avoid a race.
    atomic::Store(&amd_queue_.max_legacy_doorbell_dispatch_id_plus_1,
                  legacy_dispatch_id, std::memory_order_relaxed);

    // Write the dispatch id to the hardware MMIO doorbell.
    if (doorbell_type_ == 0) {
      // The legacy GFXIP 7 hardware doorbell expects:
      //   1. Packet index wrapped to a point within the ring buffer
      //   2. Packet index converted to DWORD count
      uint64_t queue_size_mask =
          ((1 + queue_full_workaround_) * amd_queue_.hsa_queue.size) - 1;

      *(volatile uint32_t*)signal_.legacy_hardware_doorbell_ptr =
          uint32_t((legacy_dispatch_id & queue_size_mask) *
                   (sizeof(core::AqlPacket) / sizeof(uint32_t)));
    } else if (doorbell_type_ == 1) {
      *(volatile uint32_t*)signal_.legacy_hardware_doorbell_ptr =
          uint32_t(legacy_dispatch_id);
    } else {
      assert(false && "Agent has unsupported doorbell semantics");
    }
  }

  // Release spinlock protecting the legacy doorbell.
  atomic::Store(&amd_queue_.legacy_doorbell_lock, 0U,
                std::memory_order_release);
}

void HwAqlCommandProcessor::StoreRelease(hsa_signal_value_t value) {
  std::atomic_thread_fence(std::memory_order_release);
  StoreRelaxed(value);
}

uint32_t HwAqlCommandProcessor::ComputeRingBufferMinPkts() {
  // From CP_HQD_PQ_CONTROL.QUEUE_SIZE specification:
  //   Size of the primary queue (PQ) will be: 2^(HQD_QUEUE_SIZE+1) DWs.
  //   Min Size is 7 (2^8 = 256 DWs) and max size is 29 (2^30 = 1 G-DW)
  uint32_t min_bytes = 0x400;

  if (queue_full_workaround_ == 1) {
#ifdef __linux__
    // Double mapping requires one page of backing store.
    min_bytes = Max(min_bytes, 0x1000U);
#endif
#ifdef _WIN32
    // Shared memory mapping is at system allocation granularity.
    SYSTEM_INFO sys_info;
    GetNativeSystemInfo(&sys_info);
    min_bytes = Max(min_bytes, uint32_t(sys_info.dwAllocationGranularity));
#endif
  }

  return uint32_t(min_bytes / sizeof(core::AqlPacket));
}

uint32_t HwAqlCommandProcessor::ComputeRingBufferMaxPkts() {
  // From CP_HQD_PQ_CONTROL.QUEUE_SIZE specification:
  //   Size of the primary queue (PQ) will be: 2^(HQD_QUEUE_SIZE+1) DWs.
  //   Min Size is 7 (2^8 = 256 DWs) and max size is 29 (2^30 = 1 G-DW)
  uint64_t max_bytes = 0x100000000;

  if (queue_full_workaround_ == 1) {
    // Double mapping halves maximum size.
    max_bytes /= 2;
  }

  return uint32_t(max_bytes / sizeof(core::AqlPacket));
}

void HwAqlCommandProcessor::AllocRegisteredRingBuffer(
    uint32_t queue_size_pkts) {
  if (agent_profile_ == HSA_PROFILE_FULL) {
    // Compute the physical and virtual size of the queue.
    uint32_t ring_buf_phys_size_bytes =
        uint32_t(queue_size_pkts * sizeof(core::AqlPacket));
    ring_buf_alloc_bytes_ = 2 * ring_buf_phys_size_bytes;

#ifdef __linux__
    // Create a system-unique shared memory path for this thread.
    char ring_buf_shm_path[16];
    pid_t sys_unique_tid = pid_t(syscall(__NR_gettid));
    sprintf(ring_buf_shm_path, "/%u", sys_unique_tid);

    int ring_buf_shm_fd = -1;
    void* reserve_va = NULL;

    do {
      // Create a shared memory object to back the ring buffer.
      ring_buf_shm_fd = shm_open(ring_buf_shm_path, O_CREAT | O_RDWR | O_EXCL,
                                 S_IRUSR | S_IWUSR);
      if (ring_buf_shm_fd == -1) {
        break;
      }
      if (posix_fallocate(ring_buf_shm_fd, 0, ring_buf_phys_size_bytes) != 0)
        break;

      // Reserve a VA range twice the size of the physical backing store.
      reserve_va = mmap(NULL, ring_buf_alloc_bytes_, PROT_NONE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      assert(reserve_va != MAP_FAILED && "mmap failed");

      // Remap the lower and upper halves of the VA range.
      // Map both halves to the shared memory backing store.
      // If the GPU device is KV, do not set PROT_EXEC flag.
      void* ring_buf_lower_half = NULL;
      void* ring_buf_upper_half = NULL;
      if (is_kv_queue_) {
        ring_buf_lower_half =
            mmap(reserve_va, ring_buf_phys_size_bytes, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FIXED, ring_buf_shm_fd, 0);
        assert(ring_buf_lower_half != MAP_FAILED && "mmap failed");

        ring_buf_upper_half =
            mmap((void*)(uintptr_t(reserve_va) + ring_buf_phys_size_bytes),
                 ring_buf_phys_size_bytes, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FIXED, ring_buf_shm_fd, 0);
        assert(ring_buf_upper_half != MAP_FAILED && "mmap failed");
      } else {
        ring_buf_lower_half = mmap(reserve_va, ring_buf_phys_size_bytes,
                                   PROT_READ | PROT_WRITE | PROT_EXEC,
                                   MAP_SHARED | MAP_FIXED, ring_buf_shm_fd, 0);
        assert(ring_buf_lower_half != MAP_FAILED && "mmap failed");

        ring_buf_upper_half =
            mmap((void*)(uintptr_t(reserve_va) + ring_buf_phys_size_bytes),
                 ring_buf_phys_size_bytes, PROT_READ | PROT_WRITE | PROT_EXEC,
                 MAP_SHARED | MAP_FIXED, ring_buf_shm_fd, 0);
        assert(ring_buf_upper_half != MAP_FAILED && "mmap failed");
      }

      // Release explicit reference to shared memory object.
      shm_unlink(ring_buf_shm_path);
      close(ring_buf_shm_fd);

      // Successfully created mapping.
      ring_buf_ = ring_buf_lower_half;
      return;
    } while (false);

    // Resource cleanup on failure.
    if (reserve_va) munmap(reserve_va, ring_buf_alloc_bytes_);
    if (ring_buf_shm_fd != -1) {
      shm_unlink(ring_buf_shm_path);
      close(ring_buf_shm_fd);
    }
#endif
#ifdef _WIN32
    HANDLE ring_buf_mapping = INVALID_HANDLE_VALUE;
    void* ring_buf_lower_half = NULL;
    void* ring_buf_upper_half = NULL;

    do {
      // Create a page file mapping to back the ring buffer.
      ring_buf_mapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
                                           PAGE_EXECUTE_READWRITE | SEC_COMMIT,
                                           0, ring_buf_phys_size_bytes, NULL);
      if (ring_buf_mapping == NULL) {
        break;
      }

      // Retry until obtaining an appropriate virtual address mapping.
      for (int num_attempts = 0; num_attempts < 1000; ++num_attempts) {
        // Find a virtual address range twice the size of the file mapping.
        void* reserve_va =
            VirtualAllocEx(GetCurrentProcess(), NULL, ring_buf_alloc_bytes_,
                           MEM_TOP_DOWN | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (reserve_va == NULL) {
          break;
        }
        VirtualFree(reserve_va, 0, MEM_RELEASE);

        // Map the ring buffer into the free virtual range.
        // This may fail: another thread can allocate in this range.
        ring_buf_lower_half = MapViewOfFileEx(
            ring_buf_mapping, FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE, 0, 0,
            ring_buf_phys_size_bytes, reserve_va);

        if (ring_buf_lower_half == NULL) {
          // Virtual range allocated by another thread, try again.
          continue;
        }

        ring_buf_upper_half = MapViewOfFileEx(
            ring_buf_mapping, FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE, 0, 0,
            ring_buf_phys_size_bytes,
            (void*)(uintptr_t(reserve_va) + ring_buf_phys_size_bytes));

        if (ring_buf_upper_half == NULL) {
          // Virtual range allocated by another thread, try again.
          UnmapViewOfFile(ring_buf_lower_half);
          continue;
        }

        // Successfully created mapping.
        ring_buf_ = ring_buf_lower_half;
        break;
      }

      if (ring_buf_ == NULL) {
        break;
      }

      // Release file mapping (reference counted by views).
      CloseHandle(ring_buf_mapping);

      // Don't register the memory: causes a failure in the KFD.
      // Instead use implicit registration to access the ring buffer.
      return;
    } while (false);

    // Resource cleanup on failure.
    UnmapViewOfFile(ring_buf_upper_half);
    UnmapViewOfFile(ring_buf_lower_half);
    CloseHandle(ring_buf_mapping);
#endif
  } else {
    // Allocate storage for the ring buffer.
    HsaMemFlags flags;
    flags.Value = 0;
    flags.ui32.HostAccess = 1;
    flags.ui32.AtomicAccessPartial = 1;
    flags.ui32.ExecuteAccess = 1;
    flags.ui32.AQLQueueMemory = 1;

    ring_buf_alloc_bytes_ = AlignUp(
        queue_size_pkts * static_cast<uint32_t>(sizeof(core::AqlPacket)), 4096);
    auto err = hsaKmtAllocMemory(agent_->node_id(), ring_buf_alloc_bytes_,
                                 flags, (void**)&ring_buf_);

    if (err != HSAKMT_STATUS_SUCCESS) {
      assert(false && "AQL queue memory allocation failure.");
      return;
    }

    HSAuint64 alternate_va;
    err = hsaKmtMapMemoryToGPU(ring_buf_, ring_buf_alloc_bytes_, &alternate_va);

    if (err != HSAKMT_STATUS_SUCCESS) {
      assert(false && "AQL queue memory map failure.");
      hsaKmtFreeMemory(ring_buf_, ring_buf_alloc_bytes_);
      ring_buf_ = NULL;
      return;
    }

    ring_buf_alloc_bytes_ = 2 * ring_buf_alloc_bytes_;
  }
}

void HwAqlCommandProcessor::FreeRegisteredRingBuffer() {
  if (agent_profile_ == HSA_PROFILE_FULL) {
#ifdef __linux__
    munmap(ring_buf_, ring_buf_alloc_bytes_);
#endif
#ifdef _WIN32
    UnmapViewOfFile(ring_buf_);
    UnmapViewOfFile(
        (void*)(uintptr_t(ring_buf_) + (ring_buf_alloc_bytes_ / 2)));
#endif
  } else {
    hsaKmtUnmapMemoryToGPU(ring_buf_);
    hsaKmtFreeMemory(ring_buf_, ring_buf_alloc_bytes_ / 2);
  }

  ring_buf_ = NULL;
  ring_buf_alloc_bytes_ = 0;
}

hsa_status_t HwAqlCommandProcessor::Inactivate() {
  int32_t active = atomic::Exchange((volatile int32_t*)&active_, 0);
  if (active == 1) hsaKmtDestroyQueue(this->queue_id_);
  return HSA_STATUS_SUCCESS;
}

bool HwAqlCommandProcessor::DynamicScratchHandler(hsa_signal_value_t error_code,
                                                  void* arg) {
  HwAqlCommandProcessor* queue = (HwAqlCommandProcessor*)arg;

  if ((error_code & 1) == 1) {
    // Insufficient scratch - recoverable
    auto& scratch = queue->queue_scratch_;

    queue->agent_->ReleaseQueueScratch(scratch.queue_base);

    const core::AqlPacket& pkt =
        ((core::AqlPacket*)queue->amd_queue_.hsa_queue
             .base_address)[queue->amd_queue_.read_dispatch_id];

    uint32_t scratch_request = pkt.dispatch.private_segment_size;

    scratch.size_per_thread =
        Max(uint32_t(scratch.size_per_thread * 2), scratch_request);
    // Align whole waves to 1KB.
    scratch.size_per_thread = AlignUp(scratch.size_per_thread, 16);
    scratch.size = scratch.size_per_thread * (queue->amd_queue_.max_cu_id + 1) *
                   32 * 64;  // TODO: replace constants.

    // printf("Growing scratch to %u - %u\n", uint32_t(scratch.size_per_thread),
    // uint32_t(scratch.size));

    queue->agent_->AcquireQueueScratch(scratch);
    if (scratch.queue_base == NULL) {
      // Out of scratch - promote error and invalidate queue
      queue->Inactivate();
      if (queue->errors_callback_ != NULL)
        queue->errors_callback_(HSA_STATUS_ERROR_OUT_OF_RESOURCES,
                                queue->public_handle(), queue->errors_data_);
      return false;
    }

    SQ_BUF_RSRC_WORD0 srd0;
    SQ_BUF_RSRC_WORD2 srd2;
    uintptr_t base = (uintptr_t)scratch.queue_base;

    srd0.u32All = queue->amd_queue_.scratch_resource_descriptor[0];
    srd2.u32All = queue->amd_queue_.scratch_resource_descriptor[2];

    srd0.bits.BASE_ADDRESS = uint32_t(base);
    srd2.bits.NUM_RECORDS = uint32_t(scratch.size);

    queue->amd_queue_.scratch_resource_descriptor[0] = srd0.u32All;
    queue->amd_queue_.scratch_resource_descriptor[2] = srd2.u32All;

#ifdef HSA_LARGE_MODEL
    SQ_BUF_RSRC_WORD1 srd1;
    srd1.u32All = queue->amd_queue_.scratch_resource_descriptor[1];
    srd1.bits.BASE_ADDRESS_HI = uint32_t(base >> 32);
    queue->amd_queue_.scratch_resource_descriptor[1] = srd1.u32All;
#endif

    queue->amd_queue_.scratch_backing_memory_location =
        scratch.queue_process_offset;
    queue->amd_queue_.scratch_backing_memory_byte_size = scratch.size;
    queue->amd_queue_.scratch_workitem_byte_size =
        uint32_t(scratch.size_per_thread);

    COMPUTE_TMPRING_SIZE tmpring_size = {0};
    tmpring_size.bits.WAVES = (scratch.size / scratch.size_per_thread / 64);
    tmpring_size.bits.WAVESIZE =
        (((64 * scratch.size_per_thread) + 1023) / 1024);
    queue->amd_queue_.compute_tmpring_size = tmpring_size.u32All;

  } else if ((error_code & 2) == 2) {  // Invalid dim
    queue->Inactivate();
    if (queue->errors_callback_ != NULL)
      queue->errors_callback_(HSA_STATUS_ERROR_INCOMPATIBLE_ARGUMENTS,
                              queue->public_handle(), queue->errors_data_);
    return false;

  } else if ((error_code & 4) == 4) {  // Invalid group memory
    queue->Inactivate();
    if (queue->errors_callback_ != NULL)
      queue->errors_callback_(HSA_STATUS_ERROR_INVALID_ALLOCATION,
                              queue->public_handle(), queue->errors_data_);
    return false;

  } else if ((error_code & 8) == 8) {  // Invalid (or NULL) code
    queue->Inactivate();
    if (queue->errors_callback_ != NULL)
      queue->errors_callback_(HSA_STATUS_ERROR_INVALID_CODE_OBJECT,
                              queue->public_handle(), queue->errors_data_);
    return false;

  } else if ((error_code & 32) == 32) {  // Invalid format
    queue->Inactivate();
    if (queue->errors_callback_ != NULL)
      queue->errors_callback_(HSA_STATUS_ERROR_INVALID_PACKET_FORMAT,
                              queue->public_handle(), queue->errors_data_);
    return false;
  } else if ((error_code & 64) == 64) {  // Group is too large
    queue->Inactivate();
    if (queue->errors_callback_ != NULL)
      queue->errors_callback_(HSA_STATUS_ERROR_INVALID_ARGUMENT,
                              queue->public_handle(), queue->errors_data_);
    return false;
  } else if ((error_code & 128) == 128) {  // Out of VGPRs
    queue->Inactivate();
    if (queue->errors_callback_ != NULL)
      queue->errors_callback_(HSA_STATUS_ERROR_INVALID_ISA,
                              queue->public_handle(), queue->errors_data_);
    return false;
  } else if ((error_code & 0x80000000) == 0x80000000) { // Debug trap
    queue->Inactivate();
    if (queue->errors_callback_ != NULL)
      queue->errors_callback_(HSA_STATUS_ERROR_EXCEPTION,
                              queue->public_handle(), queue->errors_data_);
    return false;
  } else {
    // Undefined code
    queue->Inactivate();
    assert(false && "Undefined queue error code");
    if (queue->errors_callback_ != NULL)
      queue->errors_callback_(HSA_STATUS_ERROR, queue->public_handle(),
                              queue->errors_data_);
    return false;
  }

  HSA::hsa_signal_store_relaxed(queue->amd_queue_.queue_inactive_signal, 0);
  return true;
}

hsa_status_t HwAqlCommandProcessor::SetCUMasking(
    const uint32_t num_cu_mask_count, const uint32_t* cu_mask) {
  HSAKMT_STATUS ret = hsaKmtSetQueueCUMask(
      queue_id_, num_cu_mask_count,
      reinterpret_cast<HSAuint32*>(const_cast<uint32_t*>(cu_mask)));
  return (HSAKMT_STATUS_SUCCESS == ret) ? HSA_STATUS_SUCCESS : HSA_STATUS_ERROR;
}
}  // namespace amd
