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

#include "libhsakmt.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include "linux/kfd_ioctl.h"
#include "fmm.h"

static HSAuint64 *events_page = NULL;

void clear_events_page(void)
{
	events_page = NULL;
}

static bool IsSystemEventType(HSA_EVENTTYPE type)
{
	// Debug events behave as signal events.
	return (type != HSA_EVENTTYPE_SIGNAL && type != HSA_EVENTTYPE_DEBUG_EVENT);
}

HSAKMT_STATUS HSAKMTAPI hsaKmtCreateEvent(HsaEventDescriptor *EventDesc,
					  bool ManualReset, bool IsSignaled,
					  HsaEvent **Event)
{
	unsigned int event_limit = KFD_SIGNAL_EVENT_LIMIT;

	CHECK_KFD_OPEN();

	if (EventDesc->EventType >= HSA_EVENTTYPE_MAXID)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	HsaEvent *e = malloc(sizeof(HsaEvent));

	if (!e)
		return HSAKMT_STATUS_ERROR;

	memset(e, 0, sizeof(*e));

	struct kfd_ioctl_create_event_args args;

	memset(&args, 0, sizeof(args));

	args.event_type = EventDesc->EventType;
	args.node_id = EventDesc->NodeId;
	args.auto_reset = !ManualReset;

	/* dGPU code */
	pthread_mutex_lock(&hsakmt_mutex);

	if (is_dgpu && !events_page) {
		events_page = allocate_exec_aligned_memory_gpu(
			KFD_SIGNAL_EVENT_LIMIT * 8, PAGE_SIZE, 0, true);
		if (!events_page) {
			pthread_mutex_unlock(&hsakmt_mutex);
			return HSAKMT_STATUS_ERROR;
		}
		fmm_get_handle(events_page, &args.event_page_offset);
	}

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_CREATE_EVENT, &args) != 0) {
		free(e);
		*Event = NULL;
		pthread_mutex_unlock(&hsakmt_mutex);
		return HSAKMT_STATUS_ERROR;
	}

	e->EventId = args.event_id;

	if (!events_page && args.event_page_offset > 0) {
		events_page = mmap(NULL, event_limit * 8, PROT_WRITE | PROT_READ,
				MAP_SHARED, kfd_fd, args.event_page_offset);
		if (events_page == MAP_FAILED) {
			/* old kernels only support 256 events */
			event_limit = 256;
			events_page = mmap(NULL, PAGE_SIZE, PROT_WRITE | PROT_READ,
					   MAP_SHARED, kfd_fd, args.event_page_offset);
		}
		if (events_page == MAP_FAILED) {
			events_page = NULL;
			pthread_mutex_unlock(&hsakmt_mutex);
			hsaKmtDestroyEvent(e);
			return HSAKMT_STATUS_ERROR;
		}
	}

	pthread_mutex_unlock(&hsakmt_mutex);

	if (args.event_page_offset > 0 && args.event_slot_index < event_limit)
		e->EventData.HWData2 = (HSAuint64)&events_page[args.event_slot_index];

	e->EventData.EventType = EventDesc->EventType;
	e->EventData.HWData1 = args.event_id;

	e->EventData.HWData3 = args.event_trigger_data;
	e->EventData.EventData.SyncVar.SyncVar.UserData =
		EventDesc->SyncVar.SyncVar.UserData;
	e->EventData.EventData.SyncVar.SyncVarSize =
		EventDesc->SyncVar.SyncVarSize;

	if (IsSignaled && !IsSystemEventType(e->EventData.EventType)) {
		struct kfd_ioctl_set_event_args set_args;

		memset(&set_args, 0, sizeof(set_args));
		set_args.event_id = args.event_id;

		kmtIoctl(kfd_fd, AMDKFD_IOC_SET_EVENT, &set_args);
	}

	*Event = e;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtDestroyEvent(HsaEvent *Event)
{
	CHECK_KFD_OPEN();

	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	struct kfd_ioctl_destroy_event_args args;

	memset(&args, 0, sizeof(args));

	args.event_id = Event->EventId;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_DESTROY_EVENT, &args) != 0)
		return HSAKMT_STATUS_ERROR;

	free(Event);
	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtSetEvent(HsaEvent *Event)
{
	CHECK_KFD_OPEN();

	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	/* Although the spec is doesn't say, don't allow system-defined events
	 * to be signaled.
	 */
	if (IsSystemEventType(Event->EventData.EventType))
		return HSAKMT_STATUS_ERROR;

	struct kfd_ioctl_set_event_args args;

	memset(&args, 0, sizeof(args));

	args.event_id = Event->EventId;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_SET_EVENT, &args) == -1)
		return HSAKMT_STATUS_ERROR;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtResetEvent(HsaEvent *Event)
{
	CHECK_KFD_OPEN();

	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	/* Although the spec is doesn't say, don't allow system-defined events
	 * to be signaled.
	 */
	if (IsSystemEventType(Event->EventData.EventType))
		return HSAKMT_STATUS_ERROR;

	struct kfd_ioctl_reset_event_args args;

	memset(&args, 0, sizeof(args));

	args.event_id = Event->EventId;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_RESET_EVENT, &args) == -1)
		return HSAKMT_STATUS_ERROR;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtQueryEventState(HsaEvent *Event)
{
	CHECK_KFD_OPEN();

	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtWaitOnEvent(HsaEvent *Event,
		HSAuint32 Milliseconds)
{
	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	return hsaKmtWaitOnMultipleEvents(&Event, 1, true, Milliseconds);
}

//Analysis memory exception data, print debug messages
static void analysis_memory_exception(struct kfd_hsa_memory_exception_data *
						memory_exception_data)
{
	HSAKMT_STATUS ret;
	HsaPointerInfo info;
	const uint64_t addr = memory_exception_data->va;
	uint32_t node_id = 0;
	unsigned int i;

	gpuid_to_nodeid(memory_exception_data->gpu_id, &node_id);
	pr_err("Memory exception on virtual address 0x%lx, ", addr);
	pr_err("node id %d : ", node_id);
	if (memory_exception_data->failure.NotPresent)
		pr_err("Page not present\n");
	else if (memory_exception_data->failure.ReadOnly)
		pr_err("Writing to readonly page\n");
	else if (memory_exception_data->failure.NoExecute)
		pr_err("Execute to none-executable page\n");

	ret = fmm_get_mem_info((const void *)addr, &info);

	if (ret != HSAKMT_STATUS_SUCCESS) {
		pr_err("Address does not belong to a known buffer\n");
		return;
	}

	pr_err("GPU address 0x%lx, node id %d, size in byte 0x%lx\n",
			info.GPUAddress, info.Node, info.SizeInBytes);
	switch (info.Type) {
	case HSA_POINTER_REGISTERED_GRAPHICS:
		pr_err("Memory is registered graphics buffer\n");
		break;
	case HSA_POINTER_REGISTERED_USER:
		pr_err("Memory is registered user pointer\n");
		pr_err("CPU address of the memory is %p\n", info.CPUAddress);
		break;
	case HSA_POINTER_ALLOCATED:
		pr_err("Memory is allocated using hsaKmtAllocMemory\n");
		pr_err("CPU address of the memory is %p\n", info.CPUAddress);
		break;
	default:
		pr_err("Invalid memory type %d\n", info.Type);
		break;
	}

	if (info.RegisteredNodes) {
		pr_err("Memory is registered to node id: ");
		for (i = 0; i < info.NRegisteredNodes; i++)
			pr_err("%d ", info.RegisteredNodes[i]);
	}
	pr_err("\n");
	if (info.MappedNodes) {
		pr_err("Memory is mapped to node id: ");
		for (i = 0; i < info.NMappedNodes; i++)
			pr_err("%d ", info.MappedNodes[i]);
	}
	pr_err("\n");
}

HSAKMT_STATUS HSAKMTAPI hsaKmtWaitOnMultipleEvents(HsaEvent *Events[],
						   HSAuint32 NumEvents,
						   bool WaitOnAll,
						   HSAuint32 Milliseconds)
{
	CHECK_KFD_OPEN();

	if (!Events)
		return HSAKMT_STATUS_INVALID_HANDLE;

	struct kfd_event_data *event_data = calloc(NumEvents, sizeof(struct kfd_event_data));

	for (HSAuint32 i = 0; i < NumEvents; i++) {
		event_data[i].event_id = Events[i]->EventId;
		event_data[i].kfd_event_data_ext = (uint64_t)(uintptr_t)NULL;
	}

	struct kfd_ioctl_wait_events_args args;

	memset(&args, 0, sizeof(args));

	args.wait_for_all = WaitOnAll;
	args.timeout = Milliseconds;
	args.num_events = NumEvents;
	args.events_ptr = (uint64_t)(uintptr_t)event_data;

	HSAKMT_STATUS result;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_WAIT_EVENTS, &args) == -1)
		result = HSAKMT_STATUS_ERROR;
	else if (args.wait_result == KFD_IOC_WAIT_RESULT_TIMEOUT)
		result = HSAKMT_STATUS_WAIT_TIMEOUT;
	else {
		result = HSAKMT_STATUS_SUCCESS;
		for (HSAuint32 i = 0; i < NumEvents; i++) {
			if (Events[i]->EventData.EventType == HSA_EVENTTYPE_MEMORY) {
				Events[i]->EventData.EventData.MemoryAccessFault.VirtualAddress = event_data[i].memory_exception_data.va;
				result = gpuid_to_nodeid(event_data[i].memory_exception_data.gpu_id, &Events[i]->EventData.EventData.MemoryAccessFault.NodeId);
				if (result != HSAKMT_STATUS_SUCCESS)
					goto out;
				Events[i]->EventData.EventData.MemoryAccessFault.Failure.NotPresent = event_data[i].memory_exception_data.failure.NotPresent;
				Events[i]->EventData.EventData.MemoryAccessFault.Failure.ReadOnly = event_data[i].memory_exception_data.failure.ReadOnly;
				Events[i]->EventData.EventData.MemoryAccessFault.Failure.NoExecute = event_data[i].memory_exception_data.failure.NoExecute;
				Events[i]->EventData.EventData.MemoryAccessFault.Failure.Imprecise = event_data[i].memory_exception_data.failure.imprecise;
				Events[i]->EventData.EventData.MemoryAccessFault.Flags = HSA_EVENTID_MEMORY_FATAL_PROCESS;
				analysis_memory_exception(&event_data[i].memory_exception_data);
			}
		}
	}
out:
	free(event_data);

	return result;
}
