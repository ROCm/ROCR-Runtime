/*
 * =============================================================================
 *   ROC Runtime Conformance Release License
 * =============================================================================
 * The University of Illinois/NCSA
 * Open Source License (NCSA)
 *
 * Copyright (c) 2017, Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Developed by:
 *
 *                 AMD Research and AMD ROC Software Development
 *
 *                 Advanced Micro Devices, Inc.
 *
 *                 www.amd.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimers in
 *    the documentation and/or other materials provided with the distribution.
 *  - Neither the names of <Name of Development Group, Name of Institution>,
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this Software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

/// \file
/// RocR related helper functions for sequeneces that come up frequently

#ifndef ROCRTST_COMMON_COMMON_H_
#define ROCRTST_COMMON_COMMON_H_

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"
#include "hsa/hsa_ext_finalize.h"

namespace rocrtst {

#if defined(_MSC_VER)
#define ALIGNED_(x) __declspec(align(x))
#else
#if defined(__GNUC__)
#define ALIGNED_(x) __attribute__ ((aligned(x)))
#endif  // __GNUC__
#endif  // _MSC_VER

#define MULTILINE(...) # __VA_ARGS__

// define below should be deleted. Leaving in commented out until code that
// refers to it has been corrected
// #define HSA_ARGUMENT_ALIGN_BYTES 16

/// If the provided agent is associated with a GPU, return that agent through
/// output parameter. This function is meant to be the call-back function used
/// with hsa_iterate_agents to find GPU agents.
/// \param[in] agent Agent to evaluate if GPU
/// \param[out] data If agent is associated with a GPU, this pointer will point
///  to the agent upon return
/// \returns HSA_STATUS_SUCCESS if no errors are encountered.
hsa_status_t FindGPUDevice(hsa_agent_t agent, void* data);

/// If the provided agent is associated with a CPU, return that agent through
/// output parameter. This function is meant to be the call-back function used
/// with hsa_iterate_agents to find CPU agents.
/// \param[in] agent Agent to evaluate if CPU
/// \param[out] data If agent is associated with a CPU, this pointer will point
///  to the agent upon return
/// \returns HSA_STATUS_SUCCESS if no errors are encountered.
hsa_status_t FindCPUDevice(hsa_agent_t agent, void* data);

// TODO(cfreehil): get rid of FindGlobalPool and replace with FindStandardPool
hsa_status_t FindGlobalPool(hsa_amd_memory_pool_t pool, void* data);

/// Find a "standard" pool. By this, we mean not a kernel args pool.
/// The pool found will have the following properties:
///     HSA_AMD_MEMORY_POOL_INFO_ACCESSIBLE_BY_ALL: Don't care
///     HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT: Off
///     HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED: Don't care
/// This function is meant to be the call-back function used
/// with hsa_amd_agent_iterate_memory_pools.
/// \param[in] pool Pool to evaluate for required properties
/// \param[in] data If pool meets criteria, this pointer will point
///  to the pool upon return
/// \returns hsa_status_t
///      -HSA_STATUS_INFO_BREAK - we found a pool that meets criteria
///      -HSA_STATUS_SUCCESS - we did not find a pool that meets the criteria
///      -else return an appropriate error code for any error encountered
hsa_status_t FindStandardPool(hsa_amd_memory_pool_t pool, void* data);

/// Find a "kernel arg" pool.
/// The pool found will have the following properties:
///     HSA_AMD_MEMORY_POOL_INFO_ACCESSIBLE_BY_ALL: Don't care
///     HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT: On
///     HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED: Don't care
/// This function is meant to be the call-back function used
/// with hsa_amd_agent_iterate_memory_pools.
/// \param[in] pool Pool to evaluate for required properties
/// \param[in] data If pool meets criteria, this pointer will point
///  to the pool upon return
/// \returns hsa_status_t
///      -HSA_STATUS_INFO_BREAK - we found a pool that meets criteria
///      -HSA_STATUS_SUCCESS - we did not find a pool that meets the criteria
///      -else return an appropriate error code for any error encountered
hsa_status_t FindKernArgPool(hsa_amd_memory_pool_t pool, void* data);

/// Dump information about provided memory pool to STDOUT
/// \param[in] pool Pool to gather and dump information for
/// \param[in] indent Number of spaces to indent output.
/// \returns hsa_status_t HSA_STATUS_SUCCESS if no errors
hsa_status_t DumpMemoryPoolInfo(const hsa_amd_memory_pool_t pool,
                                                         uint32_t indent = 0);

/// Dump information about a provided pointer to STDOUT.
/// \param[in] ptr Pointer about which information is dumped.
/// \returns HSA_STATUS_SUCCESS if there are no errors
hsa_status_t DumpPointerInfo(void* ptr);

/// This is a work-around for filling cpu-memory to be used until
/// hsa_amd_memory_fill is fixed. Should only be used for cpu memory.
/// \param[in] ptr Start address of memory to be filled.
/// \param[in] value Value to fill buffer with
/// \param[in] count Size of buffer to fill
/// \returns HSA_STATUS_SUCCESS if there are no errors
hsa_status_t hsa_memory_fill_workaround_cpu(void* ptr, uint32_t value,
                                                            size_t count);

/// This is a work-around for copying cpu-memory to be used until
/// hsa_amd_memory_copy is fixed. Should only be used for cpu memory.
/// \param[in] dst Destination address of memory to be copied
/// \param[in] src Source address of memory to be copied
/// \param[in] size Size of buffer to fill
/// \returns HSA_STATUS_SUCCESS if there are no errors
hsa_status_t hsa_memory_copy_workaround_cpu(void* dst, const void *src,
                                                            size_t size);

/// This is a work-around for copying memory to be used until
/// hsa_amd_memory_copy is fixed. Should be used when gpu local memory is
/// involved.
/// \param[in] dst Destination address of memory to be copied
/// \param[in] src Source address of memory to be copied
/// \param[in] size Size of buffer to fill
/// \param[in] dst_ag Destination agent handle
/// \param[in] src_ag Source agent handle
/// \returns HSA_STATUS_SUCCESS if there are no errors
hsa_status_t hsa_memory_copy_workaround_gen(void* dst, const void *src,
                       size_t size, hsa_agent_t dst_ag, hsa_agent_t src_ag);

}  // namespace rocrtst
#endif  // ROCRTST_COMMON_COMMON_H_
