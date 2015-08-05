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
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

/* 1024 doorbells, 4 bytes each doorbell */
#define DOORBELLS_PAGE_SIZE	1024 * 4

struct device_info
{
	uint32_t ctx_save_restore_size;
	uint32_t eop_buffer_size;
};

struct device_info kaveri_device_info = {
	.ctx_save_restore_size = 0,
	.eop_buffer_size = 0,
};

struct device_info carrizo_device_info = {
	.ctx_save_restore_size = 2756608,
	.eop_buffer_size = 4096,
};

struct device_id
{
	uint16_t dev_id;
	struct device_info *dev_info;
};

struct device_id supported_devices[] = {
	{ 0x1304, &kaveri_device_info },	/* Kaveri */
	{ 0x1305, &kaveri_device_info },	/* Kaveri */
	{ 0x1306, &kaveri_device_info },	/* Kaveri */
	{ 0x1307, &kaveri_device_info },	/* Kaveri */
	{ 0x1309, &kaveri_device_info },	/* Kaveri */
	{ 0x130A, &kaveri_device_info },	/* Kaveri */
	{ 0x130B, &kaveri_device_info },	/* Kaveri */
	{ 0x130C, &kaveri_device_info },	/* Kaveri */
	{ 0x130D, &kaveri_device_info },	/* Kaveri */
	{ 0x130E, &kaveri_device_info },	/* Kaveri */
	{ 0x130F, &kaveri_device_info },	/* Kaveri */
	{ 0x1310, &kaveri_device_info },	/* Kaveri */
	{ 0x1311, &kaveri_device_info },	/* Kaveri */
	{ 0x1312, &kaveri_device_info },	/* Kaveri */
	{ 0x1313, &kaveri_device_info },	/* Kaveri */
	{ 0x1315, &kaveri_device_info },	/* Kaveri */
	{ 0x1316, &kaveri_device_info },	/* Kaveri */
	{ 0x1317, &kaveri_device_info },	/* Kaveri */
	{ 0x1318, &kaveri_device_info },	/* Kaveri */
	{ 0x131B, &kaveri_device_info },	/* Kaveri */
	{ 0x131C, &kaveri_device_info },	/* Kaveri */
	{ 0x131D, &kaveri_device_info },	/* Kaveri */
	{ 0x9870, &carrizo_device_info },	/* Carrizo */
	{ 0x9874, &carrizo_device_info },	/* Carrizo */
	{ 0x9875, &carrizo_device_info },	/* Carrizo */
	{ 0x9876, &carrizo_device_info },	/* Carrizo */
	{ 0x9877, &carrizo_device_info },	/* Carrizo */
	{ 0, NULL }
};

struct queue
{
	uint32_t queue_id;
	uint32_t wptr;
	uint32_t rptr;
	void *eop_buffer;
	void *ctx_save_restore;
};

struct process_doorbells
{
	bool need_mmap;
	void* doorbells;
	pthread_mutex_t doorbells_mutex;
};

struct process_doorbells doorbells[] = {[0 ... (NUM_OF_SUPPORTED_GPUS-1)] = {.need_mmap = true, .doorbells = NULL, .doorbells_mutex = PTHREAD_MUTEX_INITIALIZER}};

static struct device_info *get_device_info_by_dev_id(uint16_t dev_id)
{
	int i = 0;
	while (supported_devices[i].dev_id != 0) {
		if (supported_devices[i].dev_id == dev_id) {
			return supported_devices[i].dev_info;
		}
		i++;
	}

	return NULL;
}

static void free_queue(struct queue *q)
{
	if (q->eop_buffer)
		free(q->eop_buffer);
	if (q->ctx_save_restore)
		free(q->ctx_save_restore);
	free(q);
}

static void* allocate_exec_aligned_memory(uint32_t size, uint32_t align)
{
	void *ptr;
	int retval;

	retval = posix_memalign(&ptr, align, size);
	if (retval != 0)
		return NULL;

	retval = mprotect(ptr, size, PROT_READ | PROT_WRITE | PROT_EXEC);
	if (retval != 0) {
		free(ptr);
		return NULL;
	}

	memset(ptr, 0, size);
	return ptr;
}

static int handle_concrete_asic(struct device_info *dev_info, struct queue *q,
								struct kfd_ioctl_create_queue_args *args)
{
	if (dev_info) {
		if (dev_info->eop_buffer_size > 0) {
			q->eop_buffer =
					allocate_exec_aligned_memory(dev_info->eop_buffer_size, PAGE_SIZE);
			if (q->eop_buffer == NULL) {
				return HSAKMT_STATUS_NO_MEMORY;
			}
			args->eop_buffer_address = (uintptr_t)q->eop_buffer;
			args->eop_buffer_size = dev_info->eop_buffer_size;
		}
		if (dev_info->ctx_save_restore_size > 0) {
			args->ctx_save_restore_size = dev_info->ctx_save_restore_size;
			q->ctx_save_restore =
					allocate_exec_aligned_memory(dev_info->ctx_save_restore_size, PAGE_SIZE);
			if (q->ctx_save_restore == NULL) {;
				return HSAKMT_STATUS_NO_MEMORY;
			}
			args->ctx_save_restore_address = (uintptr_t)q->ctx_save_restore;
		}
	}

	return HSAKMT_STATUS_SUCCESS;
}

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
	uint16_t dev_id;
	struct device_info *dev_info;
	int err;
	void* ptr;
	CHECK_KFD_OPEN();

	result = validate_nodeid(NodeId, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	struct queue *q = malloc(sizeof(*q));
	if (q == NULL)
		return HSAKMT_STATUS_NO_MEMORY;
	memset(q, 0, sizeof(*q));

	struct kfd_ioctl_create_queue_args args;
	memset(&args, 0, sizeof(args));

	dev_id = get_device_id_by_node(NodeId);
	dev_info = get_device_info_by_dev_id(dev_id);
	args.gpu_id = gpu_id;

	err = handle_concrete_asic(dev_info, q, &args);
	if (err != HSAKMT_STATUS_SUCCESS) {
		free_queue(q);
		return err;
	}

	switch (Type)
	{
	case HSA_QUEUE_COMPUTE: args.queue_type = KFD_IOC_QUEUE_TYPE_COMPUTE; break;
	case HSA_QUEUE_SDMA: free_queue(q); return HSAKMT_STATUS_UNAVAILABLE;
	case HSA_QUEUE_COMPUTE_AQL: args.queue_type = KFD_IOC_QUEUE_TYPE_COMPUTE_AQL; break;
	default: free_queue(q); return HSAKMT_STATUS_INVALID_PARAMETER;
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

	err = kmtIoctl(kfd_fd, AMDKFD_IOC_CREATE_QUEUE, &args);

	if (err == -1)
	{
		free_queue(q);
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
			free_queue(q);
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

	int err = kmtIoctl(kfd_fd, AMDKFD_IOC_UPDATE_QUEUE, &arg);
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

	int err = kmtIoctl(kfd_fd, AMDKFD_IOC_DESTROY_QUEUE, &args);

	if (err == -1)
	{
		return HSAKMT_STATUS_ERROR;
	}
	else
	{
		free_queue(q);
		return HSAKMT_STATUS_SUCCESS;
	}
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetQueueCUMask(
    HSA_QUEUEID         QueueId,        //IN
    HSAuint32           CUMaskCount,    //IN
    HSAuint32*          QueueCUMask     //IN
    )
{
	struct queue *q = PORT_UINT64_TO_VPTR(QueueId);
	struct kfd_ioctl_set_cu_mask_args args;

	CHECK_KFD_OPEN();

	if (CUMaskCount == 0 || QueueCUMask == NULL)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	memset(&args, 0, sizeof(args));
	args.queue_id = q->queue_id;
	args.num_cu_mask = CUMaskCount;
	args.cu_mask_ptr = (uintptr_t)QueueCUMask;

	int err = kmtIoctl(kfd_fd, AMDKFD_IOC_SET_CU_MASK, &args);
	if (err == -1)
	{
		return HSAKMT_STATUS_ERROR;
	}

	return HSAKMT_STATUS_SUCCESS;
}
