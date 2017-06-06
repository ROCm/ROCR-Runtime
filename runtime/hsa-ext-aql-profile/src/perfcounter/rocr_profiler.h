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

#ifndef _ROCR_PROFILER_H_
#define _ROCR_PROFILER_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#if defined _WIN32 || defined __CYGWIN__
#ifdef __GNUC__
#define HSA_TOOLS_API __attribute__((dllexport))
#else
#define HSA_TOOLS_API __declspec(dllexport)  // Note: actually gcc seems
// to also supports this
// syntax.
#endif
#ifndef DLL_LOCAL
#define DLL_LOCAL
#endif

#else  // defined _WIN32 || defined __CYGWIN__
#if __GNUC__ >= 4
#define HSA_TOOLS_API __attribute__((visibility("default")))
#ifndef DLL_LOCAL
#define DLL_LOCAL __attribute__((visibility("hidden")))
#endif
#else
#define HSA_TOOLS_API
#ifndef DLL_LOCAL
#define DLL_LOCAL
#endif
#endif
#endif  // defined _WIN32 || defined __CYGWIN__

//---------------------------------------------------------------------------//
// @brief Enumeration of various information that is set for a counter.      //
// @detail This enumeration defines the various counter info that could be   //
//         used in a counter. This is used by a counter object to specify    //
//         its type and other conditions that are needed to retrieve a       //
//         counter value.                                                    //
//---------------------------------------------------------------------------//
typedef enum hsa_ext_tools_counter_parameter_s {
  // Event index of a counter
  HSA_EXT_TOOLS_COUNTER_PARAMETER_EVENT_INDEX = 0,

  // Simd mask of a counter
  HSA_EXT_TOOLS_COUNTER_PARAMETER_SIMD_MASK = 1,

  // Shader engine mask of a counter
  HSA_EXT_TOOLS_COUNTER_PARAMETER_SHADER_MASK = 2,

  // Max counter info index
  HSA_EXT_TOOLS_COUNTER_PARAMETER_INFO_MAX
} hsa_ext_tools_counter_parameter_t;

//---------------------------------------------------------------------------//
// @brief Enumeration of counter block type mask                             //
// @details This enumeration define the bit mask representing types of       //
// counter broup supported by HSA. This is used by counter block object to   //
// specify its type.                                                         //
//---------------------------------------------------------------------------//
typedef enum hsa_ext_tools_counter_block_type_s {
  // Unknown counter block type
  HSA_EXT_TOOLS_COUNTER_BLOCK_TYPE_UNKNOWN = 0,

  // The CounterBlock of this type can be access at anytime.
  // note Examples are software Counters and CPU Counters.
  HSA_EXT_TOOLS_COUNTER_BLOCK_TYPE_SYNC = 1,

  // The CounterBlock type can be access asynchronously.
  // It is required that the Counter must be stopped
  // before accessing.
  HSA_EXT_TOOLS_COUNTER_BLOCK_TYPE_ASYNC = 2,

  // The CounterBlock of this counter block is used for generating
  // trace.
  HSA_EXT_TOOLS_COUNTER_BLOCK_TYPE_TRACE = 3,

  // Max CounterBlock type
  HSA_EXT_TOOLS_COUNTER_BLOCK_TYPE_MAX
} hsa_ext_tools_counter_block_type_t;

//---------------------------------------------------------------------------//
// @brief Enumeration of various information that is set for a counter block.//
// @detail This enumeration defines the various info that could be used      //
// in a counter block. This is used by a counter object to specify its type  //
// and other conditions that are needed for a counter block.                 //
//---------------------------------------------------------------------------//
/*
typedef enum hsa_ext_tools_counter_block_info_s {
  // Index of a counter block
  HSA_EXT_TOOLS_COUNTER_BLOCK_INFO_EVENT_INDEX = 0,

  // Shader bits of a counter block
  HSA_EXT_TOOLS_COUNTER_BLOCK_INFO_SHADER_BITS = 1,

  // Simd mask of a counter
  HSA_EXT_TOOLS_COUNTER_BLOCK_INFO_CONTROL_METHOD = 2,

  // Max index of counter block info
  HSA_EXT_TOOLS_COUNTER_BLOCK_INFO_MAX
} hsa_ext_tools_counter_block_info_t;
*/

//---------------------------------------------------------------------------//
// Enumeration for the methods used to index into the correct registers.    //
//---------------------------------------------------------------------------//
/*
typedef enum hsa_ext_tools_counter_index_method_s {
  // No index
  HSA_EXT_TOOLS_COUNTER_INDEX_METHOD_BY_NONE = 0,

  // Index by block instance
  HSA_EXT_TOOLS_COUNTER_INDEX_METHOD_BY_INSTANCE = 1,

  // Index by shader engine
  HSA_EXT_TOOLS_COUNTER_INDEX_METHOD_BY_SHADER_ENGINE = 2,

  // Index by shader and instance
  HSA_EXT_TOOLS_COUNTER_INDEX_METHOD_BY_SHADER_ENGINE_ANDINSTANCE = 3
} hsa_ext_tools_counter_index_method_t;
*/

//---------------------------------------------------------------------------//
// Enumeration for the HSAPerf generic error codes                           //
//---------------------------------------------------------------------------//
/*
typedef enum hsa_ext_tools_error_codes_s {
  // Successful
  HSA_EXT_TOOLS_ERROR_CODE_OK = 0,

  // Generic error code
  HSA_EXT_TOOLS_ERROR_CODE_ERROR,

  // Generic invalid HSAPerf API arguments
  HSA_EXT_TOOLS_ERROR_CODE_INVALID_ARGS,

  // The operation is not permit due to currently in the unmodifiable
  // HSAPerf state .
  HSA_EXT_TOOLS_ERROR_CODE_UNMODIFIABLE_STATE,

  // The hsa_ext_tools_set_pmu_parameter() or
  // hsa_ext_tools_get_pmu_parameter() API contains invalid parameter value.
  HSA_EXT_TOOLS_ERROR_CODE_INVALID_PARAM,

  // The hsa_ext_tools_set_pmu_parameter() or
  // hsa_ext_tools_get_pmu_parameter() API contains invalid parameter size
  // or return size.
  HSA_EXT_TOOLS_ERROR_CODE_INVALID_PARAM_SIZE,

  // The hsa_ext_tools_set_pmu_parameter() or
  // hsa_ext_tools_get_pmu_parameter() API contains invalid
  // pointer (e.g. NULL).
  HSA_EXT_TOOLS_ERROR_CODE_INVALID_PARAM_DATA,

  // The hsa_ext_tools_get_pmu_info() API contains invalid info value.
  HSA_EXT_TOOLS_ERROR_CODE_INVALID_INFO,

  // The hsa_ext_tools_get_pmu_info() API contains invalid info
  // size (e.g. zero).
  HSA_EXT_TOOLS_ERROR_CODE_INVALID_INFO_SIZE,

  // The hsa_ext_tools_get_pmu_info() API contains invalid
  // data (e.g. NULL).
  HSA_EXT_TOOLS_ERROR_CODE_INVALID_INFO_DATA
} hsa_ext_tools_error_codes_t;
*/

//---------------------------------------------------------------------------//
// Enumeration for Pmu profiling state                                       //
//---------------------------------------------------------------------------//
typedef enum rocr_pmu_state_s {
  // Profiling idle. In this state, changes can be made to
  // the PMU, counter blocks, counters. This state can represent
  // the moment prior to calling begin or after calling
  // hsa_ext_tools_pmu_wait_for_completion().
  ROCR_PMU_STATE_IDLE,

  // Profiling start. In this state, changes cannot be made to
  // the PMU, counter block, counters. The PMU is collecting
  // performance counter data. This state represents
  // the moment after calling hsa_ext_tools_pmu_begin() and before calling
  // hsa_ext_tools_pmu_end()
  ROCR_PMU_STATE_START,

  // Profiling stop. In this state, changes cannot be made to
  // the PMU, counter blocks, Counters. PMU has stopped the
  // performance counter data collection. However, the result
  // might not yet be available. This state represents
  // the moment after calling hsa_ext_tools_pmu_end() and before the call
  // to hsa_ext_tools_pmu_wait_for_completion() has returned success.
  ROCR_PMU_STATE_STOP
} rocr_pmu_state_t;

//---------------------------------------------------------------------------//
//  Opaque pointer to HSA performance monitor unit (PMU)                     //
//---------------------------------------------------------------------------//
// typedef void *  hsa_ext_tools_pmu_t;

//---------------------------------------------------------------------------//
// Opaque pointer to HSA counter block                                       //
//---------------------------------------------------------------------------//
// typedef void *  hsa_ext_tools_counter_block_t;

//---------------------------------------------------------------------------//
// Opaque pointer to HSA counter                                             //
//---------------------------------------------------------------------------//
// typedef void *  hsa_ext_tools_counter_t;

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // _ROCR_PROFILER_H_
