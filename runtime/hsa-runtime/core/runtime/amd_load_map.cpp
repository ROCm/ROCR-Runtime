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

#include <cassert>
#include "core/inc/amd_hsa_loader.hpp"
#include "core/inc/amd_load_map.h"
#include "core/inc/runtime.h"

using amd::hsa::loader::Executable;
using amd::hsa::loader::LoadedCodeObject;
using amd::hsa::loader::LoadedSegment;

hsa_status_t amd_executable_load_code_object(
  hsa_executable_t executable,
  hsa_agent_t agent,
  hsa_code_object_t code_object,
  const char *options,
  amd_loaded_code_object_t *loaded_code_object)
{
  if (!core::Runtime::runtime_singleton_->IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }
  if (nullptr == loaded_code_object) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  Executable *exec = Executable::Object(executable);
  if (nullptr == exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }
  return exec->LoadCodeObject(agent, code_object, options, loaded_code_object);
}

hsa_status_t amd_iterate_executables(
  hsa_status_t (*callback)(
    hsa_executable_t executable,
    void *data),
  void *data)
{
  if (!core::Runtime::runtime_singleton_->IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }
  if (nullptr == callback) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return core::Runtime::runtime_singleton_->loader()->IterateExecutables(callback, data);
}

hsa_status_t amd_executable_iterate_loaded_code_objects(
  hsa_executable_t executable,
  hsa_status_t (*callback)(
    amd_loaded_code_object_t loaded_code_object,
    void *data),
  void *data)
{
  if (!core::Runtime::runtime_singleton_->IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }
  if (nullptr == callback) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  Executable *exec = Executable::Object(executable);
  if (nullptr == exec) {
    return HSA_STATUS_ERROR_INVALID_EXECUTABLE;
  }
  return exec->IterateLoadedCodeObjects(callback, data);
}

hsa_status_t amd_loaded_code_object_get_info(
  amd_loaded_code_object_t loaded_code_object,
  amd_loaded_code_object_info_t attribute,
  void *value)
{
  if (!core::Runtime::runtime_singleton_->IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }
  if (nullptr == value) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  LoadedCodeObject *obj = LoadedCodeObject::Object(loaded_code_object);
  if (nullptr == obj) {
    // \todo: new error code: AMD_STATUS_ERROR_INVALID_LOADED_CODE_OBJECT.
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return false == obj->GetInfo(attribute, value) ?
    HSA_STATUS_ERROR_INVALID_ARGUMENT : HSA_STATUS_SUCCESS;
}

hsa_status_t amd_loaded_code_object_iterate_loaded_segments(
  amd_loaded_code_object_t loaded_code_object,
  hsa_status_t (*callback)(
    amd_loaded_segment_t loaded_segment,
    void *data),
  void *data)
{
  if (!core::Runtime::runtime_singleton_->IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }
  if (nullptr == callback) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  LoadedCodeObject *obj = LoadedCodeObject::Object(loaded_code_object);
  if (nullptr == obj) {
    // \todo: new error code: AMD_STATUS_ERROR_INVALID_LOADED_CODE_OBJECT.
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return obj->IterateLoadedSegments(callback, data);
}

hsa_status_t amd_loaded_segment_get_info(
  amd_loaded_segment_t loaded_segment,
  amd_loaded_segment_info_t attribute,
  void *value)
{
  if (!core::Runtime::runtime_singleton_->IsOpen()) {
    return HSA_STATUS_ERROR_NOT_INITIALIZED;
  }
  if (nullptr == value) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  LoadedSegment *obj = LoadedSegment::Object(loaded_segment);
  if (nullptr == obj) {
    // \todo: new error code: AMD_STATUS_ERROR_INVALID_LOADED_SEGMENT.
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return false == obj->GetInfo(attribute, value) ?
    HSA_STATUS_ERROR_INVALID_ARGUMENT : HSA_STATUS_SUCCESS;
}
