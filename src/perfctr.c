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
#include "linux/kfd_ioctl.h"

HSAKMT_STATUS
HSAKMTAPI
hsaKmtPmcGetCounterProperties(
    HSAuint32                   NodeId,             //IN
    HsaCounterProperties**      CounterProperties   //OUT
    )
{
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
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
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
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
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
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
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
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
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
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
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
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
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
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
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
}
