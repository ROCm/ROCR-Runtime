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

// HSA AMD extension.

#ifndef HSA_RUNTIME_EXT_AMD_H_
#define HSA_RUNTIME_EXT_AMD_H_

#include "hsa.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Agent attributes.
 */
typedef enum hsa_amd_agent_info_s {
  /**
   * Chip identifier. The type of this attribute is uint32_t.
   */
  HSA_AMD_AGENT_INFO_CHIP_ID = 0xA000,
  /**
   * Size of a cacheline in bytes. The type of this attribute is uint32_t.
   */
  HSA_AMD_AGENT_INFO_CACHELINE_SIZE = 0xA001,
  /**
   * The number of compute unit available in the agent. The type of this
   * attribute is uint32_t.
   */
  HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT = 0xA002,
  /**
   * The maximum clock frequency of the agent in MHz. The type of this
   * attribute is uint32_t.
   */
  HSA_AMD_AGENT_INFO_MAX_CLOCK_FREQUENCY = 0xA003,
  /**
   * Internal driver node identifier. The type of this attribute is uint32_t.
   */
  HSA_AMD_AGENT_INFO_DRIVER_NODE_ID = 0xA004,
  /**
   * Max number of watch points on memory address ranges to generate exception
   * events when the watched addresses are accessed.
   */
  HSA_AMD_AGENT_INFO_MAX_ADDRESS_WATCH_POINTS = 0xA005,
  /**
   * Agent BDF_ID, named LocationID in thunk. The type of this attribute is 
   * uint32_t.
   */
   HSA_AMD_AGENT_INFO_BDFID = 0xA006
} hsa_amd_agent_info_t;

/**
 * @brief Region attributes.
 */
typedef enum hsa_amd_region_info_s {
  /**
   * Determine if host can access the region. The type of this attribute
   * is bool.
   */
  HSA_AMD_REGION_INFO_HOST_ACCESSIBLE = 0xA000,
  /**
   * Base address of the region in flat address space.
   */
  HSA_AMD_REGION_INFO_BASE = 0xA001
} hsa_amd_region_info_t;

/**
 * @brief Coherency attributes of fine grain region.
 */
typedef enum hsa_amd_coherency_type_s {
  /**
   * Coherent region.
   */
  HSA_AMD_COHERENCY_TYPE_COHERENT = 0,
  /**
   * Non coherent region.
   */
  HSA_AMD_COHERENCY_TYPE_NONCOHERENT = 1
} hsa_amd_coherency_type_t;

/**
* @brief Get the coherency type of the fine grain region of an agent.
*
* @param[in] agent A valid agent.
*
* @param[out] type Pointer to a memory location where the HSA runtime will
* store the coherency type of the fine grain region.
*
* @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
*
* @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
* initialized.
*
* @retval ::HSA_STATUS_ERROR_INVALID_AGENT The agent is invalid.
*
* @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT @p type is NULL.
*/
hsa_status_t HSA_API
hsa_amd_coherency_get_type(hsa_agent_t agent, hsa_amd_coherency_type_t* type);

/**
* @brief Set the coherency type of the fine grain region of an agent.
*
* @param[in] agent A valid agent.
*
* @param[in] type The coherency type to be set.
*
* @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
*
* @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
* initialized.
*
* @retval ::HSA_STATUS_ERROR_INVALID_AGENT The agent is invalid.
*
* @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT @p type is invalid.
*/
hsa_status_t HSA_API
hsa_amd_coherency_set_type(hsa_agent_t agent, hsa_amd_coherency_type_t type);

/**
 * @brief Structure containing profiling dispatch time information.
 * 
 * Times are reported as ticks in the domain of the HSA system clock.
 * The HSA system clock tick and frequency is obtained via hsa_system_get_info.
 */
typedef struct hsa_amd_profiling_dispatch_time_s {
  /**
   * Dispatch packet processing start time.
   */
  uint64_t start;
  /**
   * Dispatch packet completion time.
   */
  uint64_t end;
} hsa_amd_profiling_dispatch_time_t;

/**
* @brief Enable or disable profiling capability of a queue.
*
* @param[in] queue A valid queue.
*
* @param[in] enable 1 to enable profiling. 0 to disable profiling.
*
* @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
*
* @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
* initialized.
*
* @retval ::HSA_STATUS_ERROR_INVALID_QUEUE The queue is invalid.
*
* @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT @p queue is NULL.
*/
hsa_status_t HSA_API
hsa_amd_profiling_set_profiler_enabled(hsa_queue_t* queue, int enable);

/**
* @brief Retrieve packet processing time stamps.
*
* @param[in] agent The agent with which the signal was last used.  For instance,
* if the profiled dispatch packet is dispatched on to queue Q, which was
* created on agent A, then this parameter must be A.
*
* @param[in] signal A signal used as the completion signal of the dispatch
* packet to retrieve time stamps from.  This dispatch packet must have been
* issued to a queue with profiling enabled and have already completed.  Also
* the signal must not have yet been used in any other packet following the
* completion of the profiled dispatch packet.
*
* @param[out] time Packet processing timestamps in the HSA system clock
* domain.
*
* @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
*
* @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
* initialized.
*
* @retval ::HSA_STATUS_ERROR_INVALID_AGENT The agent is invalid.
*
* @retval ::HSA_STATUS_ERROR_INVALID_SIGNAL The signal is invalid.
*
* @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT @p time is NULL.
*/
hsa_status_t HSA_API
hsa_amd_profiling_get_dispatch_time(hsa_agent_t agent, hsa_signal_t signal,
                                    hsa_amd_profiling_dispatch_time_t* time);

/**
 * @brief Computes the frequency ratio and offset between the agent clock and HSA system
 * clock and converts the agent’s tick to HSA system domain tick
 * 
 * @param[in] agent The agent used to retrieve the agent_tick. It is user's responsibility
 * to make sure the tick number is from this agent, otherwise, the behavior is undefined.
 *
 * @param[in] agent_tick The tick count retrieved from the specified @p agent.
 *
 * @param[out] system_tick The translated HSA system domain clock counter tick.
 *
 * @retval ::HSA_STUTUS_SUCCESS The function has been executed successfully.
 *
 * @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been initialized.
 *
 * @retval ::HSA_STATUS_ERROR_INVALID_AGENT The agent is invalid.
 *
 * @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT @p system_tick is NULL;
 */
hsa_status_t HSA_API
hsa_amd_profiling_convert_tick_to_system_domain(hsa_agent_t agent, uint64_t agent_tick,
                                                uint64_t* system_tick);

/**
 * @brief Asyncronous signal handler function type.
 *
 * @details Type definition of callback function to be used with
 * hsa_amd_signal_async_handler. This callback is invoked if the associated
 * signal and condition are met. The callback receives the value of the signal
 * which satisfied the associated wait condition and a user provided value. If
 * the callback returns true then the callback will be called again if the
 * associated signal and condition are satisfied again. If the callback returns
 * false then it will not be called again.
 *
 * @param[in] value Contains the value of the signal observed by
 * hsa_amd_signal_async_handler which caused the signal handler to be invoked.
 *
 * @param[in] arg Contains the user provided value given when the signal handler
 * was registered with hsa_amd_signal_async_handler
 *
 * @retval true resumes monitoring the signal with this handler (as if calling
 * hsa_amd_signal_async_handler again with identical parameters)
 *
 * @retval false stops monitoring the signal with this handler (handler will
 * not be called again for this signal)
 *
 */
typedef bool (*hsa_amd_signal_handler)(hsa_signal_value_t value, void* arg);

/**
 * @brief Register asynchronous signal handler function.
 *
 * @details Allows registering a callback function and user provided value with
 * a signal and wait condition. The callback will be invoked if the associated
 * signal and wait condition are satisfied. Callbacks will be invoked serially
 * but in an arbitrary order so callbacks should be independent of each other.
 * After being invoked a callback may continue to wait for its associated signal
 * and condition and, possibly, be invoked again. Or the callback may stop
 * waiting. If the callback returns true then it will continue waiting and may
 * be called again. If false then the callback will not wait again and will not
 * be called again for the associated signal and condition. It is possible to
 * register the same callback multiple times with the same or different signals
 * and/or conditions. Each registration of the callback will be treated entirely
 * independently.
 *
 * @param[in] signal hsa signal to be asynchronously monitored
 *
 * @param[in] cond condition value to monitor for
 *
 * @param[in] value signal value used in condition expression
 *
 * @param[in] handler asynchronous signal handler invoked when signal's
 * condition is met
 *
 * @param[in] arg user provided value which is provided to handler when handler
 * is invoked
 *
 * @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
 *
 * @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
 * initialized.
 *
 * @retval ::HSA_STATUS_ERROR_INVALID_SIGNAL signal is not a valid hsa_signal_t
 *
 * @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT handler is invalid (NULL)
 *
 * @retval ::HSA_STATUS_ERROR_OUT_OF_RESOURCES The HSA runtime is out of
 * resources or blocking signals are not supported by the HSA driver component.
 *
 */
hsa_status_t HSA_API
hsa_amd_signal_async_handler(hsa_signal_t signal, hsa_signal_condition_t cond,
                             hsa_signal_value_t value,
                             hsa_amd_signal_handler handler, void* arg);

/**
 * @brief Wait for any signal-condition pair to be satisfied.
 *
 * @details Allows waiting for any of several signal and conditions pairs to be
 * satisfied. The function returns the index into the list of signals of the
 * first satisfying signal-condition pair. The value of the satisfying signal’s
 * value is returned in satisfying_value unless satisfying_value is NULL. This
 * function provides only relaxed memory semantics.
 */
uint32_t HSA_API
hsa_amd_signal_wait_any(uint32_t signal_count, hsa_signal_t* signals,
                        hsa_signal_condition_t* conds,
                        hsa_signal_value_t* values, uint64_t timeout_hint,
                        hsa_wait_state_t wait_hint,
                        hsa_signal_value_t* satisfying_value);

/**
 * @brief Query image limits.
 *
 * @param[in] agent A valid agent.
 *
 * @param[in] attribute HSA image info attribute to query.
 *
 * @param[out] value Pointer to an application-allocated buffer where to store
 * the value of the attribute. If the buffer passed by the application is not
 * large enough to hold the value of @p attribute, the behavior is undefined.
 *
 * @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
 *
 * @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
 * initialized.
 *
 * @retval ::HSA_STATUS_ERROR_INVALID_QUEUE @p value is NULL or @p attribute <
 * HSA_EXT_AGENT_INFO_IMAGE_1D_MAX_ELEMENTS or @p attribute >
 * HSA_EXT_AGENT_INFO_IMAGE_ARRAY_MAX_LAYERS.
 *
 */
hsa_status_t HSA_API hsa_amd_image_get_info_max_dim(hsa_agent_t agent,
                                                    hsa_agent_info_t attribute,
                                                    void* value);

/**
 * @brief Set a CU affinity to specific queues within the process, this function
 * call is "atomic".
 *
 * @param[in] queue A pointer to HSA queue.
 *
 * @param[in] num_cu_mask_count Size of CUMask bit array passed in.
 *
 * @param[in] cu_mask Bit-vector representing the CU mask.
 *
 * @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
 *
 * @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
 * initialized.
 *
 * @retval ::HSA_STATUS_ERROR_INVALID_QUEUE @p queue is NULL or invalid.
 *
 * @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT @p num_cu_mask_count is not
 * multiple of 32 or @p cu_mask is NULL.
 *
 * @retval ::HSA_STATUS_ERROR failed to call thunk api
 *
 */
hsa_status_t HSA_API hsa_amd_queue_cu_set_mask(const hsa_queue_t* queue,
                                               uint32_t num_cu_mask_count,
                                               const uint32_t* cu_mask);

/**
* @brief Sets the first @p num of uint32_t of the block of memory pointed by
* @p ptr to the specified @p value.
*
* @param[in] ptr Pointer to the block of memory to fill.
*
* @param[in] value Value to be set.
*
* @param[in] count Number of uint32_t element to be set to the value.
*
* @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
*
* @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
* initialized.
*
* @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT @p ptr is NULL or
* not 4 bytes aligned
*
*/
hsa_status_t HSA_API
    hsa_amd_memory_fill(void* ptr, uint32_t value, size_t count);

/**
* @brief Non blocking copy a block of memory.
*
* @param[out] dst Buffer where the content is to be copied.
*
* @param[in] src A valid pointer to the source of data to be copied.
*
* @param[in] size Number of bytes to copy. If @p size is 0, no copy is
* performed and the function returns success. Copying a number of bytes larger
* than the size of the buffers pointed by @p dst or @p src results in undefined
* behavior.
*
* @param[in] copy_agent The agent that will be used to perform DMA operation.
*
* @param[in] num_dep_signals The number of signal to wait before copying.
*
* @param[in] dep_signals Array of signals to wait.
*
* @param[in] completion_signal Signal handle provided by the client to track the
copy
* completion.
*
* @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
*
* @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
* initialized.
*
* @retval ::HSA_STATUS_ERROR_OUT_OF_RESOURCES There is a failure in
* allocating the necessary resources.
*
* @retval ::HSA_STATUS_ERROR_INVALID_AGENT The @p copy_agent is not a
  valid agent.
* @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT The source or destination
* pointers are NULL. The @p dep_signal_count is 0 but @p dep_signal is not
* NULL or @p dep_signal_count is not 0 but @p dep_signal is NULL.
* @retval ::HSA_STATUS_ERROR_INVALID_SIGNAL @p dep_signal
* contain an invalid signal handle. @p out_signal is an
* invalid signal handle.
*/
hsa_status_t HSA_API
    hsa_amd_memory_async_copy(void* dst, const void* src, size_t size,
                              hsa_agent_t copy_agent, uint32_t num_dep_signals,
                              const hsa_signal_t* dep_signals,
                              hsa_signal_t completion_signal);

/**
 *
 * @brief Pin a host pointer allocated by C/C++ or OS allocator and return a new
 * pointer accessible by the @pagents.
 *
 * @param[in] host_ptr A buffer allocated by C/C++ or OS allocator. The address
 * of the host pointer needs to be aligned with the
 * HSA_AMD_AGENT_INFO_CACHELINE_SIZE property of each agent in@p agents.
 *
 * @param[in] size The size to be locked.
 *
 * @param[in] agents Array of agent handle to gain access to the @p host_ptr.
 * If this parameter is NULL and the @p num_agent is 0, all agents
 * in the platform will gain access to the @p host_ptr.
 *
 * @param[out] agent_ptr Pointer to the location where to store the new address.
 *
 * @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
 *
 * @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
 * initialized.
 *
 * @retval ::HSA_STATUS_ERROR_OUT_OF_RESOURCES There is a failure in
 * allocating the necessary resources.
 *
 * @retval ::HSA_STATUS_ERROR_INVALID_AGENT One or more agent in @p agents is
 * invalid.
 *
 * @retval ::HSA_STATUS_ERROR_INVALID_ARGUMENT @p size is 0 or @p host_ptr or
 * @ agent_ptr is NULL or @p agents not NULL but @p num_agent is 0 or @p agents
 * is NULL but @p num_agent is not 0.
 */

hsa_status_t HSA_API hsa_amd_memory_lock(void* host_ptr, size_t size,
                                 hsa_agent_t* agents, int num_agent,
                                 void** agent_ptr);

/**
 *
 * @brief Unpin the host pointer previously pinned via ::hsa_amd_memory_lock.
 *
 * @details The behavior is undefined if the host pointer being unpinned does not
 * match previous pinned address.
 *
 * @param[in] host_ptr A buffer allocated by C/C++ or OS allocator that was
 * pinned previously via ::hsa_amd_memory_lock.
 *
 * @retval ::HSA_STATUS_SUCCESS The function has been executed successfully.
 *
 * @retval ::HSA_STATUS_ERROR_NOT_INITIALIZED The HSA runtime has not been
 * initialized.
 */
hsa_status_t HSA_API hsa_amd_memory_unlock(void* host_ptr);

#ifdef __cplusplus
}  // end extern "C" block
#endif

#endif  // header guard
