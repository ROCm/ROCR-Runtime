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
#include "fmm.h"
#include "linux/kfd_ioctl.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

/* 1024 doorbells, 4 or 8 bytes each doorbell depending on ASIC generation */
#define DOORBELL_SIZE_GFX7 4
#define DOORBELL_SIZE_GFX8 4
#define DOORBELL_SIZE_GFX9 8
#define DOORBELLS_PAGE_SIZE(ds) (1024 * (ds))

#define WG_CONTEXT_DATA_SIZE_PER_CU_VI	344576
#define WAVES_PER_CU_VI		32

struct device_info {
	enum asic_family_type asic_family;
	uint32_t eop_buffer_size;
	uint32_t doorbell_size;
};

struct device_info kaveri_device_info = {
	.asic_family = CHIP_KAVERI,
	.eop_buffer_size = 0,
	.doorbell_size = DOORBELL_SIZE_GFX7,
};

struct device_info hawaii_device_info = {
	.asic_family = CHIP_HAWAII,
	.eop_buffer_size = 0,
	.doorbell_size = DOORBELL_SIZE_GFX7,
};

struct device_info carrizo_device_info = {
	.asic_family = CHIP_CARRIZO,
	.eop_buffer_size = 4096,
	.doorbell_size = DOORBELL_SIZE_GFX8,
};

struct device_info tonga_device_info = {
	.asic_family = CHIP_TONGA,
	.eop_buffer_size = TONGA_PAGE_SIZE,
	.doorbell_size = DOORBELL_SIZE_GFX8,
};

struct device_info fiji_device_info = {
	.asic_family = CHIP_FIJI,
	.eop_buffer_size = TONGA_PAGE_SIZE,
	.doorbell_size = DOORBELL_SIZE_GFX8,
};

struct device_info polaris10_device_info = {
	.asic_family = CHIP_POLARIS10,
	.eop_buffer_size = TONGA_PAGE_SIZE,
	.doorbell_size = DOORBELL_SIZE_GFX8,
};

struct device_info polaris11_device_info = {
	.asic_family = CHIP_POLARIS11,
	.eop_buffer_size = TONGA_PAGE_SIZE,
	.doorbell_size = DOORBELL_SIZE_GFX8,
};

struct device_info vega10_device_info = {
	.asic_family = CHIP_VEGA10,
	.eop_buffer_size = 4096,
	.doorbell_size = DOORBELL_SIZE_GFX9,
};

struct device_info raven_device_info = {
	.asic_family = CHIP_RAVEN,
	.eop_buffer_size = 4096,
	.doorbell_size = DOORBELL_SIZE_GFX9,
};

static struct device_info *dev_lookup_table[] = {
	[CHIP_KAVERI] = &kaveri_device_info,
	[CHIP_HAWAII] = &hawaii_device_info,
	[CHIP_CARRIZO] = &carrizo_device_info,
	[CHIP_TONGA] = &tonga_device_info,
	[CHIP_FIJI] = &fiji_device_info,
	[CHIP_POLARIS10] = &polaris10_device_info,
	[CHIP_POLARIS11] = &polaris11_device_info,
	[CHIP_VEGA10] = &vega10_device_info,
	[CHIP_RAVEN] = &raven_device_info
};

struct device_id {
	uint16_t dev_id;
	struct device_info *dev_info;
};

struct queue {
	uint32_t queue_id;
	uint64_t wptr;
	uint64_t rptr;
	void *eop_buffer;
	void *ctx_save_restore;
	uint32_t ctx_save_restore_size;
	uint32_t ctl_stack_size;
	const struct device_info *dev_info;
	/* This queue structure is allocated from GPU with page aligned size
	 * but only small bytes are used. We use the extra space in the end for
	 * cu_mask bits array.
	 */
	uint32_t cu_mask_count; /* in bits */
	uint32_t cu_mask[0];
};

struct process_doorbells {
	bool use_gpuvm;
	uint32_t size;
	void *doorbells;
	pthread_mutex_t doorbells_mutex;
};

static unsigned int num_doorbells;
static struct process_doorbells *doorbells;

HSAKMT_STATUS init_process_doorbells(unsigned int NumNodes)
{
	unsigned int i;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	/* doorbells[] is accessed using Topology NodeId. This means doorbells[0],
	 * which corresponds to CPU only Node, might not be used
	 */
	doorbells = malloc(NumNodes * sizeof(struct process_doorbells));
	if (!doorbells)
		return HSAKMT_STATUS_NO_MEMORY;

	for (i = 0; i < NumNodes; i++) {
		doorbells[i].use_gpuvm = false;
		doorbells[i].size = 0;
		doorbells[i].doorbells = NULL;
		pthread_mutex_init(&doorbells[i].doorbells_mutex, NULL);
	}

	num_doorbells = NumNodes;

	return ret;
}

static struct device_info *get_device_info_by_dev_id(uint16_t dev_id)
{
	enum asic_family_type asic;

	if (topology_get_asic_family(dev_id, &asic) != HSAKMT_STATUS_SUCCESS)
		return NULL;

	return dev_lookup_table[asic];
}

static void get_doorbell_map_info(uint16_t dev_id,
				  struct process_doorbells *doorbell)
{
	struct device_info *dev_info;

	dev_info = get_device_info_by_dev_id(dev_id);

	/*
	 * GPUVM doorbell on Tonga requires a workaround for VM TLB ACTIVE bit
	 * lookup bug. Remove ASIC check when this is implemented in amdgpu.
	 */
	doorbell->use_gpuvm = (topology_is_dgpu(dev_id) &&
			       dev_info->asic_family != CHIP_TONGA);
	doorbell->size = DOORBELLS_PAGE_SIZE(dev_info->doorbell_size);
}

void destroy_process_doorbells(void)
{
	unsigned int i;

	if (!doorbells)
		return;

	for (i = 0; i < num_doorbells; i++) {
		if (!doorbells[i].size)
			continue;

		if (doorbells[i].use_gpuvm) {
			fmm_unmap_from_gpu(doorbells[i].doorbells);
			fmm_release(doorbells[i].doorbells);
		} else
			munmap(doorbells[i].doorbells, doorbells[i].size);
	}

	free(doorbells);
	doorbells = NULL;
	num_doorbells = 0;
}

/* This is a special funcion that should be called only from the child process
 * after a fork(). This will clear doorbells duplicated from the parent.
 */
void clear_process_doorbells(void)
{
	unsigned int i;

	if (!doorbells)
		return;

	for (i = 0; i < num_doorbells; i++) {
		if (!doorbells[i].size)
			continue;

		if (!doorbells[i].use_gpuvm)
			munmap(doorbells[i].doorbells, doorbells[i].size);
	}

	free(doorbells);
	doorbells = NULL;
	num_doorbells = 0;
}

static HSAKMT_STATUS map_doorbell_apu(HSAuint32 NodeId, HSAuint32 gpu_id,
				      HSAuint64 doorbell_offset)
{
	void *ptr;

	ptr = mmap(0, doorbells[NodeId].size, PROT_READ|PROT_WRITE,
		   MAP_SHARED, kfd_fd, doorbell_offset);

	if (ptr == MAP_FAILED)
		return HSAKMT_STATUS_ERROR;

	doorbells[NodeId].doorbells = ptr;

	return HSAKMT_STATUS_SUCCESS;
}

static HSAKMT_STATUS map_doorbell_dgpu(HSAuint32 NodeId, HSAuint32 gpu_id,
				       HSAuint64 doorbell_offset)
{
	void *ptr;

	ptr = fmm_allocate_doorbell(gpu_id, doorbells[NodeId].size,
				    doorbell_offset);

	if (!ptr)
		return HSAKMT_STATUS_ERROR;

	/* map for GPU access */
	if (fmm_map_to_gpu(ptr, doorbells[NodeId].size, NULL)) {
		fmm_release(ptr);
		return HSAKMT_STATUS_ERROR;
	}

	doorbells[NodeId].doorbells = ptr;

	return HSAKMT_STATUS_SUCCESS;
}

static HSAKMT_STATUS map_doorbell(HSAuint32 NodeId, HSAuint32 gpu_id,
				  HSAuint64 doorbell_offset)
{
	HSAKMT_STATUS status = HSAKMT_STATUS_SUCCESS;

	pthread_mutex_lock(&doorbells[NodeId].doorbells_mutex);
	if (doorbells[NodeId].size) {
		pthread_mutex_unlock(&doorbells[NodeId].doorbells_mutex);
		return HSAKMT_STATUS_SUCCESS;
	}

	get_doorbell_map_info(get_device_id_by_node(NodeId),
			      &doorbells[NodeId]);

	if (doorbells[NodeId].use_gpuvm)
		status = map_doorbell_dgpu(NodeId, gpu_id, doorbell_offset);
	else
		status = map_doorbell_apu(NodeId, gpu_id, doorbell_offset);

	if (status != HSAKMT_STATUS_SUCCESS)
		doorbells[NodeId].size = 0;

	pthread_mutex_unlock(&doorbells[NodeId].doorbells_mutex);

	return status;
}

static void *allocate_exec_aligned_memory_cpu(uint32_t size)
{
	void *ptr;

	/* mmap will return a pointer with alignment equal to
	 * sysconf(_SC_PAGESIZE).
	 *
	 * MAP_ANONYMOUS initializes the memory to zero.
	 */
	ptr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED)
		return NULL;
	return ptr;
}

/* The bool return indicate whether the queue needs a context-save-restore area*/
static bool update_ctx_save_restore_size(uint32_t nodeid, struct queue *q)
{
	HsaNodeProperties node;

	if (q->dev_info->asic_family < CHIP_CARRIZO)
		return false;
	if (hsaKmtGetNodeProperties(nodeid, &node))
		return false;
	if (node.NumFComputeCores && node.NumSIMDPerCU) {
		uint32_t ctl_stack_size, wg_data_size;
		uint32_t cu_num = node.NumFComputeCores / node.NumSIMDPerCU;

		ctl_stack_size = cu_num * WAVES_PER_CU_VI * 8 + 8;
		wg_data_size = cu_num * WG_CONTEXT_DATA_SIZE_PER_CU_VI;
		q->ctl_stack_size = PAGE_ALIGN_UP(ctl_stack_size);
		q->ctx_save_restore_size = q->ctl_stack_size + PAGE_ALIGN_UP(wg_data_size);

		return true;
	}
	return false;
}

void *allocate_exec_aligned_memory_gpu(uint32_t size, uint32_t align,
				       uint32_t NodeId, bool nonPaged)
{
	void *mem;
	HSAuint64 gpu_va;
	HsaMemFlags flags;
	HSAKMT_STATUS ret;

	flags.Value = 0;
	flags.ui32.HostAccess = 1;
	flags.ui32.ExecuteAccess = 1;
	flags.ui32.NonPaged = nonPaged;
	flags.ui32.PageSize = HSA_PAGE_SIZE_4KB;

	size = ALIGN_UP(size, align);

	ret = hsaKmtAllocMemory(0, size, flags, &mem);
	if (ret != HSAKMT_STATUS_SUCCESS)
		return NULL;

	if (NodeId != 0) {
		uint32_t nodes_array[1] = {NodeId};

		if (hsaKmtRegisterMemoryToNodes(mem, size, 1, nodes_array) != HSAKMT_STATUS_SUCCESS) {
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

	if (hsaKmtUnmapMemoryToGPU(addr) == HSAKMT_STATUS_SUCCESS)
		hsaKmtFreeMemory(addr, size);
}

/*
 * Allocates memory aligned to sysconf(_SC_PAGESIZE)
 */
static void *allocate_exec_aligned_memory(uint32_t size,
					  enum asic_family_type type,
					  uint32_t NodeId)
{
	if (IS_DGPU(type))
		return allocate_exec_aligned_memory_gpu(size, PAGE_SIZE, NodeId,
							false);
	return allocate_exec_aligned_memory_cpu(size);
}

static void free_exec_aligned_memory(void *addr, uint32_t size, uint32_t align,
				     enum asic_family_type type)
{
	if (IS_DGPU(type))
		free_exec_aligned_memory_gpu(addr, size, align);
	else
		munmap(addr, size);
}

static void free_queue(struct queue *q)
{
	if (q->eop_buffer)
		free_exec_aligned_memory(q->eop_buffer,
					 q->dev_info->eop_buffer_size,
					 PAGE_SIZE, q->dev_info->asic_family);
	if (q->ctx_save_restore)
		free_exec_aligned_memory(q->ctx_save_restore,
					 q->ctx_save_restore_size,
					 PAGE_SIZE, q->dev_info->asic_family);

	free_exec_aligned_memory((void *)q, sizeof(*q), PAGE_SIZE, q->dev_info->asic_family);
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
					dev_info->asic_family,
					NodeId);
			if (!q->eop_buffer)
				return HSAKMT_STATUS_NO_MEMORY;

			args->eop_buffer_address = (uintptr_t)q->eop_buffer;
			args->eop_buffer_size = dev_info->eop_buffer_size;
		}
		if (args->queue_type != KFD_IOC_QUEUE_TYPE_SDMA &&
			update_ctx_save_restore_size(NodeId, q) == true) {
			args->ctx_save_restore_size = q->ctx_save_restore_size;
			args->ctl_stack_size = q->ctl_stack_size;
			q->ctx_save_restore =
				allocate_exec_aligned_memory(q->ctx_save_restore_size,
							     dev_info->asic_family,
							     NodeId);
			if (!q->ctx_save_restore)
				return HSAKMT_STATUS_NO_MEMORY;

			args->ctx_save_restore_address = (uintptr_t)q->ctx_save_restore;
		}
	}

	return HSAKMT_STATUS_SUCCESS;
}

/* A map to translate thunk queue priority (-3 to +3)
 * to KFD queue priority (0 to 15)
 * Indexed by thunk_queue_priority+3
 */
static uint32_t priority_map[] = {0, 3, 5, 7, 9, 11, 15};

HSAKMT_STATUS HSAKMTAPI hsaKmtCreateQueue(HSAuint32 NodeId,
					  HSA_QUEUE_TYPE Type,
					  HSAuint32 QueuePercentage,
					  HSA_QUEUE_PRIORITY Priority,
					  void *QueueAddress,
					  HSAuint64 QueueSizeInBytes,
					  HsaEvent *Event,
					  HsaQueueResource *QueueResource)
{
	HSAKMT_STATUS result;
	uint32_t gpu_id;
	uint16_t dev_id;
	uint64_t doorbell_mmap_offset;
	unsigned int doorbell_offset;
	struct device_info *dev_info;
	int err;
	HsaNodeProperties props;
	uint32_t cu_num, i;

	CHECK_KFD_OPEN();

	if (Priority < HSA_QUEUE_PRIORITY_MINIMUM ||
		Priority > HSA_QUEUE_PRIORITY_MAXIMUM)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	result = validate_nodeid(NodeId, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	dev_id = get_device_id_by_node(NodeId);
	dev_info = get_device_info_by_dev_id(dev_id);

	struct queue *q = allocate_exec_aligned_memory(sizeof(*q),
			dev_info->asic_family,
			NodeId);
	if (!q)
		return HSAKMT_STATUS_NO_MEMORY;

	memset(q, 0, sizeof(*q));

	/* By default, CUs are all turned on. Initialize cu_mask to '1
	 * for all CU bits.
	 */
	if (hsaKmtGetNodeProperties(NodeId, &props))
		q->cu_mask_count = 0;
	else {
		cu_num = props.NumFComputeCores / props.NumSIMDPerCU;
		/* cu_mask_count counts bits. It must be multiple of 32 */
		q->cu_mask_count = ALIGN_UP_32(cu_num, 32);
		for (i = 0; i < cu_num; i++)
			q->cu_mask[i/32] |= (1 << (i % 32));
	}

	struct kfd_ioctl_create_queue_args args = {0};

	args.gpu_id = gpu_id;

	q->dev_info = dev_info;

	switch (Type) {
	case HSA_QUEUE_COMPUTE:
		args.queue_type = KFD_IOC_QUEUE_TYPE_COMPUTE;
		break;
	case HSA_QUEUE_SDMA:
		args.queue_type = KFD_IOC_QUEUE_TYPE_SDMA;
		break;
	case HSA_QUEUE_COMPUTE_AQL:
		args.queue_type = KFD_IOC_QUEUE_TYPE_COMPUTE_AQL;
		break;
	default:
		return HSAKMT_STATUS_INVALID_PARAMETER;
	}

	if (Type != HSA_QUEUE_COMPUTE_AQL) {
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
	args.queue_priority = priority_map[Priority+3];

	err = kmtIoctl(kfd_fd, AMDKFD_IOC_CREATE_QUEUE, &args);

	if (err == -1) {
		free_queue(q);
		return HSAKMT_STATUS_ERROR;
	}

	q->queue_id = args.queue_id;

	if (IS_SOC15(dev_info->asic_family)) {
		/* On SOC15 chips, the doorbell offset within the
		 * doorbell page is included in the doorbell offset
		 * returned by KFD. This allows doorbells to be
		 * allocated per-device, independent of the
		 * per-process queue ID.
		 */
		doorbell_mmap_offset = args.doorbell_offset &
			~(HSAuint64)(doorbells[NodeId].size - 1);
		doorbell_offset = args.doorbell_offset &
			(doorbells[NodeId].size - 1);
	} else {
		/* On older chips, the doorbell offset within the
		 * doorbell page is based on the queue ID.
		 */
		doorbell_mmap_offset = args.doorbell_offset;
		doorbell_offset = q->queue_id * dev_info->doorbell_size;
	}

	err = map_doorbell(NodeId, gpu_id, doorbell_mmap_offset);
	if (err != HSAKMT_STATUS_SUCCESS) {
		hsaKmtDestroyQueue(q->queue_id);
		free_queue(q);
		return HSAKMT_STATUS_ERROR;
	}

	QueueResource->QueueId = PORT_VPTR_TO_UINT64(q);
	QueueResource->Queue_DoorBell = VOID_PTR_ADD(doorbells[NodeId].doorbells,
						     doorbell_offset);

	return HSAKMT_STATUS_SUCCESS;
}


HSAKMT_STATUS HSAKMTAPI hsaKmtUpdateQueue(HSA_QUEUEID QueueId,
					  HSAuint32 QueuePercentage,
					  HSA_QUEUE_PRIORITY Priority,
					  void *QueueAddress,
					  HSAuint64 QueueSize,
					  HsaEvent *Event)
{
	struct kfd_ioctl_update_queue_args arg = {0};
	struct queue *q = PORT_UINT64_TO_VPTR(QueueId);

	CHECK_KFD_OPEN();

	if (Priority < HSA_QUEUE_PRIORITY_MINIMUM ||
		Priority > HSA_QUEUE_PRIORITY_MAXIMUM)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if (!q)
		return HSAKMT_STATUS_INVALID_PARAMETER;
	arg.queue_id = (HSAuint32)q->queue_id;
	arg.ring_base_address = (uintptr_t)QueueAddress;
	arg.ring_size = QueueSize;
	arg.queue_percentage = QueuePercentage;
	arg.queue_priority = priority_map[Priority+3];

	int err = kmtIoctl(kfd_fd, AMDKFD_IOC_UPDATE_QUEUE, &arg);

	if (err == -1)
		return HSAKMT_STATUS_ERROR;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtDestroyQueue(HSA_QUEUEID QueueId)
{
	CHECK_KFD_OPEN();

	struct queue *q = PORT_UINT64_TO_VPTR(QueueId);
	struct kfd_ioctl_destroy_queue_args args = {0};

	if (!q)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	args.queue_id = q->queue_id;

	int err = kmtIoctl(kfd_fd, AMDKFD_IOC_DESTROY_QUEUE, &args);

	if (err == -1)
		return HSAKMT_STATUS_ERROR;

	free_queue(q);
	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtSetQueueCUMask(HSA_QUEUEID QueueId,
					     HSAuint32 CUMaskCount,
					     HSAuint32 *QueueCUMask)
{
	struct queue *q = PORT_UINT64_TO_VPTR(QueueId);
	struct kfd_ioctl_set_cu_mask_args args = {0};

	CHECK_KFD_OPEN();

	if (CUMaskCount == 0 || !QueueCUMask || ((CUMaskCount % 32) != 0))
		return HSAKMT_STATUS_INVALID_PARAMETER;

	args.queue_id = q->queue_id;
	args.num_cu_mask = CUMaskCount;
	args.cu_mask_ptr = (uintptr_t)QueueCUMask;

	int err = kmtIoctl(kfd_fd, AMDKFD_IOC_SET_CU_MASK, &args);

	if (err == -1)
		return HSAKMT_STATUS_ERROR;

	memcpy(q->cu_mask, QueueCUMask, CUMaskCount / 8);
	q->cu_mask_count = CUMaskCount;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetQueueInfo(
	HSA_QUEUEID QueueId,
	HsaQueueInfo *QueueInfo
)
{
	struct queue *q = PORT_UINT64_TO_VPTR(QueueId);
	struct kfd_ioctl_get_queue_wave_state_args args = {0};

	CHECK_KFD_OPEN();

	if (QueueInfo == NULL || q == NULL)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if (q->ctx_save_restore == NULL)
		return HSAKMT_STATUS_ERROR;

	args.queue_id = q->queue_id;
	args.ctl_stack_address = (uintptr_t)q->ctx_save_restore;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_GET_QUEUE_WAVE_STATE, &args) < 0)
		return HSAKMT_STATUS_ERROR;

	QueueInfo->ControlStackTop = (void *)(args.ctl_stack_address +
				q->ctl_stack_size - args.ctl_stack_used_size);
	QueueInfo->UserContextSaveArea = (void *)
				 (args.ctl_stack_address + q->ctl_stack_size);
	QueueInfo->SaveAreaSizeInBytes = args.save_area_used_size;
	QueueInfo->ControlStackUsedInBytes = args.ctl_stack_used_size;
	QueueInfo->NumCUAssigned = q->cu_mask_count;
	QueueInfo->CUMaskInfo = q->cu_mask;
	QueueInfo->QueueDetailError = 0;
	QueueInfo->QueueTypeExtended = 0;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtSetTrapHandler(HSAuint32 Node,
					     void *TrapHandlerBaseAddress,
					     HSAuint64 TrapHandlerSizeInBytes,
					     void *TrapBufferBaseAddress,
					     HSAuint64 TrapBufferSizeInBytes)
{
	struct kfd_ioctl_set_trap_handler_args args = {0};
	HSAKMT_STATUS result;
	uint32_t gpu_id;

	CHECK_KFD_OPEN();

	result = validate_nodeid(Node, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	args.gpu_id = gpu_id;
	args.tba_addr = (uintptr_t)TrapHandlerBaseAddress;
	args.tma_addr = (uintptr_t)TrapBufferBaseAddress;

	int err = kmtIoctl(kfd_fd, AMDKFD_IOC_SET_TRAP_HANDLER, &args);

	return (err == -1) ? HSAKMT_STATUS_ERROR : HSAKMT_STATUS_SUCCESS;
}

