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

// HSA C to C++ interface implementation.
// This file does argument checking and conversion to C++.
#include <cstring>
#include <set>

#include "core/inc/runtime.h"
#include "core/inc/agent.h"
#include "core/inc/host_queue.h"
#include "core/inc/isa.h"
#include "core/inc/memory_region.h"
#include "core/inc/queue.h"
#include "core/inc/signal.h"
#include "core/inc/default_signal.h"
#include "core/inc/interrupt_signal.h"
#include "core/inc/amd_loader_context.hpp"
#include "inc/hsa_ven_amd_loader.h"

using namespace amd::hsa::code;

template <class T>
struct ValidityError;
template <>
struct ValidityError<core::Signal*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_SIGNAL };
};
template <>
struct ValidityError<core::Agent*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_AGENT };
};
template <>
struct ValidityError<core::MemoryRegion*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_REGION };
};
template <>
struct ValidityError<core::Queue*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_QUEUE };
};
template <>
struct ValidityError<core::Isa*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_ISA };
};
template <class T>
struct ValidityError<const T*> {
  enum { kValue = ValidityError<T*>::kValue };
};

#define IS_BAD_PTR(ptr)                                          \
  do {                                                           \
    if ((ptr) == NULL) return HSA_STATUS_ERROR_INVALID_ARGUMENT; \
  } while (false)
#define IS_BAD_PROFILE(profile)                                  \
  do {                                                           \
    if (profile != HSA_PROFILE_BASE &&                           \
        profile != HSA_PROFILE_FULL) {                           \
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;                  \
    }                                                            \
  } while (false)
#define IS_VALID(ptr)                                            \
  do {                                                           \
    if (((ptr) == NULL) || !((ptr)->IsValid()))                  \
      return hsa_status_t(ValidityError<decltype(ptr)>::kValue); \
  } while (false)
#define CHECK_ALLOC(ptr)                                         \
  do {                                                           \
    if ((ptr) == NULL) return HSA_STATUS_ERROR_OUT_OF_RESOURCES; \
  } while (false)
#define IS_OPEN()                                     \
  do {                                                \
    if (!core::Runtime::runtime_singleton_->IsOpen()) \
      return HSA_STATUS_ERROR_NOT_INITIALIZED;        \
  } while (false)

template <class T>
static __forceinline bool IsValid(T* ptr) {
  return (ptr == NULL) ? NULL : ptr->IsValid();
}

//-----------------------------------------------------------------------------
// Basic Checks
//-----------------------------------------------------------------------------
static_assert(sizeof(hsa_barrier_and_packet_t) ==
                  sizeof(hsa_kernel_dispatch_packet_t),
              "AQL packet definitions have wrong sizes!");
static_assert(sizeof(hsa_barrier_and_packet_t) ==
                  sizeof(hsa_agent_dispatch_packet_t),
              "AQL packet definitions have wrong sizes!");
static_assert(sizeof(hsa_barrier_and_packet_t) == 64,
              "AQL packet definitions have wrong sizes!");
static_assert(sizeof(hsa_barrier_and_packet_t) ==
                  sizeof(hsa_barrier_or_packet_t),
              "AQL packet definitions have wrong sizes!");
#ifdef HSA_LARGE_MODEL
static_assert(sizeof(void*) == 8, "HSA_LARGE_MODEL is set incorrectly!");
#else
static_assert(sizeof(void*) == 4, "HSA_LARGE_MODEL is set incorrectly!");
#endif

namespace HSA {

//---------------------------------------------------------------------------//
//  Init/Shutdown routines
//---------------------------------------------------------------------------//
hsa_status_t hsa_init() {
  if (core::Runtime::runtime_singleton_->Acquire()) return HSA_STATUS_SUCCESS;
  return HSA_STATUS_ERROR_REFCOUNT_OVERFLOW;
}

hsa_status_t hsa_shut_down() {
  IS_OPEN();
  if (core::Runtime::runtime_singleton_->Release()) return HSA_STATUS_SUCCESS;
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}

//---------------------------------------------------------------------------//
//  System
//---------------------------------------------------------------------------//
hsa_status_t 
    hsa_system_get_info(hsa_system_info_t attribute, void* value) {
  IS_OPEN();
  IS_BAD_PTR(value);
  return core::Runtime::runtime_singleton_->GetSystemInfo(attribute, value);
}

hsa_status_t 
    hsa_system_extension_supported(uint16_t extension, uint16_t version_major,
                                   uint16_t version_minor, bool* result) {
  IS_OPEN();

  if (extension >= HSA_EXTENSION_COUNT || result == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  *result = false;

  uint16_t system_version_major = 0;
  hsa_status_t status = core::Runtime::runtime_singleton_->GetSystemInfo(
      HSA_SYSTEM_INFO_VERSION_MAJOR, &system_version_major);
  assert(status == HSA_STATUS_SUCCESS);

  if (version_major <= system_version_major) {
    uint16_t system_version_minor = 0;
    status = core::Runtime::runtime_singleton_->GetSystemInfo(
        HSA_SYSTEM_INFO_VERSION_MINOR, &system_version_minor);
    assert(status == HSA_STATUS_SUCCESS);

    if (version_minor <= system_version_minor) {
      *result = true;
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t 
    hsa_system_get_extension_table(uint16_t extension, uint16_t version_major,
                                   uint16_t version_minor, void* table) {
  if (table == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  IS_OPEN();

  bool supported = false;
  hsa_status_t status = hsa_system_extension_supported(
      extension, version_major, version_minor, &supported);

  if ((HSA_STATUS_SUCCESS != status) ||
      (supported == false)) {
    return status;
  }

  if (extension == HSA_EXTENSION_IMAGES) {
    // Currently there is only version 1.00.
    hsa_ext_images_1_00_pfn_t* ext_table =
        reinterpret_cast<hsa_ext_images_1_00_pfn_t*>(table);
    ext_table->hsa_ext_image_clear = hsa_ext_image_clear;
    ext_table->hsa_ext_image_copy = hsa_ext_image_copy;
    ext_table->hsa_ext_image_create = hsa_ext_image_create;
    ext_table->hsa_ext_image_data_get_info = hsa_ext_image_data_get_info;
    ext_table->hsa_ext_image_destroy = hsa_ext_image_destroy;
    ext_table->hsa_ext_image_export = hsa_ext_image_export;
    ext_table->hsa_ext_image_get_capability = hsa_ext_image_get_capability;
    ext_table->hsa_ext_image_import = hsa_ext_image_import;
    ext_table->hsa_ext_sampler_create = hsa_ext_sampler_create;
    ext_table->hsa_ext_sampler_destroy = hsa_ext_sampler_destroy;

    return HSA_STATUS_SUCCESS;
  }
  
  if (extension == HSA_EXTENSION_FINALIZER) {
    // Currently there is only version 1.00.
    hsa_ext_finalizer_1_00_pfn_s* ext_table =
        reinterpret_cast<hsa_ext_finalizer_1_00_pfn_s*>(table);
    ext_table->hsa_ext_program_add_module = hsa_ext_program_add_module;
    ext_table->hsa_ext_program_create = hsa_ext_program_create;
    ext_table->hsa_ext_program_destroy = hsa_ext_program_destroy;
    ext_table->hsa_ext_program_finalize = hsa_ext_program_finalize;
    ext_table->hsa_ext_program_get_info = hsa_ext_program_get_info;
    ext_table->hsa_ext_program_iterate_modules =
        hsa_ext_program_iterate_modules;

    return HSA_STATUS_SUCCESS;
  }

  if (extension == HSA_EXTENSION_AMD_LOADER) {
    // Currently there is only version 1.00.
    hsa_ven_amd_loader_1_00_pfn_t* ext_table =
      reinterpret_cast<hsa_ven_amd_loader_1_00_pfn_t*>(table);
    ext_table->hsa_ven_amd_loader_query_segment_descriptors =
      hsa_ven_amd_loader_query_segment_descriptors;
    ext_table->hsa_ven_amd_loader_query_host_address =
      hsa_ven_amd_loader_query_host_address;

    return HSA_STATUS_SUCCESS;
  }

  return HSA_STATUS_ERROR;
}

//---------------------------------------------------------------------------//
//  Agent
//---------------------------------------------------------------------------//
hsa_status_t 
    hsa_iterate_agents(hsa_status_t (*callback)(hsa_agent_t agent, void* data),
                       void* data) {
  IS_OPEN();
  IS_BAD_PTR(callback);
  return core::Runtime::runtime_singleton_->IterateAgent(callback, data);
}

hsa_status_t hsa_agent_get_info(hsa_agent_t agent_handle,
                                        hsa_agent_info_t attribute,
                                        void* value) {
  IS_OPEN();
  IS_BAD_PTR(value);
  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);
  return agent->GetInfo(attribute, value);
}

hsa_status_t hsa_agent_get_exception_policies(hsa_agent_t agent_handle,
                                                      hsa_profile_t profile,
                                                      uint16_t* mask) {
  IS_OPEN();
  IS_BAD_PTR(mask);
  IS_BAD_PROFILE(profile);
  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  // TODO: Fix me when exception policies are supported.
  *mask = 0;
  return HSA_STATUS_SUCCESS;
}

hsa_status_t 
    hsa_agent_extension_supported(uint16_t extension, hsa_agent_t agent_handle,
                                  uint16_t version_major,
                                  uint16_t version_minor, bool* result) {
  IS_OPEN();

  if ((result == NULL) || (extension > HSA_EXTENSION_AMD_PROFILER)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  *result = false;

  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  if (agent->device_type() == core::Agent::kAmdGpuDevice) {
    uint16_t agent_version_major = 0;
    hsa_status_t status =
        agent->GetInfo(HSA_AGENT_INFO_VERSION_MAJOR, &agent_version_major);
    assert(status == HSA_STATUS_SUCCESS);

    if (version_major <= agent_version_major) {
      uint16_t agent_version_minor = 0;
      status =
          agent->GetInfo(HSA_AGENT_INFO_VERSION_MINOR, &agent_version_minor);
      assert(status == HSA_STATUS_SUCCESS);

      if (version_minor <= agent_version_minor) {
        *result = true;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

/// @brief Api to create a user mode queue.
///
/// @param agent Hsa Agent which will execute Aql commands
///
/// @param size Size of Queue in terms of Aql packet size
///
/// @param type of Queue Single Writer or Multiple Writer
///
/// @param callback Callback function to register in case Quee
/// encounters an error
///
/// @param service_queue Pointer to a service queue
///
/// @param queue Output parameter updated with a pointer to the
/// queue being created
///
/// @return hsa_status
hsa_status_t hsa_queue_create(
    hsa_agent_t agent_handle, uint32_t size, hsa_queue_type_t type,
    void (*callback)(hsa_status_t status, hsa_queue_t* source, void* data),
    void* data, uint32_t private_segment_size, uint32_t group_segment_size,
    hsa_queue_t** queue) {
  IS_OPEN();

  if ((queue == NULL) || (size == 0) || (!IsPowerOfTwo(size)) ||
      (type < HSA_QUEUE_TYPE_MULTI) || (type > HSA_QUEUE_TYPE_SINGLE)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  hsa_queue_type_t agent_queue_type = HSA_QUEUE_TYPE_MULTI;
  hsa_status_t status =
      agent->GetInfo(HSA_AGENT_INFO_QUEUE_TYPE, &agent_queue_type);
  assert(HSA_STATUS_SUCCESS == status);

  if (agent_queue_type == HSA_QUEUE_TYPE_SINGLE &&
      type != HSA_QUEUE_TYPE_SINGLE) {
    return HSA_STATUS_ERROR_INVALID_QUEUE_CREATION;
  }

  core::Queue* cmd_queue = NULL;
  status = agent->QueueCreate(size, type, callback, data, private_segment_size,
                              group_segment_size, &cmd_queue);
  if (cmd_queue != NULL) {
    *queue = core::Queue::Convert(cmd_queue);
    if (*queue == NULL) {
      delete cmd_queue;
      return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
    }
  } else {
    *queue = NULL;
  }

  return status;
}

hsa_status_t hsa_soft_queue_create(hsa_region_t region, uint32_t size,
                                   hsa_queue_type_t type, uint32_t features,
                                   hsa_signal_t doorbell_signal,
                                   hsa_queue_t** queue) {
  IS_OPEN();

  if ((queue == NULL) || (region.handle == 0) ||
      (doorbell_signal.handle == 0) || (size == 0) || (!IsPowerOfTwo(size)) ||
      (type < HSA_QUEUE_TYPE_MULTI) || (type > HSA_QUEUE_TYPE_SINGLE) ||
      (features == 0)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const core::MemoryRegion* mem_region = core::MemoryRegion::Convert(region);
  IS_VALID(mem_region);

  const core::Signal* signal = core::Signal::Convert(doorbell_signal);
  IS_VALID(signal);

  core::HostQueue* host_queue =
      new core::HostQueue(region, size, type, features, doorbell_signal);

  if (!host_queue->active()) {
    delete host_queue;
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  *queue = core::Queue::Convert(host_queue);

  return HSA_STATUS_SUCCESS;
}

/// @brief Api to destroy a user mode queue
///
/// @param queue Pointer to the queue being destroyed
///
/// @return hsa_status
hsa_status_t hsa_queue_destroy(hsa_queue_t* queue) {
  IS_OPEN();
  IS_BAD_PTR(queue);
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  IS_VALID(cmd_queue);
  delete cmd_queue;
  return HSA_STATUS_SUCCESS;
}

/// @brief Api to inactivate a user mode queue
///
/// @param queue Pointer to the queue being inactivated
///
/// @return hsa_status
hsa_status_t hsa_queue_inactivate(hsa_queue_t* queue) {
  IS_OPEN();
  IS_BAD_PTR(queue);
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  IS_VALID(cmd_queue);
  cmd_queue->Inactivate();
  return HSA_STATUS_SUCCESS;
}

/// @brief Api to read the Read Index of Queue using Acquire semantics
///
/// @param queue Pointer to the queue whose read index is being read
///
/// @return uint64_t Value of Read index
uint64_t hsa_queue_load_read_index_acquire(const hsa_queue_t* queue) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->LoadReadIndexAcquire();
}

/// @brief Api to read the Read Index of Queue using Relaxed semantics
///
/// @param queue Pointer to the queue whose read index is being read
///
/// @return uint64_t Value of Read index
uint64_t hsa_queue_load_read_index_relaxed(const hsa_queue_t* queue) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->LoadReadIndexRelaxed();
}

/// @brief Api to read the Write Index of Queue using Acquire semantics
///
/// @param queue Pointer to the queue whose write index is being read
///
/// @return uint64_t Value of Write index
uint64_t hsa_queue_load_write_index_acquire(const hsa_queue_t* queue) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->LoadWriteIndexAcquire();
}

/// @brief Api to read the Write Index of Queue using Relaxed semantics
///
/// @param queue Pointer to the queue whose write index is being read
///
/// @return uint64_t Value of Write index
uint64_t hsa_queue_load_write_index_relaxed(const hsa_queue_t* queue) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->LoadWriteIndexAcquire();
}

/// @brief Api to store the Read Index of Queue using Relaxed semantics
///
/// @param queue Pointer to the queue whose read index is being updated
///
/// @param value Value of new read index
void hsa_queue_store_read_index_relaxed(const hsa_queue_t* queue,
                                                uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  cmd_queue->StoreReadIndexRelaxed(value);
}

/// @brief Api to store the Read Index of Queue using Release semantics
///
/// @param queue Pointer to the queue whose read index is being updated
///
/// @param value Value of new read index
void hsa_queue_store_read_index_release(const hsa_queue_t* queue,
                                                uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  cmd_queue->StoreReadIndexRelease(value);
}

/// @brief Api to store the Write Index of Queue using Relaxed semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value of new write index
void hsa_queue_store_write_index_relaxed(const hsa_queue_t* queue,
                                                 uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  cmd_queue->StoreWriteIndexRelaxed(value);
}

/// @brief Api to store the Write Index of Queue using Release semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value of new write index
void hsa_queue_store_write_index_release(const hsa_queue_t* queue,
                                                 uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  cmd_queue->StoreWriteIndexRelease(value);
}

/// @brief Api to compare and swap the Write Index of Queue using Acquire and
/// Release semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param expected Current value of write index
///
/// @param value Value of new write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_cas_write_index_acq_rel(const hsa_queue_t* queue,
                                                   uint64_t expected,
                                                   uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->CasWriteIndexAcqRel(expected, value);
}

/// @brief Api to compare and swap the Write Index of Queue using Acquire
/// Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param expected Current value of write index
///
/// @param value Value of new write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_cas_write_index_acquire(const hsa_queue_t* queue,
                                                   uint64_t expected,
                                                   uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->CasWriteIndexAcquire(expected, value);
}

/// @brief Api to compare and swap the Write Index of Queue using Relaxed
/// Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param expected Current value of write index
///
/// @param value Value of new write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_cas_write_index_relaxed(const hsa_queue_t* queue,
                                                   uint64_t expected,
                                                   uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->CasWriteIndexRelaxed(expected, value);
}

/// @brief Api to compare and swap the Write Index of Queue using Release
/// Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param expected Current value of write index
///
/// @param value Value of new write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_cas_write_index_release(const hsa_queue_t* queue,
                                                   uint64_t expected,
                                                   uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->CasWriteIndexRelease(expected, value);
}

/// @brief Api to Add to the Write Index of Queue using Acquire and Release
/// Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value to add to write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_add_write_index_acq_rel(const hsa_queue_t* queue,
                                                   uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->AddWriteIndexAcqRel(value);
}

/// @brief Api to Add to the Write Index of Queue using Acquire Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value to add to write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_add_write_index_acquire(const hsa_queue_t* queue,
                                                   uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->AddWriteIndexAcquire(value);
}

/// @brief Api to Add to the Write Index of Queue using Relaxed Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value to add to write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_add_write_index_relaxed(const hsa_queue_t* queue,
                                                   uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->AddWriteIndexRelaxed(value);
}

/// @brief Api to Add to the Write Index of Queue using Release Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value to add to write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_add_write_index_release(const hsa_queue_t* queue,
                                                   uint64_t value) {
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->AddWriteIndexRelease(value);
}

//-----------------------------------------------------------------------------
// Memory
//-----------------------------------------------------------------------------
hsa_status_t hsa_agent_iterate_regions(
    hsa_agent_t agent_handle,
    hsa_status_t (*callback)(hsa_region_t region, void* data), void* data) {
  IS_OPEN();
  IS_BAD_PTR(callback);
  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);
  return agent->IterateRegion(callback, data);
}

hsa_status_t hsa_region_get_info(hsa_region_t region,
                                         hsa_region_info_t attribute,
                                         void* value) {
  IS_OPEN();
  IS_BAD_PTR(value);

  const core::MemoryRegion* mem_region = core::MemoryRegion::Convert(region);
  IS_VALID(mem_region);

  return mem_region->GetInfo(attribute, value);
}

hsa_status_t hsa_memory_register(void* address, size_t size) {
  IS_OPEN();

  if (size == 0 && address != NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_memory_deregister(void* address, size_t size) {
  IS_OPEN();

  return HSA_STATUS_SUCCESS;
}

hsa_status_t 
    hsa_memory_allocate(hsa_region_t region, size_t size, void** ptr) {
  IS_OPEN();

  if (size == 0 || ptr == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const core::MemoryRegion* mem_region = core::MemoryRegion::Convert(region);
  IS_VALID(mem_region);

  return core::Runtime::runtime_singleton_->AllocateMemory(mem_region, size,
                                                           ptr);
}

hsa_status_t hsa_memory_free(void* ptr) {
  IS_OPEN();

  if (ptr == NULL) {
    return HSA_STATUS_SUCCESS;
  }

  return core::Runtime::runtime_singleton_->FreeMemory(ptr);
}

hsa_status_t hsa_memory_assign_agent(void* ptr,
                                             hsa_agent_t agent_handle,
                                             hsa_access_permission_t access) {
  IS_OPEN();

  if ((ptr == NULL) || (access < HSA_ACCESS_PERMISSION_RO) ||
      (access > HSA_ACCESS_PERMISSION_RW)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_memory_copy(void* dst, const void* src, size_t size) {
  IS_OPEN();

  if (dst == NULL || src == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (size == 0) {
    return HSA_STATUS_SUCCESS;
  }

  return core::Runtime::runtime_singleton_->CopyMemory(dst, src, size);
}

//-----------------------------------------------------------------------------
// Signals
//-----------------------------------------------------------------------------

typedef struct {
  bool operator()(const hsa_agent_t& lhs, const hsa_agent_t& rhs) const {
    return lhs.handle < rhs.handle;
  }
} AgentHandleCompare;

hsa_status_t 
    hsa_signal_create(hsa_signal_value_t initial_value, uint32_t num_consumers,
                      const hsa_agent_t* consumers, hsa_signal_t* hsa_signal) {
  IS_OPEN();
  IS_BAD_PTR(hsa_signal);

  core::Signal* ret;

  bool uses_host = false;

  if (num_consumers > 0) {
    IS_BAD_PTR(consumers);

    // Check for duplicates in consumers.
    std::set<hsa_agent_t, AgentHandleCompare> consumer_set =
        std::set<hsa_agent_t, AgentHandleCompare>(consumers,
                                                  consumers + num_consumers);
    if (consumer_set.size() != num_consumers) {
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }

    for (const core::Agent* cpu_agent :
         core::Runtime::runtime_singleton_->cpu_agents()) {
      uses_host |=
          (consumer_set.find(cpu_agent->public_handle()) != consumer_set.end());
    }
  } else {
    uses_host = true;
  }

  if (core::g_use_interrupt_wait && uses_host) {
    ret = new core::InterruptSignal(initial_value);
  } else {
    ret = new core::DefaultSignal(initial_value);
  }
  CHECK_ALLOC(ret);

  *hsa_signal = core::Signal::Convert(ret);

  if (hsa_signal->handle == 0) {
    delete ret;
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_signal_destroy(hsa_signal_t hsa_signal) {
  IS_OPEN();

  if (hsa_signal.handle == 0) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  core::Signal* signal = core::Signal::Convert(hsa_signal);
  IS_VALID(signal);
  delete signal;
  return HSA_STATUS_SUCCESS;
}

hsa_signal_value_t hsa_signal_load_relaxed(hsa_signal_t hsa_signal) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->LoadRelaxed();
}

hsa_signal_value_t hsa_signal_load_acquire(hsa_signal_t hsa_signal) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->LoadAcquire();
}

void hsa_signal_store_relaxed(hsa_signal_t hsa_signal,
                                      hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->StoreRelaxed(value);
}

void hsa_signal_store_release(hsa_signal_t hsa_signal,
                                      hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->StoreRelease(value);
}

hsa_signal_value_t 
    hsa_signal_wait_relaxed(hsa_signal_t hsa_signal,
                            hsa_signal_condition_t condition,
                            hsa_signal_value_t compare_value,
                            uint64_t timeout_hint,
                            hsa_wait_state_t wait_state_hint) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->WaitRelaxed(condition, compare_value, timeout_hint,
                             wait_state_hint);
}

hsa_signal_value_t 
    hsa_signal_wait_acquire(hsa_signal_t hsa_signal,
                            hsa_signal_condition_t condition,
                            hsa_signal_value_t compare_value,
                            uint64_t timeout_hint,
                            hsa_wait_state_t wait_state_hint) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->WaitAcquire(condition, compare_value, timeout_hint,
                             wait_state_hint);
}

void 
    hsa_signal_and_relaxed(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AndRelaxed(value);
}

void 
    hsa_signal_and_acquire(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AndAcquire(value);
}

void 
    hsa_signal_and_release(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AndRelease(value);
}

void 
    hsa_signal_and_acq_rel(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AndAcqRel(value);
}

void 
    hsa_signal_or_relaxed(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->OrRelaxed(value);
}

void 
    hsa_signal_or_acquire(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->OrAcquire(value);
}

void 
    hsa_signal_or_release(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->OrRelease(value);
}

void 
    hsa_signal_or_acq_rel(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->OrAcqRel(value);
}

void 
    hsa_signal_xor_relaxed(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->XorRelaxed(value);
}

void 
    hsa_signal_xor_acquire(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->XorAcquire(value);
}

void 
    hsa_signal_xor_release(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->XorRelease(value);
}

void 
    hsa_signal_xor_acq_rel(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->XorAcqRel(value);
}

void 
    hsa_signal_add_relaxed(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->AddRelaxed(value);
}

void 
    hsa_signal_add_acquire(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AddAcquire(value);
}

void 
    hsa_signal_add_release(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AddRelease(value);
}

void 
    hsa_signal_add_acq_rel(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AddAcqRel(value);
}

void hsa_signal_subtract_relaxed(hsa_signal_t hsa_signal,
                                         hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->SubRelaxed(value);
}

void hsa_signal_subtract_acquire(hsa_signal_t hsa_signal,
                                         hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->SubAcquire(value);
}

void hsa_signal_subtract_release(hsa_signal_t hsa_signal,
                                         hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->SubRelease(value);
}

void hsa_signal_subtract_acq_rel(hsa_signal_t hsa_signal,
                                         hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->SubAcqRel(value);
}

hsa_signal_value_t 
    hsa_signal_exchange_relaxed(hsa_signal_t hsa_signal,
                                hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->ExchRelaxed(value);
}

hsa_signal_value_t 
    hsa_signal_exchange_acquire(hsa_signal_t hsa_signal,
                                hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->ExchAcquire(value);
}

hsa_signal_value_t 
    hsa_signal_exchange_release(hsa_signal_t hsa_signal,
                                hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->ExchRelease(value);
}

hsa_signal_value_t 
    hsa_signal_exchange_acq_rel(hsa_signal_t hsa_signal,
                                hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->ExchAcqRel(value);
}

hsa_signal_value_t hsa_signal_cas_relaxed(hsa_signal_t hsa_signal,
                                                  hsa_signal_value_t expected,
                                                  hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->CasRelaxed(expected, value);
}

hsa_signal_value_t hsa_signal_cas_acquire(hsa_signal_t hsa_signal,
                                                  hsa_signal_value_t expected,
                                                  hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->CasAcquire(expected, value);
}

hsa_signal_value_t hsa_signal_cas_release(hsa_signal_t hsa_signal,
                                                  hsa_signal_value_t expected,
                                                  hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->CasRelease(expected, value);
}

hsa_signal_value_t hsa_signal_cas_acq_rel(hsa_signal_t hsa_signal,
                                                  hsa_signal_value_t expected,
                                                  hsa_signal_value_t value) {
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->CasAcqRel(expected, value);
}

//-----------------------------------------------------------------------------
// Isa
//-----------------------------------------------------------------------------

hsa_status_t hsa_isa_from_name(const char* name, hsa_isa_t* isa) {
  IS_OPEN();
  IS_BAD_PTR(name);
  IS_BAD_PTR(isa);

  const core::Isa* isa_object = core::IsaRegistry::GetIsa(name);
  if (!isa_object) {
    return HSA_STATUS_ERROR_INVALID_ISA_NAME;
  }

  *isa = core::Isa::Handle(isa_object);
  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_isa_get_info(hsa_isa_t isa, hsa_isa_info_t attribute,
                                      uint32_t index, void* value) {
  IS_OPEN();
  IS_BAD_PTR(value);

  if (index != 0) {
    return HSA_STATUS_ERROR_INVALID_INDEX;
  }

  const core::Isa* isa_object = core::Isa::Object(isa);
  IS_VALID(isa_object);

  return isa_object->GetInfo(attribute, value) ?
    HSA_STATUS_SUCCESS : HSA_STATUS_ERROR_INVALID_ARGUMENT;
}

hsa_status_t hsa_isa_compatible(hsa_isa_t code_object_isa,
                                        hsa_isa_t agent_isa, bool* result) {
  IS_OPEN();
  IS_BAD_PTR(result);

  const core::Isa* code_object_isa_object = core::Isa::Object(code_object_isa);
  IS_VALID(code_object_isa_object);

  const core::Isa* agent_isa_object = core::Isa::Object(agent_isa);
  IS_VALID(agent_isa_object);

  *result = code_object_isa_object->IsCompatible(agent_isa_object);
  return HSA_STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
// Code object.
//-----------------------------------------------------------------------------

namespace {

hsa_status_t IsCodeObjectAllocRegion(hsa_region_t region, void *data)
{
  assert(nullptr != data);
  assert(0 == ((hsa_region_t*)data)->handle);

  hsa_status_t status = HSA_STATUS_SUCCESS;
  bool alloc_allowed;
  if (HSA_STATUS_SUCCESS != (status = HSA::hsa_region_get_info(region, HSA_REGION_INFO_RUNTIME_ALLOC_ALLOWED, &alloc_allowed))) {
    return status;
  }
  if (true == alloc_allowed) {
    ((hsa_region_t*)data)->handle = region.handle;
    return HSA_STATUS_INFO_BREAK;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t FindCodeObjectAllocRegionFromAgent(hsa_agent_t agent, void *data)
{
  assert(nullptr != data);
  assert(0 == ((hsa_region_t*)data)->handle);

  hsa_status_t status = HSA_STATUS_SUCCESS;
  hsa_device_type_t agent_type;
  if (HSA_STATUS_SUCCESS != (status = HSA::hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &agent_type))) {
    return status;
  }
  if (HSA_DEVICE_TYPE_CPU == agent_type) {
    return HSA::hsa_agent_iterate_regions(agent, IsCodeObjectAllocRegion, data);
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t FindCodeObjectAllocRegionFromSystem(void *data)
{
  assert(nullptr != data);

  ((hsa_region_t*)data)->handle = 0;
  return HSA::hsa_iterate_agents(FindCodeObjectAllocRegionFromAgent, data);
}

} // namespace anonymous

hsa_status_t hsa_code_object_serialize(
    hsa_code_object_t code_object,
    hsa_status_t (*alloc_callback)(size_t size, hsa_callback_data_t data,
                                   void** address),
    hsa_callback_data_t callback_data, const char* options,
    void** serialized_code_object, size_t* serialized_code_object_size) {
  IS_OPEN();
  IS_BAD_PTR(alloc_callback);
  IS_BAD_PTR(serialized_code_object);
  IS_BAD_PTR(serialized_code_object_size);

  AmdHsaCode* code = core::Runtime::runtime_singleton_->code_manager()->FromHandle(code_object);
  if (!code) { return HSA_STATUS_ERROR_INVALID_CODE_OBJECT; }
  size_t elfmemsz = code->ElfSize();
  const char* elfmemrd = code->ElfData();

  hsa_status_t hsc = alloc_callback(elfmemsz,
                                    callback_data,
                                    serialized_code_object);
  if (HSA_STATUS_SUCCESS != hsc) {
    return hsc;
  }

  memcpy(*serialized_code_object, elfmemrd, elfmemsz);
  *serialized_code_object_size = elfmemsz;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t 
    hsa_code_object_deserialize(void* serialized_code_object,
                                size_t serialized_code_object_size,
                                const char* options,
                                hsa_code_object_t* code_object) {
  IS_OPEN();
  IS_BAD_PTR(serialized_code_object);
  IS_BAD_PTR(code_object);

  if (!serialized_code_object_size) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  hsa_status_t status = HSA_STATUS_SUCCESS;

  // Find code object allocation region.
  hsa_region_t code_object_alloc_region;
  status = FindCodeObjectAllocRegionFromSystem(&code_object_alloc_region);
  if (HSA_STATUS_SUCCESS != status && HSA_STATUS_INFO_BREAK != status) {
    return status;
  }
  assert(0 != code_object_alloc_region.handle);

  // Allocate code object memory.
  void *code_object_alloc_mem = nullptr;
  status = HSA::hsa_memory_allocate(code_object_alloc_region,
                                    serialized_code_object_size,
                                    &code_object_alloc_mem);
  if (HSA_STATUS_SUCCESS != status) {
    return status;
  }
  assert(nullptr != code_object_alloc_mem);

  // Copy code object into allocated code object memory.
  status = HSA::hsa_memory_copy(code_object_alloc_mem,
                                serialized_code_object,
                                serialized_code_object_size);
  if (HSA_STATUS_SUCCESS != status) {
    return status;
  }
  code_object->handle = (uint64_t) (uintptr_t) code_object_alloc_mem;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_code_object_destroy(hsa_code_object_t code_object) {
  IS_OPEN();

  void *elfmemrd = reinterpret_cast<void*>(code_object.handle);
  if (!elfmemrd) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  if (!core::Runtime::runtime_singleton_->code_manager()->Destroy(code_object)) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  HSA::hsa_memory_free(elfmemrd);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_code_object_get_info(hsa_code_object_t code_object,
                                              hsa_code_object_info_t attribute,
                                              void* value) {
  IS_OPEN();
  IS_BAD_PTR(value);

  AmdHsaCode* code = core::Runtime::runtime_singleton_->code_manager()->FromHandle(code_object);
  if (!code) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }
  switch (attribute) {
  case HSA_CODE_OBJECT_INFO_ISA: {
    // TODO: currently AmdHsaCode::GetInfo return string representation.
    // Fix when compute capability is available in libamdhsacode.
    char isa_name[64];
    hsa_status_t status = code->GetInfo(attribute, &isa_name);
    if (status != HSA_STATUS_SUCCESS) { return status; }
    if (HSA_STATUS_SUCCESS != HSA::hsa_isa_from_name(isa_name, (hsa_isa_t*)value)) {
      return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
    }
    return HSA_STATUS_SUCCESS;
  }
  default:
    return code->GetInfo(attribute, value);
  }
}

hsa_status_t hsa_code_object_get_symbol(hsa_code_object_t code_object,
                                                const char *symbol_name,
                                                hsa_code_symbol_t *symbol) {
  IS_OPEN();
  IS_BAD_PTR(symbol_name);
  IS_BAD_PTR(symbol);

  AmdHsaCode* code = core::Runtime::runtime_singleton_->code_manager()->FromHandle(code_object);
  if (!code) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  // TODO: module_name is NULL until spec is changed, waiting for
  // Mario.
  return code->GetSymbol(NULL, symbol_name, symbol);
}

hsa_status_t hsa_code_symbol_get_info(hsa_code_symbol_t code_symbol,
                                              hsa_code_symbol_info_t attribute,
                                              void* value) {
  IS_OPEN();
  IS_BAD_PTR(value);

  Symbol* sym = Symbol::FromHandle(code_symbol);
  return sym->GetInfo(attribute, value);
}

hsa_status_t hsa_code_object_iterate_symbols(
    hsa_code_object_t code_object,
    hsa_status_t (*callback)(hsa_code_object_t code_object,
                             hsa_code_symbol_t symbol, void* data),
    void* data) {
  IS_OPEN();
  IS_BAD_PTR(callback);

  AmdHsaCode* code = core::Runtime::runtime_singleton_->code_manager()->FromHandle(code_object);
  if (!code) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  return code->IterateSymbols(code_object, callback, data);
}

//-----------------------------------------------------------------------------
// Executable
//-----------------------------------------------------------------------------

hsa_status_t 
    hsa_executable_create(hsa_profile_t profile,
                          hsa_executable_state_t executable_state,
                          const char* options, hsa_executable_t* executable) {
  IS_OPEN();
  IS_BAD_PTR(executable);

  if (HSA_PROFILE_BASE != profile && HSA_PROFILE_FULL != profile) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  if (HSA_EXECUTABLE_STATE_FROZEN != executable_state &&
      HSA_EXECUTABLE_STATE_UNFROZEN != executable_state) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  amd::hsa::loader::Executable *exec = core::Runtime::runtime_singleton_->loader()->CreateExecutable(
    profile, options);
  if (!exec) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  // TODO: why did we make it possible to create frozen executable?
  if (HSA_EXECUTABLE_STATE_FROZEN == executable_state) {
    exec->Freeze(NULL);
  }

  *executable = amd::hsa::loader::Executable::Handle(exec);
  return HSA_STATUS_SUCCESS;
}

hsa_status_t hsa_executable_destroy(hsa_executable_t executable) {
  IS_OPEN();

  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  core::Runtime::runtime_singleton_->loader()->DestroyExecutable(exec);
  return HSA_STATUS_SUCCESS;
}

hsa_status_t 
    hsa_executable_load_code_object(hsa_executable_t executable,
                                    hsa_agent_t agent,
                                    hsa_code_object_t code_object,
                                    const char* options) {
  IS_OPEN();

  amd_loaded_code_object_t loaded_code_object = {0};
  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }
  return exec->LoadCodeObject(agent, code_object, options, &loaded_code_object);
}

hsa_status_t 
    hsa_executable_freeze(hsa_executable_t executable, const char* options) {
  IS_OPEN();

  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->Freeze(options);
}

hsa_status_t hsa_executable_get_info(hsa_executable_t executable,
                                             hsa_executable_info_t attribute,
                                             void* value) {
  IS_OPEN();
  IS_BAD_PTR(value);

  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->GetInfo(attribute, value);
}

hsa_status_t 
    hsa_executable_global_variable_define(hsa_executable_t executable,
                                          const char* variable_name,
                                          void* address) {
  IS_OPEN();
  IS_BAD_PTR(variable_name);
  IS_BAD_PTR(address);

  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->DefineProgramExternalVariable(variable_name, address);
}

hsa_status_t 
    hsa_executable_agent_global_variable_define(hsa_executable_t executable,
                                                hsa_agent_t agent,
                                                const char* variable_name,
                                                void* address) {
  IS_OPEN();
  IS_BAD_PTR(variable_name);
  IS_BAD_PTR(address);

  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->DefineAgentExternalVariable(
    variable_name, agent, HSA_VARIABLE_SEGMENT_GLOBAL, address);
}

hsa_status_t 
    hsa_executable_readonly_variable_define(hsa_executable_t executable,
                                            hsa_agent_t agent,
                                            const char* variable_name,
                                            void* address) {
  IS_OPEN();
  IS_BAD_PTR(variable_name);
  IS_BAD_PTR(address);

  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->DefineAgentExternalVariable(
    variable_name, agent, HSA_VARIABLE_SEGMENT_READONLY, address);
}

hsa_status_t 
    hsa_executable_validate(hsa_executable_t executable, uint32_t* result) {
  IS_OPEN();
  IS_BAD_PTR(result);

  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->Validate(result);
}

hsa_status_t 
    hsa_executable_get_symbol(hsa_executable_t executable,
                              const char* module_name, const char* symbol_name,
                              hsa_agent_t agent, int32_t call_convention,
                              hsa_executable_symbol_t* symbol) {
  IS_OPEN();
  IS_BAD_PTR(symbol_name);
  IS_BAD_PTR(symbol);

  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  amd::hsa::loader::Symbol *sym =
    exec->GetSymbol(module_name == NULL ? "" : module_name, symbol_name, agent, call_convention);
  if (!sym) {
    return HSA_STATUS_ERROR_INVALID_SYMBOL_NAME;
  }
  *symbol = amd::hsa::loader::Symbol::Handle(sym);
  return HSA_STATUS_SUCCESS;
}

hsa_status_t 
    hsa_executable_symbol_get_info(hsa_executable_symbol_t executable_symbol,
                                   hsa_executable_symbol_info_t attribute,
                                   void* value) {
  IS_OPEN();
  IS_BAD_PTR(value);

  amd::hsa::loader::Symbol *sym = amd::hsa::loader::Symbol::Object(executable_symbol);
  if (!sym) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return sym->GetInfo(attribute, value) ?
    HSA_STATUS_SUCCESS : HSA_STATUS_ERROR_INVALID_ARGUMENT;
}

hsa_status_t hsa_executable_iterate_symbols(
    hsa_executable_t executable,
    hsa_status_t (*callback)(hsa_executable_t executable,
                             hsa_executable_symbol_t symbol, void* data),
    void* data) {
  IS_OPEN();
  IS_BAD_PTR(callback);

  amd::hsa::loader::Executable *exec = amd::hsa::loader::Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->IterateSymbols(callback, data);
}

//-----------------------------------------------------------------------------
// Errors
//-----------------------------------------------------------------------------

hsa_status_t 
    hsa_status_string(hsa_status_t status, const char** status_string) {
  IS_OPEN();
  IS_BAD_PTR(status_string);
  const size_t status_u = static_cast<size_t>(status);
  switch (status_u) {
    case HSA_STATUS_SUCCESS:
      *status_string =
          "HSA_STATUS_SUCCESS: The function has been executed successfully.";
      break;
    case HSA_STATUS_INFO_BREAK:
      *status_string =
          "HSA_STATUS_INFO_BREAK: A traversal over a list of "
          "elements has been interrupted by the application before "
          "completing.";
      break;
    case HSA_STATUS_ERROR:
      *status_string = "HSA_STATUS_ERROR: A generic error has occurred.";
      break;
    case HSA_STATUS_ERROR_INVALID_ARGUMENT:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_ARGUMENT: One of the actual "
          "arguments does not meet a precondition stated in the "
          "documentation of the corresponding formal argument.";
      break;
    case HSA_STATUS_ERROR_INVALID_QUEUE_CREATION:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_QUEUE_CREATION: The requested "
          "queue creation is not valid.";
      break;
    case HSA_STATUS_ERROR_INVALID_ALLOCATION:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_ALLOCATION: The requested "
          "allocation is not valid.";
      break;
    case HSA_STATUS_ERROR_INVALID_AGENT:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_AGENT: The agent is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_REGION:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_REGION: The memory region is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_SIGNAL:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_SIGNAL: The signal is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_QUEUE:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_QUEUE: The queue is invalid.";
      break;
    case HSA_STATUS_ERROR_OUT_OF_RESOURCES:
      *status_string =
          "HSA_STATUS_ERROR_OUT_OF_RESOURCES: The runtime failed to "
          "allocate the necessary resources. This error may also "
          "occur when the core runtime library needs to spawn "
          "threads or create internal OS-specific events.";
      break;
    case HSA_STATUS_ERROR_INVALID_PACKET_FORMAT:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_PACKET_FORMAT: The AQL packet "
          "is malformed.";
      break;
    case HSA_STATUS_ERROR_RESOURCE_FREE:
      *status_string =
          "HSA_STATUS_ERROR_RESOURCE_FREE: An error has been "
          "detected while releasing a resource.";
      break;
    case HSA_STATUS_ERROR_NOT_INITIALIZED:
      *status_string =
          "HSA_STATUS_ERROR_NOT_INITIALIZED: An API other than "
          "hsa_init has been invoked while the reference count of "
          "the HSA runtime is zero.";
      break;
    case HSA_STATUS_ERROR_REFCOUNT_OVERFLOW:
      *status_string =
          "HSA_STATUS_ERROR_REFCOUNT_OVERFLOW: The maximum "
          "reference count for the object has been reached.";
      break;
    case HSA_STATUS_ERROR_INCOMPATIBLE_ARGUMENTS:
      *status_string =
          "HSA_STATUS_ERROR_INCOMPATIBLE_ARGUMENTS: The arguments passed to "
          "a functions are not compatible.";
      break;
    case HSA_STATUS_ERROR_INVALID_INDEX:
      *status_string = "The index is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_ISA:
      *status_string = "The instruction set architecture is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_CODE_OBJECT:
      *status_string = "The code object is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_EXECUTABLE:
      *status_string = "The executable is invalid.";
      break;
    case HSA_STATUS_ERROR_FROZEN_EXECUTABLE:
      *status_string = "The executable is frozen.";
      break;
    case HSA_STATUS_ERROR_INVALID_SYMBOL_NAME:
      *status_string = "There is no symbol with the given name.";
      break;
    case HSA_STATUS_ERROR_VARIABLE_ALREADY_DEFINED:
      *status_string = "The variable is already defined.";
      break;
    case HSA_STATUS_ERROR_VARIABLE_UNDEFINED:
      *status_string = "The variable is undefined.";
      break;
    case HSA_EXT_STATUS_ERROR_IMAGE_FORMAT_UNSUPPORTED:
      *status_string =
          "HSA_EXT_STATUS_ERROR_IMAGE_FORMAT_UNSUPPORTED: Image "
          "format is not supported.";
      break;
    case HSA_EXT_STATUS_ERROR_IMAGE_SIZE_UNSUPPORTED:
      *status_string =
          "HSA_EXT_STATUS_ERROR_IMAGE_SIZE_UNSUPPORTED: Image size "
          "is not supported.";
      break;
    case HSA_EXT_STATUS_ERROR_INVALID_PROGRAM:
      *status_string =
          "HSA_EXT_STATUS_ERROR_INVALID_PROGRAM: Invalid program";
      break;
    case HSA_EXT_STATUS_ERROR_INVALID_MODULE:
      *status_string = "HSA_EXT_STATUS_ERROR_INVALID_MODULE: Invalid module";
      break;
    case HSA_EXT_STATUS_ERROR_INCOMPATIBLE_MODULE:
      *status_string =
          "HSA_EXT_STATUS_ERROR_INCOMPATIBLE_MODULE: Incompatible module";
      break;
    case HSA_EXT_STATUS_ERROR_MODULE_ALREADY_INCLUDED:
      *status_string =
          "HSA_EXT_STATUS_ERROR_MODULE_ALREADY_INCLUDED: Module already "
          "included";
      break;
    case HSA_EXT_STATUS_ERROR_SYMBOL_MISMATCH:
      *status_string =
          "HSA_EXT_STATUS_ERROR_SYMBOL_MISMATCH: Symbol mismatch";
      break;
    case HSA_EXT_STATUS_ERROR_FINALIZATION_FAILED:
      *status_string =
          "HSA_EXT_STATUS_ERROR_FINALIZATION_FAILED: Finalization failed";
      break;
    case HSA_EXT_STATUS_ERROR_DIRECTIVE_MISMATCH:
      *status_string =
          "HSA_EXT_STATUS_ERROR_DIRECTIVE_MISMATCH: Directive mismatch";
      break;
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
}

}  // end of namespace HSA
