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

enum asic_family_type {
	CHIP_KAVERI = 0,
	CHIP_CARRIZO,
	CHIP_TONGA,
	CHIP_FIJI
};

#define IS_VI(chip) ((chip) >= CHIP_CARRIZO && (chip) <= CHIP_FIJI)
#define IS_DGPU(chip) ((chip) >= CHIP_TONGA && (chip) <= CHIP_FIJI)

struct device_info
{
	enum asic_family_type asic_family;
	uint32_t ctx_save_restore_size;
	uint32_t ctl_stack_size;
	uint32_t eop_buffer_size;
};

struct device_info kaveri_device_info = {
	.asic_family = CHIP_KAVERI,
	.ctx_save_restore_size = 0,
	.ctl_stack_size = 0,
	.eop_buffer_size = 0,
};

struct device_info carrizo_device_info = {
	.asic_family = CHIP_CARRIZO,
	.ctx_save_restore_size = PAGE_ALIGN_UP(2756608 + 4096),
	.ctl_stack_size = 4096,
	.eop_buffer_size = 4096,
};

struct device_info tonga_device_info = {
	.asic_family = CHIP_TONGA,
	.ctx_save_restore_size = PAGE_ALIGN_UP(8269824 + PAGE_ALIGN_UP(6152)),
	.ctl_stack_size = PAGE_ALIGN_UP(6152),
	.eop_buffer_size = TONGA_PAGE_SIZE,
};

struct device_info fiji_device_info = {
	.asic_family = CHIP_FIJI,
	/*SR size = work group ctx data  + ctl stack size*/
	.ctx_save_restore_size = PAGE_ALIGN_UP(20674560 +  PAGE_ALIGN_UP(15368)),
	.ctl_stack_size = PAGE_ALIGN_UP(15368),
	.eop_buffer_size = TONGA_PAGE_SIZE,
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
	{ 0x6920, &tonga_device_info },
	{ 0x6921, &tonga_device_info },
	{ 0x6928, &tonga_device_info },
	{ 0x6929, &tonga_device_info },
	{ 0x692b, &tonga_device_info },
	{ 0x692f, &tonga_device_info },
	{ 0x6930, &tonga_device_info },
	{ 0x6938, &tonga_device_info },
	{ 0x6939, &tonga_device_info },
	{ 0x7300, &fiji_device_info },
	{ 0, NULL }
};

struct queue
{
	uint32_t queue_id;
	uint32_t wptr;
	uint32_t rptr;
	void *eop_buffer;
	void *ctx_save_restore;
	const struct device_info *dev_info;
};

struct process_doorbells
{
	bool need_mmap;
	void* doorbells;
	pthread_mutex_t doorbells_mutex;
};

static struct process_doorbells *doorbells;

HSAKMT_STATUS init_process_doorbells(unsigned int NumNodes)
{
	unsigned int i;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	/* doorbells[] is accessed using Topology NodeId. This means doorbells[0],
	 * which corresponds to CPU only Node, might not be used */
	doorbells = malloc(NumNodes * sizeof(struct process_doorbells));
	if (doorbells == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	for (i = 0; i < NumNodes; i++) {
		doorbells[i].need_mmap = true;
		doorbells[i].doorbells = NULL;
		pthread_mutex_init(&doorbells[i].doorbells_mutex, NULL);
	}
	return ret;
}

void destroy_process_doorbells(void)
{
	if (doorbells) {
		free(doorbells);
		doorbells = NULL;
	}
}

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

static void free_queue_cpu(struct queue *q)
{
	if (q->eop_buffer)
		free(q->eop_buffer);
	if (q->ctx_save_restore)
		free(q->ctx_save_restore);
	free(q);
}

static void* allocate_exec_aligned_memory_cpu(uint32_t size, uint32_t align)
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

void* allocate_exec_aligned_memory_gpu(uint32_t size, uint32_t align,
				       uint32_t NodeId)
{
	void *mem;
	HSAuint64 gpu_va;
	HsaMemFlags flags;
	HSAKMT_STATUS ret;

	flags.Value = 0;
	flags.ui32.HostAccess = 1;
	flags.ui32.ExecuteAccess = 1;
	flags.ui32.PageSize = HSA_PAGE_SIZE_4KB;

	size = ALIGN_UP(size, align);

	ret = hsaKmtAllocMemory(0, size, flags, &mem);
	if (ret != HSAKMT_STATUS_SUCCESS) {
		return NULL;
	}

	if (NodeId != 0) {
		uint32_t nodes_array[1] = {NodeId};
		if (hsaKmtRegisterMemoryToNodes(mem, size, 1, nodes_array)
		    != HSAKMT_STATUS_SUCCESS) {
			hsaKmtFreeMemory(mem, size);
			return NULL;
		}
	}

	if (hsaKmtMapMemoryToGPU(mem, size, &gpu_va) != HSAKMT_STATUS_SUCCESS) {
		hsaKmtFreeMemory(mem, size);
		return NULL;
	}

	return mem;
}

void free_exec_aligned_memory_gpu(void *addr, uint32_t size, uint32_t align)
{
	size = ALIGN_UP(size, align);

	if (hsaKmtUnmapMemoryToGPU(addr) == HSAKMT_STATUS_SUCCESS) {
		hsaKmtFreeMemory(addr, size);
	}
}

static void* allocate_exec_aligned_memory(uint32_t size,
					uint32_t align,
					enum asic_family_type type,
					uint32_t NodeId)
{
	if (IS_DGPU(type))
		return allocate_exec_aligned_memory_gpu(size, align, NodeId);
	return allocate_exec_aligned_memory_cpu(size, align);
}

static void free_exec_aligned_memory(void *addr, uint32_t size, uint32_t align,
				     enum asic_family_type type)
{
	if (IS_DGPU(type))
		free_exec_aligned_memory_gpu(addr, size, align);
	else
		free(addr);
}

static void free_queue_gpu(struct queue *q)
{
	if (q->eop_buffer) {
		hsaKmtUnmapMemoryToGPU(q->eop_buffer);
		hsaKmtFreeMemory(q->eop_buffer, q->dev_info->eop_buffer_size);
	}
	if (q->ctx_save_restore) {
		hsaKmtUnmapMemoryToGPU(q->ctx_save_restore);
		hsaKmtFreeMemory(q->ctx_save_restore, q->dev_info->ctx_save_restore_size);
	}
	free_exec_aligned_memory((void *)q, sizeof(*q), PAGE_SIZE, q->dev_info->asic_family);
}

static void free_queue(struct queue *q)
{
	if (IS_DGPU(q->dev_info->asic_family))
		return free_queue_gpu(q);
	return free_queue_cpu(q);
}

static int handle_concrete_asic(struct queue *q,
				struct kfd_ioctl_create_queue_args *args,
				uint32_t NodeId)
{
	const struct device_info *dev_info = q->dev_info;
	if (dev_info) {
		if (dev_info->eop_buffer_size > 0) {
			q->eop_buffer =
					allocate_exec_aligned_memory(q->dev_info->eop_buffer_size,
					PAGE_SIZE,
					dev_info->asic_family,
					NodeId);
			if (q->eop_buffer == NULL) {
				return HSAKMT_STATUS_NO_MEMORY;
			}
			args->eop_buffer_address = (uintptr_t)q->eop_buffer;
			args->eop_buffer_size = dev_info->eop_buffer_size;
		}
		if (args->queue_type != KFD_IOC_QUEUE_TYPE_SDMA &&
			dev_info->ctx_save_restore_size > 0) {
			args->ctx_save_restore_size = dev_info->ctx_save_restore_size;
			args->ctl_stack_size = dev_info->ctl_stack_size;
			if (IS_DGPU(dev_info->asic_family)) {
				void *mem;
				HsaMemFlags flags;
				HSAKMT_STATUS ret;
				HSAuint64 size = dev_info->ctx_save_restore_size;
				flags.Value = 0;
				flags.ui32.NonPaged = 1; /* device memory*/

				ret = hsaKmtAllocMemory(NodeId, size, flags, &mem);
				if (ret != HSAKMT_STATUS_SUCCESS)
					return ret;
				ret = hsaKmtMapMemoryToGPU(mem, size, NULL);
				if (ret != HSAKMT_STATUS_SUCCESS) {
					hsaKmtFreeMemory(mem, size);
					return ret;
				}
				q->ctx_save_restore = mem;

			} else
				q->ctx_save_restore =
					allocate_exec_aligned_memory(dev_info->ctx_save_restore_size,
					PAGE_SIZE,
					dev_info->asic_family,
					NodeId);
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

	dev_id = get_device_id_by_node(NodeId);
	dev_info = get_device_info_by_dev_id(dev_id);

	struct queue *q = allocate_exec_aligned_memory(sizeof (*q),
			PAGE_SIZE, dev_info->asic_family,
			NodeId);
	if (q == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	memset(q, 0, sizeof(*q));

	struct kfd_ioctl_create_queue_args args;
	memset(&args, 0, sizeof(args));

	args.gpu_id = gpu_id;

	q->dev_info = dev_info;

	switch (Type)
	{
	case HSA_QUEUE_COMPUTE: args.queue_type = KFD_IOC_QUEUE_TYPE_COMPUTE; break;
	case HSA_QUEUE_SDMA: args.queue_type = KFD_IOC_QUEUE_TYPE_SDMA; break;
	case HSA_QUEUE_COMPUTE_AQL: args.queue_type = KFD_IOC_QUEUE_TYPE_COMPUTE_AQL; break;
	default: return HSAKMT_STATUS_INVALID_PARAMETER;
	}

	if (Type != HSA_QUEUE_COMPUTE_AQL)
	{
		QueueResource->QueueRptrValue = (uintptr_t)&q->rptr;
		QueueResource->QueueWptrValue = (uintptr_t)&q->wptr;
	}

	err = handle_concrete_asic(q, &args, NodeId);
	if (err != HSAKMT_STATUS_SUCCESS) {
		free_queue(q);
		return err;
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

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetTrapHandler(
	HSAuint32	Node,
	void *TrapHandlerBaseAddress,
	HSAuint64	TrapHandlerSizeInBytes,
	void *TrapBufferBaseAddress,
	HSAuint64 TrapBufferSizeInBytes
)
{
	struct kfd_ioctl_set_trap_handler_args args;
	HSAKMT_STATUS result;
	uint32_t gpu_id;

	CHECK_KFD_OPEN();

	result = validate_nodeid(Node, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	memset(&args, 0, sizeof(args));

	args.gpu_id = gpu_id;
	args.tba_addr = (uintptr_t)TrapHandlerBaseAddress;
	args.tma_addr = (uintptr_t)TrapBufferBaseAddress;

	int err = kmtIoctl(kfd_fd, AMDKFD_IOC_SET_TRAP_HANDLER, &args);

	return (err == -1) ? HSAKMT_STATUS_ERROR : HSAKMT_STATUS_SUCCESS;
}

