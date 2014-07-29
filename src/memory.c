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
#include "linux/kfd_ioctl.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetMemoryPolicy(
	HSAuint32 Node,
	HSAuint32 DefaultPolicy,
	HSAuint32 AlternatePolicy,
	void* MemoryAddressAlternate,
	HSAuint64 MemorySizeInBytes
	)
{
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtAllocMemory(
    HSAuint32       PreferredNode,          //IN
    HSAuint64       SizeInBytes,            //IN  (multiple of page size)
    HsaMemFlags     MemFlags,               //IN
    void**          MemoryAddress           //OUT (page-aligned)
    )
{
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtFreeMemory(
    void*       MemoryAddress,      //IN (page-aligned)
    HSAuint64   SizeInBytes         //IN
    )
{
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterMemory(
    void*       MemoryAddress,      //IN (page-aligned)
    HSAuint64   MemorySizeInBytes   //IN (page-aligned)
    )
{
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDeregisterMemory(
    void*       MemoryAddress  //IN
    )
{
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtMapMemoryToGPU(
    void*           MemoryAddress,     //IN (page-aligned)
    HSAuint64       MemorySizeInBytes, //IN (page-aligned)
    HSAuint64*      AlternateVAGPU     //OUT (page-aligned)
    )
{
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtUnmapMemoryToGPU(
    void*           MemoryAddress       //IN (page-aligned)
    )
{
	CHECK_KFD_OPEN();

	return HSAKMT_STATUS_NOT_SUPPORTED;
}
