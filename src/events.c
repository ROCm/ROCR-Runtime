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
#include "linux/kfd_ioctl.h"

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
	args.auto_reset = !ManualReset;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_CREATE_EVENT, &args) == -1)
		return HSAKMT_STATUS_ERROR;

	e->EventId = args.event_id;
	e->EventData.HWData1 = args.event_id;
	e->EventData.HWData2 = args.event_trigger_address;
	e->EventData.HWData3 = args.event_trigger_data;

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

	struct kfd_ioctl_destroy_event_args args;
	memset(&args, 0, sizeof(args));

	args.event_id = Event->EventId;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_DESTROY_EVENT, &args) == -1)
		return HSAKMT_STATUS_ERROR;

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

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtWaitOnEvent(
    HsaEvent*   Event,          //IN
    HSAuint32   Milliseconds    //IN
    )
{
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

	uint32_t *event_ids = malloc(NumEvents * sizeof(uint32_t));
	for (HSAuint32 i = 0; i < NumEvents; i++) {
		event_ids[i] = Events[i]->EventId;
	}

	struct kfd_ioctl_wait_events_args args;
	memset(&args, 0, sizeof(args));

	args.wait_for_all = WaitOnAll;
	args.timeout = Milliseconds;
	args.num_events = NumEvents;
	args.events_ptr = (uint64_t)(uintptr_t)event_ids;

	HSAKMT_STATUS result;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_WAIT_EVENTS, &args) == -1) {
		result = HSAKMT_STATUS_ERROR;
	}
	else if (args.wait_result == KFD_IOC_WAIT_RESULT_TIMEOUT) {
		result = HSAKMT_STATUS_WAIT_TIMEOUT;
	}
	else {
		result = HSAKMT_STATUS_SUCCESS;
	}

	free(event_ids);

	return result;
}
