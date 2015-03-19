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

#include "fmm.h"
#include "linux/kfd_ioctl.h"
#include "libhsakmt.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/mman.h>

#define NON_VALID_GPU_ID 0
#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

#define INIT_APERTURE(base_value, limit_value) {		\
	.base = (void *) base_value,				\
	.limit = (void *) limit_value				\
	}

#define INIT_MANAGEBLE_APERTURE(base_value, limit_value) {	\
	.base = (void *) base_value,				\
	.limit = (void *) limit_value,				\
	.vm_ranges = NULL,					\
	.vm_objects = NULL,					\
	.fmm_mutex = PTHREAD_MUTEX_INITIALIZER			\
	}

#define INIT_GPU_MEM {						\
	.gpu_id = NON_VALID_GPU_ID,				\
	.lds_aperture = INIT_APERTURE(0, 0),			\
	.scratch_aperture = INIT_MANAGEBLE_APERTURE(0, 0),	\
	.gpuvm_aperture =  INIT_MANAGEBLE_APERTURE(0, 0)	\
}

#define INIT_GPUs_MEM {[0 ... (NUM_OF_SUPPORTED_GPUS-1)] = INIT_GPU_MEM}

struct vm_object {
	void *start;
	uint64_t size;
	uint64_t handle; /* opaque */
	struct vm_object *next;
	struct vm_object *prev;
};
typedef struct vm_object vm_object_t;

struct vm_area {
	void *start;
	void *end;
	struct vm_area *next;
	struct vm_area *prev;
};
typedef struct vm_area vm_area_t;

typedef struct {
	void *base;
	void *limit;
	vm_area_t *vm_ranges;
	vm_object_t *vm_objects;
	pthread_mutex_t fmm_mutex;
} manageble_aperture_t;

typedef struct {
	void *base;
	void *limit;
} aperture_t;

typedef struct {
	uint32_t gpu_id;
	aperture_t lds_aperture;
	manageble_aperture_t scratch_aperture;
	manageble_aperture_t gpuvm_aperture;
} gpu_mem_t;

static gpu_mem_t gpu_mem[] = INIT_GPUs_MEM;

static vm_area_t *vm_create_and_init_area(void *start, void *end)
{
	vm_area_t *area = (vm_area_t *) malloc(sizeof(vm_area_t));

	if (area) {
		area->start = start;
		area->end = end;
		area->next = area->prev = NULL;
	}

	return area;
}

static vm_object_t *vm_create_and_init_object(void *start, uint64_t size,
						uint64_t handle)
{
	vm_object_t *object = (vm_object_t *) malloc(sizeof(vm_object_t));

	if (object) {
		object->start = start;
		object->size = size;
		object->handle = handle;
		object->next = object->prev = NULL;
	}

	return object;
}


static void vm_remove_area(manageble_aperture_t *app, vm_area_t *area)
{
	vm_area_t *next;
	vm_area_t *prev;

	next = area->next;
	prev = area->prev;

	if (prev == NULL) /* The first element */
		app->vm_ranges = next;
	else
		prev->next = next;

	if (next) /* If not the last element */
		next->prev = prev;

	free(area);
}

static void vm_remove_object(manageble_aperture_t *app, vm_object_t *object)
{
	vm_object_t *next;
	vm_object_t *prev;

	next = object->next;
	prev = object->prev;

	if (prev == NULL) /* The first element */
		app->vm_objects = next;
	else
		prev->next = next;

	if (next) /* If not the last element */
		next->prev = prev;

	free(object);

}

static void vm_add_area_after(vm_area_t *after_this, vm_area_t *new_area)
{
	vm_area_t *next = after_this->next;

	after_this->next = new_area;
	new_area->next = next;

	new_area->prev = after_this;
	if (next)
		next->prev = new_area;
}

static void vm_add_object_before(vm_object_t *before_this,
				vm_object_t *new_object)
{
	vm_object_t *prev = before_this->prev;

	before_this->prev = new_object;
	new_object->next = before_this;

	new_object->prev = prev;
	if (prev)
		prev->next = new_object;
}

static void vm_split_area(manageble_aperture_t *app, vm_area_t *area,
				void *address, uint64_t MemorySizeInBytes)
{
	/*
	 * The existing area is split to: [area->start, address - 1]
	 * and [address + MemorySizeInBytes, area->end]
	 */
	vm_area_t *new_area = vm_create_and_init_area(
				VOID_PTR_ADD(address, MemorySizeInBytes),
				area->end);

	/* Shrink the existing area */
	area->end = VOID_PTR_SUB(address, 1);

	vm_add_area_after(area, new_area);
}

static vm_object_t *vm_find_object_by_address(manageble_aperture_t *app,
						void *address, uint64_t size)
{
	vm_object_t *cur = app->vm_objects;

	/* Look up the appropriate address range containing the given address */
	while (cur) {
		if (cur->start == address && (cur->size == size || size == 0))
			break;
		cur = cur->next;
	};

	return cur; /* NULL if not found */
}

static vm_area_t *vm_find(manageble_aperture_t *app, void *address)
{
	vm_area_t *cur = app->vm_ranges;

	/* Look up the appropriate address range containing the given address */
	while (cur) {
		if (cur->start <= address && cur->end >= address)
			break;
		cur = cur->next;
	};

	return cur; /* NULL if not found */
}

static bool aperture_is_valid(void *app_base, void *app_limit)
{
	if (app_base && app_limit && app_base < app_limit)
		return true;
	return false;
}

/*
 * Assumes that fmm_mutex is locked on entry.
 */
static void aperture_release_area(manageble_aperture_t *app, void *address,
					uint64_t MemorySizeInBytes)
{
	vm_area_t *area;
	uint64_t SizeOfRegion;

	area = vm_find(app, address);
	if (!area)
		return;

	SizeOfRegion = VOID_PTRS_SUB(area->end, area->start) + 1;

	/* check if block is whole region or part of it */
	if (SizeOfRegion == MemorySizeInBytes) {
		vm_remove_area(app, area);
	} else if (SizeOfRegion > MemorySizeInBytes) {
		/* shrink from the start */
		if (area->start == address)
			area->start =
				VOID_PTR_ADD(area->start, MemorySizeInBytes);
		/* shrink from the end */
		else if (VOID_PTRS_SUB(area->end, address) + 1 ==
				MemorySizeInBytes)
			area->end = VOID_PTR_SUB(area->end, MemorySizeInBytes);
		/* split the area */
		else
			vm_split_area(app, area, address, MemorySizeInBytes);
	}
}

/*
 * returns allocated address or NULL. Assumes, that fmm_mutex is locked
 * on entry.
 */
static void *aperture_allocate_area(manageble_aperture_t *app,
					uint64_t MemorySizeInBytes,
					uint64_t offset)
{
	vm_area_t *cur, *next, *new_area, *start;
	void *new_address = NULL;

	next = NULL;
	new_area = NULL;

	cur = app->vm_ranges;
	if (cur) { /* not empty */
		/*
		 * Look up the appropriate address space "hole" or end of
		 * the list
		 */
		while (cur) {
			next = cur->next;

			/* End of the list reached */
			if (!next)
				break;

			/* address space "hole" */
			if ((VOID_PTRS_SUB(next->start, cur->end) >=
							MemorySizeInBytes))
				break;

			cur = next;
		};

		/* If the new range is inside the reserved aperture */
		if (VOID_PTRS_SUB(app->limit, cur->end) + 1 >=
				MemorySizeInBytes) {
			/*
			 * cur points to the last inspected element: the tail
			 * of the list or the found "hole".
			 * Just extend the existing region
			 */
			new_address = VOID_PTR_ADD(cur->end, 1);
			cur->end = VOID_PTR_ADD(cur->end, MemorySizeInBytes);
		} else {
			new_address = NULL;
		}
	} else { /* empty - create the first area */
		/* Some offset from the base */
		start = VOID_PTR_ADD(app->base, offset);
		new_area = vm_create_and_init_area(start,
				VOID_PTR_ADD(start, (MemorySizeInBytes - 1)));
		if (new_area) {
			app->vm_ranges = new_area;
			new_address = new_area->start;
		}
	}

	return new_address;
}

/* returns 0 on success. Assumes, that fmm_mutex is locked on entry */
static int aperture_allocate_object(manageble_aperture_t *app,
					void *new_address,
					uint64_t handle,
					uint64_t MemorySizeInBytes)
{
	vm_object_t *new_object;

	/* Allocate new object */
	new_object = vm_create_and_init_object(new_address,
						MemorySizeInBytes,
						handle);
	if (!new_object)
		return -1;

	/* check for non-empty list */
	if (app->vm_objects != NULL)
		/* Add it before the first element */
		vm_add_object_before(app->vm_objects, new_object);

	app->vm_objects = new_object; /* Update head */

	return 0;
}

static int32_t gpu_mem_find_by_gpu_id(uint32_t gpu_id)
{
	int32_t i;

	for (i = 0 ; i < NUM_OF_SUPPORTED_GPUS ; i++)
		if (gpu_mem[i].gpu_id == gpu_id)
			return i;

	return -1;
}

static manageble_aperture_t *find_valid_gpuvm_apperture_of_gpu(uint32_t gpu_id)
{
	manageble_aperture_t *aperture;
	int32_t gpu_mem_id;

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return NULL;

	aperture = &gpu_mem[gpu_mem_id].gpuvm_aperture;

	/* Check that aperture is properly initialized/supported */
	if (!aperture_is_valid(aperture->base, aperture->limit))
		return NULL;

	return aperture;
}

static int fmm_allocate_memory_in_device(uint32_t gpu_id, void *mem,
						uint64_t MemorySizeInBytes)
{
	struct kfd_ioctl_alloc_memory_of_gpu_args args;
	struct kfd_ioctl_free_memory_of_gpu_args free_args;
	manageble_aperture_t *aperture;

	if (!mem)
		return -1;

	/* Retrieve gpuvm aperture according to gpu_id */
	aperture = find_valid_gpuvm_apperture_of_gpu(gpu_id);
	if (!aperture)
		return -1;

	/* Allocate memory from amdkfd */
	args.gpu_id = gpu_id;
	args.size = MemorySizeInBytes;

	/* va_addr is 40 bit GPUVM address */
	args.va_addr = VOID_PTRS_SUB(mem, aperture->base);

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_ALLOC_MEMORY_OF_GPU, &args))
		return -1;

	/* Allocate object */
	pthread_mutex_lock(&aperture->fmm_mutex);
	if (aperture_allocate_object(aperture, mem, args.handle,
					MemorySizeInBytes))
		goto err_object_allocation_failed;
	pthread_mutex_unlock(&aperture->fmm_mutex);

	return 0;

err_object_allocation_failed:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	free_args.handle = args.handle;
	kmtIoctl(kfd_fd, AMDKFD_IOC_FREE_MEMORY_OF_GPU, &free_args);

	return -1;
}

bool fmm_is_inside_some_aperture(void *address)
{
	int32_t i;

	for (i = 0 ; i < NUM_OF_SUPPORTED_GPUS ; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;
		if ((address >= gpu_mem[i].lds_aperture.base) &&
				(address <= gpu_mem[i].lds_aperture.limit))
			return true;
		if ((address >= gpu_mem[i].gpuvm_aperture.base) &&
				(address <= gpu_mem[i].gpuvm_aperture.limit))
			return true;
		if ((address >= gpu_mem[i].scratch_aperture.base) &&
				(address <= gpu_mem[i].scratch_aperture.limit))
			return true;
	}

	return false;
}

#ifdef DEBUG_PRINT_APERTURE
static void aperture_print(aperture_t *app)
{
	printf("\t Base: %p\n", app->base);
	printf("\t Limit: %p\n", app->limit);
}

static void manageble_aperture_print(manageble_aperture_t *app)
{
	vm_area_t *cur = app->vm_ranges;
	vm_object_t *object = app->vm_objects;

	printf("\t Base: %p\n", app->base);
	printf("\t Limit: %p\n", app->limit);
	printf("\t Ranges:\n");
	while (cur) {
		printf("\t\t Range [%p - %p]\n", cur->start, cur->end);
		cur = cur->next;
	};
	printf("\t Objects:\n");
	while (object) {
		printf("\t\t Object [%p - %" PRIu64 "]\n",
				object->start, object->size);
		object = object->next;
	};
}

void fmm_print(uint32_t gpu_id)
{
	int32_t i = gpu_mem_find_by_gpu_id(gpu_id);

	if (i >= 0) { /* Found */
		printf("LDS aperture:\n");
		aperture_print(&gpu_mem[i].lds_aperture);
		printf("GPUVM aperture:\n");
		manageble_aperture_print(&gpu_mem[i].gpuvm_aperture);
		printf("Scratch aperture:\n");
		manageble_aperture_print(&gpu_mem[i].scratch_aperture);
	}
}
#else
void fmm_print(uint32_t gpu_id)
{
}
#endif


void *fmm_allocate_scratch(uint32_t gpu_id, uint64_t MemorySizeInBytes)
{
	/* Not supported yet */
	return NULL;
}

/*
 * The offset from GPUVM aperture base address to ensure that address 0
 * (after base subtraction) won't be used
 */
#define GPUVM_APP_OFFSET 0x10000
void *fmm_allocate_device(uint32_t gpu_id, uint64_t MemorySizeInBytes)
{
	manageble_aperture_t *aperture;
	int32_t gpu_mem_id;
	void *mem = NULL;

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return NULL;

	aperture = &gpu_mem[gpu_mem_id].gpuvm_aperture;

	/* Check that aperture is properly initialized/supported */
	if (!aperture_is_valid(aperture->base, aperture->limit))
		return NULL;

	/* Allocate address space */
	pthread_mutex_lock(&aperture->fmm_mutex);
	mem = aperture_allocate_area(aperture,
					MemorySizeInBytes, GPUVM_APP_OFFSET);
	pthread_mutex_unlock(&aperture->fmm_mutex);

	/*
	 * Now that we have the area reserved, allocate memory in the device
	 * itself
	 */
	if (fmm_allocate_memory_in_device(gpu_id, mem, MemorySizeInBytes)) {
		/*
		 * allocation of memory in device failed.
		 * Release region in aperture
		 */
		pthread_mutex_lock(&aperture->fmm_mutex);
		aperture_release_area(aperture, mem, MemorySizeInBytes);
		pthread_mutex_unlock(&aperture->fmm_mutex);

		/* Assign NULL to mem to indicate failure to calling function */
		mem = NULL;
	}

	return mem;
}

void *fmm_open_graphic_handle(uint32_t gpu_id,
		int32_t graphic_device_handle,
		uint32_t graphic_handle,
		uint64_t MemorySizeInBytes)
{

	void *mem = NULL;
	int32_t i = gpu_mem_find_by_gpu_id(gpu_id);
	struct kfd_ioctl_open_graphic_handle_args open_graphic_handle_args;
	struct kfd_ioctl_unmap_memory_from_gpu_args unmap_args;

	/* If not found or aperture isn't properly initialized/supported */
	if (i < 0 || !aperture_is_valid(gpu_mem[i].gpuvm_aperture.base,
					gpu_mem[i].gpuvm_aperture.limit))
		return NULL;

	pthread_mutex_lock(&gpu_mem[i].gpuvm_aperture.fmm_mutex);
	/* Allocate address space */
	mem = aperture_allocate_area(&gpu_mem[i].gpuvm_aperture,
					MemorySizeInBytes, GPUVM_APP_OFFSET);
	if (!mem)
		goto out;

	/* Allocate local memory */
	open_graphic_handle_args.gpu_id = gpu_id;
	open_graphic_handle_args.graphic_device_fd = graphic_device_handle;
	open_graphic_handle_args.graphic_handle = graphic_handle;
	open_graphic_handle_args.va_addr =
			VOID_PTRS_SUB(mem, gpu_mem[i].gpuvm_aperture.base);

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_OPEN_GRAPHIC_HANDLE,
			&open_graphic_handle_args))
		goto release_area;

	/* Allocate object */
	if (aperture_allocate_object(&gpu_mem[i].gpuvm_aperture, mem,
					open_graphic_handle_args.handle,
					MemorySizeInBytes))
		goto release_mem;

	pthread_mutex_unlock(&gpu_mem[i].gpuvm_aperture.fmm_mutex);

	/* That's all. Just return the new address */
	return mem;

release_mem:
	unmap_args.handle = open_graphic_handle_args.handle;
	kmtIoctl(kfd_fd, AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU, &unmap_args);
release_area:
	aperture_release_area(&gpu_mem[i].gpuvm_aperture, mem,
				MemorySizeInBytes);
out:
	pthread_mutex_unlock(&gpu_mem[i].gpuvm_aperture.fmm_mutex);

	return NULL;
}

static void __fmm_release(uint32_t gpu_id, void *address,
				uint64_t MemorySizeInBytes)
{
	struct kfd_ioctl_free_memory_of_gpu_args args;
	manageble_aperture_t *aperture;
	vm_object_t *object;

	if (!address)
		return;

	/* Retrieve gpuvm aperture according to gpu_id */
	aperture = find_valid_gpuvm_apperture_of_gpu(gpu_id);
	if (!aperture)
		return;

	pthread_mutex_lock(&aperture->fmm_mutex);

	/* Find the object to retrieve the handle */
	object = vm_find_object_by_address(aperture, address, MemorySizeInBytes);
	if (!object) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return;
	}

	args.handle = object->handle;
	kmtIoctl(kfd_fd, AMDKFD_IOC_FREE_MEMORY_OF_GPU, &args);

	vm_remove_object(aperture, object);
	aperture_release_area(aperture, address, MemorySizeInBytes);

	pthread_mutex_unlock(&aperture->fmm_mutex);
}

void fmm_release(void *address, uint64_t MemorySizeInBytes)
{
	uint32_t i;
	bool found = false;

	for (i = 0 ; i < NUM_OF_SUPPORTED_GPUS && !found ; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;

		if (address >= gpu_mem[i].gpuvm_aperture.base &&
			address <= gpu_mem[i].gpuvm_aperture.limit) {
			found = true;
			__fmm_release(gpu_mem[i].gpu_id, address, MemorySizeInBytes);
			fmm_print(gpu_mem[i].gpu_id);
		}
	}

	/*
	 * If memory address isn't inside of any defined aperture - it refers
	 * to the system memory
	 */
	if (!found)
		free(address);
}

HSAKMT_STATUS fmm_init_process_apertures(void)
{
	struct kfd_ioctl_get_process_apertures_args args;
	uint8_t node_id;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_GET_PROCESS_APERTURES, (void *) &args))
		return HSAKMT_STATUS_ERROR;

	for (node_id = 0 ; node_id < args.num_of_nodes ; node_id++) {
		gpu_mem[node_id].gpu_id =
			args.process_apertures[node_id].gpu_id;

		gpu_mem[node_id].lds_aperture.base =
			PORT_UINT64_TO_VPTR(args.process_apertures[node_id].lds_base);

		gpu_mem[node_id].lds_aperture.limit =
			PORT_UINT64_TO_VPTR(args.process_apertures[node_id].lds_limit);

		gpu_mem[node_id].gpuvm_aperture.base =
			PORT_UINT64_TO_VPTR(args.process_apertures[node_id].gpuvm_base);

		gpu_mem[node_id].gpuvm_aperture.limit =
			PORT_UINT64_TO_VPTR(args.process_apertures[node_id].gpuvm_limit);

		gpu_mem[node_id].scratch_aperture.base =
			PORT_UINT64_TO_VPTR(args.process_apertures[node_id].scratch_base);

		gpu_mem[node_id].scratch_aperture.limit =
			PORT_UINT64_TO_VPTR(args.process_apertures[node_id].scratch_limit);
	}

	return HSAKMT_STATUS_SUCCESS;
}

HSAuint64 fmm_get_aperture_base(aperture_type_e aperture_type, HSAuint32 gpu_id)
{
	int32_t slot = gpu_mem_find_by_gpu_id(gpu_id);

	if (slot < 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	switch (aperture_type) {
	case FMM_GPUVM:
		return aperture_is_valid(gpu_mem[slot].gpuvm_aperture.base,
				gpu_mem[slot].gpuvm_aperture.limit) ?
					PORT_VPTR_TO_UINT64(gpu_mem[slot].gpuvm_aperture.base) : 0;
		break;

	case FMM_SCRATCH:
		return aperture_is_valid(gpu_mem[slot].scratch_aperture.base,
				gpu_mem[slot].scratch_aperture.limit) ?
					PORT_VPTR_TO_UINT64(gpu_mem[slot].scratch_aperture.base) : 0;
		break;

	case FMM_LDS:
		return aperture_is_valid(gpu_mem[slot].lds_aperture.base,
				gpu_mem[slot].lds_aperture.limit) ?
					PORT_VPTR_TO_UINT64(gpu_mem[slot].lds_aperture.base) : 0;
		break;

	default:
		return 0;
	}
}

static int _fmm_map_to_gpu(uint32_t gpu_id, manageble_aperture_t *aperture,
				void *address, uint64_t size,
				uint64_t *gpuvm_address)
{
	struct kfd_ioctl_map_memory_to_gpu_args args;
	vm_object_t *object;

	/* Check that address space was previously reserved */
	if (vm_find(aperture, address) == NULL)
		return -1;

	pthread_mutex_lock(&aperture->fmm_mutex);

	/* Find the object to retrieve the handle */
	object = vm_find_object_by_address(aperture, address, 0);
	if (!object)
		goto err_object_not_found;

	args.handle = object->handle;
	if (kmtIoctl(kfd_fd, AMDKFD_IOC_MAP_MEMORY_TO_GPU, &args))
		goto err_map_ioctl_failed;

	pthread_mutex_unlock(&aperture->fmm_mutex);

	*gpuvm_address = VOID_PTRS_SUB(object->start, aperture->base);

	return 0;

err_map_ioctl_failed:
err_object_not_found:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	*gpuvm_address = 0;
	return -1;
}

int fmm_map_to_gpu(void *address, uint64_t size, uint64_t *gpuvm_address)
{
	int32_t i;
	uint64_t pi;

	/* Find an aperture the requested address belongs to */
	for (i = 0; i < NUM_OF_SUPPORTED_GPUS; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;

		if ((address >= gpu_mem[i].gpuvm_aperture.base) &&
			(address <= gpu_mem[i].gpuvm_aperture.limit))
			/* map it */
			return _fmm_map_to_gpu(gpu_mem[i].gpu_id,
						&gpu_mem[i].gpuvm_aperture,
						address, size, gpuvm_address);
	}

	/*
	 * If address isn't Local memory address, we assume that this is
	 * system memory address accessed through IOMMU. Thus we "prefetch" it
	 */
	for (pi = 0; pi < size / PAGE_SIZE; pi++)
		((char *) address)[pi * PAGE_SIZE] = 0;

	return 0;
}

static int _fmm_unmap_from_gpu(manageble_aperture_t *aperture, void *address)
{
	vm_object_t *object;
	struct kfd_ioctl_unmap_memory_from_gpu_args args;

	pthread_mutex_lock(&aperture->fmm_mutex);

	/* Find the object to retrieve the handle */
	object = vm_find_object_by_address(aperture, address, 0);
	if (!object)
		goto err;

	args.handle = object->handle;
	kmtIoctl(kfd_fd, AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU, &args);

	pthread_mutex_unlock(&aperture->fmm_mutex);

	return 0;
err:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	return -1;
}

int fmm_unmap_from_gpu(void *address)
{
	int32_t i;

	/* Find the aperture the requested address belongs to */
	for (i = 0; i < NUM_OF_SUPPORTED_GPUS; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;

		if ((address >= gpu_mem[i].gpuvm_aperture.base) &&
			(address <= gpu_mem[i].gpuvm_aperture.limit))
			/* unmap it */
			return _fmm_unmap_from_gpu(&gpu_mem[i].gpuvm_aperture,
							address);
	}

	return 0;
}
