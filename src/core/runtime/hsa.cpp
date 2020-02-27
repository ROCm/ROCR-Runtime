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
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/types.h>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#define __read__  _read
#define __lseek__ _lseek
#else
#include <unistd.h>
#define __read__  read
#define __lseek__ lseek
#endif // _WIN32 || _WIN64

#include "core/inc/runtime.h"
#include "core/inc/agent.h"
#include "core/inc/host_queue.h"
#include "core/inc/isa.h"
#include "core/inc/memory_region.h"
#include "core/inc/queue.h"
#include "core/inc/signal.h"
#include "core/inc/cache.h"
#include "core/inc/amd_loader_context.hpp"
#include "inc/hsa_ven_amd_loader.h"
#include "inc/hsa_ven_amd_aqlprofile.h"
#include "core/inc/hsa_ext_amd_impl.h"

using namespace amd::hsa;

template <class T>
struct ValidityError;
template <> struct ValidityError<core::Signal*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_SIGNAL };
};
template <> struct ValidityError<core::SignalGroup*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_SIGNAL_GROUP };
};
template <> struct ValidityError<core::Agent*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_AGENT };
};
template <> struct ValidityError<core::MemoryRegion*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_REGION };
};
template <> struct ValidityError<core::Queue*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_QUEUE };
};
template <> struct ValidityError<core::Cache*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_CACHE };
};
template <> struct ValidityError<core::Isa*> {
  enum { kValue = HSA_STATUS_ERROR_INVALID_ISA };
};
template <class T> struct ValidityError<const T*> {
  enum { kValue = ValidityError<T*>::kValue };
};

#define IS_BAD_PTR(ptr)                                                        \
  do {                                                                         \
    if ((ptr) == NULL) return HSA_STATUS_ERROR_INVALID_ARGUMENT;               \
  } while (false)
#define IS_BAD_PROFILE(profile)                                                \
  do {                                                                         \
    if (profile != HSA_PROFILE_BASE &&                                         \
        profile != HSA_PROFILE_FULL) {                                         \
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;                                \
    }                                                                          \
  } while (false)
#define IS_BAD_EXECUTABLE_STATE(executable_state)                              \
  do {                                                                         \
    if (executable_state != HSA_EXECUTABLE_STATE_FROZEN &&                     \
        executable_state != HSA_EXECUTABLE_STATE_UNFROZEN) {                   \
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;                                \
    }                                                                          \
  } while (false)
#define IS_BAD_ROUNDING_MODE(rounding_mode)                                    \
  do {                                                                         \
    if (rounding_mode != HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT &&            \
        rounding_mode != HSA_DEFAULT_FLOAT_ROUNDING_MODE_ZERO &&               \
        rounding_mode != HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR) {               \
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;                                \
    }                                                                          \
  } while (false)
#define IS_BAD_FP_TYPE(fp_type)                                                \
  do {                                                                         \
    if (fp_type != HSA_FP_TYPE_16 &&                                           \
        fp_type != HSA_FP_TYPE_32 &&                                           \
        fp_type != HSA_FP_TYPE_64) {                                           \
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;                                \
    }                                                                          \
  } while (false)
#define IS_BAD_FLUSH_MODE(flush_mode)                                          \
  do {                                                                         \
    if (flush_mode != HSA_FLUSH_MODE_FTZ &&                                    \
        flush_mode != HSA_FLUSH_MODE_NON_FTZ) {                                \
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;                                \
    }                                                                          \
  } while (false)
#define IS_VALID(ptr)                                                          \
  do {                                                                         \
    if (((ptr) == NULL) || !((ptr)->IsValid()))                                \
      return hsa_status_t(ValidityError<decltype(ptr)>::kValue);               \
  } while (false)
#define CHECK_ALLOC(ptr)                                                       \
  do {                                                                         \
    if ((ptr) == NULL) return HSA_STATUS_ERROR_OUT_OF_RESOURCES;               \
  } while (false)
#define IS_OPEN()                                                              \
  do {                                                                         \
    if (!core::Runtime::runtime_singleton_->IsOpen())                          \
      return HSA_STATUS_ERROR_NOT_INITIALIZED;                                 \
  } while (false)

template <class T>
static __forceinline bool IsValid(T* ptr) {
  return (ptr == NULL) ? NULL : ptr->IsValid();
}

namespace AMD {
hsa_status_t handleException();

template <class T> static __forceinline T handleExceptionT() {
  handleException();
  abort();
  return T();
}
}

#define TRY try {
#define CATCH } catch(...) { return AMD::handleException(); }
#define CATCHRET(RETURN_TYPE) } catch(...) { return AMD::handleExceptionT<RETURN_TYPE>(); }

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

#if !defined(HSA_LARGE_MODEL) || !defined(__linux__)
// static_assert(false, "Only HSA_LARGE_MODEL (64bit mode) and Linux supported.");
#endif

namespace HSA {

//---------------------------------------------------------------------------//
//  Init/Shutdown routines
//---------------------------------------------------------------------------//
hsa_status_t hsa_init() {
  TRY;
  return core::Runtime::runtime_singleton_->Acquire();
  CATCH;
}

hsa_status_t hsa_shut_down() {
  TRY;
  IS_OPEN();
  return core::Runtime::runtime_singleton_->Release();
  CATCH;
}

//---------------------------------------------------------------------------//
//  System
//---------------------------------------------------------------------------//
hsa_status_t
    hsa_system_get_info(hsa_system_info_t attribute, void* value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);
  return core::Runtime::runtime_singleton_->GetSystemInfo(attribute, value);
  CATCH;
}

hsa_status_t hsa_extension_get_name(uint16_t extension, const char** name) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(name);
  switch (extension) {
    case HSA_EXTENSION_FINALIZER:
      *name = "HSA_EXTENSION_FINALIZER";
      break;
    case HSA_EXTENSION_IMAGES:
      *name = "HSA_EXTENSION_IMAGES";
      break;
    case HSA_EXTENSION_PERFORMANCE_COUNTERS:
      *name = "HSA_EXTENSION_PERFORMANCE_COUNTERS";
      break;
    case HSA_EXTENSION_PROFILING_EVENTS:
      *name = "HSA_EXTENSION_PROFILING_EVENTS";
      break;
    case HSA_EXTENSION_AMD_PROFILER:
      *name = "HSA_EXTENSION_AMD_PROFILER";
      break;
    case HSA_EXTENSION_AMD_LOADER:
      *name = "HSA_EXTENSION_AMD_LOADER";
      break;
    case HSA_EXTENSION_AMD_AQLPROFILE:
      *name = "HSA_EXTENSION_AMD_AQLPROFILE";
      break;
    default:
      *name = "HSA_EXTENSION_INVALID";
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t
    hsa_system_extension_supported(uint16_t extension, uint16_t version_major,
                                   uint16_t version_minor, bool* result) {
  TRY;
  IS_OPEN();

  if ((extension > HSA_EXTENSION_STD_LAST &&
       (extension < HSA_AMD_FIRST_EXTENSION || extension > HSA_AMD_LAST_EXTENSION)) ||
      result == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  *result = false;

  if (extension == HSA_EXTENSION_PERFORMANCE_COUNTERS ||
      extension == HSA_EXTENSION_PROFILING_EVENTS)
    return HSA_STATUS_SUCCESS;

  uint16_t system_version_major = 0;
  hsa_status_t status = core::Runtime::runtime_singleton_->GetSystemInfo(
      HSA_SYSTEM_INFO_VERSION_MAJOR, &system_version_major);
  assert(status == HSA_STATUS_SUCCESS);

  if (version_major <= system_version_major) {
    uint16_t system_version_minor = 0;
    if (version_minor <= system_version_minor) {
      *result = true;
    }
  }

  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_system_major_extension_supported(uint16_t extension, uint16_t version_major,
                                                  uint16_t* version_minor, bool* result) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(version_minor);
  IS_BAD_PTR(result);

  if ((extension == HSA_EXTENSION_IMAGES) && (version_major == 1)) {
    *version_minor = 0;
    *result = true;
    return HSA_STATUS_SUCCESS;
  }

  if ((extension == HSA_EXTENSION_FINALIZER) && (version_major == 1)) {
    *version_minor = 0;
    *result = true;
    return HSA_STATUS_SUCCESS;
  }

  if ((extension == HSA_EXTENSION_AMD_LOADER) && (version_major == 1)) {
    *version_minor = 0;
    *result = true;
    return HSA_STATUS_SUCCESS;
  }

  if ((extension == HSA_EXTENSION_AMD_AQLPROFILE) && (version_major == 1)) {
    *version_minor = 0;
    *result = true;
    return HSA_STATUS_SUCCESS;
  }

  *result = false;
  return HSA_STATUS_SUCCESS;
  CATCH;
}

static size_t get_extension_table_length(uint16_t extension, uint16_t major, uint16_t minor) {
  // Table to convert from major/minor to major/length
  struct sizes_t {
    std::string name;
    size_t size;
  };
  static sizes_t sizes[] = {
      {"hsa_ext_images_1_00_pfn_t", sizeof(hsa_ext_images_1_00_pfn_t)},
      {"hsa_ext_finalizer_1_00_pfn_t", sizeof(hsa_ext_finalizer_1_00_pfn_t)},
      {"hsa_ven_amd_loader_1_00_pfn_t", sizeof(hsa_ven_amd_loader_1_00_pfn_t)},
      {"hsa_ven_amd_loader_1_01_pfn_t", sizeof(hsa_ven_amd_loader_1_01_pfn_t)},
      {"hsa_ven_amd_aqlprofile_1_00_pfn_t", sizeof(hsa_ven_amd_aqlprofile_1_00_pfn_t)}};
  static const size_t num_tables = sizeof(sizes) / sizeof(sizes_t);

  if (minor > 99) return 0;

  std::string name;

  switch (extension) {
    case HSA_EXTENSION_FINALIZER:
      name = "hsa_ext_finalizer_";
      break;
    case HSA_EXTENSION_IMAGES:
      name = "hsa_ext_images_";
      break;
    // case HSA_EXTENSION_PERFORMANCE_COUNTERS:
    //  name = "hsa_ext_perf_counter_";
    //  break;
    // case HSA_EXTENSION_PROFILING_EVENTS:
    //  name = "hsa_ext_profiling_event_";
    //  break;
    // case HSA_EXTENSION_AMD_PROFILER:
    //  name = "hsa_ven_amd_profiler_";
    //  break;
    case HSA_EXTENSION_AMD_LOADER:
      name = "hsa_ven_amd_loader_";
      break;
    case HSA_EXTENSION_AMD_AQLPROFILE:
      name = "hsa_ven_amd_aqlprofile_";
      break;
    default:
      return 0;
  }

  char buff[6];
  sprintf(buff, "%02u", minor);
  name += std::to_string(major) + "_" + buff + "_pfn_t";

  for (size_t i = 0; i < num_tables; i++) {
    if (sizes[i].name == name) return sizes[i].size;
  }
  return 0;
}

hsa_status_t hsa_system_get_extension_table(uint16_t extension, uint16_t version_major,
                                            uint16_t version_minor, void* table) {
  TRY;
  return HSA::hsa_system_get_major_extension_table(
      extension, version_major, get_extension_table_length(extension, version_major, version_minor),
      table);
  CATCH;
}

hsa_status_t hsa_system_get_major_extension_table(uint16_t extension, uint16_t version_major,
                                                  size_t table_length, void* table) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(table);

  if (table_length == 0) return HSA_STATUS_ERROR_INVALID_ARGUMENT;

  if (extension == HSA_EXTENSION_IMAGES) {
    if (version_major !=
        core::Runtime::runtime_singleton_->extensions_.image_api.version.major_id) {
      return HSA_STATUS_ERROR;
    }

    hsa_ext_images_1_pfn_t ext_table;
    ext_table.hsa_ext_image_clear = hsa_ext_image_clear;
    ext_table.hsa_ext_image_copy = hsa_ext_image_copy;
    ext_table.hsa_ext_image_create = hsa_ext_image_create;
    ext_table.hsa_ext_image_data_get_info = hsa_ext_image_data_get_info;
    ext_table.hsa_ext_image_destroy = hsa_ext_image_destroy;
    ext_table.hsa_ext_image_export = hsa_ext_image_export;
    ext_table.hsa_ext_image_get_capability = hsa_ext_image_get_capability;
    ext_table.hsa_ext_image_import = hsa_ext_image_import;
    ext_table.hsa_ext_sampler_create = hsa_ext_sampler_create;
    ext_table.hsa_ext_sampler_destroy = hsa_ext_sampler_destroy;
    ext_table.hsa_ext_image_get_capability_with_layout = hsa_ext_image_get_capability_with_layout;
    ext_table.hsa_ext_image_data_get_info_with_layout = hsa_ext_image_data_get_info_with_layout;
    ext_table.hsa_ext_image_create_with_layout = hsa_ext_image_create_with_layout;

    memcpy(table, &ext_table, Min(sizeof(ext_table), table_length));

    return HSA_STATUS_SUCCESS;
  }

  if (extension == HSA_EXTENSION_FINALIZER) {
    if (version_major !=
        core::Runtime::runtime_singleton_->extensions_.finalizer_api.version.major_id) {
      return HSA_STATUS_ERROR;
    }

    hsa_ext_finalizer_1_00_pfn_t ext_table;
    ext_table.hsa_ext_program_add_module = hsa_ext_program_add_module;
    ext_table.hsa_ext_program_create = hsa_ext_program_create;
    ext_table.hsa_ext_program_destroy = hsa_ext_program_destroy;
    ext_table.hsa_ext_program_finalize = hsa_ext_program_finalize;
    ext_table.hsa_ext_program_get_info = hsa_ext_program_get_info;
    ext_table.hsa_ext_program_iterate_modules = hsa_ext_program_iterate_modules;

    memcpy(table, &ext_table, Min(sizeof(ext_table), table_length));

    return HSA_STATUS_SUCCESS;
  }

  if (extension == HSA_EXTENSION_AMD_LOADER) {
    if (version_major != 1) return HSA_STATUS_ERROR;
    hsa_ven_amd_loader_1_01_pfn_t ext_table;
    ext_table.hsa_ven_amd_loader_query_host_address = hsa_ven_amd_loader_query_host_address;
    ext_table.hsa_ven_amd_loader_query_segment_descriptors =
        hsa_ven_amd_loader_query_segment_descriptors;
    ext_table.hsa_ven_amd_loader_query_executable = hsa_ven_amd_loader_query_executable;
    ext_table.hsa_ven_amd_loader_executable_iterate_loaded_code_objects =
        hsa_ven_amd_loader_executable_iterate_loaded_code_objects;
    ext_table.hsa_ven_amd_loader_loaded_code_object_get_info =
        hsa_ven_amd_loader_loaded_code_object_get_info;

    memcpy(table, &ext_table, Min(sizeof(ext_table), table_length));

    return HSA_STATUS_SUCCESS;
  }

  if (extension == HSA_EXTENSION_AMD_AQLPROFILE) {
    if (version_major != hsa_ven_amd_aqlprofile_VERSION_MAJOR) {
      debug_print("aqlprofile API incompatible ver %d, current ver %d\n",
        version_major, hsa_ven_amd_aqlprofile_VERSION_MAJOR);
      return HSA_STATUS_ERROR;
    }

    os::LibHandle lib = os::LoadLib(kAqlProfileLib);
    if (lib == NULL) {
      debug_print("Loading '%s' failed\n", kAqlProfileLib);
      return HSA_STATUS_ERROR;
    }

    hsa_ven_amd_aqlprofile_pfn_t ext_table;
    ext_table.hsa_ven_amd_aqlprofile_version_major =
      (decltype(::hsa_ven_amd_aqlprofile_version_major)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_version_major");
    ext_table.hsa_ven_amd_aqlprofile_version_minor =
      (decltype(::hsa_ven_amd_aqlprofile_version_minor)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_version_minor");
    ext_table.hsa_ven_amd_aqlprofile_error_string =
      (decltype(::hsa_ven_amd_aqlprofile_error_string)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_error_string");
    ext_table.hsa_ven_amd_aqlprofile_validate_event =
      (decltype(::hsa_ven_amd_aqlprofile_validate_event)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_validate_event");
    ext_table.hsa_ven_amd_aqlprofile_start =
      (decltype(::hsa_ven_amd_aqlprofile_start)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_start");
    ext_table.hsa_ven_amd_aqlprofile_stop =
      (decltype(::hsa_ven_amd_aqlprofile_stop)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_stop");
    ext_table.hsa_ven_amd_aqlprofile_read =
      (decltype(::hsa_ven_amd_aqlprofile_read)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_read");
    ext_table.hsa_ven_amd_aqlprofile_legacy_get_pm4 =
      (decltype(::hsa_ven_amd_aqlprofile_legacy_get_pm4)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_legacy_get_pm4");
    ext_table.hsa_ven_amd_aqlprofile_get_info =
      (decltype(::hsa_ven_amd_aqlprofile_get_info)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_get_info");
    ext_table.hsa_ven_amd_aqlprofile_iterate_data =
      (decltype(::hsa_ven_amd_aqlprofile_iterate_data)*)
        os::GetExportAddress(lib, "hsa_ven_amd_aqlprofile_iterate_data");

    bool version_incompatible = true;
    uint32_t version_curr = 0;
    version_major = HSA_AQLPROFILE_VERSION_MAJOR;
    if (ext_table.hsa_ven_amd_aqlprofile_version_major != NULL) {
      version_curr = ext_table.hsa_ven_amd_aqlprofile_version_major();
      version_incompatible = (version_major != version_curr);
    }
    if (version_incompatible == true) {
      debug_print("Loading '%s' failed, incompatible ver %d, current ver %d\n",
        kAqlProfileLib, version_major, version_curr);
      return HSA_STATUS_ERROR;
    }

    memcpy(table, &ext_table, Min(sizeof(ext_table), table_length));

    return HSA_STATUS_SUCCESS;
  }

  return HSA_STATUS_ERROR;
  CATCH;
}

//---------------------------------------------------------------------------//
//  Agent
//---------------------------------------------------------------------------//
hsa_status_t
    hsa_iterate_agents(hsa_status_t (*callback)(hsa_agent_t agent, void* data),
                       void* data) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(callback);
  return core::Runtime::runtime_singleton_->IterateAgent(callback, data);
  CATCH;
}

hsa_status_t hsa_agent_get_info(hsa_agent_t agent_handle,
                                        hsa_agent_info_t attribute,
                                        void* value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);
  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);
  return agent->GetInfo(attribute, value);
  CATCH;
}

hsa_status_t hsa_agent_get_exception_policies(hsa_agent_t agent_handle,
                                                      hsa_profile_t profile,
                                                      uint16_t* mask) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(mask);
  IS_BAD_PROFILE(profile);
  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  *mask = 0;
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_cache_get_info(hsa_cache_t cache, hsa_cache_info_t attribute, void* value) {
  TRY;
  IS_OPEN();
  core::Cache* Cache = core::Cache::Convert(cache);
  IS_VALID(Cache);
  IS_BAD_PTR(value);
  return Cache->GetInfo(attribute, value);
  CATCH;
}

hsa_status_t hsa_agent_iterate_caches(hsa_agent_t agent_handle,
                                      hsa_status_t (*callback)(hsa_cache_t cache, void* data),
                                      void* data) {
  TRY;
  IS_OPEN();
  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);
  IS_BAD_PTR(callback);
  return agent->IterateCache(callback, data);
  CATCH;
}

hsa_status_t
    hsa_agent_extension_supported(uint16_t extension, hsa_agent_t agent_handle,
                                  uint16_t version_major,
                                  uint16_t version_minor, bool* result) {
  TRY;
  IS_OPEN();

  if ((extension > HSA_EXTENSION_STD_LAST &&
       (extension < HSA_AMD_FIRST_EXTENSION || extension > HSA_AMD_LAST_EXTENSION)) ||
      result == NULL) {
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
      if (version_minor <= agent_version_minor) {
        *result = true;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_agent_major_extension_supported(uint16_t extension, hsa_agent_t agent_handle,
                                                 uint16_t version_major, uint16_t* version_minor,
                                                 bool* result) {
  TRY;
  IS_OPEN();

  if ((extension > HSA_EXTENSION_STD_LAST &&
       (extension < HSA_AMD_FIRST_EXTENSION || extension > HSA_AMD_LAST_EXTENSION)) ||
      result == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  *result = false;

  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  if (agent->device_type() == core::Agent::kAmdGpuDevice) {
    uint16_t agent_version_major = 0;
    hsa_status_t status = agent->GetInfo(HSA_AGENT_INFO_VERSION_MAJOR, &agent_version_major);
    assert(status == HSA_STATUS_SUCCESS);

    if (version_major <= agent_version_major) {
      *version_minor = 0;
      *result = true;
    }
  }

  return HSA_STATUS_SUCCESS;
  CATCH;
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
    hsa_agent_t agent_handle, uint32_t size, hsa_queue_type32_t type,
    void (*callback)(hsa_status_t status, hsa_queue_t* source, void* data),
    void* data, uint32_t private_segment_size, uint32_t group_segment_size,
    hsa_queue_t** queue) {
  TRY;
  IS_OPEN();

  if ((queue == nullptr) || (size == 0) || (!IsPowerOfTwo(size)) ||
      (type > HSA_QUEUE_TYPE_COOPERATIVE)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  hsa_queue_type32_t agent_queue_type = HSA_QUEUE_TYPE_MULTI;
  hsa_status_t status =
      agent->GetInfo(HSA_AGENT_INFO_QUEUE_TYPE, &agent_queue_type);
  assert(HSA_STATUS_SUCCESS == status);

  if (agent_queue_type == HSA_QUEUE_TYPE_SINGLE &&
      ((type & HSA_QUEUE_TYPE_SINGLE) != HSA_QUEUE_TYPE_SINGLE)) {
    return HSA_STATUS_ERROR_INVALID_QUEUE_CREATION;
  }

  if ((type & HSA_QUEUE_TYPE_COOPERATIVE) &&
      ((type & HSA_QUEUE_TYPE_SINGLE) != HSA_QUEUE_TYPE_MULTI)) {
    return HSA_STATUS_ERROR_INVALID_QUEUE_CREATION;
  }

  if (callback == nullptr) callback = core::Queue::DefaultErrorHandler;

  core::Queue* cmd_queue = nullptr;
  status = agent->QueueCreate(size, type, callback, data, private_segment_size,
                              group_segment_size, &cmd_queue);
  if (status != HSA_STATUS_SUCCESS) return status;

  assert(cmd_queue != nullptr && "Queue not returned but status was success.\n");
  *queue = core::Queue::Convert(cmd_queue);
  return status;

  CATCH;
}

hsa_status_t hsa_soft_queue_create(hsa_region_t region, uint32_t size,
                                   hsa_queue_type32_t type, uint32_t features,
                                   hsa_signal_t doorbell_signal,
                                   hsa_queue_t** queue) {
  TRY;
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

  core::HostQueue* host_queue = new core::HostQueue(region, size, type, features, doorbell_signal);

  *queue = core::Queue::Convert(host_queue);

  return HSA_STATUS_SUCCESS;
  CATCH;
}

/// @brief Api to destroy a user mode queue
///
/// @param queue Pointer to the queue being destroyed
///
/// @return hsa_status
hsa_status_t hsa_queue_destroy(hsa_queue_t* queue) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(queue);
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  IS_VALID(cmd_queue);
  cmd_queue->Destroy();
  return HSA_STATUS_SUCCESS;
  CATCH;
}

/// @brief Api to inactivate a user mode queue
///
/// @param queue Pointer to the queue being inactivated
///
/// @return hsa_status
hsa_status_t hsa_queue_inactivate(hsa_queue_t* queue) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(queue);
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  IS_VALID(cmd_queue);
  cmd_queue->Inactivate();
  return HSA_STATUS_SUCCESS;
  CATCH;
}

/// @brief Api to read the Read Index of Queue using Acquire semantics
///
/// @param queue Pointer to the queue whose read index is being read
///
/// @return uint64_t Value of Read index
uint64_t hsa_queue_load_read_index_scacquire(const hsa_queue_t* queue) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->LoadReadIndexAcquire();
  CATCHRET(uint64_t);
}

/// @brief Api to read the Read Index of Queue using Relaxed semantics
///
/// @param queue Pointer to the queue whose read index is being read
///
/// @return uint64_t Value of Read index
uint64_t hsa_queue_load_read_index_relaxed(const hsa_queue_t* queue) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->LoadReadIndexRelaxed();
  CATCHRET(uint64_t);
}

/// @brief Api to read the Write Index of Queue using Acquire semantics
///
/// @param queue Pointer to the queue whose write index is being read
///
/// @return uint64_t Value of Write index
uint64_t hsa_queue_load_write_index_scacquire(const hsa_queue_t* queue) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->LoadWriteIndexAcquire();
  CATCHRET(uint64_t);
}

/// @brief Api to read the Write Index of Queue using Relaxed semantics
///
/// @param queue Pointer to the queue whose write index is being read
///
/// @return uint64_t Value of Write index
uint64_t hsa_queue_load_write_index_relaxed(const hsa_queue_t* queue) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->LoadWriteIndexRelaxed();
  CATCHRET(uint64_t);
}

/// @brief Api to store the Read Index of Queue using Relaxed semantics
///
/// @param queue Pointer to the queue whose read index is being updated
///
/// @param value Value of new read index
void hsa_queue_store_read_index_relaxed(const hsa_queue_t* queue,
                                                uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  cmd_queue->StoreReadIndexRelaxed(value);
  CATCHRET(void);
}

/// @brief Api to store the Read Index of Queue using Release semantics
///
/// @param queue Pointer to the queue whose read index is being updated
///
/// @param value Value of new read index
void hsa_queue_store_read_index_screlease(const hsa_queue_t* queue, uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  cmd_queue->StoreReadIndexRelease(value);
  CATCHRET(void);
}

/// @brief Api to store the Write Index of Queue using Relaxed semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value of new write index
void hsa_queue_store_write_index_relaxed(const hsa_queue_t* queue,
                                                 uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  cmd_queue->StoreWriteIndexRelaxed(value);
  CATCHRET(void);
}

/// @brief Api to store the Write Index of Queue using Release semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value of new write index
void hsa_queue_store_write_index_screlease(const hsa_queue_t* queue, uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  cmd_queue->StoreWriteIndexRelease(value);
  CATCHRET(void);
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
uint64_t hsa_queue_cas_write_index_scacq_screl(const hsa_queue_t* queue, uint64_t expected,
                                               uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->CasWriteIndexAcqRel(expected, value);
  CATCHRET(uint64_t);
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
uint64_t hsa_queue_cas_write_index_scacquire(const hsa_queue_t* queue, uint64_t expected,
                                             uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->CasWriteIndexAcquire(expected, value);
  CATCHRET(uint64_t);
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
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->CasWriteIndexRelaxed(expected, value);
  CATCHRET(uint64_t);
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
uint64_t hsa_queue_cas_write_index_screlease(const hsa_queue_t* queue, uint64_t expected,
                                             uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->CasWriteIndexRelease(expected, value);
  CATCHRET(uint64_t);
}

/// @brief Api to Add to the Write Index of Queue using Acquire and Release
/// Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value to add to write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_add_write_index_scacq_screl(const hsa_queue_t* queue, uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->AddWriteIndexAcqRel(value);
  CATCHRET(uint64_t);
}

/// @brief Api to Add to the Write Index of Queue using Acquire Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value to add to write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_add_write_index_scacquire(const hsa_queue_t* queue, uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->AddWriteIndexAcquire(value);
  CATCHRET(uint64_t);
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
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->AddWriteIndexRelaxed(value);
  CATCHRET(uint64_t);
}

/// @brief Api to Add to the Write Index of Queue using Release Semantics
///
/// @param queue Pointer to the queue whose write index is being updated
///
/// @param value Value to add to write index
///
/// @return uint64_t Value of write index before the update
uint64_t hsa_queue_add_write_index_screlease(const hsa_queue_t* queue, uint64_t value) {
  TRY;
  core::Queue* cmd_queue = core::Queue::Convert(queue);
  assert(IsValid(cmd_queue));
  return cmd_queue->AddWriteIndexRelease(value);
  CATCHRET(uint64_t);
}

//-----------------------------------------------------------------------------
// Memory
//-----------------------------------------------------------------------------
hsa_status_t hsa_agent_iterate_regions(
    hsa_agent_t agent_handle,
    hsa_status_t (*callback)(hsa_region_t region, void* data), void* data) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(callback);
  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);
  return agent->IterateRegion(callback, data);
  CATCH;
}

hsa_status_t hsa_region_get_info(hsa_region_t region,
                                         hsa_region_info_t attribute,
                                         void* value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);

  const core::MemoryRegion* mem_region = core::MemoryRegion::Convert(region);
  IS_VALID(mem_region);

  return mem_region->GetInfo(attribute, value);
  CATCH;
}

hsa_status_t hsa_memory_register(void* address, size_t size) {
  TRY;
  IS_OPEN();

  if (size == 0 && address != NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_memory_deregister(void* address, size_t size) {
  TRY;
  IS_OPEN();

  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t
    hsa_memory_allocate(hsa_region_t region, size_t size, void** ptr) {
  TRY;
  IS_OPEN();

  if (size == 0 || ptr == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const core::MemoryRegion* mem_region = core::MemoryRegion::Convert(region);
  IS_VALID(mem_region);

  return core::Runtime::runtime_singleton_->AllocateMemory(
      mem_region, size, core::MemoryRegion::AllocateNoFlags, ptr);
  CATCH;
}

hsa_status_t hsa_memory_free(void* ptr) {
  TRY;
  IS_OPEN();

  if (ptr == NULL) {
    return HSA_STATUS_SUCCESS;
  }

  return core::Runtime::runtime_singleton_->FreeMemory(ptr);
  CATCH;
}

hsa_status_t hsa_memory_assign_agent(void* ptr,
                                             hsa_agent_t agent_handle,
                                             hsa_access_permission_t access) {
  TRY;
  IS_OPEN();

  if ((ptr == NULL) || (access < HSA_ACCESS_PERMISSION_RO) ||
      (access > HSA_ACCESS_PERMISSION_RW)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const core::Agent* agent = core::Agent::Convert(agent_handle);
  IS_VALID(agent);

  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_memory_copy(void* dst, const void* src, size_t size) {
  TRY;
  IS_OPEN();

  if (dst == NULL || src == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (size == 0) {
    return HSA_STATUS_SUCCESS;
  }

  return core::Runtime::runtime_singleton_->CopyMemory(dst, src, size);
  CATCH;
}

//-----------------------------------------------------------------------------
// Signals
//-----------------------------------------------------------------------------

hsa_status_t
    hsa_signal_create(hsa_signal_value_t initial_value, uint32_t num_consumers,
                      const hsa_agent_t* consumers, hsa_signal_t* hsa_signal) {
  return AMD::hsa_amd_signal_create(initial_value, num_consumers, consumers, 0, hsa_signal);
}

hsa_status_t hsa_signal_destroy(hsa_signal_t hsa_signal) {
  TRY;
  IS_OPEN();
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  signal->DestroySignal();
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_signal_value_t hsa_signal_load_relaxed(hsa_signal_t hsa_signal) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->LoadRelaxed();
  CATCHRET(hsa_signal_value_t);
}

hsa_signal_value_t hsa_signal_load_scacquire(hsa_signal_t hsa_signal) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->LoadAcquire();
  CATCHRET(hsa_signal_value_t);
}

void hsa_signal_store_relaxed(hsa_signal_t hsa_signal,
                                      hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->StoreRelaxed(value);
  CATCHRET(void);
}

void hsa_signal_store_screlease(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->StoreRelease(value);
  CATCHRET(void);
}

hsa_signal_value_t
    hsa_signal_wait_relaxed(hsa_signal_t hsa_signal,
                            hsa_signal_condition_t condition,
                            hsa_signal_value_t compare_value,
                            uint64_t timeout_hint,
                            hsa_wait_state_t wait_state_hint) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->WaitRelaxed(condition, compare_value, timeout_hint,
                             wait_state_hint);
  CATCHRET(hsa_signal_value_t);
}

hsa_signal_value_t hsa_signal_wait_scacquire(hsa_signal_t hsa_signal,
                                             hsa_signal_condition_t condition,
                                             hsa_signal_value_t compare_value,
                                             uint64_t timeout_hint,
                                             hsa_wait_state_t wait_state_hint) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->WaitAcquire(condition, compare_value, timeout_hint,
                             wait_state_hint);
  CATCHRET(hsa_signal_value_t);
}

hsa_status_t hsa_signal_group_create(uint32_t num_signals, const hsa_signal_t* signals,
                                     uint32_t num_consumers, const hsa_agent_t* consumers,
                                     hsa_signal_group_t* signal_group) {
  TRY;
  IS_OPEN();
  if (num_signals == 0) return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  for (uint i = 0; i < num_signals; i++) IS_VALID(core::Signal::Convert(signals[i]));
  for (uint i = 0; i < num_consumers; i++) IS_VALID(core::Agent::Convert(consumers[i]));
  core::SignalGroup* group = new core::SignalGroup(num_signals, signals);
  CHECK_ALLOC(group);
  if (!group->IsValid()) {
    delete group;
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }
  *signal_group = core::SignalGroup::Convert(group);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_signal_group_destroy(hsa_signal_group_t signal_group) {
  TRY;
  IS_OPEN();
  core::SignalGroup* group = core::SignalGroup::Convert(signal_group);
  IS_VALID(group);
  delete group;
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_signal_group_wait_any_relaxed(hsa_signal_group_t signal_group,
                                               const hsa_signal_condition_t* conditions,
                                               const hsa_signal_value_t* compare_values,
                                               hsa_wait_state_t wait_state_hint,
                                               hsa_signal_t* signal, hsa_signal_value_t* value) {
  TRY;
  IS_OPEN();
  const core::SignalGroup* group = core::SignalGroup::Convert(signal_group);
  IS_VALID(group);
  const uint32_t index = AMD::hsa_amd_signal_wait_any(
      group->Count(), const_cast<hsa_signal_t*>(group->List()),
      const_cast<hsa_signal_condition_t*>(conditions),
      const_cast<hsa_signal_value_t*>(compare_values), uint64_t(-1), wait_state_hint, value);
  if (index >= group->Count()) return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  *signal = group->List()[index];
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_signal_group_wait_any_scacquire(hsa_signal_group_t signal_group,
                                                 const hsa_signal_condition_t* conditions,
                                                 const hsa_signal_value_t* compare_values,
                                                 hsa_wait_state_t wait_state_hint,
                                                 hsa_signal_t* signal, hsa_signal_value_t* value) {
  TRY;
  hsa_status_t ret = HSA::hsa_signal_group_wait_any_relaxed(
      signal_group, conditions, compare_values, wait_state_hint, signal, value);
  std::atomic_thread_fence(std::memory_order_acquire);
  return ret;
  CATCH;
}

void hsa_signal_and_relaxed(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AndRelaxed(value);
  CATCHRET(void);
}

void hsa_signal_and_scacquire(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AndAcquire(value);
  CATCHRET(void);
}

void hsa_signal_and_screlease(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AndRelease(value);
  CATCHRET(void);
}

void hsa_signal_and_scacq_screl(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AndAcqRel(value);
  CATCHRET(void);
}

void hsa_signal_or_relaxed(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->OrRelaxed(value);
  CATCHRET(void);
}

void hsa_signal_or_scacquire(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->OrAcquire(value);
  CATCHRET(void);
}

void hsa_signal_or_screlease(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->OrRelease(value);
  CATCHRET(void);
}

void hsa_signal_or_scacq_screl(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->OrAcqRel(value);
  CATCHRET(void);
}

void hsa_signal_xor_relaxed(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->XorRelaxed(value);
  CATCHRET(void);
}

void hsa_signal_xor_scacquire(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->XorAcquire(value);
  CATCHRET(void);
}

void hsa_signal_xor_screlease(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->XorRelease(value);
  CATCHRET(void);
}

void hsa_signal_xor_scacq_screl(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->XorAcqRel(value);
  CATCHRET(void);
}

void hsa_signal_add_relaxed(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AddRelaxed(value);
  CATCHRET(void);
}

void hsa_signal_add_scacquire(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AddAcquire(value);
  CATCHRET(void);
}

void hsa_signal_add_screlease(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AddRelease(value);
  CATCHRET(void);
}

void hsa_signal_add_scacq_screl(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->AddAcqRel(value);
  CATCHRET(void);
}

void hsa_signal_subtract_relaxed(hsa_signal_t hsa_signal,
                                         hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->SubRelaxed(value);
  CATCHRET(void);
}

void hsa_signal_subtract_scacquire(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->SubAcquire(value);
  CATCHRET(void);
}

void hsa_signal_subtract_screlease(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->SubRelease(value);
  CATCHRET(void);
}

void hsa_signal_subtract_scacq_screl(hsa_signal_t hsa_signal, hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  signal->SubAcqRel(value);
  CATCHRET(void);
}

hsa_signal_value_t
    hsa_signal_exchange_relaxed(hsa_signal_t hsa_signal,
                                hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->ExchRelaxed(value);
  CATCHRET(hsa_signal_value_t);
}

hsa_signal_value_t hsa_signal_exchange_scacquire(hsa_signal_t hsa_signal,
                                                 hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->ExchAcquire(value);
  CATCHRET(hsa_signal_value_t);
}

hsa_signal_value_t hsa_signal_exchange_screlease(hsa_signal_t hsa_signal,
                                                 hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->ExchRelease(value);
  CATCHRET(hsa_signal_value_t);
}

hsa_signal_value_t hsa_signal_exchange_scacq_screl(hsa_signal_t hsa_signal,
                                                   hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->ExchAcqRel(value);
  CATCHRET(hsa_signal_value_t);
}

hsa_signal_value_t hsa_signal_cas_relaxed(hsa_signal_t hsa_signal,
                                                  hsa_signal_value_t expected,
                                                  hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->CasRelaxed(expected, value);
  CATCHRET(hsa_signal_value_t);
}

hsa_signal_value_t hsa_signal_cas_scacquire(hsa_signal_t hsa_signal, hsa_signal_value_t expected,
                                            hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->CasAcquire(expected, value);
  CATCHRET(hsa_signal_value_t);
}

hsa_signal_value_t hsa_signal_cas_screlease(hsa_signal_t hsa_signal, hsa_signal_value_t expected,
                                            hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->CasRelease(expected, value);
  CATCHRET(hsa_signal_value_t);
}

hsa_signal_value_t hsa_signal_cas_scacq_screl(hsa_signal_t hsa_signal, hsa_signal_value_t expected,
                                              hsa_signal_value_t value) {
  TRY;
  core::Signal* signal = core::Signal::Convert(hsa_signal);
  assert(IsValid(signal));
  return signal->CasAcqRel(expected, value);
  CATCHRET(hsa_signal_value_t);
}

//===--- Instruction Set Architecture -------------------------------------===//

using core::Isa;
using core::IsaRegistry;
using core::Wavefront;

hsa_status_t hsa_isa_from_name(
    const char *name,
    hsa_isa_t *isa) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(name);
  IS_BAD_PTR(isa);

  const Isa *isa_object = IsaRegistry::GetIsa(name);
  if (!isa_object) {
    return HSA_STATUS_ERROR_INVALID_ISA_NAME;
  }

  *isa = Isa::Handle(isa_object);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_agent_iterate_isas(
    hsa_agent_t agent,
    hsa_status_t (*callback)(hsa_isa_t isa,
                             void *data),
    void *data) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(callback);

  const core::Agent *agent_object = core::Agent::Convert(agent);
  IS_VALID(agent_object);

  const Isa *isa_object = agent_object->isa();
  if (!isa_object) {
    return HSA_STATUS_ERROR_INVALID_AGENT;
  }

  return callback(Isa::Handle(isa_object), data);
  CATCH;
}

/* deprecated */
hsa_status_t hsa_isa_get_info(
    hsa_isa_t isa,
    hsa_isa_info_t attribute,
    uint32_t index,
    void *value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);

  if (index != 0) {
    return HSA_STATUS_ERROR_INVALID_INDEX;
  }

  const Isa *isa_object = Isa::Object(isa);
  IS_VALID(isa_object);

  return isa_object->GetInfo(attribute, value) ?
      HSA_STATUS_SUCCESS : HSA_STATUS_ERROR_INVALID_ARGUMENT;
  CATCH;
}

hsa_status_t hsa_isa_get_info_alt(
    hsa_isa_t isa,
    hsa_isa_info_t attribute,
    void *value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);

  const Isa *isa_object = Isa::Object(isa);
  IS_VALID(isa_object);

  return isa_object->GetInfo(attribute, value) ?
      HSA_STATUS_SUCCESS : HSA_STATUS_ERROR_INVALID_ARGUMENT;
  CATCH;
}

hsa_status_t hsa_isa_get_exception_policies(
    hsa_isa_t isa,
    hsa_profile_t profile,
    uint16_t *mask) {
  TRY;
  IS_OPEN();
  IS_BAD_PROFILE(profile);
  IS_BAD_PTR(mask);

  const Isa *isa_object = Isa::Object(isa);
  IS_VALID(isa_object);

  // FIXME: update when exception policies are supported.
  *mask = 0;
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_isa_get_round_method(
    hsa_isa_t isa,
    hsa_fp_type_t fp_type,
    hsa_flush_mode_t flush_mode,
    hsa_round_method_t *round_method) {
  TRY;
  IS_OPEN();
  IS_BAD_FP_TYPE(fp_type);
  IS_BAD_FLUSH_MODE(flush_mode);
  IS_BAD_PTR(round_method);

  const Isa *isa_object = Isa::Object(isa);
  IS_VALID(isa_object);

  *round_method = isa_object->GetRoundMethod(fp_type, flush_mode);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_wavefront_get_info(
    hsa_wavefront_t wavefront,
    hsa_wavefront_info_t attribute,
    void *value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);

  const Wavefront *wavefront_object = Wavefront::Object(wavefront);
  if (!wavefront_object) {
    return HSA_STATUS_ERROR_INVALID_WAVEFRONT;
  }

  return wavefront_object->GetInfo(attribute, value) ?
      HSA_STATUS_SUCCESS : HSA_STATUS_ERROR_INVALID_ARGUMENT;
  CATCH;
}

hsa_status_t hsa_isa_iterate_wavefronts(
    hsa_isa_t isa,
    hsa_status_t (*callback)(hsa_wavefront_t wavefront,
                             void *data),
    void *data) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(callback);

  const Isa *isa_object = Isa::Object(isa);
  IS_VALID(isa_object);

  const Wavefront *wavefront_object = isa_object->GetWavefront();
  assert(wavefront_object);

  return callback(Wavefront::Handle(wavefront_object), data);
  CATCH;
}

/* deprecated */
hsa_status_t hsa_isa_compatible(
    hsa_isa_t code_object_isa,
    hsa_isa_t agent_isa,
    bool *result) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(result);

  const Isa *code_object_isa_object = Isa::Object(code_object_isa);
  IS_VALID(code_object_isa_object);

  const Isa *agent_isa_object = Isa::Object(agent_isa);
  IS_VALID(agent_isa_object);

  *result = code_object_isa_object->IsCompatible(agent_isa_object);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

//===--- Code Objects (deprecated) ----------------------------------------===//

using code::AmdHsaCode;
using code::AmdHsaCodeManager;

namespace {

hsa_status_t IsCodeObjectAllocRegion(
    hsa_region_t region,
    void *data) {
  assert(data);
  assert(((hsa_region_t*)data)->handle == 0);

  bool runtime_alloc_allowed = false;
  hsa_status_t status = HSA::hsa_region_get_info(
      region, HSA_REGION_INFO_RUNTIME_ALLOC_ALLOWED, &runtime_alloc_allowed);
  if (status != HSA_STATUS_SUCCESS) {
    return status;
  }

  if (runtime_alloc_allowed) {
    ((hsa_region_t*)data)->handle = region.handle;
    return HSA_STATUS_INFO_BREAK;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t FindCodeObjectAllocRegionForAgent(
    hsa_agent_t agent,
    void *data) {
  assert(data);
  assert(((hsa_region_t*)data)->handle == 0);

  hsa_device_type_t device = HSA_DEVICE_TYPE_CPU;
  hsa_status_t status = HSA::hsa_agent_get_info(
      agent, HSA_AGENT_INFO_DEVICE, &device);
  if (status != HSA_STATUS_SUCCESS) {
    return status;
  }

  if (device == HSA_DEVICE_TYPE_CPU) {
    return HSA::hsa_agent_iterate_regions(agent, IsCodeObjectAllocRegion, data);
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t FindCodeObjectAllocRegion(
    void *data) {
  assert(data);
  assert(((hsa_region_t*)data)->handle == 0);

  return HSA::hsa_iterate_agents(FindCodeObjectAllocRegionForAgent, data);
}

AmdHsaCodeManager *GetCodeManager() {
  return core::Runtime::runtime_singleton_->code_manager();
}

} // namespace anonymous

/* deprecated */
hsa_status_t hsa_code_object_serialize(
    hsa_code_object_t code_object,
    hsa_status_t (*alloc_callback)(size_t size,
                                   hsa_callback_data_t data,
                                   void **address),
    hsa_callback_data_t callback_data,
    const char *options,
    void **serialized_code_object,
    size_t *serialized_code_object_size) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(alloc_callback);
  IS_BAD_PTR(serialized_code_object);
  IS_BAD_PTR(serialized_code_object_size);

  AmdHsaCode *code = GetCodeManager()->FromHandle(code_object);
  if (!code) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  hsa_status_t status = alloc_callback(
      code->ElfSize(), callback_data, serialized_code_object);
  if (status != HSA_STATUS_SUCCESS) {
    return status;
  }
  assert(*serialized_code_object);

  memcpy(*serialized_code_object, code->ElfData(), code->ElfSize());
  *serialized_code_object_size = code->ElfSize();

  return HSA_STATUS_SUCCESS;
  CATCH;
}

/* deprecated */
hsa_status_t hsa_code_object_deserialize(
    void *serialized_code_object,
    size_t serialized_code_object_size,
    const char *options,
    hsa_code_object_t *code_object) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(serialized_code_object);
  IS_BAD_PTR(code_object);

  if (serialized_code_object_size == 0) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  hsa_region_t code_object_alloc_region = {0};
  hsa_status_t status = FindCodeObjectAllocRegion(&code_object_alloc_region);
  if (status != HSA_STATUS_SUCCESS && status != HSA_STATUS_INFO_BREAK) {
    return status;
  }
  assert(code_object_alloc_region.handle != 0);

  void *code_object_alloc_data = nullptr;
  status = HSA::hsa_memory_allocate(
      code_object_alloc_region, serialized_code_object_size,
      &code_object_alloc_data);
  if (status != HSA_STATUS_SUCCESS) {
    return status;
  }
  assert(code_object_alloc_data);

  memcpy(
      code_object_alloc_data, serialized_code_object,
      serialized_code_object_size);
  code_object->handle = reinterpret_cast<uint64_t>(code_object_alloc_data);

  return HSA_STATUS_SUCCESS;
  CATCH;
}

/* deprecated */
hsa_status_t hsa_code_object_destroy(
    hsa_code_object_t code_object) {
  TRY;
  IS_OPEN();

  void *code_object_data = reinterpret_cast<void*>(code_object.handle);
  if (!code_object_data) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  if (!GetCodeManager()->Destroy(code_object)) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  HSA::hsa_memory_free(code_object_data);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

static std::string ConvertOldTargetNameToNew(
    const std::string &OldName, bool IsFinalizer, uint32_t EFlags) {
  std::string NewName = "";

  // FIXME #1: Should 9:0:3 be completely (loader, sc, etc.) removed?
  // FIXME #2: What does PAL do with respect to boltzmann/usual fiji/tonga?
  if (OldName == "AMD:AMDGPU:7:0:0")
    NewName = "amdgcn-amd-amdhsa--gfx700";
  else if (OldName == "AMD:AMDGPU:7:0:1")
    NewName = "amdgcn-amd-amdhsa--gfx701";
  else if (OldName == "AMD:AMDGPU:7:0:2")
    NewName = "amdgcn-amd-amdhsa--gfx702";
  else if (OldName == "AMD:AMDGPU:7:0:3")
    NewName = "amdgcn-amd-amdhsa--gfx703";
  else if (OldName == "AMD:AMDGPU:7:0:4")
    NewName = "amdgcn-amd-amdhsa--gfx704";
  else if (OldName == "AMD:AMDGPU:8:0:0")
    NewName = "amdgcn-amd-amdhsa--gfx800";
  else if (OldName == "AMD:AMDGPU:8:0:1")
    NewName = "amdgcn-amd-amdhsa--gfx801";
  else if (OldName == "AMD:AMDGPU:8:0:2")
    NewName = "amdgcn-amd-amdhsa--gfx802";
  else if (OldName == "AMD:AMDGPU:8:0:3")
    NewName = "amdgcn-amd-amdhsa--gfx803";
  else if (OldName == "AMD:AMDGPU:8:0:4")
    NewName = "amdgcn-amd-amdhsa--gfx804";
  else if (OldName == "AMD:AMDGPU:8:1:0")
    NewName = "amdgcn-amd-amdhsa--gfx810";
  else if (OldName == "AMD:AMDGPU:9:0:0")
    NewName = "amdgcn-amd-amdhsa--gfx900";
  else if (OldName == "AMD:AMDGPU:9:0:1")
    NewName = "amdgcn-amd-amdhsa--gfx900";
  else if (OldName == "AMD:AMDGPU:9:0:2")
    NewName = "amdgcn-amd-amdhsa--gfx902";
  else if (OldName == "AMD:AMDGPU:9:0:3")
    NewName = "amdgcn-amd-amdhsa--gfx902";
  else if (OldName == "AMD:AMDGPU:9:0:4")
    NewName = "amdgcn-amd-amdhsa--gfx904";
  else if (OldName == "AMD:AMDGPU:9:0:6")
    NewName = "amdgcn-amd-amdhsa--gfx906";
  else if (OldName == "AMD:AMDGPU:9:0:8")
    NewName = "amdgcn-amd-amdhsa--gfx908";
  else if (OldName == "AMD:AMDGPU:10:1:0")
    NewName = "amdgcn-amd-amdhsa--gfx1010";
  else if (OldName == "AMD:AMDGPU:10:1:1")
    NewName = "amdgcn-amd-amdhsa--gfx1011";
  else if (OldName == "AMD:AMDGPU:10:1:2")
    NewName = "amdgcn-amd-amdhsa--gfx1012";
  else
    assert(false && "Unhandled target");

  if (IsFinalizer && (EFlags & EF_AMDGPU_XNACK)) {
    NewName = NewName + "+xnack";
  } else {
    if (EFlags != 0 && (EFlags & EF_AMDGPU_XNACK_LC)) {
      NewName = NewName + "+xnack";
    } else {
      if (OldName == "AMD:AMDGPU:8:0:1")
        NewName = NewName + "+xnack";
      else if (OldName == "AMD:AMDGPU:8:1:0")
        NewName = NewName + "+xnack";
      else if (OldName == "AMD:AMDGPU:9:0:1")
        NewName = NewName + "+xnack";
      else if (OldName == "AMD:AMDGPU:9:0:2")
        NewName = NewName + "+xnack";
      else if (OldName == "AMD:AMDGPU:9:0:3")
        NewName = NewName + "+xnack";
    }
  }

  return NewName;
}

/* deprecated */
hsa_status_t hsa_code_object_get_info(
    hsa_code_object_t code_object,
    hsa_code_object_info_t attribute,
    void *value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);

  AmdHsaCode *code = GetCodeManager()->FromHandle(code_object);
  if (!code) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  switch (attribute) {
    case HSA_CODE_OBJECT_INFO_ISA: {
      char isa_name[64];
      hsa_status_t status = code->GetInfo(attribute, &isa_name);
      if (status != HSA_STATUS_SUCCESS) {
        return status;
      }

      std::string isa_name_str(isa_name);

      bool IsFinalizer = true;
      uint32_t codeHsailMajor;
      uint32_t codeHsailMinor;
      hsa_profile_t codeProfile;
      hsa_machine_model_t codeMachineModel;
      hsa_default_float_rounding_mode_t codeRoundingMode;
      if (!code->GetNoteHsail(&codeHsailMajor, &codeHsailMinor,
                              &codeProfile, &codeMachineModel,
                              &codeRoundingMode)) {
        // Only finalizer generated the "HSAIL" note.
        IsFinalizer = false;
      }

      std::string new_isa_name_str =
          ConvertOldTargetNameToNew(isa_name_str, IsFinalizer, code->EFlags());

      hsa_isa_t isa_handle = {0};
      status = HSA::hsa_isa_from_name(new_isa_name_str.c_str(), &isa_handle);
      if (status != HSA_STATUS_SUCCESS) {
        return status;
      }

      *((hsa_isa_t*)value) = isa_handle;
      return HSA_STATUS_SUCCESS;
    }
    default: {
      return code->GetInfo(attribute, value);
    }
  }
  CATCH;
}

/* deprecated */
hsa_status_t hsa_code_object_get_symbol(
    hsa_code_object_t code_object,
    const char *symbol_name,
    hsa_code_symbol_t *symbol) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(symbol_name);
  IS_BAD_PTR(symbol);

  AmdHsaCode *code = GetCodeManager()->FromHandle(code_object);
  if (!code) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  return code->GetSymbol(nullptr, symbol_name, symbol);
  CATCH;
}

/* deprecated */
hsa_status_t hsa_code_object_get_symbol_from_name(
    hsa_code_object_t code_object,
    const char *module_name,
    const char *symbol_name,
    hsa_code_symbol_t *symbol) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(symbol_name);
  IS_BAD_PTR(symbol);

  AmdHsaCode *code = GetCodeManager()->FromHandle(code_object);
  if (!code) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  return code->GetSymbol(module_name, symbol_name, symbol);
  CATCH;
}

/* deprecated */
hsa_status_t hsa_code_symbol_get_info(
    hsa_code_symbol_t code_symbol,
    hsa_code_symbol_info_t attribute,
    void *value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);

  code::Symbol *symbol = code::Symbol::FromHandle(code_symbol);
  if (!symbol) {
    return HSA_STATUS_ERROR_INVALID_CODE_SYMBOL;
  }

  return symbol->GetInfo(attribute, value);
  CATCH;
}

/* deprecated */
hsa_status_t hsa_code_object_iterate_symbols(
    hsa_code_object_t code_object,
    hsa_status_t (*callback)(hsa_code_object_t code_object,
                             hsa_code_symbol_t symbol,
                             void *data),
    void *data) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(callback);

  AmdHsaCode *code = GetCodeManager()->FromHandle(code_object);
  if (!code) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT;
  }

  return code->IterateSymbols(code_object, callback, data);
  CATCH;
}

//===--- Executable -------------------------------------------------------===//

using common::Signed;
using loader::Executable;
using loader::Loader;

namespace {

/// @class CodeObjectReaderWrapper.
/// @brief Code Object Reader Wrapper.
struct CodeObjectReaderWrapper final : Signed<0x266E71EDBC718D2C> {
  /// @returns Handle equivalent of @p object.
  static hsa_code_object_reader_t Handle(
      const CodeObjectReaderWrapper *object) {
    hsa_code_object_reader_t handle = {reinterpret_cast<uint64_t>(object)};
    return handle;
  }

  /// @returns Object equivalent of @p handle.
  static CodeObjectReaderWrapper *Object(
      const hsa_code_object_reader_t &handle) {
    CodeObjectReaderWrapper *object = common::ObjectAt<CodeObjectReaderWrapper>(
        handle.handle);
    return object;
  }

  /// @brief Default constructor.
  CodeObjectReaderWrapper(
      const void *_code_object_memory, size_t _code_object_size,
      bool _comes_from_file)
    : code_object_memory(_code_object_memory)
    , code_object_size(_code_object_size)
    , comes_from_file(_comes_from_file) {}

  /// @brief Default destructor.
  ~CodeObjectReaderWrapper() {}

  const void *code_object_memory;
  const size_t code_object_size;
  const bool comes_from_file;
};

Loader *GetLoader() {
  return core::Runtime::runtime_singleton_->loader();
}

} // namespace anonymous

hsa_status_t hsa_code_object_reader_create_from_file(
    hsa_file_t file,
    hsa_code_object_reader_t *code_object_reader) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(code_object_reader);

  off_t file_size = __lseek__(file, 0, SEEK_END);
  if (file_size == (off_t)-1) {
    return HSA_STATUS_ERROR_INVALID_FILE;
  }

  if (__lseek__(file, 0, SEEK_SET) == (off_t)-1) {
    return HSA_STATUS_ERROR_INVALID_FILE;
  }

  unsigned char *code_object_memory = new unsigned char[file_size];
  CHECK_ALLOC(code_object_memory);

  if (__read__(file, code_object_memory, file_size) != file_size) {
    delete [] code_object_memory;
    return HSA_STATUS_ERROR_INVALID_FILE;
  }

  CodeObjectReaderWrapper *wrapper = new (std::nothrow) CodeObjectReaderWrapper(
      code_object_memory, file_size, true);
  if (!wrapper) {
    delete [] code_object_memory;
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  *code_object_reader = CodeObjectReaderWrapper::Handle(wrapper);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_code_object_reader_create_from_memory(
    const void *code_object,
    size_t size,
    hsa_code_object_reader_t *code_object_reader) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(code_object);
  IS_BAD_PTR(code_object_reader);

  if (size == 0) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  CodeObjectReaderWrapper *wrapper = new (std::nothrow) CodeObjectReaderWrapper(
      code_object, size, false);
  CHECK_ALLOC(wrapper);

  *code_object_reader = CodeObjectReaderWrapper::Handle(wrapper);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_code_object_reader_destroy(
    hsa_code_object_reader_t code_object_reader) {
  TRY;
  IS_OPEN();

  CodeObjectReaderWrapper *wrapper = CodeObjectReaderWrapper::Object(
      code_object_reader);
  if (!wrapper) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT_READER;
  }

  if (wrapper->comes_from_file) {
    delete [] (unsigned char*)wrapper->code_object_memory;
  }
  delete wrapper;

  return HSA_STATUS_SUCCESS;
  CATCH;
}

/* deprecated */
hsa_status_t hsa_executable_create(
    hsa_profile_t profile,
    hsa_executable_state_t executable_state,
    const char *options,
    hsa_executable_t *executable) {
  TRY;
  IS_OPEN();
  IS_BAD_PROFILE(profile);
  IS_BAD_EXECUTABLE_STATE(executable_state);
  IS_BAD_PTR(executable);

  // Invoke non-deprecated API.
  hsa_status_t status = HSA::hsa_executable_create_alt(
      profile, HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT, options, executable);
  if (status != HSA_STATUS_SUCCESS) {
    return status;
  }

  Executable *exec = Executable::Object(*executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  if (executable_state == HSA_EXECUTABLE_STATE_FROZEN) {
    exec->Freeze(nullptr);
  }

  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_executable_create_alt(
    hsa_profile_t profile,
    hsa_default_float_rounding_mode_t default_float_rounding_mode,
    const char *options,
    hsa_executable_t *executable) {
  TRY;
  IS_OPEN();
  IS_BAD_PROFILE(profile);
  IS_BAD_ROUNDING_MODE(default_float_rounding_mode); // NOTES: should we check
                                                     // if default float
                                                     // rounding mode is valid?
                                                     // spec does not say so.
  IS_BAD_PTR(executable);

  Executable *exec = GetLoader()->CreateExecutable(
      profile, options, default_float_rounding_mode);
  CHECK_ALLOC(exec);

  *executable = Executable::Handle(exec);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_executable_destroy(
    hsa_executable_t executable) {
  TRY;
  IS_OPEN();

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  GetLoader()->DestroyExecutable(exec);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

/* deprecated */
hsa_status_t hsa_executable_load_code_object(
    hsa_executable_t executable,
    hsa_agent_t agent,
    hsa_code_object_t code_object,
    const char *options) {
  TRY;
  IS_OPEN();

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->LoadCodeObject(agent, code_object, options);
  CATCH;
}

hsa_status_t hsa_executable_load_program_code_object(
    hsa_executable_t executable,
    hsa_code_object_reader_t code_object_reader,
    const char *options,
    hsa_loaded_code_object_t *loaded_code_object) {
  TRY;
  IS_OPEN();

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  CodeObjectReaderWrapper *wrapper = CodeObjectReaderWrapper::Object(
      code_object_reader);
  if (!wrapper) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT_READER;
  }

  hsa_code_object_t code_object =
      {reinterpret_cast<uint64_t>(wrapper->code_object_memory)};
  return exec->LoadCodeObject(
      {0}, code_object, options, loaded_code_object);
  CATCH;
}

hsa_status_t hsa_executable_load_agent_code_object(
    hsa_executable_t executable,
    hsa_agent_t agent,
    hsa_code_object_reader_t code_object_reader,
    const char *options,
    hsa_loaded_code_object_t *loaded_code_object) {
  TRY;
  IS_OPEN();

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  CodeObjectReaderWrapper *wrapper = CodeObjectReaderWrapper::Object(
      code_object_reader);
  if (!wrapper) {
    return HSA_STATUS_ERROR_INVALID_CODE_OBJECT_READER;
  }

  hsa_code_object_t code_object =
      {reinterpret_cast<uint64_t>(wrapper->code_object_memory)};
  return exec->LoadCodeObject(
      agent, code_object, options, loaded_code_object);
  CATCH;
}

hsa_status_t hsa_executable_freeze(
    hsa_executable_t executable,
    const char *options) {
  TRY;
  IS_OPEN();

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return GetLoader()->FreezeExecutable(exec, options);
  CATCH;
}

hsa_status_t hsa_executable_get_info(
    hsa_executable_t executable,
    hsa_executable_info_t attribute,
    void *value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->GetInfo(attribute, value);
  CATCH;
}

hsa_status_t hsa_executable_global_variable_define(
    hsa_executable_t executable,
    const char *variable_name,
    void *address) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(variable_name);

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->DefineProgramExternalVariable(variable_name, address);
  CATCH;
}

hsa_status_t hsa_executable_agent_global_variable_define(
    hsa_executable_t executable,
    hsa_agent_t agent,
    const char *variable_name,
    void *address) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(variable_name);

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->DefineAgentExternalVariable(
      variable_name, agent, HSA_VARIABLE_SEGMENT_GLOBAL, address);
  CATCH;
}

hsa_status_t hsa_executable_readonly_variable_define(
    hsa_executable_t executable,
    hsa_agent_t agent,
    const char *variable_name,
    void *address) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(variable_name);

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->DefineAgentExternalVariable(
      variable_name, agent, HSA_VARIABLE_SEGMENT_READONLY, address);
  CATCH;
}

hsa_status_t hsa_executable_validate(
    hsa_executable_t executable,
    uint32_t *result) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(result);

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->Validate(result);
  CATCH;
}

hsa_status_t hsa_executable_validate_alt(
    hsa_executable_t executable,
    const char *options,
    uint32_t *result) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(result);

  return HSA::hsa_executable_validate(executable, result);
  CATCH;
}

/* deprecated */
hsa_status_t hsa_executable_get_symbol(
    hsa_executable_t executable,
    const char *module_name,
    const char *symbol_name,
    hsa_agent_t agent,
    int32_t call_convention,
    hsa_executable_symbol_t *symbol) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(symbol_name);
  IS_BAD_PTR(symbol);

  std::string mangled_name(symbol_name);
  if (mangled_name.empty()) {
    return HSA_STATUS_ERROR_INVALID_SYMBOL_NAME;
  }
  if (module_name && !std::string(module_name).empty()) {
    mangled_name.insert(0, "::");
    mangled_name.insert(0, std::string(module_name));
  }

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  // Invoke non-deprecated API.
  return HSA::hsa_executable_get_symbol_by_name(
      executable, mangled_name.c_str(),
      exec->IsProgramSymbol(mangled_name.c_str()) ? nullptr : &agent, symbol);
  CATCH;
}

hsa_status_t hsa_executable_get_symbol_by_name(
    hsa_executable_t executable,
    const char *symbol_name,
    const hsa_agent_t *agent, // NOTES: this is not consistent with the rest of
                              // of the specification, but seems like a better
                              // approach to distinguish program/agent symbols.
    hsa_executable_symbol_t *symbol) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(symbol_name);
  IS_BAD_PTR(symbol);

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  loader::Symbol *sym = exec->GetSymbol(symbol_name, agent);
  if (!sym) {
    return HSA_STATUS_ERROR_INVALID_SYMBOL_NAME;
  }

  *symbol = loader::Symbol::Handle(sym);
  return HSA_STATUS_SUCCESS;
  CATCH;
}

hsa_status_t hsa_executable_symbol_get_info(
    hsa_executable_symbol_t executable_symbol,
    hsa_executable_symbol_info_t attribute,
    void *value) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(value);

  loader::Symbol *sym = loader::Symbol::Object(executable_symbol);
  if (!sym) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE_SYMBOL;
  }

  return sym->GetInfo(attribute, value) ?
    HSA_STATUS_SUCCESS : HSA_STATUS_ERROR_INVALID_ARGUMENT;
  CATCH;
}

/* deprecated */
hsa_status_t hsa_executable_iterate_symbols(
    hsa_executable_t executable,
    hsa_status_t (*callback)(hsa_executable_t executable,
                             hsa_executable_symbol_t symbol,
                             void *data),
    void *data) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(callback);

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->IterateSymbols(callback, data);
  CATCH;
}

hsa_status_t hsa_executable_iterate_agent_symbols(
    hsa_executable_t executable,
    hsa_agent_t agent,
    hsa_status_t (*callback)(hsa_executable_t exec,
                             hsa_agent_t agent,
                             hsa_executable_symbol_t symbol,
                             void *data),
    void *data) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(callback);

  // NOTES: should we check if agent is valid? spec does not say so.
  const core::Agent *agent_object = core::Agent::Convert(agent);
  IS_VALID(agent_object);

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->IterateAgentSymbols(agent, callback, data);
  CATCH;
}

hsa_status_t hsa_executable_iterate_program_symbols(
    hsa_executable_t executable,
    hsa_status_t (*callback)(hsa_executable_t exec,
                             hsa_executable_symbol_t symbol,
                             void *data),
    void *data) {
  TRY;
  IS_OPEN();
  IS_BAD_PTR(callback);

  Executable *exec = Executable::Object(executable);
  if (!exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }

  return exec->IterateProgramSymbols(callback, data);
  CATCH;
}

//===--- Runtime Notifications --------------------------------------------===//

hsa_status_t hsa_status_string(
    hsa_status_t status,
    const char **status_string) {
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
      *status_string = "HSA_STATUS_ERROR_INVALID_INDEX: The index is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_ISA:
      *status_string = "HSA_STATUS_ERROR_INVALID_ISA: The instruction set architecture is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_ISA_NAME:
      *status_string = "HSA_STATUS_ERROR_INVALID_ISA_NAME: The instruction set architecture name is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_CODE_OBJECT:
      *status_string = "HSA_STATUS_ERROR_INVALID_CODE_OBJECT: The code object is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_EXECUTABLE:
      *status_string = "HSA_STATUS_ERROR_INVALID_EXECUTABLE: The executable is invalid.";
      break;
    case HSA_STATUS_ERROR_FROZEN_EXECUTABLE:
      *status_string = "HSA_STATUS_ERROR_FROZEN_EXECUTABLE: The executable is frozen.";
      break;
    case HSA_STATUS_ERROR_INVALID_SYMBOL_NAME:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_SYMBOL_NAME: There is no symbol with the given name.";
      break;
    case HSA_STATUS_ERROR_VARIABLE_ALREADY_DEFINED:
      *status_string =
          "HSA_STATUS_ERROR_VARIABLE_ALREADY_DEFINED: The variable is already defined.";
      break;
    case HSA_STATUS_ERROR_VARIABLE_UNDEFINED:
      *status_string = "HSA_STATUS_ERROR_VARIABLE_UNDEFINED: The variable is undefined.";
      break;
    case HSA_STATUS_ERROR_EXCEPTION:
      *status_string =
          "HSA_STATUS_ERROR_EXCEPTION: An HSAIL operation resulted in a hardware exception.";
      break;
    case HSA_STATUS_ERROR_INVALID_CODE_SYMBOL:
      *status_string = "HSA_STATUS_ERROR_INVALID_CODE_SYMBOL:  The code object symbol is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_EXECUTABLE_SYMBOL:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_EXECUTABLE_SYMBOL:  The executable symbol is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_FILE:
      *status_string = "HSA_STATUS_ERROR_INVALID_FILE:  The file descriptor is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_CODE_OBJECT_READER:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_CODE_OBJECT_READER:  The code object reader is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_CACHE:
      *status_string = "HSA_STATUS_ERROR_INVALID_CACHE:  The cache is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_WAVEFRONT:
      *status_string = "HSA_STATUS_ERROR_INVALID_WAVEFRONT:  The wavefront is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_SIGNAL_GROUP:
      *status_string = "HSA_STATUS_ERROR_INVALID_SIGNAL_GROUP:  The signal group is invalid.";
      break;
    case HSA_STATUS_ERROR_INVALID_RUNTIME_STATE:
      *status_string =
          "HSA_STATUS_ERROR_INVALID_RUNTIME_STATE:  The HSA runtime is not in the configuration "
          "state.";
      break;
    case HSA_STATUS_ERROR_FATAL:
      *status_string =
          "HSA_STATUS_ERROR_FATAL:  The queue received an error that may require process "
          "termination.";
      break;
    case HSA_STATUS_ERROR_MEMORY_APERTURE_VIOLATION:
      *status_string =
          "HSA_STATUS_ERROR_MEMORY_APERTURE_VIOLATION:  The agent attempted to access "
          "memory beyond the largest legal address.";
      break;
    case HSA_STATUS_ERROR_ILLEGAL_INSTRUCTION:
      *status_string =
          "HSA_STATUS_ERROR_ILLEGAL_INSTRUCTION:  The agent attempted to execute an "
          "illegal shader instruction.";
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
    case HSA_EXT_STATUS_ERROR_IMAGE_PITCH_UNSUPPORTED:
      *status_string = "Image pitch is not supported or invalid.";
      break;
    case HSA_EXT_STATUS_ERROR_SAMPLER_DESCRIPTOR_UNSUPPORTED:
      *status_string = "Sampler descriptor is not supported or invalid.";
      break;
    case HSA_EXT_STATUS_ERROR_INVALID_PROGRAM:
      *status_string = "HSA_EXT_STATUS_ERROR_INVALID_PROGRAM: Invalid program";
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
