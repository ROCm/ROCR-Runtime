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
#include <utility>

#include "core/inc/hsa_ext_interface.h"
#include "core/inc/hsa_internal.h"
#include "core/inc/hsa_ext_amd_impl.h"

#include "core/inc/agent.h"
#include "core/inc/exceptions.h"
#include "core/inc/memory_region.h"
#include "core/inc/signal.h"
#include "core/inc/interrupt_signal.h"
#include "core/util/flag.h"
#include "core/util/locks.h"
#include "core/util/os.h"
#include "core/util/utils.h"

#include "core/inc/amd_loader_context.hpp"
#include "amd_hsa_code.hpp"

//---------------------------------------------------------------------------//
//    Constants                                                              //
//---------------------------------------------------------------------------//

#define HSA_ARGUMENT_ALIGN_BYTES 16
#define HSA_QUEUE_ALIGN_BYTES 64
#define HSA_PACKET_ALIGN_BYTES 64

//Avoids include
namespace amd {
  class MemoryRegion;
}

namespace core {
extern bool g_use_interrupt_wait;

/// @brief  Runtime class provides the following functions:
/// - open and close connection to kernel driver.
/// - load supported extension library (image and finalizer).
/// - load tools library.
/// - expose supported agents.
/// - allocate and free memory.
/// - memory copy and fill.
/// - grant access to memory (dgpu memory pool extension).
/// - maintain loader state.
/// - monitor asynchronous event from agent.
class Runtime {
 friend class amd::MemoryRegion;
 public:
  /// @brief Structure to describe connectivity between agents.
  struct LinkInfo {
    LinkInfo() : num_hop(0), info{0} {}

    uint32_t num_hop;
    hsa_amd_memory_pool_link_info_t info;
  };

  /// @brief Open connection to kernel driver and increment reference count.
  static hsa_status_t Acquire();

  /// @brief Decrement reference count and close connection to kernel driver.
  static hsa_status_t Release();

  /// @brief Checks if connection to kernel driver is opened.
  /// @retval True if the connection to kernel driver is opened.
  static bool IsOpen();

  // @brief Callback handler for VM fault access.
  static bool VMFaultHandler(hsa_signal_value_t val, void* arg);

  /// @brief Singleton object of the runtime.
  static Runtime* runtime_singleton_;

  /// @brief Insert agent into agent list ::agents_.
  /// @param [in] agent Pointer to the agent object.
  void RegisterAgent(Agent* agent);

  /// @brief Delete all agent objects from ::agents_.
  void DestroyAgents();

  /// @brief Set the number of links connecting the agents in the platform.
  void SetLinkCount(size_t num_link);

  /// @brief Register link information connecting @p node_id_from and @p
  /// node_id_to.
  /// @param [in] node_id_from Node id of the source node.
  /// @param [in] node_id_to Node id of the destination node.
  /// @param [in] link_info The link information between source and destination
  /// nodes.
  void RegisterLinkInfo(uint32_t node_id_from, uint32_t node_id_to,
                        uint32_t num_hop,
                        hsa_amd_memory_pool_link_info_t& link_info);

  /// @brief Query link information between two nodes.
  /// @param [in] node_id_from Node id of the source node.
  /// @param [in] node_id_to Node id of the destination node.
  /// @retval The link information between source and destination nodes.
  const LinkInfo GetLinkInfo(uint32_t node_id_from, uint32_t node_id_to);

  /// @brief Invoke the user provided call back for each agent in the agent
  /// list.
  ///
  /// @param [in] callback User provided callback function.
  /// @param [in] data User provided pointer as input for @p callback.
  ///
  /// @retval ::HSA_STATUS_SUCCESS if the callback function for each traversed
  /// agent returns ::HSA_STATUS_SUCCESS.
  hsa_status_t IterateAgent(hsa_status_t (*callback)(hsa_agent_t agent,
                                                     void* data),
                            void* data);

  /// @brief Allocate memory on a particular region.
  ///
  /// @param [in] region Pointer to region object.
  /// @param [in] size Allocation size in bytes.
  /// @param [in] alloc_flags Modifiers to pass to MemoryRegion allocator.
  /// @param [out] address Pointer to store the allocation result.
  ///
  /// @retval ::HSA_STATUS_SUCCESS If allocation is successful.
  hsa_status_t AllocateMemory(const MemoryRegion* region, size_t size,
                              MemoryRegion::AllocateFlags alloc_flags,
                              void** address);

  /// @brief Free memory previously allocated with AllocateMemory.
  ///
  /// @param [in] ptr Address of the memory to be freed.
  ///
  /// @retval ::HSA_STATUS_ERROR If @p ptr is not the address of previous
  /// allocation via ::core::Runtime::AllocateMemory
  /// @retval ::HSA_STATUS_SUCCESS if @p ptr is successfully released.
  hsa_status_t FreeMemory(void* ptr);

  /// @brief Blocking memory copy from src to dst.
  ///
  /// @param [in] dst Memory address of the destination.
  /// @param [in] src Memory address of the source.
  /// @param [in] size Copy size in bytes.
  ///
  /// @retval ::HSA_STATUS_SUCCESS if memory copy is successful and completed.
  hsa_status_t CopyMemory(void* dst, const void* src, size_t size);

  /// @brief Non-blocking memory copy from src to dst.
  ///
  /// @details The memory copy will be performed after all signals in
  /// @p dep_signals have value of 0. On completion @p completion_signal
  /// will be decremented.
  ///
  /// @param [in] dst Memory address of the destination.
  /// @param [in] dst_agent Agent object associated with the destination. This
  /// agent should be able to access the destination and source.
  /// @param [in] src Memory address of the source.
  /// @param [in] src_agent Agent object associated with the source. This
  /// agent should be able to access the destination and source.
  /// @param [in] size Copy size in bytes.
  /// @param [in] dep_signals Array of signal dependency.
  /// @param [in] completion_signal Completion signal object.
  ///
  /// @retval ::HSA_STATUS_SUCCESS if copy command has been submitted
  /// successfully to the agent DMA queue.
  hsa_status_t CopyMemory(void* dst, core::Agent& dst_agent, const void* src,
                          core::Agent& src_agent, size_t size,
                          std::vector<core::Signal*>& dep_signals,
                          core::Signal& completion_signal);

  /// @brief Fill the first @p count of uint32_t in ptr with value.
  ///
  /// @param [in] ptr Memory address to be filled.
  /// @param [in] value The value/pattern that will be used to set @p ptr.
  /// @param [in] count Number of uint32_t element to be set.
  ///
  /// @retval ::HSA_STATUS_SUCCESS if memory fill is successful and completed.
  hsa_status_t FillMemory(void* ptr, uint32_t value, size_t count);

  /// @brief Set agents as the whitelist to access ptr.
  ///
  /// @param [in] num_agents The number of agent handles in @p agents array.
  /// @param [in] agents Agent handle array.
  /// @param [in] ptr Pointer of memory previously allocated via
  /// core::Runtime::AllocateMemory.
  ///
  /// @retval ::HSA_STATUS_SUCCESS The whitelist has been configured
  /// successfully and all agents in the @p agents could start accessing @p ptr.
  hsa_status_t AllowAccess(uint32_t num_agents, const hsa_agent_t* agents,
                           const void* ptr);

  /// @brief Query system information.
  ///
  /// @param [in] attribute System info attribute to query.
  /// @param [out] value Pointer to store the attribute value.
  ///
  /// @retval HSA_STATUS_SUCCESS The attribute is valid and the @p value is
  /// set.
  hsa_status_t GetSystemInfo(hsa_system_info_t attribute, void* value);

  /// @brief Query next available queue id.
  ///
  /// @retval Next available queue id.
  uint32_t GetQueueId();

  /// @brief Register a callback function @p handler that is associated with
  /// @p signal to asynchronous event monitor thread.
  ///
  /// @param [in] signal Signal handle associated with @p handler.
  /// @param [in] cond The condition to execute the @p handler.
  /// @param [in] value The value to compare with @p signal value. If the
  /// comparison satisfy @p cond, the @p handler will be called.
  /// @param [in] arg Pointer to the argument that will be provided to @p
  /// handler.
  ///
  /// @retval ::HSA_STATUS_SUCCESS Registration is successful.
  hsa_status_t SetAsyncSignalHandler(hsa_signal_t signal,
                                     hsa_signal_condition_t cond,
                                     hsa_signal_value_t value,
                                     hsa_amd_signal_handler handler, void* arg);

  hsa_status_t InteropMap(uint32_t num_agents, Agent** agents,
                          int interop_handle, uint32_t flags, size_t* size,
                          void** ptr, size_t* metadata_size,
                          const void** metadata);

  hsa_status_t InteropUnmap(void* ptr);

  struct PtrInfoBlockData {
    void* base;
    size_t length;
  };

  hsa_status_t PtrInfo(void* ptr, hsa_amd_pointer_info_t* info, void* (*alloc)(size_t),
                       uint32_t* num_agents_accessible, hsa_agent_t** accessible,
                       PtrInfoBlockData* block_info = nullptr);

  hsa_status_t SetPtrInfoData(void* ptr, void* userptr);

  hsa_status_t IPCCreate(void* ptr, size_t len, hsa_amd_ipc_memory_t* handle);

  hsa_status_t IPCAttach(const hsa_amd_ipc_memory_t* handle, size_t len, uint32_t num_agents,
                         Agent** mapping_agents, void** mapped_ptr);

  hsa_status_t IPCDetach(void* ptr);

  const std::vector<Agent*>& cpu_agents() { return cpu_agents_; }

  const std::vector<Agent*>& gpu_agents() { return gpu_agents_; }

  const std::vector<uint32_t>& gpu_ids() { return gpu_ids_; }

  Agent* region_gpu() { return region_gpu_; }

  const std::vector<const MemoryRegion*>& system_regions_fine() const {
    return system_regions_fine_;
  }

  const std::vector<const MemoryRegion*>& system_regions_coarse() const {
    return system_regions_coarse_;
  }

  amd::hsa::loader::Loader* loader() { return loader_; }

  amd::LoaderContext* loader_context() { return &loader_context_; }

  amd::hsa::code::AmdHsaCodeManager* code_manager() { return &code_manager_; }

  std::function<void*(size_t, size_t, MemoryRegion::AllocateFlags)>&
  system_allocator() {
    return system_allocator_;
  }

  std::function<void(void*)>& system_deallocator() {
    return system_deallocator_;
  }

  const Flag& flag() const { return flag_; }

  ExtensionEntryPoints extensions_;

  hsa_status_t SetCustomSystemEventHandler(hsa_amd_system_event_callback_t callback,
                                           void* data);

  hsa_status_t SetInternalQueueCreateNotifier(hsa_amd_runtime_queue_notifier callback,
                                              void* user_data);

  void InternalQueueCreateNotify(const hsa_queue_t* queue, hsa_agent_t agent);

  SharedSignalPool_t* GetSharedSignalPool() { return &SharedSignalPool; }

  InterruptSignal::EventPool* GetEventPool() { return &EventPool; }

 protected:
  static void AsyncEventsLoop(void*);

  struct AllocationRegion {
    AllocationRegion() : region(NULL), size(0), user_ptr(nullptr) {}
    AllocationRegion(const MemoryRegion* region_arg, size_t size_arg)
        : region(region_arg), size(size_arg), user_ptr(nullptr) {}

    const MemoryRegion* region;
    size_t size;
    void* user_ptr;
  };

  struct AsyncEventsControl {
    AsyncEventsControl() : async_events_thread_(NULL) {}
    void Shutdown();

    hsa_signal_t wake;
    os::Thread async_events_thread_;
    KernelMutex lock;
    bool exit;
  };

  struct AsyncEvents {
    void PushBack(hsa_signal_t signal, hsa_signal_condition_t cond,
                  hsa_signal_value_t value, hsa_amd_signal_handler handler,
                  void* arg);

    void CopyIndex(size_t dst, size_t src);

    size_t Size();

    void PopBack();

    void Clear();

    std::vector<hsa_signal_t> signal_;
    std::vector<hsa_signal_condition_t> cond_;
    std::vector<hsa_signal_value_t> value_;
    std::vector<hsa_amd_signal_handler> handler_;
    std::vector<void*> arg_;
  };

  // Will be created before any user could call hsa_init but also could be
  // destroyed before incorrectly written programs call hsa_shutdown.
  static KernelMutex bootstrap_lock_;

  Runtime();

  Runtime(const Runtime&);

  Runtime& operator=(const Runtime&);

  ~Runtime() {}

  /// @brief Open connection to kernel driver.
  hsa_status_t Load();

  /// @brief Close connection to kernel driver and cleanup resources.
  void Unload();

  /// @brief Dynamically load extension libraries (images, finalizer) and
  /// call OnLoad method on each loaded library.
  void LoadExtensions();

  /// @brief Call OnUnload method on each extension library then close it.
  void UnloadExtensions();

  /// @brief Dynamically load tool libraries and call OnUnload method on each
  /// loaded library.
  void LoadTools();

  /// @brief Call OnUnload method of each tool library.
  void UnloadTools();

  /// @brief Close tool libraries.
  void CloseTools();

  // @brief Binds virtual memory access fault handler to this node.
  void BindVmFaultHandler();

  // @brief Acquire snapshot of system event handlers.
  // Returns a copy to avoid holding a lock during callbacks.
  std::vector<std::pair<AMD::callback_t<hsa_amd_system_event_callback_t>, void*>>
  GetSystemEventHandlers();

  /// @brief Get the index of ::link_matrix_.
  /// @param [in] node_id_from Node id of the source node.
  /// @param [in] node_id_to Node id of the destination node.
  /// @retval Index in ::link_matrix_.
  uint32_t GetIndexLinkInfo(uint32_t node_id_from, uint32_t node_id_to);

  // Mutex object to protect multithreaded access to ::allocation_map_,
  // KFD map/unmap, register/unregister, and access to hsaKmtQueryPointerInfo
  // registered & mapped arrays.
  KernelMutex memory_lock_;

  // Array containing tools library handles.
  std::vector<os::LibHandle> tool_libs_;

  // Agent list containing all CPU agents in the platform.
  std::vector<Agent*> cpu_agents_;

  // Agent list containing all compatible GPU agents in the platform.
  std::vector<Agent*> gpu_agents_;

  // Agent map containing all agents indexed by their KFD node IDs.
  std::map<uint32_t, std::vector<Agent*> > agents_by_node_;

  // Agent list containing all compatible gpu agent ids in the platform.
  std::vector<uint32_t> gpu_ids_;

  // List of all fine grain system memory region in the platform.
  std::vector<const MemoryRegion*> system_regions_fine_;

  // List of all coarse grain system memory region in the platform.
  std::vector<const MemoryRegion*> system_regions_coarse_;

  // Matrix of IO link.
  std::vector<LinkInfo> link_matrix_;

  // Loader instance.
  amd::hsa::loader::Loader* loader_;

  // Loader context.
  amd::LoaderContext loader_context_;

  // Code object manager.
  amd::hsa::code::AmdHsaCodeManager code_manager_;

  // Contains the region, address, and size of previously allocated memory.
  std::map<const void*, AllocationRegion> allocation_map_;

  // Allocator using ::system_region_
  std::function<void*(size_t, size_t, MemoryRegion::AllocateFlags)>
      system_allocator_;

  // Deallocator using ::system_region_
  std::function<void(void*)> system_deallocator_;

  // Deprecated HSA Region API GPU (for legacy APU support only)
  Agent* region_gpu_;

  AsyncEventsControl async_events_control_;

  AsyncEvents async_events_;

  AsyncEvents new_async_events_;

  // System clock frequency.
  uint64_t sys_clock_freq_;

  // Number of Numa Nodes
  size_t num_nodes_;

  // @brief AMD HSA event to monitor for virtual memory access fault.
  HsaEvent* vm_fault_event_;

  // @brief HSA signal to contain the VM fault event.
  Signal* vm_fault_signal_;

  // Custom system event handlers.
  std::vector<std::pair<AMD::callback_t<hsa_amd_system_event_callback_t>, void*>>
      system_event_handlers_;

  // System event handler lock
  KernelMutex system_event_lock_;

  // Internal queue creation notifier
  AMD::callback_t<hsa_amd_runtime_queue_notifier> internal_queue_create_notifier_;

  void* internal_queue_create_notifier_user_data_;

  // Holds reference count to runtime object.
  std::atomic<uint32_t> ref_count_;

  // Track environment variables.
  Flag flag_;

  // Pools memory for SharedSignal (Signal ABI blocks)
  SharedSignalPool_t SharedSignalPool;

  // Pools KFD Events for InterruptSignal
  InterruptSignal::EventPool EventPool;

  // Frees runtime memory when the runtime library is unloaded if safe to do so.
  // Failure to release the runtime indicates an incorrect application but is
  // common (example: calls library routines at process exit).
  friend class RuntimeCleanup;
};

}  // namespace core
#endif  // header guard
