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

#include "hsa_api_trace.h"

static const ApiTable* HsaApiTable;

void hsa_table_interface_init(const ApiTable* Table) { HsaApiTable = Table; }

const ApiTable* hsa_table_interface_get_table() { return HsaApiTable; }

// Pass through stub functions
hsa_status_t HSA_API hsa_init() { return HsaApiTable->hsa_init_fn(); }

hsa_status_t HSA_API hsa_shut_down() { return HsaApiTable->hsa_shut_down_fn(); }

hsa_status_t HSA_API
    hsa_system_get_info(hsa_system_info_t attribute, void* value) {
  return HsaApiTable->hsa_system_get_info_fn(attribute, value);
}

hsa_status_t HSA_API
    hsa_system_extension_supported(uint16_t extension, uint16_t version_major,
                                   uint16_t version_minor, bool* result) {
  return HsaApiTable->hsa_system_extension_supported_fn(
      extension, version_major, version_minor, result);
}

hsa_status_t HSA_API
    hsa_system_get_extension_table(uint16_t extension, uint16_t version_major,
                                   uint16_t version_minor, void* table) {
  return HsaApiTable->hsa_system_get_extension_table_fn(
      extension, version_major, version_minor, table);
}

hsa_status_t HSA_API
    hsa_iterate_agents(hsa_status_t (*callback)(hsa_agent_t agent, void* data),
                       void* data) {
  return HsaApiTable->hsa_iterate_agents_fn(callback, data);
}

hsa_status_t HSA_API hsa_agent_get_info(hsa_agent_t agent,
                                        hsa_agent_info_t attribute,
                                        void* value) {
  return HsaApiTable->hsa_agent_get_info_fn(agent, attribute, value);
}

hsa_status_t HSA_API hsa_agent_get_exception_policies(hsa_agent_t agent,
                                                      hsa_profile_t profile,
                                                      uint16_t* mask) {
  return HsaApiTable->hsa_agent_get_exception_policies_fn(agent, profile, mask);
}

hsa_status_t HSA_API
    hsa_agent_extension_supported(uint16_t extension, hsa_agent_t agent,
                                  uint16_t version_major,
                                  uint16_t version_minor, bool* result) {
  return HsaApiTable->hsa_agent_extension_supported_fn(
      extension, agent, version_major, version_minor, result);
}

hsa_status_t HSA_API
    hsa_queue_create(hsa_agent_t agent, uint32_t size, hsa_queue_type_t type,
                     void (*callback)(hsa_status_t status, hsa_queue_t* source,
                                      void* data),
                     void* data, uint32_t private_segment_size,
                     uint32_t group_segment_size, hsa_queue_t** queue) {
  return HsaApiTable->hsa_queue_create_fn(agent, size, type, callback, data,
                                          private_segment_size,
                                          group_segment_size, queue);
}

hsa_status_t HSA_API
    hsa_soft_queue_create(hsa_region_t region, uint32_t size,
                          hsa_queue_type_t type, uint32_t features,
                          hsa_signal_t completion_signal, hsa_queue_t** queue) {
  return HsaApiTable->hsa_soft_queue_create_fn(region, size, type, features,
                                               completion_signal, queue);
}

hsa_status_t HSA_API hsa_queue_destroy(hsa_queue_t* queue) {
  return HsaApiTable->hsa_queue_destroy_fn(queue);
}

hsa_status_t HSA_API hsa_queue_inactivate(hsa_queue_t* queue) {
  return HsaApiTable->hsa_queue_inactivate_fn(queue);
}

uint64_t HSA_API hsa_queue_load_read_index_acquire(const hsa_queue_t* queue) {
  return HsaApiTable->hsa_queue_load_read_index_acquire_fn(queue);
}

uint64_t HSA_API hsa_queue_load_read_index_relaxed(const hsa_queue_t* queue) {
  return HsaApiTable->hsa_queue_load_read_index_relaxed_fn(queue);
}

uint64_t HSA_API hsa_queue_load_write_index_acquire(const hsa_queue_t* queue) {
  return HsaApiTable->hsa_queue_load_write_index_acquire_fn(queue);
}

uint64_t HSA_API hsa_queue_load_write_index_relaxed(const hsa_queue_t* queue) {
  return HsaApiTable->hsa_queue_load_write_index_relaxed_fn(queue);
}

void HSA_API hsa_queue_store_write_index_relaxed(const hsa_queue_t* queue,
                                                 uint64_t value) {
  return HsaApiTable->hsa_queue_store_write_index_relaxed_fn(queue, value);
}

void HSA_API hsa_queue_store_write_index_release(const hsa_queue_t* queue,
                                                 uint64_t value) {
  return HsaApiTable->hsa_queue_store_write_index_release_fn(queue, value);
}

uint64_t HSA_API hsa_queue_cas_write_index_acq_rel(const hsa_queue_t* queue,
                                                   uint64_t expected,
                                                   uint64_t value) {
  return HsaApiTable->hsa_queue_cas_write_index_acq_rel_fn(queue, expected,
                                                           value);
}

uint64_t HSA_API hsa_queue_cas_write_index_acquire(const hsa_queue_t* queue,
                                                   uint64_t expected,
                                                   uint64_t value) {
  return HsaApiTable->hsa_queue_cas_write_index_acquire_fn(queue, expected,
                                                           value);
}

uint64_t HSA_API hsa_queue_cas_write_index_relaxed(const hsa_queue_t* queue,
                                                   uint64_t expected,
                                                   uint64_t value) {
  return HsaApiTable->hsa_queue_cas_write_index_relaxed_fn(queue, expected,
                                                           value);
}

uint64_t HSA_API hsa_queue_cas_write_index_release(const hsa_queue_t* queue,
                                                   uint64_t expected,
                                                   uint64_t value) {
  return HsaApiTable->hsa_queue_cas_write_index_release_fn(queue, expected,
                                                           value);
}

uint64_t HSA_API hsa_queue_add_write_index_acq_rel(const hsa_queue_t* queue,
                                                   uint64_t value) {
  return HsaApiTable->hsa_queue_add_write_index_acq_rel_fn(queue, value);
}

uint64_t HSA_API hsa_queue_add_write_index_acquire(const hsa_queue_t* queue,
                                                   uint64_t value) {
  return HsaApiTable->hsa_queue_add_write_index_acquire_fn(queue, value);
}

uint64_t HSA_API hsa_queue_add_write_index_relaxed(const hsa_queue_t* queue,
                                                   uint64_t value) {
  return HsaApiTable->hsa_queue_add_write_index_relaxed_fn(queue, value);
}

uint64_t HSA_API hsa_queue_add_write_index_release(const hsa_queue_t* queue,
                                                   uint64_t value) {
  return HsaApiTable->hsa_queue_add_write_index_release_fn(queue, value);
}

void HSA_API hsa_queue_store_read_index_relaxed(const hsa_queue_t* queue,
                                                uint64_t value) {
  return HsaApiTable->hsa_queue_store_read_index_relaxed_fn(queue, value);
}

void HSA_API hsa_queue_store_read_index_release(const hsa_queue_t* queue,
                                                uint64_t value) {
  return HsaApiTable->hsa_queue_store_read_index_release_fn(queue, value);
}

hsa_status_t HSA_API hsa_agent_iterate_regions(
    hsa_agent_t agent,
    hsa_status_t (*callback)(hsa_region_t region, void* data), void* data) {
  return HsaApiTable->hsa_agent_iterate_regions_fn(agent, callback, data);
}

hsa_status_t HSA_API hsa_region_get_info(hsa_region_t region,
                                         hsa_region_info_t attribute,
                                         void* value) {
  return HsaApiTable->hsa_region_get_info_fn(region, attribute, value);
}

hsa_status_t HSA_API hsa_memory_register(void* address, size_t size) {
  return HsaApiTable->hsa_memory_register_fn(address, size);
}

hsa_status_t HSA_API hsa_memory_deregister(void* address, size_t size) {
  return HsaApiTable->hsa_memory_deregister_fn(address, size);
}

hsa_status_t HSA_API
    hsa_memory_allocate(hsa_region_t region, size_t size, void** ptr) {
  return HsaApiTable->hsa_memory_allocate_fn(region, size, ptr);
}

hsa_status_t HSA_API hsa_memory_free(void* ptr) {
  return HsaApiTable->hsa_memory_free_fn(ptr);
}

hsa_status_t HSA_API hsa_memory_copy(void* dst, const void* src, size_t size) {
  return HsaApiTable->hsa_memory_copy_fn(dst, src, size);
}

hsa_status_t HSA_API hsa_memory_assign_agent(void* ptr, hsa_agent_t agent,
                                             hsa_access_permission_t access) {
  return HsaApiTable->hsa_memory_assign_agent_fn(ptr, agent, access);
}

hsa_status_t HSA_API
    hsa_signal_create(hsa_signal_value_t initial_value, uint32_t num_consumers,
                      const hsa_agent_t* consumers, hsa_signal_t* signal) {
  return HsaApiTable->hsa_signal_create_fn(initial_value, num_consumers,
                                           consumers, signal);
}

hsa_status_t HSA_API hsa_signal_destroy(hsa_signal_t signal) {
  return HsaApiTable->hsa_signal_destroy_fn(signal);
}

hsa_signal_value_t HSA_API hsa_signal_load_relaxed(hsa_signal_t signal) {
  return HsaApiTable->hsa_signal_load_relaxed_fn(signal);
}

hsa_signal_value_t HSA_API hsa_signal_load_acquire(hsa_signal_t signal) {
  return HsaApiTable->hsa_signal_load_acquire_fn(signal);
}

void HSA_API
    hsa_signal_store_relaxed(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_store_relaxed_fn(signal, value);
}

void HSA_API
    hsa_signal_store_release(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_store_release_fn(signal, value);
}

hsa_signal_value_t HSA_API
    hsa_signal_wait_relaxed(hsa_signal_t signal,
                            hsa_signal_condition_t condition,
                            hsa_signal_value_t compare_value,
                            uint64_t timeout_hint,
                            hsa_wait_state_t wait_expectancy_hint) {
  return HsaApiTable->hsa_signal_wait_relaxed_fn(
      signal, condition, compare_value, timeout_hint, wait_expectancy_hint);
}

hsa_signal_value_t HSA_API
    hsa_signal_wait_acquire(hsa_signal_t signal,
                            hsa_signal_condition_t condition,
                            hsa_signal_value_t compare_value,
                            uint64_t timeout_hint,
                            hsa_wait_state_t wait_expectancy_hint) {
  return HsaApiTable->hsa_signal_wait_acquire_fn(
      signal, condition, compare_value, timeout_hint, wait_expectancy_hint);
}

void HSA_API
    hsa_signal_and_relaxed(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_and_relaxed_fn(signal, value);
}

void HSA_API
    hsa_signal_and_acquire(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_and_acquire_fn(signal, value);
}

void HSA_API
    hsa_signal_and_release(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_and_release_fn(signal, value);
}

void HSA_API
    hsa_signal_and_acq_rel(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_and_acq_rel_fn(signal, value);
}

void HSA_API
    hsa_signal_or_relaxed(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_or_relaxed_fn(signal, value);
}

void HSA_API
    hsa_signal_or_acquire(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_or_acquire_fn(signal, value);
}

void HSA_API
    hsa_signal_or_release(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_or_release_fn(signal, value);
}

void HSA_API
    hsa_signal_or_acq_rel(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_or_acq_rel_fn(signal, value);
}

void HSA_API
    hsa_signal_xor_relaxed(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_xor_relaxed_fn(signal, value);
}

void HSA_API
    hsa_signal_xor_acquire(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_xor_acquire_fn(signal, value);
}

void HSA_API
    hsa_signal_xor_release(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_xor_release_fn(signal, value);
}

void HSA_API
    hsa_signal_xor_acq_rel(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_xor_acq_rel_fn(signal, value);
}

void HSA_API
    hsa_signal_add_relaxed(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_add_relaxed_fn(signal, value);
}

void HSA_API
    hsa_signal_add_acquire(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_add_acquire_fn(signal, value);
}

void HSA_API
    hsa_signal_add_release(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_add_release_fn(signal, value);
}

void HSA_API
    hsa_signal_add_acq_rel(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_add_acq_rel_fn(signal, value);
}

void HSA_API
    hsa_signal_subtract_relaxed(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_subtract_relaxed_fn(signal, value);
}

void HSA_API
    hsa_signal_subtract_acquire(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_subtract_acquire_fn(signal, value);
}

void HSA_API
    hsa_signal_subtract_release(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_subtract_release_fn(signal, value);
}

void HSA_API
    hsa_signal_subtract_acq_rel(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_subtract_acq_rel_fn(signal, value);
}

hsa_signal_value_t HSA_API
    hsa_signal_exchange_relaxed(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_exchange_relaxed_fn(signal, value);
}

hsa_signal_value_t HSA_API
    hsa_signal_exchange_acquire(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_exchange_acquire_fn(signal, value);
}

hsa_signal_value_t HSA_API
    hsa_signal_exchange_release(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_exchange_release_fn(signal, value);
}

hsa_signal_value_t HSA_API
    hsa_signal_exchange_acq_rel(hsa_signal_t signal, hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_exchange_acq_rel_fn(signal, value);
}

hsa_signal_value_t HSA_API hsa_signal_cas_relaxed(hsa_signal_t signal,
                                                  hsa_signal_value_t expected,
                                                  hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_cas_relaxed_fn(signal, expected, value);
}

hsa_signal_value_t HSA_API hsa_signal_cas_acquire(hsa_signal_t signal,
                                                  hsa_signal_value_t expected,
                                                  hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_cas_acquire_fn(signal, expected, value);
}

hsa_signal_value_t HSA_API hsa_signal_cas_release(hsa_signal_t signal,
                                                  hsa_signal_value_t expected,
                                                  hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_cas_release_fn(signal, expected, value);
}

hsa_signal_value_t HSA_API hsa_signal_cas_acq_rel(hsa_signal_t signal,
                                                  hsa_signal_value_t expected,
                                                  hsa_signal_value_t value) {
  return HsaApiTable->hsa_signal_cas_acq_rel_fn(signal, expected, value);
}

hsa_status_t hsa_isa_from_name(const char* name, hsa_isa_t* isa) {
  return HsaApiTable->hsa_isa_from_name_fn(name, isa);
}

hsa_status_t HSA_API hsa_isa_get_info(hsa_isa_t isa, hsa_isa_info_t attribute,
                                      uint32_t index, void* value) {
  return HsaApiTable->hsa_isa_get_info_fn(isa, attribute, index, value);
}

hsa_status_t hsa_isa_compatible(hsa_isa_t code_object_isa, hsa_isa_t agent_isa,
                                bool* result) {
  return HsaApiTable->hsa_isa_compatible_fn(code_object_isa, agent_isa, result);
}

hsa_status_t HSA_API hsa_code_object_serialize(
    hsa_code_object_t code_object,
    hsa_status_t (*alloc_callback)(size_t size, hsa_callback_data_t data,
                                   void** address),
    hsa_callback_data_t callback_data, const char* options,
    void** serialized_code_object, size_t* serialized_code_object_size) {
  return HsaApiTable->hsa_code_object_serialize_fn(
      code_object, alloc_callback, callback_data, options,
      serialized_code_object, serialized_code_object_size);
}

hsa_status_t HSA_API
    hsa_code_object_deserialize(void* serialized_code_object,
                                size_t serialized_code_object_size,
                                const char* options,
                                hsa_code_object_t* code_object) {
  return HsaApiTable->hsa_code_object_deserialize_fn(
      serialized_code_object, serialized_code_object_size, options,
      code_object);
}

hsa_status_t HSA_API hsa_code_object_destroy(hsa_code_object_t code_object) {
  return HsaApiTable->hsa_code_object_destroy_fn(code_object);
}

hsa_status_t HSA_API hsa_code_object_get_info(hsa_code_object_t code_object,
                                              hsa_code_object_info_t attribute,
                                              void* value) {
  return HsaApiTable->hsa_code_object_get_info_fn(code_object, attribute,
                                                  value);
}

hsa_status_t HSA_API hsa_code_object_get_symbol(hsa_code_object_t code_object,
                                                const char* symbol_name,
                                                hsa_code_symbol_t* symbol) {
  return HsaApiTable->hsa_code_object_get_symbol_fn(code_object, symbol_name,
                                                    symbol);
}

hsa_status_t HSA_API hsa_code_symbol_get_info(hsa_code_symbol_t code_symbol,
                                              hsa_code_symbol_info_t attribute,
                                              void* value) {
  return HsaApiTable->hsa_code_symbol_get_info_fn(code_symbol, attribute,
                                                  value);
}

hsa_status_t HSA_API hsa_code_object_iterate_symbols(
    hsa_code_object_t code_object,
    hsa_status_t (*callback)(hsa_code_object_t code_object,
                             hsa_code_symbol_t symbol, void* data),
    void* data) {
  return HsaApiTable->hsa_code_object_iterate_symbols_fn(code_object, callback,
                                                         data);
}

hsa_status_t HSA_API
    hsa_executable_create(hsa_profile_t profile,
                          hsa_executable_state_t executable_state,
                          const char* options, hsa_executable_t* executable) {
  return HsaApiTable->hsa_executable_create_fn(profile, executable_state,
                                               options, executable);
}

hsa_status_t HSA_API hsa_executable_destroy(hsa_executable_t executable) {
  return HsaApiTable->hsa_executable_destroy_fn(executable);
}

hsa_status_t HSA_API
    hsa_executable_load_code_object(hsa_executable_t executable,
                                    hsa_agent_t agent,
                                    hsa_code_object_t code_object,
                                    const char* options) {
  return HsaApiTable->hsa_executable_load_code_object_fn(executable, agent,
                                                         code_object, options);
}

hsa_status_t HSA_API
    hsa_executable_freeze(hsa_executable_t executable, const char* options) {
  return HsaApiTable->hsa_executable_freeze_fn(executable, options);
}

hsa_status_t HSA_API hsa_executable_get_info(hsa_executable_t executable,
                                             hsa_executable_info_t attribute,
                                             void* value) {
  return HsaApiTable->hsa_executable_get_info_fn(executable, attribute, value);
}

hsa_status_t HSA_API
    hsa_executable_global_variable_define(hsa_executable_t executable,
                                          const char* variable_name,
                                          void* address) {
  return HsaApiTable->hsa_executable_global_variable_define_fn(
      executable, variable_name, address);
}

hsa_status_t HSA_API
    hsa_executable_agent_global_variable_define(hsa_executable_t executable,
                                                hsa_agent_t agent,
                                                const char* variable_name,
                                                void* address) {
  return HsaApiTable->hsa_executable_agent_global_variable_define_fn(
      executable, agent, variable_name, address);
}

hsa_status_t HSA_API
    hsa_executable_readonly_variable_define(hsa_executable_t executable,
                                            hsa_agent_t agent,
                                            const char* variable_name,
                                            void* address) {
  return HsaApiTable->hsa_executable_readonly_variable_define_fn(
      executable, agent, variable_name, address);
}

hsa_status_t HSA_API
    hsa_executable_validate(hsa_executable_t executable, uint32_t* result) {
  return HsaApiTable->hsa_executable_validate_fn(executable, result);
}

hsa_status_t HSA_API
    hsa_executable_get_symbol(hsa_executable_t executable,
                              const char* module_name, const char* symbol_name,
                              hsa_agent_t agent, int32_t call_convention,
                              hsa_executable_symbol_t* symbol) {
  return HsaApiTable->hsa_executable_get_symbol_fn(
      executable, module_name, symbol_name, agent, call_convention, symbol);
}

hsa_status_t HSA_API
    hsa_executable_symbol_get_info(hsa_executable_symbol_t executable_symbol,
                                   hsa_executable_symbol_info_t attribute,
                                   void* value) {
  return HsaApiTable->hsa_executable_symbol_get_info_fn(executable_symbol,
                                                        attribute, value);
}

hsa_status_t HSA_API hsa_executable_iterate_symbols(
    hsa_executable_t executable,
    hsa_status_t (*callback)(hsa_executable_t executable,
                             hsa_executable_symbol_t symbol, void* data),
    void* data) {
  return HsaApiTable->hsa_executable_iterate_symbols_fn(executable, callback,
                                                        data);
}

hsa_status_t HSA_API
    hsa_status_string(hsa_status_t status, const char** status_string) {
  return HsaApiTable->hsa_status_string_fn(status, status_string);
}
