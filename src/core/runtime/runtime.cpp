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

#include "core/inc/runtime.h"

// TODO(bwicakso) : remove this once SVM property from KFD is available
#if defined(__linux__)
#include <sys/types.h>
#include <unistd.h>
#endif
#include <fstream>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "core/common/shared.h"

#include "core/inc/hsa_ext_interface.h"
#include "core/inc/amd_cpu_agent.h"
#include "core/inc/amd_gpu_agent.h"
#include "core/inc/amd_memory_region.h"
#include "core/inc/amd_topology.h"
#include "core/inc/signal.h"

#include "core/inc/hsa_api_trace_int.h"

#define HSA_VERSION_MAJOR 1
#define HSA_VERSION_MINOR 0

namespace core {
bool g_use_interrupt_wait = true;

Runtime* Runtime::runtime_singleton_ = NULL;

KernelMutex Runtime::bootstrap_lock_;

static bool loaded = true;

class RuntimeCleanup {
 public:
  ~RuntimeCleanup() {
    if (!Runtime::IsOpen()) delete Runtime::runtime_singleton_;
    loaded = false;
  }
};
static RuntimeCleanup cleanup_at_unload_;

bool Runtime::Acquire() {
  // Check to see if HSA has been cleaned up (process exit)
  if (!loaded) return false;

  // Handle initialization races
  ScopedAcquire<KernelMutex> boot(&bootstrap_lock_);
  if (runtime_singleton_ == NULL) runtime_singleton_ = new Runtime();

  // Serialize with release
  ScopedAcquire<KernelMutex> lock(&runtime_singleton_->kernel_lock_);
  if (runtime_singleton_->ref_count_ == INT32_MAX) return false;
  runtime_singleton_->ref_count_++;
  if (runtime_singleton_->ref_count_ == 1) runtime_singleton_->Load();
  return true;
}

bool Runtime::Release() {
  ScopedAcquire<KernelMutex> lock(&kernel_lock_);
  if (ref_count_ == 0) return false;
  if (ref_count_ == 1)  // Release all registered memory, then unload backends
  {
    Unload();
  }
  ref_count_--;
  return true;
}

bool Runtime::IsOpen() {
  return (Runtime::runtime_singleton_ != NULL) &&
         (Runtime::runtime_singleton_->ref_count_ != 0);
}

void Runtime::RegisterAgent(Agent* agent) {
  agents_.push_back(agent);

  // Setup system clock frequency for the first time.
  if (sys_clock_freq_ == 0 &&
    agent->device_type() == Agent::DeviceType::kAmdCpuDevice) {
    // Cache system clock frequency
    HsaClockCounters clocks;
    hsaKmtGetClockCounters(0, &clocks);
    sys_clock_freq_ = clocks.SystemClockFrequencyHz;
    host_agent_ = agent;
  }

  // Assign the first discovered gpu agent as blit agent that will provide
  // DMA operation.
  if (agent->device_type() == Agent::DeviceType::kAmdGpuDevice) {
    if (blit_agent_ == NULL) {
      blit_agent_ = agent;
    }

    gpu_ids_.push_back(reinterpret_cast<amd::GpuAgent*>(agent)->node_id());
  }

  // Query the start and end address of the SVM address space in this platform.
  if (agent->device_type() == Agent::DeviceType::kAmdGpuDevice) {
    if (reinterpret_cast<amd::GpuAgentInt*>(agent)->profile() ==
        HSA_PROFILE_BASE) {
      // TODO(bwicakso): temporary code until KFD expose new SVM memory
      // property.
#if defined(__linux__)
      pid_t pid = getpid();
      std::string maps_file = "/proc/" + std::to_string(pid) + "/maps";
      std::ifstream in(maps_file.c_str(), std::ios::in);

      if (in.good()) {
        std::string line;
        while (std::getline(in, line)) {
          char* end = NULL;
          uintptr_t start_addr = strtoul(line.c_str(), &end, 16);
          uintptr_t end_addr = strtoul(end + 1, &end, 16);

          if ((end_addr - start_addr) >= (8ULL * 1024 * 1024 * 1024)) {
            start_svm_address_ = start_addr;
            end_svm_address_ = end_addr;
            break;
          }
        }
      }

      assert(start_svm_address_ > 0);
#endif
    } else {
      start_svm_address_ = 0;
      end_svm_address_ = os::GetUserModeVirtualMemoryBase() +
                         os::GetUserModeVirtualMemorySize();
    }
  }
}

void Runtime::DestroyAgents() {
  std::for_each(agents_.begin(), agents_.end(), DeleteObject());
  agents_.clear();

  gpu_ids_.clear();

  blit_agent_ = NULL;
}

void Runtime::RegisterMemoryRegion(MemoryRegion* region) {
  regions_.push_back(region);

  amd::MemoryRegion* amd_region = reinterpret_cast<amd::MemoryRegion*>(region);

  if (amd_region->IsSystem()) {
    if (region->fine_grain() && system_region_.handle == 0) {
      system_region_ = MemoryRegion::Convert(region);

      // Setup fine grain system region allocator.
      if (amd::MemoryRegion::Convert(system_region_)->full_profile()) {
        system_allocator_ = [](size_t size, size_t alignment) -> void * {
          return _aligned_malloc(size, alignment);
        };

        system_deallocator_ = [](void* ptr) { _aligned_free(ptr); };

        // On APU, there is no coarse grain system memory, so treat it like
        // fine grain memory.
        system_region_coarse_.handle = system_region_.handle;
      } else {
        // TODO(bwicakso): might need memory pooling to cover allocation that
        // requires less than 4096 bytes.
        system_allocator_ = [&](size_t size, size_t alignment) -> void * {
          assert(alignment <= 4096);
          void* ptr = NULL;
          return (HSA_STATUS_SUCCESS ==
                  HSA::hsa_memory_allocate(system_region_, size, &ptr))
                     ? ptr
                     : NULL;
        };

        system_deallocator_ = [](void* ptr) { HSA::hsa_memory_free(ptr); };
      }

      BaseShared::SetAllocateAndFree(system_allocator_, system_deallocator_);
    } else if (!region->fine_grain() && system_region_coarse_.handle == 0) {
      assert(system_region_coarse_.handle == 0);
      system_region_coarse_ = MemoryRegion::Convert(region);
    }
  } else if (amd_region->IsSvm()) {
    start_svm_address_ = static_cast<uintptr_t>(amd_region->GetBaseAddress());
    end_svm_address_ = start_svm_address_ +
                       static_cast<uintptr_t>(amd_region->GetVirtualSize());
  }
}

void Runtime::DestroyMemoryRegions() {
  std::for_each(regions_.begin(), regions_.end(), DeleteObject());
  regions_.clear();
  system_region_.handle = 0;
  system_region_coarse_.handle = 0;
}

hsa_status_t Runtime::IterateAgent(hsa_status_t (*callback)(hsa_agent_t agent,
                                                            void* data),
                                   void* data) {
  const size_t num_agent = agents_.size();

  if (!IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }

  for (size_t i = 0; i < num_agent; ++i) {
    hsa_agent_t agent = Agent::Convert(agents_[i]);
    hsa_status_t status = callback(agent, data);

    if (status != HSA_STATUS_SUCCESS) {
      return status;
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::AllocateMemory(const MemoryRegion* region, size_t size,
                                     void** ptr) {
  return AllocateMemory(false, region, size, ptr);
}

hsa_status_t Runtime::AllocateMemory(bool restrict_access,
                                     const MemoryRegion* region, size_t size,
                                     void** address) {
  const amd::MemoryRegion* amd_region =
      reinterpret_cast<const amd::MemoryRegion*>(region);
  hsa_status_t status = amd_region->Allocate(restrict_access, size, address);

  // Track the allocation result so that it could be freed properly.
  if (status == HSA_STATUS_SUCCESS) {
    ScopedAcquire<KernelMutex> lock(&memory_lock_);
    allocation_map_[*address] = AllocationRegion(region, size);
  }

  return status;
}

hsa_status_t Runtime::FreeMemory(void* ptr) {
  if (ptr == NULL) {
    return HSA_STATUS_SUCCESS;
  }

  const MemoryRegion* region = NULL;
  size_t size = 0;
  {
    ScopedAcquire<KernelMutex> lock(&memory_lock_);

    std::map<const void*, AllocationRegion>::const_iterator it =
        allocation_map_.find(ptr);

    if (it == allocation_map_.end()) {
      assert(false && "Can't find address in allocation map");
      return HSA_STATUS_ERROR;
    }

    region = it->second.region;
    size = it->second.size;

    allocation_map_.erase(it);
  }

  return region->Free(ptr, size);
}

hsa_status_t Runtime::CopyMemory(void* dst, const void* src, size_t size) {
  assert(dst != NULL && src != NULL && size != 0);

  bool is_src_system = false;
  bool is_dst_system = false;
  const uintptr_t src_uptr = reinterpret_cast<uintptr_t>(src);
  const uintptr_t dst_uptr = reinterpret_cast<uintptr_t>(dst);

  if ((reinterpret_cast<amd::GpuAgentInt*>(blit_agent_)->profile() ==
       HSA_PROFILE_FULL)) {
    is_src_system = (src_uptr < end_svm_address_);
    is_dst_system = (dst_uptr < end_svm_address_);
  } else {
    is_src_system =
        ((src_uptr < start_svm_address_) || (src_uptr >= end_svm_address_));
    is_dst_system =
        ((dst_uptr < start_svm_address_) || (dst_uptr >= end_svm_address_));

    if ((is_src_system && !is_dst_system) ||
        (!is_src_system && is_dst_system)) {
      // Use staging buffer or pin if either src or dst is gpuvm and the other
      // is system memory allocated via OS or C/C++ allocator.
      return CopyMemoryHostAlloc(dst, src, size, is_dst_system);
    }
  }

  if (is_src_system && is_dst_system) {
    memmove(dst, src, size);
    return HSA_STATUS_SUCCESS;
  }

  return blit_agent_->DmaCopy(dst, src, size);
}

hsa_status_t Runtime::CopyMemoryHostAlloc(void* dst, const void* src,
                                          size_t size, bool dst_malloc) {
  void* usrptr = (dst_malloc) ? dst : const_cast<void*>(src);
  void* agent_ptr = NULL;

  hsa_agent_t blit_agent = core::Agent::Convert(blit_agent_);

  amd::MemoryRegion* system_region = amd::MemoryRegion::Convert(
      core::Runtime::runtime_singleton_->system_region());
  hsa_status_t stat =
      system_region->Lock(1, &blit_agent, usrptr, size, &agent_ptr);

  if (stat != HSA_STATUS_SUCCESS) {
    return stat;
  }

  stat = blit_agent_->DmaCopy((dst_malloc) ? agent_ptr : dst,
                              (dst_malloc) ? src : agent_ptr, size);

  system_region->Unlock(usrptr);

  return stat;
}

hsa_status_t Runtime::CopyMemory(void* dst, core::Agent& dst_agent,
                                 const void* src, core::Agent& src_agent,
                                 size_t size,
                                 std::vector<core::Signal*>& dep_signals,
                                 core::Signal& completion_signal) {
  const bool dst_gpu =
      (dst_agent.device_type() == core::Agent::DeviceType::kAmdGpuDevice);
  const bool src_gpu =
      (src_agent.device_type() == core::Agent::DeviceType::kAmdGpuDevice);
  if (dst_gpu || src_gpu) {
    core::Agent& copy_agent = (src_gpu) ? src_agent : dst_agent;
    return copy_agent.DmaCopy(dst, src, size, dep_signals, completion_signal);
  }

  // For cpu to cpu, fire and forget a copy thread.
  std::thread([](void* dst, const void* src, size_t size,
                 std::vector<core::Signal*> dep_signals,
                 core::Signal* completion_signal) {
                for (core::Signal* dep : dep_signals) {
                  dep->WaitRelaxed(HSA_SIGNAL_CONDITION_EQ, 0, UINT64_MAX,
                                   HSA_WAIT_STATE_BLOCKED);
                }

                memcpy(dst, src, size);

                completion_signal->SubRelease(1);
              },
              dst, src, size, dep_signals, &completion_signal).detach();

  return HSA_STATUS_SUCCESS;
}

hsa_status_t Runtime::FillMemory(void* ptr, uint32_t value, size_t count) {
  assert(blit_agent_ != NULL);
  return blit_agent_->DmaFill(ptr, value, count);
}

hsa_status_t Runtime::AllowAccess(uint32_t num_agents,
                                  const hsa_agent_t* agents, const void* ptr) {
  const amd::MemoryRegion* amd_region = NULL;
  size_t alloc_size = 0;

  {
    ScopedAcquire<KernelMutex> lock(&memory_lock_);

    std::map<const void*, AllocationRegion>::const_iterator it =
        allocation_map_.find(ptr);

    if (it == allocation_map_.end()) {
      return HSA_STATUS_ERROR;
    }

    amd_region = reinterpret_cast<const amd::MemoryRegion*>(it->second.region);
    alloc_size = it->second.size;
  }

  return amd_region->AllowAccess(num_agents, agents, ptr, alloc_size);
}

hsa_status_t Runtime::GetSystemInfo(hsa_system_info_t attribute, void* value) {
  switch (attribute) {
    case HSA_SYSTEM_INFO_VERSION_MAJOR:
      *((uint16_t*)value) = HSA_VERSION_MAJOR;
      break;
    case HSA_SYSTEM_INFO_VERSION_MINOR:
      *((uint16_t*)value) = HSA_VERSION_MINOR;
      break;
    case HSA_SYSTEM_INFO_TIMESTAMP: {
      HsaClockCounters clocks;
      hsaKmtGetClockCounters(0, &clocks);
      *((uint64_t*)value) = clocks.SystemClockCounter;
      break;
    }
    case HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY: {
      assert(sys_clock_freq_ != 0 &&
             "Use of HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY before HSA "
             "initialization completes.");
      *(uint64_t*)value = sys_clock_freq_;
      break;
    }
    case HSA_SYSTEM_INFO_SIGNAL_MAX_WAIT:
      *((uint64_t*)value) = 0xFFFFFFFFFFFFFFFF;
      break;
    case HSA_SYSTEM_INFO_ENDIANNESS:
#if defined(HSA_LITTLE_ENDIAN)
      *((hsa_endianness_t*)value) = HSA_ENDIANNESS_LITTLE;
#else
      *((hsa_endianness_t*)value) = HSA_ENDIANNESS_BIG;
#endif
      break;
    case HSA_SYSTEM_INFO_MACHINE_MODEL:
#if defined(HSA_LARGE_MODEL)
      *((hsa_machine_model_t*)value) = HSA_MACHINE_MODEL_LARGE;
#else
      *((hsa_machine_model_t*)value) = HSA_MACHINE_MODEL_SMALL;
#endif
      break;
    case HSA_SYSTEM_INFO_EXTENSIONS:
      memset(value, 0, sizeof(uint8_t) * 128);

      if (extensions_.table.hsa_ext_program_finalize_fn != NULL) {
        *((uint8_t*)value) = 1 << HSA_EXTENSION_FINALIZER;
      }

      if (extensions_.table.hsa_ext_image_create_fn != NULL) {
        *((uint8_t*)value) |= 1 << HSA_EXTENSION_IMAGES;
      }

      *((uint8_t*)value) |= 1 << HSA_EXTENSION_AMD_PROFILER;

      break;
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
}

uint32_t Runtime::GetQueueId() { return atomic::Increment(&queue_count_); }

hsa_status_t Runtime::SetAsyncSignalHandler(hsa_signal_t signal,
                                            hsa_signal_condition_t cond,
                                            hsa_signal_value_t value,
                                            hsa_amd_signal_handler handler,
                                            void* arg) {
  // Asyncronous signal handler is only supported when KFD events are on.
  if (!core::g_use_interrupt_wait) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;

  // Indicate that this signal is in use.
  if (signal.handle != 0) hsa_signal_handle(signal)->Retain();

  ScopedAcquire<KernelMutex> scope_lock(&async_events_control_.lock);

  // Lazy initializer
  if (async_events_control_.async_events_thread_ == NULL) {
    // Create monitoring thread control signal
    auto err = HSA::hsa_signal_create(0, 0, NULL, &async_events_control_.wake);
    if (err != HSA_STATUS_SUCCESS) {
      assert(false && "Asyncronous events control signal creation error.");
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
    async_events_.PushBack(async_events_control_.wake, HSA_SIGNAL_CONDITION_NE,
                           0, NULL, NULL);

    // Start event monitoring thread
    async_events_control_.exit = false;
    async_events_control_.async_events_thread_ =
        os::CreateThread(AsyncEventsLoop, NULL);
    if (async_events_control_.async_events_thread_ == NULL) {
      assert(false && "Asyncronous events thread creation error.");
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  }

  new_async_events_.PushBack(signal, cond, value, handler, arg);

  hsa_signal_handle(async_events_control_.wake)->StoreRelease(1);

  return HSA_STATUS_SUCCESS;
}

void Runtime::AsyncEventsLoop(void*) {
  auto& async_events_control_ = runtime_singleton_->async_events_control_;
  auto& async_events_ = runtime_singleton_->async_events_;
  auto& new_async_events_ = runtime_singleton_->new_async_events_;

  while (!async_events_control_.exit) {
    // Wait for a signal
    hsa_signal_value_t value;
    uint32_t index = hsa_amd_signal_wait_any(
        uint32_t(async_events_.Size()), &async_events_.signal_[0],
        &async_events_.cond_[0], &async_events_.value_[0], uint64_t(-1),
        HSA_WAIT_STATE_BLOCKED, &value);

    // Reset the control signal
    if (index == 0) {
      hsa_signal_handle(async_events_control_.wake)->StoreRelaxed(0);
    } else if (index != -1) {
      // No error or timout occured, process the handler
      assert(async_events_.handler_[index] != NULL);
      bool keep =
          async_events_.handler_[index](value, async_events_.arg_[index]);
      if (!keep) {
        hsa_signal_handle(async_events_.signal_[index])->Release();
        async_events_.CopyIndex(index, async_events_.Size() - 1);
        async_events_.PopBack();
      }
    }

    // Check for dead signals
    index = 0;
    while (index != async_events_.Size()) {
      if (!hsa_signal_handle(async_events_.signal_[index])->IsValid()) {
        hsa_signal_handle(async_events_.signal_[index])->Release();
        async_events_.CopyIndex(index, async_events_.Size() - 1);
        async_events_.PopBack();
        continue;
      }
      index++;
    }

    // Insert new signals and find plain functions
    typedef std::pair<void (*)(void*), void*> func_arg_t;
    std::vector<func_arg_t> functions;
    {
      ScopedAcquire<KernelMutex> scope_lock(&async_events_control_.lock);
      for (size_t i = 0; i < new_async_events_.Size(); i++) {
        if (new_async_events_.signal_[i].handle == 0) {
          functions.push_back(
              func_arg_t((void (*)(void*))new_async_events_.handler_[i],
                         new_async_events_.arg_[i]));
          continue;
        }
        async_events_.PushBack(
            new_async_events_.signal_[i], new_async_events_.cond_[i],
            new_async_events_.value_[i], new_async_events_.handler_[i],
            new_async_events_.arg_[i]);
      }
      new_async_events_.Clear();
    }

    // Call plain functions
    for (size_t i = 0; i < functions.size(); i++)
      functions[i].first(functions[i].second);
    functions.clear();
  }

  // Release wait count of all pending signals
  for (size_t i = 1; i < async_events_.Size(); i++)
    hsa_signal_handle(async_events_.signal_[i])->Release();
  async_events_.Clear();

  for (size_t i = 0; i < new_async_events_.Size(); i++)
    hsa_signal_handle(new_async_events_.signal_[i])->Release();
  new_async_events_.Clear();
}

Runtime::Runtime()
    : host_agent_(NULL),
      blit_agent_(NULL),
      queue_count_(0),
      sys_clock_freq_(0),
      ref_count_(0) {
  system_region_.handle = 0;
  system_region_coarse_.handle = 0;

  start_svm_address_ = 0;
#if defined(HSA_LARGE_MODEL)
  end_svm_address_ = UINT64_MAX;
#else
  end_svm_address_ = UINT32_MAX;
#endif
}

void Runtime::Load() {
  // Load interrupt enable option
  std::string interrupt = os::GetEnvVar("HSA_ENABLE_INTERRUPT");
  g_use_interrupt_wait = (interrupt != "0");

  // Default system allocator is fine grain memory allocator using C malloc.
  system_allocator_ = [](size_t size, size_t alignment) -> void * {
    return _aligned_malloc(size, alignment);
  };
  system_deallocator_ = [](void* ptr) { _aligned_free(ptr); };
  BaseShared::SetAllocateAndFree(system_allocator_, system_deallocator_);

  if (!amd::Load()) {
    return;
  }

  loader_ = amd::hsa::loader::Loader::Create(&loader_context_);

  // Load extensions
  LoadExtensions();

  // Load tools libraries
  LoadTools();
}

void Runtime::Unload() {
  UnloadTools();
  UnloadExtensions();

  amd::hsa::loader::Loader::Destroy(loader_);
  loader_ = nullptr;
  DestroyAgents();
  CloseTools();

  async_events_control_.Shutdown();

  DestroyMemoryRegions();

  amd::Unload();
}

void Runtime::LoadExtensions() {
// Load finalizer and extension library
#ifdef HSA_LARGE_MODEL
  static const std::string kFinalizerLib[] = {"hsa-ext-finalize64.dll",
                                              "libhsa-ext-finalize64.so.1"};
  static const std::string kImageLib[] = {"hsa-ext-image64.dll",
                                          "libhsa-ext-image64.so.1"};
#else
  static const std::string kFinalizerLib[] = {"hsa-ext-finalize.dll",
                                              "libhsa-ext-finalize.so.1"};
  static const std::string kImageLib[] = {"hsa-ext-image.dll",
                                          "libhsa-ext-image.so.1"};
#endif
  extensions_.Load(kFinalizerLib[os_index(os::current_os)]);
  extensions_.Load(kImageLib[os_index(os::current_os)]);
}

void Runtime::UnloadExtensions() { extensions_.Unload(); }

static std::vector<std::string> parse_tool_names(std::string tool_names) {
  std::vector<std::string> names;
  std::string name = "";
  bool quoted = false;
  while (tool_names.size() != 0) {
    auto index = tool_names.find_first_of(" \"\\");
    if (index == std::string::npos) {
      name += tool_names;
      break;
    }
    switch (tool_names[index]) {
      case ' ': {
        if (!quoted) {
          name += tool_names.substr(0, index);
          tool_names.erase(0, index + 1);
          names.push_back(name);
          name = "";
        } else {
          name += tool_names.substr(0, index + 1);
          tool_names.erase(0, index + 1);
        }
        break;
      }
      case '\"': {
        if (quoted) {
          quoted = false;
          name += tool_names.substr(0, index);
          tool_names.erase(0, index + 1);
          names.push_back(name);
          name = "";
        } else {
          quoted = true;
          tool_names.erase(0, index + 1);
        }
        break;
      }
      case '\\': {
        if (tool_names.size() > index + 1) {
          name += tool_names.substr(0, index) + tool_names[index + 1];
          tool_names.erase(0, index + 2);
        }
        break;
      }
    }  // end switch
  }    // end while

  if (name != "") names.push_back(name);
  return names;
}

void Runtime::LoadTools() {
  typedef bool (*tool_init_t)(::ApiTable*, uint64_t, uint64_t,
                              const char* const*);
  typedef Agent* (*tool_wrap_t)(Agent*);
  typedef void (*tool_add_t)(Runtime*);

  // Link extensions to API interception
  hsa_api_table_.LinkExts(&extensions_.table);

  // Load tool libs
  std::string tool_names = os::GetEnvVar("HSA_TOOLS_LIB");
  if (tool_names != "") {
    std::vector<std::string> names = parse_tool_names(tool_names);
    std::vector<const char*> failed;
    for (int i = 0; i < names.size(); i++) {
      os::LibHandle tool = os::LoadLib(names[i]);

      if (tool != NULL) {
        tool_libs_.push_back(tool);

        size_t size = agents_.size();

        tool_init_t ld;
        ld = (tool_init_t)os::GetExportAddress(tool, "OnLoad");
        if (ld) {
          if (!ld(&hsa_api_table_.table, 0, failed.size(), &failed[0])) {
            failed.push_back(names[i].c_str());
            os::CloseLib(tool);
            continue;
          }
        }
        tool_wrap_t wrap;
        wrap = (tool_wrap_t)os::GetExportAddress(tool, "WrapAgent");
        if (wrap) {
          for (int j = 0; j < size; j++) {
            Agent* agent = wrap(agents_[j]);
            if (agent != NULL) {
              assert(agent->IsValid() &&
                     "Agent returned from WrapAgent is not valid");
              agents_[j] = agent;
            }
          }
        }

        tool_add_t add;
        add = (tool_add_t)os::GetExportAddress(tool, "AddAgent");
        if (add) add(this);
      }
    }
  }
}

void Runtime::UnloadTools() {
  typedef void (*tool_unload_t)();
  for (size_t i = tool_libs_.size(); i != 0; i--) {
    tool_unload_t unld;
    unld = (tool_unload_t)os::GetExportAddress(tool_libs_[i - 1], "OnUnload");
    if (unld) unld();
  }

  // Reset API table in case some tool doesn't cleanup properly
  hsa_api_table_.Reset();
}

void Runtime::CloseTools() {
  // Due to valgrind bug, runtime cannot dlclose extensions see:
  // http://valgrind.org/docs/manual/faq.html#faq.unhelpful
  if (os::GetEnvVar("HSA_RUNNING_UNDER_VALGRIND") != "1") {
    for (int i = 0; i < tool_libs_.size(); i++) os::CloseLib(tool_libs_[i]);
  }
  tool_libs_.clear();
}

void Runtime::AsyncEventsControl::Shutdown() {
  if (async_events_thread_ != NULL) {
    exit = true;
    hsa_signal_handle(wake)->StoreRelaxed(1);
    os::WaitForThread(async_events_thread_);
    os::CloseThread(async_events_thread_);
    async_events_thread_ = NULL;
    HSA::hsa_signal_destroy(wake);
  }
}

void Runtime::AsyncEvents::PushBack(hsa_signal_t signal,
                                    hsa_signal_condition_t cond,
                                    hsa_signal_value_t value,
                                    hsa_amd_signal_handler handler, void* arg) {
  signal_.push_back(signal);
  cond_.push_back(cond);
  value_.push_back(value);
  handler_.push_back(handler);
  arg_.push_back(arg);
}

void Runtime::AsyncEvents::CopyIndex(size_t dst, size_t src) {
  signal_[dst] = signal_[src];
  cond_[dst] = cond_[src];
  value_[dst] = value_[src];
  handler_[dst] = handler_[src];
  arg_[dst] = arg_[src];
}

size_t Runtime::AsyncEvents::Size() { return signal_.size(); }

void Runtime::AsyncEvents::PopBack() {
  signal_.pop_back();
  cond_.pop_back();
  value_.pop_back();
  handler_.pop_back();
  arg_.pop_back();
}

void Runtime::AsyncEvents::Clear() {
  signal_.clear();
  cond_.clear();
  value_.clear();
  handler_.clear();
  arg_.clear();
}

}  // namespace core
