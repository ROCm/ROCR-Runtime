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

// HSA runtime C++ interface file.

#ifndef HSA_RUNTME_CORE_INC_RUNTIME_H_
#define HSA_RUNTME_CORE_INC_RUNTIME_H_

#include <vector>
#include <map>

#include "core/inc/hsa_ext_interface.h"
#include "core/inc/hsa_internal.h"

#include "core/inc/agent.h"
#include "core/inc/memory_region.h"
#include "core/inc/memory_database.h"
#include "core/inc/signal.h"
#include "core/util/utils.h"
#include "core/util/locks.h"
#include "core/util/os.h"

#include "core/inc/amd_loader_context.hpp"
#include "amd_hsa_code.hpp"

//---------------------------------------------------------------------------//
//    Constants                                                              //
//---------------------------------------------------------------------------//

#define HSA_ARGUMENT_ALIGN_BYTES 16
#define HSA_QUEUE_ALIGN_BYTES 64
#define HSA_PACKET_ALIGN_BYTES 64

namespace core {
extern bool g_use_interrupt_wait;
// class Signal;

/// @brief  Singleton for helper library attach/cleanup.
/// Protects global classes from automatic destruction during process exit.
class Runtime {
 public:
  ExtensionEntryPoints extensions_;

  static Runtime* runtime_singleton_;

  static bool IsOpen();

  static bool Acquire();

  bool Release();

  /// @brief Insert agent into agent list.
  void RegisterAgent(Agent* agent);

  /// @brief Remove agent from agent list.
  void DestroyAgents();

  /// @brief Insert memory region into memory region list.
  void RegisterMemoryRegion(MemoryRegion* region);

  /// @brief Remove memory region from list.
  void DestroyMemoryRegions();

  hsa_status_t GetSystemInfo(hsa_system_info_t attribute, void* value);

  /// @brief Call the user provided call back for each agent in the agent list.
  hsa_status_t IterateAgent(hsa_status_t (*callback)(hsa_agent_t agent,
                                                     void* data),
                            void* data);

  uint32_t GetQueueId();

  amd::hsa::loader::Loader* loader();
  amd::LoaderContext* loader_context();

  amd::hsa::code::AmdHsaCodeManager* code_manager();

  /// @brief Memory registration - tracks and provides page aligned regions to
  /// drivers
  bool Register(void* ptr, size_t length, bool registerWithDrivers = true);

  /// @brief Remove memory range from the registration list.
  bool Deregister(void* ptr);

  /// @brief Allocate memory on a particular region.
  hsa_status_t AllocateMemory(const MemoryRegion* region, size_t size,
                              void** address);

  /// @brief Free memory previously allocated with AllocateMemory.
  hsa_status_t FreeMemory(void* ptr);

  hsa_status_t AssignMemoryToAgent(void* ptr, const Agent& agent,
                                   hsa_access_permission_t access);

  hsa_status_t CopyMemory(void* dst, const void* src, size_t size);

  hsa_status_t CopyMemory(core::Agent& agent, void* dst, const void* src,
                          size_t size, std::vector<core::Signal*>& dep_signals,
                          core::Signal& out_signal);

  hsa_status_t FillMemory(void* ptr, uint32_t value, size_t count);

  /// @brief Backends hookup driver registration APIs in these functions.
  /// The runtime calls this with ranges which are whole pages
  /// and never registers a page more than once.
  bool RegisterWithDrivers(void* ptr, size_t length);
  void DeregisterWithDrivers(void* ptr);

  hsa_status_t SetAsyncSignalHandler(hsa_signal_t signal,
                                     hsa_signal_condition_t cond,
                                     hsa_signal_value_t value,
                                     hsa_amd_signal_handler handler, void* arg);

  hsa_region_t system_region() { return system_region_; }

  hsa_region_t system_region_coarse() { return system_region_coarse_; }

  std::function<void*(size_t, size_t)>& system_allocator() {
    return system_allocator_;
  }

  std::function<void(void*)>& system_deallocator() {
    return system_deallocator_;
  }

  void InitStgBuffer();

 protected:
  Runtime();

  Runtime(const Runtime&);

  Runtime& operator=(const Runtime&);

  ~Runtime() {}

  void Load();  // for dll attach and KFD open

  void Unload();  // for dll detatch and KFD close

  
  void DestroyStgBuffer();

  struct AllocationRegion {
    const MemoryRegion* region;
    const Agent* assigned_agent_;
    size_t size;

    AllocationRegion() : region(NULL), assigned_agent_(NULL), size(0) {}
    AllocationRegion(const MemoryRegion* region_arg, size_t size_arg)
        : region(region_arg), assigned_agent_(NULL), size(size_arg) {}
  };

  const AllocationRegion FindAllocatedRegion(const void* ptr);

  // Will be created before any user could call hsa_init but also could be
  // destroyed before incorrectly written programs call hsa_shutdown.
  static KernelMutex bootstrap_lock_;

  KernelMutex kernel_lock_;

  KernelMutex memory_lock_;

  volatile uint32_t ref_count_;

  // Agent list containing compatible agent in the platform.
  std::vector<Agent*> agents_;

  // Region list containing all physical memory region in the platform.
  std::vector<MemoryRegion*> regions_;

  // Shared fine grain system memory region
  hsa_region_t system_region_;

  // Shared coarse grain system memory region
  hsa_region_t system_region_coarse_;

  uint32_t queue_count_;

  // Loader instance.
  amd::hsa::loader::Loader* loader_;

  // Loader context.
  amd::LoaderContext loader_context_;

  // Code object manager.
  amd::hsa::code::AmdHsaCodeManager code_manager_;

  uintptr_t system_memory_limit_;

  // Contains list of registered memory.
  MemoryDatabase registered_memory_;

  // Contains the region, address, and size of previously allocated memory.
  std::map<const void*, AllocationRegion> allocation_map_;

  // Allocator using ::system_region_
  std::function<void*(size_t, size_t)> system_allocator_;

  // Deallocator using ::system_region_
  std::function<void(void*)> system_deallocator_;

  uint64_t sys_clock_freq_;

  Agent* blit_agent_;

  void* stg_buffer_;
  size_t stg_buffer_size_;

  struct async_events_control_t {
    hsa_signal_t wake;
    os::Thread async_events_thread_;
    KernelMutex lock;
    bool exit;

    async_events_control_t() : async_events_thread_(NULL) {}
    void Shutdown();
  } async_events_control_;

  struct {
    std::vector<hsa_signal_t> signal_;
    std::vector<hsa_signal_condition_t> cond_;
    std::vector<hsa_signal_value_t> value_;
    std::vector<hsa_amd_signal_handler> handler_;
    std::vector<void*> arg_;

    void push_back(hsa_signal_t signal, hsa_signal_condition_t cond,
                   hsa_signal_value_t value, hsa_amd_signal_handler handler,
                   void* arg) {
      signal_.push_back(signal);
      cond_.push_back(cond);
      value_.push_back(value);
      handler_.push_back(handler);
      arg_.push_back(arg);
    }

    void copy_index(size_t dst, size_t src) {
      signal_[dst] = signal_[src];
      cond_[dst] = cond_[src];
      value_[dst] = value_[src];
      handler_[dst] = handler_[src];
      arg_[dst] = arg_[src];
    }

    size_t size() { return signal_.size(); }

    void pop_back() {
      signal_.pop_back();
      cond_.pop_back();
      value_.pop_back();
      handler_.pop_back();
      arg_.pop_back();
    }

    void clear() {
      signal_.clear();
      cond_.clear();
      value_.clear();
      handler_.clear();
      arg_.clear();
    }

  } async_events_, new_async_events_;
  static void async_events_loop(void*);

  // Frees runtime memory when the runtime library is unloaded if safe to do so.
  // Failure to release the runtime indicates an incorrect application but is
  // common (example: calls library routines at process exit).
  friend class RuntimeCleanup;

  void LoadExtensions();
  void UnloadExtensions();
  void LoadTools();
  void UnloadTools();
  void CloseTools();
  std::vector<os::LibHandle> tool_libs_;

  uintptr_t start_svm_address_;
  uintptr_t end_svm_address_;  // start_svm_address_ + size
};

}  // namespace core
#endif  // header guard
