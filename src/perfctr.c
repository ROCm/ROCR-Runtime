/*
 * Copyright © 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including
 * the next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include "libhsakmt.h"
#include "pmc_table.h"
#include "linux/kfd_ioctl.h"
#include <unistd.h>

#define BITS_PER_BYTE		CHAR_BIT

#define HSA_PERF_MAGIC4CC	0x54415348

enum perf_trace_state {
	PERF_TRACE_STATE__STOPPED = 0,
	PERF_TRACE_STATE__STARTED
};

struct perf_trace {
	uint32_t magic4cc;
	uint32_t  gpu_id;
	enum perf_trace_state state;
};

extern int amd_hsa_thunk_lock_fd;

static HsaCounterProperties **counter_props;
static unsigned int counter_props_count;

HSAKMT_STATUS init_counter_props(unsigned int NumNodes)
{
	counter_props = calloc(NumNodes, sizeof(struct HsaCounterProperties*));
	if (counter_props == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	counter_props_count = NumNodes;
	return HSAKMT_STATUS_SUCCESS;
}

void destroy_counter_props(void)
{
	unsigned int i;

	if (counter_props == NULL)
		return;

	for (i = 0; i<counter_props_count; i++)
		if (counter_props[i] != NULL) {
			free(counter_props[i]);
			counter_props[i] = NULL;
		}

	free(counter_props);
}

static int blockid2uuid(enum perf_block_id block_id, HSA_UUID *uuid)
{
	int rc = 0;
	switch (block_id) {
	case PERFCOUNTER_BLOCKID__SQ:
		*uuid = HSA_PROFILEBLOCK_AMD_SQ;
		break;
	default:
		/* If we reach this point, it's a bug */
		rc = -1;
	}

	return rc;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcGetCounterProperties(
	HSAuint32                   NodeId,             //IN
	HsaCounterProperties**      CounterProperties   //OUT
	)
{
	HSAKMT_STATUS rc = HSAKMT_STATUS_SUCCESS;
	uint32_t gpu_id, i, block_id;
	uint16_t dev_id;
	uint32_t counter_props_size = 0;
	uint32_t total_counters = 0;
	uint32_t total_concurrent = 0;
	struct perf_counter_block block = {0};

	if (counter_props == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	if (CounterProperties == NULL)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if (validate_nodeid(NodeId, &gpu_id) != 0)
		return HSAKMT_STATUS_INVALID_NODE_UNIT;

	if (counter_props[NodeId] == NULL) {
		dev_id = get_device_id_by_node(NodeId);
		for (i = 0; i < PERFCOUNTER_BLOCKID__MAX; i++) {
			rc = get_block_properties(dev_id, i, &block);
			if (rc != HSAKMT_STATUS_SUCCESS)
				return rc;
			total_concurrent += block.num_of_slots;
			total_counters += block.num_of_counters;
		}

		counter_props_size = sizeof(HsaCounterProperties) +
		sizeof(HsaCounterBlockProperties)*(PERFCOUNTER_BLOCKID__MAX-1) +
		sizeof(HsaCounter)*(total_counters-1);

		counter_props[NodeId] = malloc(counter_props_size);

		if (counter_props[NodeId] == NULL)
			return HSAKMT_STATUS_NO_MEMORY;

		counter_props[NodeId]->NumBlocks = PERFCOUNTER_BLOCKID__MAX;
		counter_props[NodeId]->NumConcurrent = total_concurrent;

		for (block_id = 0; block_id < PERFCOUNTER_BLOCKID__MAX; block_id++)
		{
			rc = get_block_properties(dev_id, block_id, &block);
			if (rc != HSAKMT_STATUS_SUCCESS) {
				free(counter_props[NodeId]);
				return rc;
			}

			/* Filling the SQ block */
			blockid2uuid(block_id, &counter_props[NodeId]->Blocks[block_id].BlockId);
			counter_props[NodeId]->Blocks[block_id].NumCounters = block.num_of_counters;
			counter_props[NodeId]->Blocks[block_id].NumConcurrent = block.num_of_slots;

			for (i = 0; i < block.num_of_counters; i++) {
				counter_props[NodeId]->Blocks[block_id].Counters[i].BlockIndex = block_id;
				counter_props[NodeId]->Blocks[block_id].Counters[i].CounterId = block.counter_ids[i];
				counter_props[NodeId]->Blocks[block_id].Counters[i].CounterSizeInBits = block.counter_size_in_bits;
				counter_props[NodeId]->Blocks[block_id].Counters[i].CounterMask = block.counter_mask;
				counter_props[NodeId]->Blocks[block_id].Counters[i].Flags.ui32.Global = 1;
				counter_props[NodeId]->Blocks[block_id].Counters[i].Type = HSA_PROFILE_TYPE_NONPRIV_IMMEDIATE;
			}
		}
	}

	*CounterProperties = counter_props[NodeId];

	return HSAKMT_STATUS_SUCCESS;
}

/**
  Registers a set of (HW) counters to be used for tracing/profiling
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcRegisterTrace(
	HSAuint32           NodeId,             //IN
	HSAuint32           NumberOfCounters,   //IN
	HsaCounter*         Counters,           //IN
	HsaPmcTraceRoot*    TraceRoot           //OUT
	)
{
	uint32_t gpu_id, i;
	uint64_t min_buf_size = 0;
	uint32_t concurrent_counters[PERFCOUNTER_BLOCKID__MAX] = {0};
	struct perf_trace *trace = NULL;

	if (counter_props == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	if (Counters == NULL || TraceRoot == NULL || NumberOfCounters == 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if (validate_nodeid(NodeId, &gpu_id) != 0)
		return HSAKMT_STATUS_INVALID_NODE_UNIT;

	/* Calculating the minimum buffer size */
	for (i = 0; i < NumberOfCounters; i++) {
		if (Counters[i].BlockIndex >= PERFCOUNTER_BLOCKID__MAX)
			return HSAKMT_STATUS_INVALID_PARAMETER;
		min_buf_size += Counters[i].CounterSizeInBits/BITS_PER_BYTE;
		concurrent_counters[Counters[i].BlockIndex]++;
	}

	/* Verifying that the number of counters per block is not larger than the amount of slots */
	if (concurrent_counters[PERFCOUNTER_BLOCKID__SQ] > counter_props[NodeId]->Blocks[PERFCOUNTER_BLOCKID__SQ].NumConcurrent)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	trace = malloc(sizeof(trace));
	if (trace == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	trace->magic4cc = HSA_PERF_MAGIC4CC;
	trace->gpu_id = gpu_id;
	trace->state = PERF_TRACE_STATE__STOPPED;

	TraceRoot->NumberOfPasses = 1;
	TraceRoot->TraceBufferMinSizeBytes = PAGE_ALIGN_UP(min_buf_size);
	TraceRoot->TraceId = PORT_VPTR_TO_UINT64(trace);

	return HSAKMT_STATUS_SUCCESS;
}

/**
  Unregisters a set of (HW) counters used for tracing/profiling
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcUnregisterTrace(
	HSAuint32   NodeId,     //IN
	HSATraceId  TraceId     //IN
	)
{
	uint32_t gpu_id;
	struct perf_trace *trace;

	if (TraceId == 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if (validate_nodeid(NodeId, &gpu_id) != 0)
		return HSAKMT_STATUS_INVALID_NODE_UNIT;

	trace = (struct perf_trace *)PORT_UINT64_TO_VPTR(TraceId);

	if (trace->magic4cc != HSA_PERF_MAGIC4CC)
		return HSAKMT_STATUS_INVALID_HANDLE;

	if (trace->gpu_id != gpu_id)
		return HSAKMT_STATUS_INVALID_NODE_UNIT;

	/* If the trace is in the running state, stop it */
	if (trace->state == PERF_TRACE_STATE__STARTED) {
		HSAKMT_STATUS status = hsaKmtPmcStopTrace(TraceId);
		if (status != HSAKMT_STATUS_SUCCESS)
			return status;
	}

	free(trace);

	return HSAKMT_STATUS_SUCCESS;
}


/**
  Allows a user mode process to get exclusive access to the defined set of (HW) counters
  used for tracing/profiling
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcAcquireTraceAccess(
	HSAuint32   NodeId,     //IN
	HSATraceId  TraceId     //IN
	)
{
	struct perf_trace *trace;

	if (TraceId == 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	trace = (struct perf_trace *)PORT_UINT64_TO_VPTR(TraceId);

	if (trace->magic4cc != HSA_PERF_MAGIC4CC)
		return HSAKMT_STATUS_INVALID_HANDLE;

	if (amd_hsa_thunk_lock_fd > 0) {
	if (lockf( amd_hsa_thunk_lock_fd, F_TLOCK, 0 ) != 0)
		return HSAKMT_STATUS_ERROR;
	else
	   return HSAKMT_STATUS_SUCCESS;
	}
	else {
		return HSAKMT_STATUS_ERROR;
	}
}


/**
  Allows a user mode process to release exclusive access to the defined set of (HW) counters
  used for tracing/profiling
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcReleaseTraceAccess(
	HSAuint32   NodeId,     //IN
	HSATraceId  TraceId     //IN
	)
{
	struct perf_trace *trace;

	if (TraceId == 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	trace = (struct perf_trace *)PORT_UINT64_TO_VPTR(TraceId);

	if (trace->magic4cc != HSA_PERF_MAGIC4CC)
		return HSAKMT_STATUS_INVALID_HANDLE;

	if (amd_hsa_thunk_lock_fd > 0) {
	if (lockf( amd_hsa_thunk_lock_fd, F_ULOCK, 0 ) != 0)
		return HSAKMT_STATUS_ERROR;
	else
	   return HSAKMT_STATUS_SUCCESS;
	}
	else {
		return HSAKMT_STATUS_ERROR;
	}

}


/**
  Starts tracing operation on a previously established set of performance counters
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcStartTrace(
	HSATraceId  TraceId,                //IN
	void*       TraceBuffer,            //IN (page aligned)
	HSAuint64   TraceBufferSizeBytes    //IN (page aligned)
	)
{
	struct perf_trace *trace = (struct perf_trace *)PORT_UINT64_TO_VPTR(TraceId);

	if (TraceId == 0 || TraceBuffer == NULL || TraceBufferSizeBytes == 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if (trace->magic4cc != HSA_PERF_MAGIC4CC)
		return HSAKMT_STATUS_INVALID_HANDLE;

	trace->state = PERF_TRACE_STATE__STARTED;

	return HSAKMT_STATUS_SUCCESS;
}


/**
   Forces an update of all the counters that a previously started trace operation has registered
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcQueryTrace(
	HSATraceId    TraceId   //IN
	)
{
	struct perf_trace *trace = (struct perf_trace *)PORT_UINT64_TO_VPTR(TraceId);

	if (TraceId == 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if (trace->magic4cc != HSA_PERF_MAGIC4CC)
		return HSAKMT_STATUS_INVALID_HANDLE;

	return HSAKMT_STATUS_SUCCESS;
}


/**
  Stops tracing operation on a previously established set of performance counters
*/

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcStopTrace(
	HSATraceId  TraceId     //IN
	)
{
	struct perf_trace *trace = (struct perf_trace *)PORT_UINT64_TO_VPTR(TraceId);

	if (TraceId == 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if (trace->magic4cc != HSA_PERF_MAGIC4CC)
		return HSAKMT_STATUS_INVALID_HANDLE;

	trace->state = PERF_TRACE_STATE__STOPPED;

	return HSAKMT_STATUS_SUCCESS;
}
