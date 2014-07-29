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
#include <time.h>
#include <errno.h>
#include <unistd.h>

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
	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtResetEvent(
    HsaEvent*  Event    //IN
    )
{
	CHECK_KFD_OPEN();
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
	CHECK_KFD_OPEN();

	if (Milliseconds == HSA_EVENTTIMEOUT_INFINITE)
	{
		while (1) { pause(); }
	}
	else if (Milliseconds != HSA_EVENTTIMEOUT_IMMEDIATE)
	{
		struct timespec req;

		req.tv_sec = Milliseconds / 1000;
		req.tv_nsec = (long)(Milliseconds % 1000) * 1000000;

		while (1)
		{
			struct timespec rem;

			int err = nanosleep(&req, &rem);
			if (err == -1 && errno == EINTR)
			{
				req = rem;
			}
			else
			{
				break; // success or other error
			}
		}
	}

	return HSAKMT_STATUS_WAIT_TIMEOUT;
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

	return hsaKmtWaitOnEvent(NULL, Milliseconds);
}
