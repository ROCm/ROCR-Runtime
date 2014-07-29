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
#include <sys/mman.h>
#include <math.h>
#include <stdio.h>

/* 1024 doorbells, 4 bytes each doorbell */
#define DOORBELLS_PAGE_SIZE	1024 * 4

struct queue
{
	uint32_t queue_id;
	uint32_t wptr;
	uint32_t rptr;
};

struct process_doorbells
{
	bool need_mmap;
	void* doorbells;
	pthread_mutex_t doorbells_mutex;
};

struct process_doorbells doorbells[] = {[0 ... (NUM_OF_SUPPORTED_GPUS-1)] = {.need_mmap = true, .doorbells = NULL, .doorbells_mutex = PTHREAD_MUTEX_INITIALIZER}};

HSAKMT_STATUS
HSAKMTAPI
hsaKmtCreateQueue(
    HSAuint32           NodeId,           //IN
    HSA_QUEUE_TYPE      Type,             //IN
    HSAuint32           QueuePercentage,  //IN
    HSA_QUEUE_PRIORITY  Priority,         //IN
    void*               QueueAddress,     //IN
    HSAuint64           QueueSizeInBytes, //IN
    HsaEvent*           Event,            //IN
    HsaQueueResource*   QueueResource     //OUT
    )
{
	HSAKMT_STATUS result;
	uint32_t gpu_id;
	int err;
	void* ptr;
	CHECK_KFD_OPEN();

	result = validate_nodeid(NodeId, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	struct queue *q = malloc(sizeof(struct queue));
	if (q == NULL)
	{
		return HSAKMT_STATUS_NO_MEMORY;
	}

	memset(q, 0, sizeof(*q));

	struct kfd_ioctl_create_queue_args args;
	memset(&args, 0, sizeof(args));

	args.gpu_id = gpu_id;

	switch (Type)
	{
	case HSA_QUEUE_COMPUTE: args.queue_type = KFD_IOC_QUEUE_TYPE_COMPUTE; break;
	case HSA_QUEUE_SDMA: free(q); return HSAKMT_STATUS_NOT_IMPLEMENTED;
	case HSA_QUEUE_COMPUTE_AQL: args.queue_type = KFD_IOC_QUEUE_TYPE_COMPUTE_AQL; break;
	default: free(q); return HSAKMT_STATUS_INVALID_PARAMETER;
	}

	if (Type != HSA_QUEUE_COMPUTE_AQL)
	{
		QueueResource->QueueRptrValue = (uintptr_t)&q->rptr;
		QueueResource->QueueWptrValue = (uintptr_t)&q->wptr;
	}

	args.read_pointer_address = QueueResource->QueueRptrValue;
	args.write_pointer_address = QueueResource->QueueWptrValue;
	args.ring_base_address = (uintptr_t)QueueAddress;
	args.ring_size = QueueSizeInBytes;
	args.queue_percentage = QueuePercentage;
	args.queue_priority = Priority;

	err = kfd_ioctl(KFD_IOC_CREATE_QUEUE, &args);

	if (err == -1)
	{
		free(q);
		return HSAKMT_STATUS_ERROR;
	}

	q->queue_id = args.queue_id;

	pthread_mutex_lock(&doorbells[NodeId].doorbells_mutex);

	if (doorbells[NodeId].need_mmap) {
		ptr = mmap(0, DOORBELLS_PAGE_SIZE, PROT_READ|PROT_WRITE,
				MAP_SHARED, kfd_fd, args.doorbell_offset);

		if (ptr == MAP_FAILED) {
			pthread_mutex_unlock(&doorbells[NodeId].doorbells_mutex);
			hsaKmtDestroyQueue(q->queue_id);
			return HSAKMT_STATUS_ERROR;
		}

		doorbells[NodeId].need_mmap = false;
		doorbells[NodeId].doorbells = ptr;
	}

	pthread_mutex_unlock(&doorbells[NodeId].doorbells_mutex);

	QueueResource->QueueId = PORT_VPTR_TO_UINT64(q);
	QueueResource->Queue_DoorBell = VOID_PTR_ADD32(doorbells[NodeId].doorbells, q->queue_id);

	return HSAKMT_STATUS_SUCCESS;
}


HSAKMT_STATUS
HSAKMTAPI
hsaKmtUpdateQueue(
    HSA_QUEUEID         QueueId,        //IN
    HSAuint32           QueuePercentage,//IN
    HSA_QUEUE_PRIORITY  Priority,       //IN
    void*               QueueAddress,   //IN
    HSAuint64           QueueSize,      //IN
    HsaEvent*           Event           //IN
    )
{
	struct kfd_ioctl_update_queue_args arg;
	struct queue *q = PORT_UINT64_TO_VPTR(QueueId);

	CHECK_KFD_OPEN();

	if (q == NULL)
		return (HSAKMT_STATUS_INVALID_PARAMETER);
	arg.queue_id = (HSAuint32)q->queue_id;
	arg.ring_base_address = (uintptr_t)QueueAddress;
	arg.ring_size = QueueSize;
	arg.queue_percentage = QueuePercentage;
	arg.queue_priority = Priority;

	int err = kfd_ioctl(KFD_IOC_UPDATE_QUEUE, &arg);
	if (err == -1)
	{
		return HSAKMT_STATUS_ERROR;
	}

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDestroyQueue(
    HSA_QUEUEID         QueueId         //IN
    )
{
	CHECK_KFD_OPEN();

	struct queue *q = PORT_UINT64_TO_VPTR(QueueId);
	struct kfd_ioctl_destroy_queue_args args;

	if (q == NULL)
			return (HSAKMT_STATUS_INVALID_PARAMETER);

	memset(&args, 0, sizeof(args));

	args.queue_id = q->queue_id;

	int err = kfd_ioctl(KFD_IOC_DESTROY_QUEUE, &args);

	if (err == -1)
	{
		return HSAKMT_STATUS_ERROR;
	}
	else
	{
		free(q);
		return HSAKMT_STATUS_SUCCESS;
	}
}
