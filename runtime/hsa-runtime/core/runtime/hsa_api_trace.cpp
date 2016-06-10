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

#include "core/inc/hsa_api_trace_int.h"
#include "core/inc/runtime.h"
#include "core/inc/hsa_ext_amd_impl.h"
#include "core/inc/hsa_table_interface.h"

#include <iostream>

namespace core {

HsaApiTable hsa_api_table_;
HsaApiTable hsa_internal_api_table_;

HsaApiTable::HsaApiTable() {
  Init();
}

// Initialize member fields for Hsa Core and Amd Extension Api's
// Member fields for Finalizer and Image extensions will be
// updated as part of Hsa Runtime initialization.
void HsaApiTable::Init() {

  // Initialize Version of Api Table
  hsa_api.version.major_id = HSA_API_TABLE_MAJOR_VERSION;
  hsa_api.version.minor_id = sizeof(::HsaApiTable);
  hsa_api.version.step_id = HSA_API_TABLE_STEP_VERSION;

  // Update Api table for Core and its major id
  UpdateCore();
  hsa_api.core_ = &core_api;

  // Update Api table for Amd Extensions and its major id
  UpdateAmdExts();
  hsa_api.amd_ext_ = &amd_ext_api;
 
  // Initialize Api tables for Finalizer and Image to NULL
  // Tables for Finalizer and Images are initialized as part
  // of Hsa Runtime initialization, including their major ids
  hsa_api.finalizer_ext_ = NULL;
  hsa_api.image_ext_ = NULL;
}

void HsaApiTable::Reset() {
  Init();
}

void HsaApiTable::CloneExts(void* ext_table, uint32_t table_id) {
  
  assert(ext_table != NULL && "Invalid extension table linked.");

  // Update HSA Extension Finalizer Api table
  if (table_id == HSA_EXT_FINALIZER_API_TABLE_ID) {
    finalizer_api = (*(FinalizerExtTable *)ext_table);
    hsa_api.finalizer_ext_ = &finalizer_api;
    return;
  }

  // Update HSA Extension Image Api table
  if (table_id == HSA_EXT_IMAGE_API_TABLE_ID) {
    image_api = (*(ImageExtTable *)ext_table);
    hsa_api.image_ext_ = &image_api;
    return;
  }
}

void HsaApiTable::LinkExts(void* ext_table, uint32_t table_id) {
  
  assert(ext_table != NULL && "Invalid extension table linked.");

  // Update HSA Extension Finalizer Api table
  if (table_id == HSA_EXT_FINALIZER_API_TABLE_ID) {
    finalizer_api = (*(FinalizerExtTable *)ext_table);
    hsa_api.finalizer_ext_ = (FinalizerExtTable *)ext_table; 
    return;
  }

  // Update HSA Extension Image Api table
  if (table_id == HSA_EXT_IMAGE_API_TABLE_ID) {
    image_api = (*(ImageExtTable *)ext_table);
    hsa_api.image_ext_ = (ImageExtTable *)ext_table; 
    return;
  }
}

// Update Api table for Hsa Core Runtime
void HsaApiTable::UpdateCore() {
  
  // Initialize Version of Api Table
  core_api.version.major_id = HSA_CORE_API_TABLE_MAJOR_VERSION;
  core_api.version.minor_id = sizeof(::CoreApiTable);
  core_api.version.step_id = HSA_CORE_API_TABLE_STEP_VERSION;

  // Initialize function pointers for Hsa Core Runtime Api's
  core_api.hsa_init_fn = HSA::hsa_init;
  core_api.hsa_shut_down_fn = HSA::hsa_shut_down;
  core_api.hsa_system_get_info_fn = HSA::hsa_system_get_info;
  core_api.hsa_system_extension_supported_fn = HSA::hsa_system_extension_supported;
  core_api.hsa_system_get_extension_table_fn = HSA::hsa_system_get_extension_table;
  core_api.hsa_iterate_agents_fn = HSA::hsa_iterate_agents;
  core_api.hsa_agent_get_info_fn = HSA::hsa_agent_get_info;
  core_api.hsa_agent_get_exception_policies_fn =
      HSA::hsa_agent_get_exception_policies;
  core_api.hsa_agent_extension_supported_fn = HSA::hsa_agent_extension_supported;
  core_api.hsa_queue_create_fn = HSA::hsa_queue_create;
  core_api.hsa_soft_queue_create_fn = HSA::hsa_soft_queue_create;
  core_api.hsa_queue_destroy_fn = HSA::hsa_queue_destroy;
  core_api.hsa_queue_inactivate_fn = HSA::hsa_queue_inactivate;
  core_api.hsa_queue_load_read_index_acquire_fn =
      HSA::hsa_queue_load_read_index_acquire;
  core_api.hsa_queue_load_read_index_relaxed_fn =
      HSA::hsa_queue_load_read_index_relaxed;
  core_api.hsa_queue_load_write_index_acquire_fn =
      HSA::hsa_queue_load_write_index_acquire;
  core_api.hsa_queue_load_write_index_relaxed_fn =
      HSA::hsa_queue_load_write_index_relaxed;
  core_api.hsa_queue_store_write_index_relaxed_fn =
      HSA::hsa_queue_store_write_index_relaxed;
  core_api.hsa_queue_store_write_index_release_fn =
      HSA::hsa_queue_store_write_index_release;
  core_api.hsa_queue_cas_write_index_acq_rel_fn =
      HSA::hsa_queue_cas_write_index_acq_rel;
  core_api.hsa_queue_cas_write_index_acquire_fn =
      HSA::hsa_queue_cas_write_index_acquire;
  core_api.hsa_queue_cas_write_index_relaxed_fn =
      HSA::hsa_queue_cas_write_index_relaxed;
  core_api.hsa_queue_cas_write_index_release_fn =
      HSA::hsa_queue_cas_write_index_release;
  core_api.hsa_queue_add_write_index_acq_rel_fn =
      HSA::hsa_queue_add_write_index_acq_rel;
  core_api.hsa_queue_add_write_index_acquire_fn =
      HSA::hsa_queue_add_write_index_acquire;
  core_api.hsa_queue_add_write_index_relaxed_fn =
      HSA::hsa_queue_add_write_index_relaxed;
  core_api.hsa_queue_add_write_index_release_fn =
      HSA::hsa_queue_add_write_index_release;
  core_api.hsa_queue_store_read_index_relaxed_fn =
      HSA::hsa_queue_store_read_index_relaxed;
  core_api.hsa_queue_store_read_index_release_fn =
      HSA::hsa_queue_store_read_index_release;
  core_api.hsa_agent_iterate_regions_fn = HSA::hsa_agent_iterate_regions;
  core_api.hsa_region_get_info_fn = HSA::hsa_region_get_info;
  core_api.hsa_memory_register_fn = HSA::hsa_memory_register;
  core_api.hsa_memory_deregister_fn = HSA::hsa_memory_deregister;
  core_api.hsa_memory_allocate_fn = HSA::hsa_memory_allocate;
  core_api.hsa_memory_free_fn = HSA::hsa_memory_free;
  core_api.hsa_memory_copy_fn = HSA::hsa_memory_copy;
  core_api.hsa_memory_assign_agent_fn = HSA::hsa_memory_assign_agent;
  core_api.hsa_signal_create_fn = HSA::hsa_signal_create;
  core_api.hsa_signal_destroy_fn = HSA::hsa_signal_destroy;
  core_api.hsa_signal_load_relaxed_fn = HSA::hsa_signal_load_relaxed;
  core_api.hsa_signal_load_acquire_fn = HSA::hsa_signal_load_acquire;
  core_api.hsa_signal_store_relaxed_fn = HSA::hsa_signal_store_relaxed;
  core_api.hsa_signal_store_release_fn = HSA::hsa_signal_store_release;
  core_api.hsa_signal_wait_relaxed_fn = HSA::hsa_signal_wait_relaxed;
  core_api.hsa_signal_wait_acquire_fn = HSA::hsa_signal_wait_acquire;
  core_api.hsa_signal_and_relaxed_fn = HSA::hsa_signal_and_relaxed;
  core_api.hsa_signal_and_acquire_fn = HSA::hsa_signal_and_acquire;
  core_api.hsa_signal_and_release_fn = HSA::hsa_signal_and_release;
  core_api.hsa_signal_and_acq_rel_fn = HSA::hsa_signal_and_acq_rel;
  core_api.hsa_signal_or_relaxed_fn = HSA::hsa_signal_or_relaxed;
  core_api.hsa_signal_or_acquire_fn = HSA::hsa_signal_or_acquire;
  core_api.hsa_signal_or_release_fn = HSA::hsa_signal_or_release;
  core_api.hsa_signal_or_acq_rel_fn = HSA::hsa_signal_or_acq_rel;
  core_api.hsa_signal_xor_relaxed_fn = HSA::hsa_signal_xor_relaxed;
  core_api.hsa_signal_xor_acquire_fn = HSA::hsa_signal_xor_acquire;
  core_api.hsa_signal_xor_release_fn = HSA::hsa_signal_xor_release;
  core_api.hsa_signal_xor_acq_rel_fn = HSA::hsa_signal_xor_acq_rel;
  core_api.hsa_signal_exchange_relaxed_fn = HSA::hsa_signal_exchange_relaxed;
  core_api.hsa_signal_exchange_acquire_fn = HSA::hsa_signal_exchange_acquire;
  core_api.hsa_signal_exchange_release_fn = HSA::hsa_signal_exchange_release;
  core_api.hsa_signal_exchange_acq_rel_fn = HSA::hsa_signal_exchange_acq_rel;
  core_api.hsa_signal_add_relaxed_fn = HSA::hsa_signal_add_relaxed;
  core_api.hsa_signal_add_acquire_fn = HSA::hsa_signal_add_acquire;
  core_api.hsa_signal_add_release_fn = HSA::hsa_signal_add_release;
  core_api.hsa_signal_add_acq_rel_fn = HSA::hsa_signal_add_acq_rel;
  core_api.hsa_signal_subtract_relaxed_fn = HSA::hsa_signal_subtract_relaxed;
  core_api.hsa_signal_subtract_acquire_fn = HSA::hsa_signal_subtract_acquire;
  core_api.hsa_signal_subtract_release_fn = HSA::hsa_signal_subtract_release;
  core_api.hsa_signal_subtract_acq_rel_fn = HSA::hsa_signal_subtract_acq_rel;
  core_api.hsa_signal_cas_relaxed_fn = HSA::hsa_signal_cas_relaxed;
  core_api.hsa_signal_cas_acquire_fn = HSA::hsa_signal_cas_acquire;
  core_api.hsa_signal_cas_release_fn = HSA::hsa_signal_cas_release;
  core_api.hsa_signal_cas_acq_rel_fn = HSA::hsa_signal_cas_acq_rel;
  core_api.hsa_isa_from_name_fn = HSA::hsa_isa_from_name;
  core_api.hsa_isa_get_info_fn = HSA::hsa_isa_get_info;
  core_api.hsa_isa_compatible_fn = HSA::hsa_isa_compatible;
  core_api.hsa_code_object_serialize_fn = HSA::hsa_code_object_serialize;
  core_api.hsa_code_object_deserialize_fn = HSA::hsa_code_object_deserialize;
  core_api.hsa_code_object_destroy_fn = HSA::hsa_code_object_destroy;
  core_api.hsa_code_object_get_info_fn = HSA::hsa_code_object_get_info;
  core_api.hsa_code_object_get_symbol_fn = HSA::hsa_code_object_get_symbol;
  core_api.hsa_code_symbol_get_info_fn = HSA::hsa_code_symbol_get_info;
  core_api.hsa_code_object_iterate_symbols_fn =
      HSA::hsa_code_object_iterate_symbols;
  core_api.hsa_executable_create_fn = HSA::hsa_executable_create;
  core_api.hsa_executable_destroy_fn = HSA::hsa_executable_destroy;
  core_api.hsa_executable_load_code_object_fn =
      HSA::hsa_executable_load_code_object;
  core_api.hsa_executable_freeze_fn = HSA::hsa_executable_freeze;
  core_api.hsa_executable_get_info_fn = HSA::hsa_executable_get_info;
  core_api.hsa_executable_global_variable_define_fn =
      HSA::hsa_executable_global_variable_define;
  core_api.hsa_executable_agent_global_variable_define_fn =
      HSA::hsa_executable_agent_global_variable_define;
  core_api.hsa_executable_readonly_variable_define_fn =
      HSA::hsa_executable_readonly_variable_define;
  core_api.hsa_executable_validate_fn = HSA::hsa_executable_validate;
  core_api.hsa_executable_get_symbol_fn = HSA::hsa_executable_get_symbol;
  core_api.hsa_executable_symbol_get_info_fn = HSA::hsa_executable_symbol_get_info;
  core_api.hsa_executable_iterate_symbols_fn = HSA::hsa_executable_iterate_symbols;
  core_api.hsa_status_string_fn = HSA::hsa_status_string;
}

// Update Api table for Amd Extensions.
// @note: Current implementation will initialize the
// member variable hsa_amd_image_create_fn while loading
// Image extension library
void HsaApiTable::UpdateAmdExts() {
  
  // Initialize Version of Api Table
  amd_ext_api.version.major_id = HSA_AMD_EXT_API_TABLE_MAJOR_VERSION;
  amd_ext_api.version.minor_id = sizeof(::AmdExtTable);
  amd_ext_api.version.step_id = HSA_AMD_EXT_API_TABLE_STEP_VERSION;
  
  // Initialize function pointers for Amd Extension Api's
  amd_ext_api.hsa_amd_coherency_get_type_fn = AMD::hsa_amd_coherency_get_type;
  amd_ext_api.hsa_amd_coherency_set_type_fn = AMD::hsa_amd_coherency_set_type;
  amd_ext_api.hsa_amd_profiling_set_profiler_enabled_fn = AMD::hsa_amd_profiling_set_profiler_enabled;
  amd_ext_api.hsa_amd_profiling_async_copy_enable_fn = AMD::hsa_amd_profiling_async_copy_enable;
  amd_ext_api.hsa_amd_profiling_get_dispatch_time_fn = AMD::hsa_amd_profiling_get_dispatch_time;
  amd_ext_api.hsa_amd_profiling_get_async_copy_time_fn = AMD::hsa_amd_profiling_get_async_copy_time;
  amd_ext_api.hsa_amd_profiling_convert_tick_to_system_domain_fn = AMD::hsa_amd_profiling_convert_tick_to_system_domain;
  amd_ext_api.hsa_amd_signal_async_handler_fn = AMD::hsa_amd_signal_async_handler;
  amd_ext_api.hsa_amd_async_function_fn = AMD::hsa_amd_async_function;
  amd_ext_api.hsa_amd_signal_wait_any_fn = AMD::hsa_amd_signal_wait_any;
  amd_ext_api.hsa_amd_queue_cu_set_mask_fn = AMD::hsa_amd_queue_cu_set_mask;
  amd_ext_api.hsa_amd_memory_pool_get_info_fn = AMD::hsa_amd_memory_pool_get_info;
  amd_ext_api.hsa_amd_agent_iterate_memory_pools_fn = AMD::hsa_amd_agent_iterate_memory_pools;
  amd_ext_api.hsa_amd_memory_pool_allocate_fn = AMD::hsa_amd_memory_pool_allocate;
  amd_ext_api.hsa_amd_memory_pool_free_fn = AMD::hsa_amd_memory_pool_free;
  amd_ext_api.hsa_amd_memory_async_copy_fn = AMD::hsa_amd_memory_async_copy;
  amd_ext_api.hsa_amd_agent_memory_pool_get_info_fn = AMD::hsa_amd_agent_memory_pool_get_info;
  amd_ext_api.hsa_amd_agents_allow_access_fn = AMD::hsa_amd_agents_allow_access;
  amd_ext_api.hsa_amd_memory_pool_can_migrate_fn = AMD::hsa_amd_memory_pool_can_migrate;
  amd_ext_api.hsa_amd_memory_migrate_fn = AMD::hsa_amd_memory_migrate;
  amd_ext_api.hsa_amd_memory_lock_fn = AMD::hsa_amd_memory_lock;
  amd_ext_api.hsa_amd_memory_unlock_fn = AMD::hsa_amd_memory_unlock;
  amd_ext_api.hsa_amd_memory_fill_fn = AMD::hsa_amd_memory_fill;
  amd_ext_api.hsa_amd_interop_map_buffer_fn = AMD::hsa_amd_interop_map_buffer;
  amd_ext_api.hsa_amd_interop_unmap_buffer_fn = AMD::hsa_amd_interop_unmap_buffer;
}

class Init {
 public:
  Init() { hsa_table_interface_init(&hsa_api_table_.hsa_api); }
};
static Init LinkAtLoad;
}
