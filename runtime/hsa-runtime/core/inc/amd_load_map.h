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

#ifndef AMD_LOAD_MAP_H
#define AMD_LOAD_MAP_H

#include "hsa.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/// @todo.
enum {
  AMD_EXTENSION_LOAD_MAP = 0x1002
};

/// @todo.
typedef struct amd_loaded_code_object_s {
  uint64_t handle;
} amd_loaded_code_object_t;

/// @todo.
enum amd_loaded_code_object_info_t {
  AMD_LOADED_CODE_OBJECT_INFO_ELF_IMAGE = 0,
  AMD_LOADED_CODE_OBJECT_INFO_ELF_IMAGE_SIZE = 1
};

/// @todo.
typedef struct amd_loaded_segment_s {
  uint64_t handle;
} amd_loaded_segment_t;

/// @todo.
enum amd_loaded_segment_info_t {
  AMD_LOADED_SEGMENT_INFO_TYPE = 0,
  AMD_LOADED_SEGMENT_INFO_ELF_BASE_ADDRESS = 1,
  AMD_LOADED_SEGMENT_INFO_LOAD_BASE_ADDRESS = 2,
  AMD_LOADED_SEGMENT_INFO_SIZE = 3
};

/// @todo.
hsa_status_t amd_executable_load_code_object(
  hsa_executable_t executable,
  hsa_agent_t agent,
  hsa_code_object_t code_object,
  const char *options,
  amd_loaded_code_object_t *loaded_code_object);

/// @brief Invokes @p callback for each available executable in current
/// process.
hsa_status_t amd_iterate_executables(
  hsa_status_t (*callback)(
    hsa_executable_t executable,
    void *data),
  void *data);

/// @brief Invokes @p callback for each loaded code object in specified
/// @p executable.
hsa_status_t amd_executable_iterate_loaded_code_objects(
  hsa_executable_t executable,
  hsa_status_t (*callback)(
    amd_loaded_code_object_t loaded_code_object,
    void *data),
  void *data);

/// @brief Retrieves current value of specified @p loaded_code_object's
/// @p attribute.
hsa_status_t amd_loaded_code_object_get_info(
  amd_loaded_code_object_t loaded_code_object,
  amd_loaded_code_object_info_t attribute,
  void *value);

/// @brief Invokes @p callback for each loaded segment in specified
/// @p loaded_code_object.
hsa_status_t amd_loaded_code_object_iterate_loaded_segments(
  amd_loaded_code_object_t loaded_code_object,
  hsa_status_t (*callback)(
    amd_loaded_segment_t loaded_segment,
    void *data),
  void *data);

/// @brief Retrieves current value of specified @p loaded_segment's
/// @p attribute.
hsa_status_t amd_loaded_segment_get_info(
  amd_loaded_segment_t loaded_segment,
  amd_loaded_segment_info_t attribute,
  void *value);

#define amd_load_map_1_00

typedef struct amd_load_map_1_00_pfn_s {
  hsa_status_t (*amd_executable_load_code_object)(
    hsa_executable_t executable,
    hsa_agent_t agent,
    hsa_code_object_t code_object,
    const char *options,
    amd_loaded_code_object_t *loaded_code_object);

  hsa_status_t (*amd_iterate_executables)(
    hsa_status_t (*callback)(
      hsa_executable_t executable,
      void *data),
    void *data);

  hsa_status_t (*amd_executable_iterate_loaded_code_objects)(
    hsa_executable_t executable,
    hsa_status_t (*callback)(
      amd_loaded_code_object_t loaded_code_object,
      void *data),
    void *data);

  hsa_status_t (*amd_loaded_code_object_get_info)(
    amd_loaded_code_object_t loaded_code_object,
    amd_loaded_code_object_info_t attribute,
    void *value);

  hsa_status_t (*amd_loaded_code_object_iterate_loaded_segments)(
    amd_loaded_code_object_t loaded_code_object,
    hsa_status_t (*callback)(
      amd_loaded_segment_t loaded_segment,
      void *data),
    void *data);

  hsa_status_t (*amd_loaded_segment_get_info)(
    amd_loaded_segment_t loaded_segment,
    amd_loaded_segment_info_t attribute,
    void *value);
} amd_load_map_1_00_pfn_t;

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // AMD_LOAD_MAP_H
