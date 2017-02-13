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

HSAKMT_STATUS
HSAKMTAPI
hsaKmtCreateEvent(
    HsaEventDescriptor* EventDesc,              //IN
    bool                ManualReset,            //IN
    bool                IsSignaled,             //IN
    HsaEvent**          Event                   //OUT
    )
{
	CHECK_KFD_OPEN();

	if (EventDesc->EventType >= HSA_EVENTTYPE_MAXID)
	{
		return HSAKMT_STATUS_INVALID_PARAMETER;
	}

	HsaEvent* e = malloc(sizeof(HsaEvent));
	if (e == NULL)
	{
		return HSAKMT_STATUS_ERROR;
	}

	memset(e, 0, sizeof(*e));

	struct kfd_ioctl_create_event_args args;
	memset(&args, 0, sizeof(args));

	args.event_type = EventDesc->EventType;
	args.node_id = EventDesc->NodeId;
	args.auto_reset = !ManualReset;

	/* dGPU code */
	pthread_mutex_lock(&hsakmt_mutex);

	if (is_dgpu && events_page == NULL) {
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

	if (events_page == NULL && args.event_page_offset > 0) {
		events_page = mmap(NULL, KFD_SIGNAL_EVENT_LIMIT * 8, PROT_WRITE | PROT_READ,
				MAP_SHARED, kfd_fd, args.event_page_offset);
		if (events_page == MAP_FAILED) {
			events_page = NULL;
			pthread_mutex_unlock(&hsakmt_mutex);
			hsaKmtDestroyEvent(e);
			return HSAKMT_STATUS_ERROR;
		}
	}

	pthread_mutex_unlock(&hsakmt_mutex);

	if (args.event_page_offset > 0 && args.event_slot_index < KFD_SIGNAL_EVENT_LIMIT)
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

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDestroyEvent(
    HsaEvent*   Event    //IN
    )
{
	CHECK_KFD_OPEN();

	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	struct kfd_ioctl_destroy_event_args args;
	memset(&args, 0, sizeof(args));

	args.event_id = Event->EventId;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_DESTROY_EVENT, &args) != 0) {
		return HSAKMT_STATUS_ERROR;
	}

	free(Event);
	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetEvent(
    HsaEvent*  Event    //IN
    )
{
	CHECK_KFD_OPEN();

	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	/* Although the spec is doesn't say, don't allow system-defined events to be signaled. */
	if (IsSystemEventType(Event->EventData.EventType))
		return HSAKMT_STATUS_ERROR;

	struct kfd_ioctl_set_event_args args;
	memset(&args, 0, sizeof(args));

	args.event_id = Event->EventId;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_SET_EVENT, &args) == -1)
		return HSAKMT_STATUS_ERROR;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtResetEvent(
    HsaEvent*  Event    //IN
    )
{
	CHECK_KFD_OPEN();

	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	/* Although the spec is doesn't say, don't allow system-defined events to be signaled. */
	if (IsSystemEventType(Event->EventData.EventType))
		return HSAKMT_STATUS_ERROR;

	struct kfd_ioctl_reset_event_args args;
	memset(&args, 0, sizeof(args));

	args.event_id = Event->EventId;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_RESET_EVENT, &args) == -1)
		return HSAKMT_STATUS_ERROR;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtQueryEventState(
    HsaEvent*  Event    //IN
    )
{
	CHECK_KFD_OPEN();

	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtWaitOnEvent(
    HsaEvent*   Event,          //IN
    HSAuint32   Milliseconds    //IN
    )
{
	if (!Event)
		return HSAKMT_STATUS_INVALID_HANDLE;

	return hsaKmtWaitOnMultipleEvents(&Event, 1, true, Milliseconds);
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtWaitOnMultipleEvents(
    HsaEvent*   Events[],       //IN
    HSAuint32   NumEvents,      //IN
    bool        WaitOnAll,      //IN
    HSAuint32   Milliseconds    //IN
    )
{
	CHECK_KFD_OPEN();

	if (!Events)
		return HSAKMT_STATUS_INVALID_HANDLE;

	struct kfd_event_data *event_data = malloc(NumEvents * sizeof(struct kfd_event_data));
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

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_WAIT_EVENTS, &args) == -1) {
		result = HSAKMT_STATUS_ERROR;
	}
	else if (args.wait_result == KFD_IOC_WAIT_RESULT_TIMEOUT) {
		result = HSAKMT_STATUS_WAIT_TIMEOUT;
	}
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
			}
		}
	}
out:
	free(event_data);

	return result;
}
