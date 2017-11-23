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

#include "core/inc/hsa_ext_interface.h"

#include <string>

#include "core/inc/runtime.h"

namespace core {
// Implementations for missing / unsupported extensions
template <class T0>
static T0 hsa_ext_null() {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1>
static T0 hsa_ext_null(T1) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2>
static T0 hsa_ext_null(T1, T2) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3>
static T0 hsa_ext_null(T1, T2, T3) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4>
static T0 hsa_ext_null(T1, T2, T3, T4) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5>
static T0 hsa_ext_null(T1, T2, T3, T4, T5) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                       T14) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14, class T15>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                       T14, T15) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14, class T15, class T16>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                       T14, T15, T16) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14, class T15, class T16, class T17>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                       T14, T15, T16, T17) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14, class T15, class T16, class T17, class T18>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                       T14, T15, T16, T17, T18) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14, class T15, class T16, class T17, class T18,
          class T19>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                       T14, T15, T16, T17, T18, T19) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14, class T15, class T16, class T17, class T18,
          class T19, class T20>
static T0 hsa_ext_null(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                       T14, T15, T16, T17, T18, T19, T20) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6>
static T0 hsa_amd_null(T1, T2, T3, T4, T5, T6) {
  return HSA_STATUS_ERROR_NOT_INITIALIZED;
}

ExtensionEntryPoints::ExtensionEntryPoints() {
  InitFinalizerExtTable();
  InitImageExtTable();
  InitAmdExtTable();
}

// Initialize Finalizer function table to be NULLs
void ExtensionEntryPoints::InitFinalizerExtTable() {
  
  // Initialize Version of Api Table
  finalizer_api.version.major_id = 0x00;
  finalizer_api.version.minor_id = 0x00;
  finalizer_api.version.step_id = 0x00;

  finalizer_api.hsa_ext_program_create_fn = hsa_ext_null;
  finalizer_api.hsa_ext_program_destroy_fn = hsa_ext_null;
  finalizer_api.hsa_ext_program_add_module_fn = hsa_ext_null;
  finalizer_api.hsa_ext_program_iterate_modules_fn = hsa_ext_null;
  finalizer_api.hsa_ext_program_get_info_fn = hsa_ext_null;
  finalizer_api.hsa_ext_program_finalize_fn = hsa_ext_null;
}

// Initialize Image function table to be NULLs
void ExtensionEntryPoints::InitImageExtTable() {
 
  // Initialize Version of Api Table
  image_api.version.major_id = 0x00;
  image_api.version.minor_id = 0x00;
  image_api.version.step_id = 0x00;

  image_api.hsa_ext_image_get_capability_fn = hsa_ext_null;
  image_api.hsa_ext_image_data_get_info_fn = hsa_ext_null;
  image_api.hsa_ext_image_create_fn = hsa_ext_null;
  image_api.hsa_ext_image_import_fn = hsa_ext_null;
  image_api.hsa_ext_image_export_fn = hsa_ext_null;
  image_api.hsa_ext_image_copy_fn = hsa_ext_null;
  image_api.hsa_ext_image_clear_fn = hsa_ext_null;
  image_api.hsa_ext_image_destroy_fn = hsa_ext_null;
  image_api.hsa_ext_sampler_create_fn = hsa_ext_null;
  image_api.hsa_ext_sampler_destroy_fn = hsa_ext_null;
  image_api.hsa_amd_image_get_info_max_dim_fn = hsa_ext_null;
  image_api.hsa_ext_image_get_capability_with_layout_fn = hsa_ext_null;
  image_api.hsa_ext_image_data_get_info_with_layout_fn = hsa_ext_null;
  image_api.hsa_ext_image_create_with_layout_fn = hsa_ext_null;
}

// Initialize Amd Ext table for Api related to Images
void ExtensionEntryPoints::InitAmdExtTable() {
  hsa_api_table_.amd_ext_api.hsa_amd_image_create_fn = hsa_ext_null;
  hsa_internal_api_table_.amd_ext_api.hsa_amd_image_create_fn = hsa_ext_null;
}

// Update Amd Ext table for Api related to Images.
// @note: Interface should be updated when Amd Ext table
// begins hosting Api's from other extension libraries
void ExtensionEntryPoints::UpdateAmdExtTable(void *func_ptr) {
  
  assert(hsa_api_table_.amd_ext_api.hsa_amd_image_create_fn ==
             (decltype(hsa_amd_image_create)*)hsa_ext_null && 
             "Duplicate load of extension import.");
  assert(hsa_internal_api_table_.amd_ext_api.hsa_amd_image_create_fn ==
             (decltype(hsa_amd_image_create)*)hsa_ext_null && 
             "Duplicate load of extension import.");
  hsa_api_table_.amd_ext_api.hsa_amd_image_create_fn = 
             (decltype(hsa_amd_image_create)*)func_ptr;
  hsa_internal_api_table_.amd_ext_api.hsa_amd_image_create_fn = 
             (decltype(hsa_amd_image_create)*)func_ptr;
}

void ExtensionEntryPoints::Unload() {
  for (auto lib : libs_) {
    void* ptr = os::GetExportAddress(lib, "Unload");
    if (ptr) {
      ((Unload_t)ptr)();
    }
  }
  // Due to valgrind bug, runtime cannot dlclose extensions see:
  // http://valgrind.org/docs/manual/faq.html#faq.unhelpful
  if (!core::Runtime::runtime_singleton_->flag().running_valgrind()) {
    for (auto lib : libs_) {
      os::CloseLib(lib);
    }
  }
  libs_.clear();

  InitFinalizerExtTable();
  InitImageExtTable();
  InitAmdExtTable();
  core::hsa_internal_api_table_.Reset();
}

bool ExtensionEntryPoints::LoadImage(std::string library_name) {
  os::LibHandle lib = os::LoadLib(library_name);
  if (lib == NULL) {
    return false;
  }
  libs_.push_back(lib);
  
  void* ptr;

  ptr = os::GetExportAddress(lib, "hsa_ext_image_get_capability_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_get_capability_fn ==
               (decltype(::hsa_ext_image_get_capability)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_get_capability_fn =
        (decltype(::hsa_ext_image_get_capability)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_data_get_info_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_data_get_info_fn ==
               (decltype(::hsa_ext_image_data_get_info)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_data_get_info_fn =
        (decltype(::hsa_ext_image_data_get_info)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_create_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_create_fn ==
               (decltype(::hsa_ext_image_create)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_create_fn = (decltype(::hsa_ext_image_create)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_import_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_import_fn ==
               (decltype(::hsa_ext_image_import)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_import_fn = (decltype(::hsa_ext_image_import)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_export_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_export_fn ==
               (decltype(::hsa_ext_image_export)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_export_fn = (decltype(::hsa_ext_image_export)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_copy_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_copy_fn ==
               (decltype(::hsa_ext_image_copy)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_copy_fn = (decltype(::hsa_ext_image_copy)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_clear_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_clear_fn ==
               (decltype(::hsa_ext_image_clear)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_clear_fn = (decltype(::hsa_ext_image_clear)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_destroy_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_destroy_fn ==
               (decltype(::hsa_ext_image_destroy)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_destroy_fn = (decltype(::hsa_ext_image_destroy)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_sampler_create_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_sampler_create_fn ==
               (decltype(::hsa_ext_sampler_create)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_sampler_create_fn = (decltype(::hsa_ext_sampler_create)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_sampler_destroy_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_sampler_destroy_fn ==
               (decltype(::hsa_ext_sampler_destroy)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_sampler_destroy_fn =
        (decltype(::hsa_ext_sampler_destroy)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_get_capability_with_layout_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_get_capability_with_layout_fn ==
               (decltype(::hsa_ext_image_get_capability_with_layout)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_get_capability_with_layout_fn =
        (decltype(::hsa_ext_image_get_capability_with_layout)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_data_get_info_with_layout_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_data_get_info_with_layout_fn ==
               (decltype(::hsa_ext_image_data_get_info_with_layout)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_data_get_info_with_layout_fn =
        (decltype(::hsa_ext_image_data_get_info_with_layout)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_image_create_with_layout_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_ext_image_create_with_layout_fn ==
               (decltype(::hsa_ext_image_create_with_layout)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_ext_image_create_with_layout_fn = (decltype(::hsa_ext_image_create_with_layout)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_amd_image_get_info_max_dim_impl");
  if (ptr != NULL) {
    assert(image_api.hsa_amd_image_get_info_max_dim_fn ==
               (decltype(::hsa_amd_image_get_info_max_dim)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    image_api.hsa_amd_image_get_info_max_dim_fn =
        (decltype(::hsa_amd_image_get_info_max_dim)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_amd_image_create_impl");
  if (ptr != NULL) {
    UpdateAmdExtTable(ptr);
  }
 
  // Initialize Version of Api Table
  image_api.version.major_id = HSA_IMAGE_API_TABLE_MAJOR_VERSION;
  image_api.version.minor_id = sizeof(ImageExtTable);
  image_api.version.step_id = HSA_IMAGE_API_TABLE_STEP_VERSION;

  // Update private copy of Api table with handle for Image extensions
  hsa_internal_api_table_.CloneExts(&image_api,
                                    core::HsaApiTable::HSA_EXT_IMAGE_API_TABLE_ID);

  ptr = os::GetExportAddress(lib, "Load");
  if (ptr != NULL) {
    ((Load_t)ptr)(&core::hsa_internal_api_table_.hsa_api);
  }

  return true;
}

bool ExtensionEntryPoints::LoadFinalizer(std::string library_name) {
  os::LibHandle lib = os::LoadLib(library_name);
  if (lib == NULL) {
    return false;
  }
  libs_.push_back(lib);
  
  void* ptr;

  ptr = os::GetExportAddress(lib, "hsa_ext_program_create_impl");
  if (ptr != NULL) {
    assert(finalizer_api.hsa_ext_program_create_fn ==
               (decltype(::hsa_ext_program_create)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    finalizer_api.hsa_ext_program_create_fn = (decltype(::hsa_ext_program_create)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_program_destroy_impl");
  if (ptr != NULL) {
    assert(finalizer_api.hsa_ext_program_destroy_fn ==
               (decltype(::hsa_ext_program_destroy)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    finalizer_api.hsa_ext_program_destroy_fn =
        (decltype(::hsa_ext_program_destroy)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_program_add_module_impl");
  if (ptr != NULL) {
    assert(finalizer_api.hsa_ext_program_add_module_fn ==
               (decltype(::hsa_ext_program_add_module)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    finalizer_api.hsa_ext_program_add_module_fn =
        (decltype(::hsa_ext_program_add_module)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_program_iterate_modules_impl");
  if (ptr != NULL) {
    assert(finalizer_api.hsa_ext_program_iterate_modules_fn ==
               (decltype(::hsa_ext_program_iterate_modules)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    finalizer_api.hsa_ext_program_iterate_modules_fn =
        (decltype(::hsa_ext_program_iterate_modules)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_program_get_info_impl");
  if (ptr != NULL) {
    assert(finalizer_api.hsa_ext_program_get_info_fn ==
               (decltype(::hsa_ext_program_get_info)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    finalizer_api.hsa_ext_program_get_info_fn =
        (decltype(::hsa_ext_program_get_info)*)ptr;
  }

  ptr = os::GetExportAddress(lib, "hsa_ext_program_finalize_impl");
  if (ptr != NULL) {
    assert(finalizer_api.hsa_ext_program_finalize_fn ==
               (decltype(::hsa_ext_program_finalize)*)hsa_ext_null &&
           "Duplicate load of extension import.");
    finalizer_api.hsa_ext_program_finalize_fn =
        (decltype(::hsa_ext_program_finalize)*)ptr;
  }
  
  // Initialize Version of Api Table
  finalizer_api.version.major_id = HSA_FINALIZER_API_TABLE_MAJOR_VERSION;
  finalizer_api.version.minor_id = sizeof(::FinalizerExtTable);
  finalizer_api.version.step_id = HSA_FINALIZER_API_TABLE_STEP_VERSION;
 
  // Update handle of table of HSA extensions
  hsa_internal_api_table_.CloneExts(&finalizer_api,
                                    core::HsaApiTable::HSA_EXT_FINALIZER_API_TABLE_ID);

  ptr = os::GetExportAddress(lib, "Load");
  if (ptr != NULL) {
    ((Load_t)ptr)(&core::hsa_internal_api_table_.hsa_api);
  }

  return true;
}

}  // namespace core

//---------------------------------------------------------------------------//
//   Exported extension stub functions
//---------------------------------------------------------------------------//

hsa_status_t hsa_ext_program_create(
    hsa_machine_model_t machine_model, hsa_profile_t profile,
    hsa_default_float_rounding_mode_t default_float_rounding_mode,
    const char* options, hsa_ext_program_t* program) {
  return core::Runtime::runtime_singleton_->extensions_.finalizer_api
      .hsa_ext_program_create_fn(machine_model, profile,
                                 default_float_rounding_mode, options, program);
}

hsa_status_t hsa_ext_program_destroy(hsa_ext_program_t program) {
  return core::Runtime::runtime_singleton_->extensions_.finalizer_api
      .hsa_ext_program_destroy_fn(program);
}

hsa_status_t hsa_ext_program_add_module(hsa_ext_program_t program,
                                        hsa_ext_module_t module) {
  return core::Runtime::runtime_singleton_->extensions_.finalizer_api
      .hsa_ext_program_add_module_fn(program, module);
}

hsa_status_t hsa_ext_program_iterate_modules(
    hsa_ext_program_t program,
    hsa_status_t (*callback)(hsa_ext_program_t program, hsa_ext_module_t module,
                             void* data),
    void* data) {
  return core::Runtime::runtime_singleton_->extensions_.finalizer_api
      .hsa_ext_program_iterate_modules_fn(program, callback, data);
}

hsa_status_t hsa_ext_program_get_info(hsa_ext_program_t program,
                                      hsa_ext_program_info_t attribute,
                                      void* value) {
  return core::Runtime::runtime_singleton_->extensions_.finalizer_api
      .hsa_ext_program_get_info_fn(program, attribute, value);
}

hsa_status_t hsa_ext_program_finalize(
    hsa_ext_program_t program, hsa_isa_t isa, int32_t call_convention,
    hsa_ext_control_directives_t control_directives, const char* options,
    hsa_code_object_type_t code_object_type, hsa_code_object_t* code_object) {
  return core::Runtime::runtime_singleton_->extensions_.finalizer_api
      .hsa_ext_program_finalize_fn(program, isa, call_convention,
                                   control_directives, options,
                                   code_object_type, code_object);
}

hsa_status_t hsa_ext_image_get_capability(
    hsa_agent_t agent, hsa_ext_image_geometry_t geometry,
    const hsa_ext_image_format_t* image_format, uint32_t* capability_mask) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_get_capability_fn(agent, geometry, image_format,
                                       capability_mask);
}

hsa_status_t hsa_ext_image_data_get_info(
    hsa_agent_t agent, const hsa_ext_image_descriptor_t* image_descriptor,
    hsa_access_permission_t access_permission,
    hsa_ext_image_data_info_t* image_data_info) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_data_get_info_fn(agent, image_descriptor,
                                      access_permission, image_data_info);
}

hsa_status_t hsa_ext_image_create(
    hsa_agent_t agent, const hsa_ext_image_descriptor_t* image_descriptor,
    const void* image_data, hsa_access_permission_t access_permission,
    hsa_ext_image_t* image) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_create_fn(agent, image_descriptor, image_data,
                               access_permission, image);
}

hsa_status_t hsa_ext_image_import(hsa_agent_t agent, const void* src_memory,
                                  size_t src_row_pitch, size_t src_slice_pitch,
                                  hsa_ext_image_t dst_image,
                                  const hsa_ext_image_region_t* image_region) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_import_fn(agent, src_memory, src_row_pitch,
                               src_slice_pitch, dst_image, image_region);
}

hsa_status_t hsa_ext_image_export(hsa_agent_t agent, hsa_ext_image_t src_image,
                                  void* dst_memory, size_t dst_row_pitch,
                                  size_t dst_slice_pitch,
                                  const hsa_ext_image_region_t* image_region) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_export_fn(agent, src_image, dst_memory, dst_row_pitch,
                               dst_slice_pitch, image_region);
}

hsa_status_t hsa_ext_image_copy(hsa_agent_t agent, hsa_ext_image_t src_image,
                                const hsa_dim3_t* src_offset,
                                hsa_ext_image_t dst_image,
                                const hsa_dim3_t* dst_offset,
                                const hsa_dim3_t* range) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_copy_fn(agent, src_image, src_offset, dst_image,
                             dst_offset, range);
}

hsa_status_t hsa_ext_image_clear(hsa_agent_t agent, hsa_ext_image_t image,
                                 const void* data,
                                 const hsa_ext_image_region_t* image_region) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_clear_fn(agent, image, data, image_region);
}

hsa_status_t hsa_ext_image_destroy(hsa_agent_t agent, hsa_ext_image_t image) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_destroy_fn(agent, image);
}

hsa_status_t hsa_ext_sampler_create(
    hsa_agent_t agent, const hsa_ext_sampler_descriptor_t* sampler_descriptor,
    hsa_ext_sampler_t* sampler) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_sampler_create_fn(agent, sampler_descriptor, sampler);
}

hsa_status_t hsa_ext_sampler_destroy(hsa_agent_t agent,
                                     hsa_ext_sampler_t sampler) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_sampler_destroy_fn(agent, sampler);
}

hsa_status_t hsa_ext_image_get_capability_with_layout(
    hsa_agent_t agent, hsa_ext_image_geometry_t geometry,
    const hsa_ext_image_format_t* image_format,
    hsa_ext_image_data_layout_t image_data_layout,
    uint32_t* capability_mask) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_get_capability_with_layout_fn(agent, geometry, image_format,
                                       image_data_layout, capability_mask);
}

hsa_status_t hsa_ext_image_data_get_info_with_layout(
    hsa_agent_t agent, const hsa_ext_image_descriptor_t* image_descriptor,
    hsa_access_permission_t access_permission,
    hsa_ext_image_data_layout_t image_data_layout,
    size_t image_data_row_pitch,
    size_t image_data_slice_pitch,
    hsa_ext_image_data_info_t* image_data_info) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_data_get_info_with_layout_fn(agent, image_descriptor,
                                      access_permission, image_data_layout,
                                      image_data_row_pitch, image_data_slice_pitch,
                                      image_data_info);
}

hsa_status_t hsa_ext_image_create_with_layout(
    hsa_agent_t agent, const hsa_ext_image_descriptor_t* image_descriptor,
    const void* image_data, hsa_access_permission_t access_permission,
    hsa_ext_image_data_layout_t image_data_layout,
    size_t image_data_row_pitch,
    size_t image_data_slice_pitch,
    hsa_ext_image_t* image) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_ext_image_create_with_layout_fn(agent, image_descriptor, image_data,
                               access_permission, image_data_layout,
                               image_data_row_pitch, image_data_slice_pitch,
                               image);
}

//---------------------------------------------------------------------------//
//  Stubs for internal extension functions
//---------------------------------------------------------------------------//

// Use the function pointer from local instance Image Extension
hsa_status_t hsa_amd_image_get_info_max_dim(hsa_agent_t component,
                                            hsa_agent_info_t attribute,
                                            void* value) {
  return core::Runtime::runtime_singleton_->extensions_.image_api
      .hsa_amd_image_get_info_max_dim_fn(component, attribute, value);
}
