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
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/time.h>

#define NON_VALID_GPU_ID 0
#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

#define INIT_MANAGEBLE_APERTURE(base_value, limit_value) {	\
	.base = (void *) base_value,				\
	.limit = (void *) limit_value,				\
	.align = PAGE_SIZE,					\
	.vm_ranges = NULL,					\
	.vm_objects = NULL,					\
	.fmm_mutex = PTHREAD_MUTEX_INITIALIZER			\
	}

struct vm_object {
	void *start;
	void *userptr;
	uint64_t size;
	uint64_t handle; /* opaque */
	struct vm_object *next;
	struct vm_object *prev;
	uint32_t flags; /* memory allocation flags */
	/*
	 * Nodes to map on SVM mGPU
	 */
	uint32_t *device_ids_array;
	uint32_t device_ids_array_size;
};
typedef struct vm_object vm_object_t;

struct vm_area {
	void *start;
	void *end;
	struct vm_area *next;
	struct vm_area *prev;
};
typedef struct vm_area vm_area_t;

/* Memory manager for an aperture */
typedef struct {
	void *base;
	void *limit;
	uint64_t align;
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
	uint32_t device_id;
	uint32_t node_id;
	uint64_t local_mem_size;
	aperture_t lds_aperture;
	manageble_aperture_t scratch_aperture;
	manageble_aperture_t scratch_physical; /* For dGPU, scratch physical
				is allocated from dgpu_aperture. When requested by RT, each
				GPU will get a differnt range */
	manageble_aperture_t gpuvm_aperture; /* used for device mem on APU and for Gfx interop,
						unusable on dGPU with small-ish VA range */
	/* TODO: Merge gpuvm and dgpu apertures. When we have bigger
	 * VA range, we can add a new invisible aperture for invisible
	 * device mem on dGPU. */
} gpu_mem_t;

/* The main structure for GPU Memory Management */
typedef struct {
	/* used for non-coherent system and invisible device mem on dGPU.
	 * This aperture is shared by all dGPUs */
	manageble_aperture_t dgpu_aperture;

	/* used for coherent (fine-grain) system memory on dGPU,
	 * This aperture is shared by all dGPUs */
	manageble_aperture_t dgpu_alt_aperture;
} svm_t;

/* The other apertures are specific to each GPU. gpu_mem_t manages GPU
* specific memory apertures. */
static gpu_mem_t *gpu_mem;
static unsigned int gpu_mem_count;

static void *dgpu_shared_aperture_base = NULL;
static void *dgpu_shared_aperture_limit = NULL;

static svm_t svm = {
	INIT_MANAGEBLE_APERTURE(0, 0),
	INIT_MANAGEBLE_APERTURE(0, 0)
};

/* GPU node array for default mappings */
static uint32_t all_gpu_id_array_size = 0;
static uint32_t *all_gpu_id_array = NULL;

extern int debug_get_reg_status(uint32_t node_id, bool* is_debugged);
static HSAKMT_STATUS dgpu_mem_init(uint32_t node_id, void **base, void **limit);
static int set_dgpu_aperture(uint32_t gpu_id, uint64_t base, uint64_t limit);
static void __fmm_release(void *address,
				uint64_t MemorySizeInBytes, manageble_aperture_t *aperture);
static int _fmm_unmap_from_gpu_scratch(uint32_t gpu_id,
				       manageble_aperture_t *aperture,
				       void *address);

static int32_t find_first_dgpu(HSAuint32 *gpu_id) {
	int32_t i;

	*gpu_id = NON_VALID_GPU_ID;

	for (i = 0; i < NUM_OF_SUPPORTED_GPUS; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;
		if (!topology_is_dgpu(gpu_mem[i].device_id))
			continue;
		*gpu_id = gpu_mem[i].gpu_id;
		return i;
	}

	return -1;
}

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
					      uint64_t handle, uint32_t flags)
{
	vm_object_t *object = (vm_object_t *) malloc(sizeof(vm_object_t));

	if (object) {
		object->start = start;
		object->userptr = NULL;
		object->size = size;
		object->handle = handle;
		object->next = object->prev = NULL;
		object->device_ids_array_size = 0;
		object->flags = flags;
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

	size = ALIGN_UP(size, app->align);

	/* Look up the appropriate address range containing the given address */
	while (cur) {
		if (cur->start == address && (cur->size == size || size == 0))
			break;
		cur = cur->next;
	};

	return cur; /* NULL if not found */
}

static vm_object_t *vm_find_object_by_userptr(manageble_aperture_t *app,
						void *address)
{
	vm_object_t *cur = app->vm_objects;

	/* Look up the appropriate address range containing the given address */
	while (cur) {
		if (cur->userptr == address)
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

	MemorySizeInBytes = ALIGN_UP(MemorySizeInBytes, app->align);

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
static void *aperture_allocate_area_aligned(manageble_aperture_t *app,
					    uint64_t MemorySizeInBytes,
					    uint64_t offset,
					    uint64_t align)
{
	vm_area_t *cur, *next;
	void *start;

	MemorySizeInBytes = ALIGN_UP(MemorySizeInBytes, app->align);

	if (align < app->align)
		align = app->align;

	/* Find a big enough "hole" in the address space */
	cur = NULL;
	next = app->vm_ranges;
	start = (void *)ALIGN_UP((uint64_t)VOID_PTR_ADD(app->base, offset),
				 align);
	while (next) {
		if (next->start > start &&
		    VOID_PTRS_SUB(next->start, start) >= MemorySizeInBytes)
			break;

		cur = next;
		next = next->next;
		start = (void *)ALIGN_UP((uint64_t)cur->end + 1, align);
	}
	if (!next && VOID_PTRS_SUB(app->limit, start) + 1 < MemorySizeInBytes)
		/* No hole found and not enough space after the last area */
		return NULL;

	if (cur && VOID_PTR_ADD(cur->end, 1) == start) {
		/* extend existing area */
		cur->end = VOID_PTR_ADD(start, MemorySizeInBytes-1);
	} else {
		vm_area_t *new_area;
		/* create a new area between cur and next */
		new_area = vm_create_and_init_area(start,
				VOID_PTR_ADD(start, (MemorySizeInBytes - 1)));
		if (!new_area)
			return NULL;
		new_area->next = next;
		new_area->prev = cur;
		if (cur)
			cur->next = new_area;
		else
			app->vm_ranges = new_area;
		if (next)
			next->prev = new_area;
	}

	return start;
}
static void *aperture_allocate_area(manageble_aperture_t *app,
				    uint64_t MemorySizeInBytes,
				    uint64_t offset)
{
	return aperture_allocate_area_aligned(app, MemorySizeInBytes, offset, app->align);
}

/* returns 0 on success. Assumes, that fmm_mutex is locked on entry */
static int aperture_allocate_object(manageble_aperture_t *app,
					void *new_address,
					uint64_t handle,
					uint64_t MemorySizeInBytes,
					uint32_t flags)
{
	vm_object_t *new_object;

	MemorySizeInBytes = ALIGN_UP(MemorySizeInBytes, app->align);

	/* Allocate new object */
	new_object = vm_create_and_init_object(new_address,
					       MemorySizeInBytes,
					       handle, flags);
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
	uint32_t i;

	for (i = 0 ; i < gpu_mem_count ; i++)
		if (gpu_mem[i].gpu_id == gpu_id)
			return i;

	return -1;
}

static int fmm_allocate_memory_in_device(uint32_t gpu_id, void *mem,
						uint64_t MemorySizeInBytes,
						manageble_aperture_t *aperture,
						uint64_t *mmap_offset,
						uint32_t flags)
{
	struct kfd_ioctl_alloc_memory_of_gpu_new_args args;
	struct kfd_ioctl_free_memory_of_gpu_args free_args;

	if (!mem)
		return -1;

	/* Allocate memory from amdkfd */
	args.gpu_id = gpu_id;
	args.size = ALIGN_UP(MemorySizeInBytes, aperture->align);

	args.flags = flags;
	args.va_addr = (uint64_t)mem;
	if (flags == KFD_IOC_ALLOC_MEM_FLAGS_APU_DEVICE)
		args.va_addr = VOID_PTRS_SUB(mem, aperture->base);
	if (flags & KFD_IOC_ALLOC_MEM_FLAGS_USERPTR)
		args.mmap_offset = *mmap_offset;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_ALLOC_MEMORY_OF_GPU_NEW, &args))
		return -1;

	/* Allocate object */
	pthread_mutex_lock(&aperture->fmm_mutex);
	if (aperture_allocate_object(aperture, mem, args.handle,
				     MemorySizeInBytes, flags))
		goto err_object_allocation_failed;
	pthread_mutex_unlock(&aperture->fmm_mutex);

	if (mmap_offset)
		*mmap_offset = args.mmap_offset;

	return 0;

err_object_allocation_failed:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	free_args.handle = args.handle;
	kmtIoctl(kfd_fd, AMDKFD_IOC_FREE_MEMORY_OF_GPU, &free_args);

	return -1;
}

bool fmm_is_inside_some_aperture(void *address)
{
	uint32_t i;

	for (i = 0; i < gpu_mem_count; i++) {
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
	int32_t gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);

	if (gpu_mem_id >= 0) { /* Found */
		printf("LDS aperture:\n");
		aperture_print(&gpu_mem[gpu_mem_id].lds_aperture);
		printf("GPUVM aperture:\n");
		manageble_aperture_print(&gpu_mem[gpu_mem_id].gpuvm_aperture);
		printf("Scratch aperture:\n");
		manageble_aperture_print(&gpu_mem[gpu_mem_id].scratch_aperture);
		printf("Scratch backing memory:\n");
		manageble_aperture_print(&gpu_mem[gpu_mem_id].scratch_physical);
	}

	printf("dGPU aperture:\n");
	manageble_aperture_print(&svm.dgpu_aperture);
	printf("dGPU alt aperture:\n");
	manageble_aperture_print(&svm.dgpu_alt_aperture);

}
#else
void fmm_print(uint32_t gpu_id)
{
}
#endif

static void fmm_release_scratch(uint32_t gpu_id)
{
	int32_t gpu_mem_id;
	uint64_t size;
	vm_object_t *obj;
	manageble_aperture_t *aperture;

	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return;

	aperture = &gpu_mem[gpu_mem_id].scratch_physical;

	size = VOID_PTRS_SUB(aperture->limit, aperture->base) + 1;

	if (topology_is_dgpu(gpu_mem[gpu_mem_id].device_id)) {
		/* unmap and remove all remaining objects */
		pthread_mutex_lock(&aperture->fmm_mutex);
		while ((obj = aperture->vm_objects)) {
			void *obj_addr = obj->start;
			pthread_mutex_unlock(&aperture->fmm_mutex);

			_fmm_unmap_from_gpu_scratch(gpu_id, aperture, obj_addr);

			pthread_mutex_lock(&aperture->fmm_mutex);
		}
		pthread_mutex_unlock(&aperture->fmm_mutex);

		/* release address space */
		pthread_mutex_lock(&svm.dgpu_aperture.fmm_mutex);
		aperture_release_area(&svm.dgpu_aperture,
				      gpu_mem[gpu_mem_id].scratch_physical.base,
				      size);
		pthread_mutex_unlock(&svm.dgpu_aperture.fmm_mutex);
	} else
		/* release address space */
		munmap(gpu_mem[gpu_mem_id].scratch_physical.base, size);

	/* invalidate scratch backing aperture */
	gpu_mem[gpu_mem_id].scratch_physical.base = NULL;
	gpu_mem[gpu_mem_id].scratch_physical.limit = NULL;
}

#define SCRATCH_ALIGN 0x10000
void *fmm_allocate_scratch(uint32_t gpu_id, uint64_t MemorySizeInBytes)
{
	manageble_aperture_t *aperture_phy;
	struct kfd_ioctl_alloc_memory_of_gpu_args args;
	int32_t gpu_mem_id;
	void *mem = NULL;
	uint64_t aligned_size = ALIGN_UP(MemorySizeInBytes, SCRATCH_ALIGN);

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return NULL;

	aperture_phy = &gpu_mem[gpu_mem_id].scratch_physical;
	if (aperture_phy->base != NULL || aperture_phy->limit != NULL)
		/* Scratch was already allocated for this GPU */
		return NULL;

	/* Allocate address space for scratch backing, 64KB aligned */
	if (topology_is_dgpu(gpu_mem[gpu_mem_id].device_id)) {
		pthread_mutex_lock(&svm.dgpu_aperture.fmm_mutex);
		mem = aperture_allocate_area_aligned(
			&svm.dgpu_aperture,
			aligned_size, 0, SCRATCH_ALIGN);
		pthread_mutex_unlock(&svm.dgpu_aperture.fmm_mutex);
	} else {
		uint64_t aligned_padded_size = aligned_size +
			SCRATCH_ALIGN - PAGE_SIZE;
		void *padded_end, *aligned_start, *aligned_end;
		mem = mmap(0, aligned_padded_size,
			   PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
			   -1, 0);
		if (mem == NULL)
			return NULL;
		/* align start and unmap padding */
		padded_end = VOID_PTR_ADD(mem, aligned_padded_size);
		aligned_start = (void *)ALIGN_UP((uint64_t)mem, SCRATCH_ALIGN);
		aligned_end = VOID_PTR_ADD(aligned_start, aligned_size);
		if (aligned_start > mem)
			munmap(mem, VOID_PTRS_SUB(aligned_start, mem));
		if (aligned_end < padded_end)
			munmap(aligned_end,
			       VOID_PTRS_SUB(padded_end, aligned_end));
		mem = aligned_start;
	}

	/* Remember scratch backing aperture for later */
	aperture_phy->base = mem;
	aperture_phy->limit = VOID_PTR_ADD(mem, aligned_size-1);

	/* Allocate memory from amdkfd (just programs SH_HIDDEN_PRIVATE_BASE) */
	args.gpu_id = gpu_id;
	args.size = MemorySizeInBytes;
	args.va_addr = ((uint64_t)mem) >> 16;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_ALLOC_MEMORY_OF_SCRATCH, &args)) {
		fmm_release_scratch(gpu_id);
		return NULL;
	}

	return mem;
}

static void* __fmm_allocate_device(uint32_t gpu_id, uint64_t MemorySizeInBytes,
		manageble_aperture_t *aperture, uint64_t offset, uint64_t *mmap_offset,
		uint32_t flags)
{
	void *mem = NULL;
	/* Check that aperture is properly initialized/supported */
	if (!aperture_is_valid(aperture->base, aperture->limit))
		return NULL;

	/* Allocate address space */
	pthread_mutex_lock(&aperture->fmm_mutex);
	mem = aperture_allocate_area(aperture,
					MemorySizeInBytes, offset);
	pthread_mutex_unlock(&aperture->fmm_mutex);

	/*
	 * Now that we have the area reserved, allocate memory in the device
	 * itself
	 */
	if (fmm_allocate_memory_in_device(gpu_id, mem,
			MemorySizeInBytes, aperture, mmap_offset, flags)) {
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

/*
 * The offset from GPUVM aperture base address to ensure that address 0
 * (after base subtraction) won't be used
 */
#define GPUVM_APP_OFFSET 0x10000
void *fmm_allocate_device(uint32_t gpu_id, uint64_t MemorySizeInBytes)
{
	manageble_aperture_t *aperture;
	int32_t gpu_mem_id;
	uint32_t flags, offset;

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return NULL;

	if (topology_is_dgpu(get_device_id_by_gpu_id(gpu_id))) {
		flags = KFD_IOC_ALLOC_MEM_FLAGS_DGPU_DEVICE;
		/*
		 * TODO: Once VA limit is raised from 0x200000000 (8GB) use gpuvm_aperture.
		 * In that way the host access range won't be used for local memory
		 */
		aperture = &svm.dgpu_aperture;
		offset = 0;
	} else {
		flags = KFD_IOC_ALLOC_MEM_FLAGS_APU_DEVICE;
		aperture = &gpu_mem[gpu_mem_id].gpuvm_aperture;
		offset = GPUVM_APP_OFFSET;
	}

	return __fmm_allocate_device(gpu_id, MemorySizeInBytes,
			aperture, offset, NULL,
			flags);
	/* TODO: honor host access mem flag and map to user mode VM if
	 * needed */
}

static void* fmm_allocate_host_cpu(uint64_t MemorySizeInBytes,
				HsaMemFlags flags)
{
	int err;
	HSAuint64 page_size;
	void *mem = NULL;

	page_size = PageSizeFromFlags(flags.ui32.PageSize);
	err = posix_memalign(&mem, page_size, MemorySizeInBytes);
	if (err != 0)
		return NULL;

	if (flags.ui32.ExecuteAccess) {
		err = mprotect(mem, MemorySizeInBytes,
				PROT_READ | PROT_WRITE | PROT_EXEC);

		if (err != 0) {
			free(mem);
			return NULL;
		}
	}
	return mem;
}

static void* fmm_allocate_host_gpu(uint64_t MemorySizeInBytes,
				   HsaMemFlags flags)
{
	void *mem;
	manageble_aperture_t *aperture;
	uint64_t mmap_offset;
	uint32_t ioc_flags;
	uint32_t size;
	int32_t i;
	uint32_t gpu_id;

	i = find_first_dgpu(&gpu_id);
	if (i < 0)
		return NULL;

	size = MemorySizeInBytes;
	ioc_flags = KFD_IOC_ALLOC_MEM_FLAGS_DGPU_HOST;
	if (flags.ui32.CoarseGrain)
		aperture = &svm.dgpu_aperture;
	else
		aperture = &svm.dgpu_alt_aperture; /* coherent */
	if (flags.ui32.AQLQueueMemory) {
		size = MemorySizeInBytes * 2;
		ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_DGPU_AQL_QUEUE_MEM;
	}

	mem =  __fmm_allocate_device(gpu_id, size,
			aperture, 0, &mmap_offset,
			ioc_flags);

	if (flags.ui32.HostAccess) {
		void *ret = mmap(mem, MemorySizeInBytes,
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_FIXED, kfd_fd , mmap_offset);
		if (ret == MAP_FAILED) {
			__fmm_release(mem, MemorySizeInBytes, aperture);
			return NULL;
		}
		if (flags.ui32.AQLQueueMemory) {
			uint64_t my_buf_size = ALIGN_UP(size, aperture->align) / 2;
			memset(ret, 0, MemorySizeInBytes);
			mmap(VOID_PTR_ADD(mem, my_buf_size), MemorySizeInBytes,
			     PROT_READ | PROT_WRITE,
			     MAP_SHARED | MAP_FIXED, kfd_fd , mmap_offset);
		}
	}


	return mem;
}

void* fmm_allocate_host(uint64_t MemorySizeInBytes, HsaMemFlags flags)
{
	if (is_dgpu)
		return fmm_allocate_host_gpu(MemorySizeInBytes, flags);
	return fmm_allocate_host_cpu(MemorySizeInBytes, flags);
}

void *fmm_open_graphic_handle(uint32_t gpu_id,
		int32_t graphic_device_handle,
		uint32_t graphic_handle,
		uint64_t MemorySizeInBytes)
{

	void *mem = NULL;
	int32_t i = gpu_mem_find_by_gpu_id(gpu_id);
	struct kfd_ioctl_open_graphic_handle_args open_graphic_handle_args;
	struct kfd_ioctl_unmap_memory_from_gpu_new_args unmap_args;

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
					MemorySizeInBytes, 0))
		goto release_mem;

	pthread_mutex_unlock(&gpu_mem[i].gpuvm_aperture.fmm_mutex);

	/* That's all. Just return the new address */
	return mem;

release_mem:
	unmap_args.handle = open_graphic_handle_args.handle;
	unmap_args.device_ids_array = NULL;
	unmap_args.device_ids_array_size = 0;
	kmtIoctl(kfd_fd, AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU_NEW, &unmap_args);
release_area:
	aperture_release_area(&gpu_mem[i].gpuvm_aperture, mem,
				MemorySizeInBytes);
out:
	pthread_mutex_unlock(&gpu_mem[i].gpuvm_aperture.fmm_mutex);

	return NULL;
}

static void __fmm_release(void *address,
				uint64_t MemorySizeInBytes, manageble_aperture_t *aperture)
{
	struct kfd_ioctl_free_memory_of_gpu_args args;
	vm_object_t *object;

	if (!address)
		return;

	pthread_mutex_lock(&aperture->fmm_mutex);

	/* Find the object to retrieve the handle */
	object = vm_find_object_by_address(aperture, address, 0);
	if (!object) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return;
	}

	if (object->device_ids_array_size > 0)
		free(object->device_ids_array);

	if (address >= dgpu_shared_aperture_base &&
	    address <= dgpu_shared_aperture_limit) {
		/* Remove any CPU mapping, but keep the address range reserved */
		mmap(address, object->size, PROT_NONE,
		     MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE | MAP_FIXED, -1, 0);
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

	for (i = 0; i < gpu_mem_count && !found; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;
		if (address >= gpu_mem[i].scratch_physical.base &&
			address <= gpu_mem[i].scratch_physical.limit) {
			fmm_release_scratch(gpu_mem[i].gpu_id);
			return;
		}

		if (address >= gpu_mem[i].gpuvm_aperture.base &&
			address <= gpu_mem[i].gpuvm_aperture.limit) {
			found = true;
			__fmm_release(address,
					MemorySizeInBytes, &gpu_mem[i].gpuvm_aperture);
			fmm_print(gpu_mem[i].gpu_id);
		}
	}

	if (!found) {
		if (address >= svm.dgpu_aperture.base &&
			address <= svm.dgpu_aperture.limit) {
			found = true;
			__fmm_release(address,
					MemorySizeInBytes, &svm.dgpu_aperture);
			fmm_print(gpu_mem[i].gpu_id);
		}
		else if (address >= svm.dgpu_alt_aperture.base &&
			address <= svm.dgpu_alt_aperture.limit) {
			found = true;
			__fmm_release(address,
					MemorySizeInBytes, &svm.dgpu_alt_aperture);
			fmm_print(gpu_mem[i].gpu_id);
		}
	}

	/*
	 * If memory address isn't inside of any defined aperture - it refers
	 * to the system memory
	 */
	if (!found) {
		free(address);
	}
}

static int fmm_set_memory_policy(uint32_t gpu_id, int default_policy, int alt_policy,
				 uintptr_t alt_base, uint64_t alt_size)
{
	struct kfd_ioctl_set_memory_policy_args args;

	args.gpu_id = gpu_id;
	args.default_policy = default_policy;
	args.alternate_policy = alt_policy;
	args.alternate_aperture_base = alt_base;
	args.alternate_aperture_size = alt_size;

	return kmtIoctl(kfd_fd, AMDKFD_IOC_SET_MEMORY_POLICY, &args);
}

HSAKMT_STATUS fmm_init_process_apertures(unsigned int NumNodes)
{
	struct kfd_ioctl_get_process_apertures_new_args args;
	uint32_t i = 0;
	int32_t gpu_mem_id =0;
	uint32_t gpu_id;
	HsaNodeProperties props;
	struct kfd_process_device_apertures * process_apertures;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	/* Trade off - NumNodes includes GPU nodes + CPU Node. So in
	 *	systems with CPU node, slightly more memory is allocated than
	 *	necessary*/
	gpu_mem = (gpu_mem_t *)calloc(NumNodes, sizeof(gpu_mem_t));
	if (gpu_mem == NULL)
		return HSAKMT_STATUS_NO_MEMORY;

	/* Initialize gpu_mem[] from sysfs topology. Rest of the members are set to
	 * 0 by calloc. This is necessary because this function
	 * gets called before hsaKmtAcquireSystemProperties() is called.*/
	gpu_mem_count = 0;
	while (i < NumNodes) {
		ret = topology_sysfs_get_node_props(i, &props, &gpu_id);
		if (ret != HSAKMT_STATUS_SUCCESS)
			goto sysfs_parse_failed;

		/* Skip non-GPU nodes */
		if (gpu_id != 0) {
			gpu_mem[gpu_mem_count].gpu_id = gpu_id;
			gpu_mem[gpu_mem_count].local_mem_size = props.LocalMemSize;
			gpu_mem[gpu_mem_count].device_id = props.DeviceId;
			gpu_mem[gpu_mem_count].node_id = i;
			gpu_mem[gpu_mem_count].scratch_physical.align = PAGE_SIZE;
			pthread_mutex_init(&gpu_mem[gpu_mem_count].scratch_physical.fmm_mutex, NULL);
			gpu_mem[gpu_mem_count].scratch_aperture.align = PAGE_SIZE;
			pthread_mutex_init(&gpu_mem[gpu_mem_count].scratch_aperture.fmm_mutex, NULL);
			gpu_mem[gpu_mem_count].gpuvm_aperture.align = PAGE_SIZE;
			pthread_mutex_init(&gpu_mem[gpu_mem_count].gpuvm_aperture.fmm_mutex, NULL);
			gpu_mem_count++;
		}
		i++;
	}

	/* The ioctl will also return Number of Nodes if args.kfd_process_device_apertures_ptr
	* is set to NULL. This is not required since Number of nodes is already known. Kernel
	* will fill in the apertures in kfd_process_device_apertures_ptr */
	process_apertures = malloc(gpu_mem_count * sizeof(struct kfd_process_device_apertures));
	if (process_apertures == NULL) {
		ret = HSAKMT_STATUS_NO_MEMORY;
		goto sysfs_parse_failed;
	}

	args.kfd_process_device_apertures_ptr = (uintptr_t)process_apertures;
	args.num_of_nodes = gpu_mem_count;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_GET_PROCESS_APERTURES_NEW, (void *)&args)) {
		ret = HSAKMT_STATUS_ERROR;
		goto get_aperture_ioctl_failed;
	}

	all_gpu_id_array_size = 0;
	all_gpu_id_array = NULL;
	if (args.num_of_nodes > 0) {
		all_gpu_id_array = malloc(sizeof(uint32_t) * args.num_of_nodes);
		if (all_gpu_id_array == NULL) {
			ret = HSAKMT_STATUS_NO_MEMORY;
			goto get_aperture_ioctl_failed;
		}
	}

	for (i = 0 ; i < args.num_of_nodes ; i++) {
		/* Map Kernel process device data node i <--> gpu_mem_id which indexes into gpu_mem[]
		 * based on gpu_id */
		gpu_mem_id = gpu_mem_find_by_gpu_id(process_apertures[i].gpu_id);
		if (gpu_mem_id < 0) {
			ret = HSAKMT_STATUS_ERROR;
			goto invalid_gpu_id;
		}

		all_gpu_id_array[i] = process_apertures[i].gpu_id;
		all_gpu_id_array_size += sizeof(uint32_t);

		gpu_mem[gpu_mem_id].lds_aperture.base =
			PORT_UINT64_TO_VPTR(process_apertures[i].lds_base);

		gpu_mem[gpu_mem_id].lds_aperture.limit =
			PORT_UINT64_TO_VPTR(process_apertures[i].lds_limit);

		gpu_mem[gpu_mem_id].gpuvm_aperture.base =
			PORT_UINT64_TO_VPTR(process_apertures[i].gpuvm_base);

		gpu_mem[gpu_mem_id].gpuvm_aperture.limit =
			PORT_UINT64_TO_VPTR(process_apertures[i].gpuvm_limit);

		gpu_mem[gpu_mem_id].scratch_aperture.base =
			PORT_UINT64_TO_VPTR(process_apertures[i].scratch_base);

		gpu_mem[gpu_mem_id].scratch_aperture.limit =
			PORT_UINT64_TO_VPTR(process_apertures[i].scratch_limit);

		if (topology_is_dgpu(gpu_mem[gpu_mem_id].device_id)) {
			uintptr_t alt_base;
			uint64_t alt_size;
			int err;
			uint64_t vm_alignment = PAGE_SIZE;

			if (gpu_mem[gpu_mem_id].device_id >= 0x6920 &&
			    gpu_mem[gpu_mem_id].device_id <= 0x6939)
				/* Workaround for Tonga GPUVM HW bug */
				vm_alignment = TONGA_PAGE_SIZE;

			dgpu_mem_init(gpu_mem_id, &svm.dgpu_aperture.base,
					&svm.dgpu_aperture.limit);

			/* Set proper alignment for scratch backing aperture */
			gpu_mem[gpu_mem_id].scratch_physical.align = vm_alignment;

			/* Set kernel process dgpu aperture. */
			set_dgpu_aperture(process_apertures[i].gpu_id,
				(uint64_t)svm.dgpu_aperture.base,
				(uint64_t)svm.dgpu_aperture.limit);
			svm.dgpu_aperture.align = vm_alignment;

			/* Place GPUVM aperture after dGPU aperture
				* (FK: I think this is broken but leaving it for now) */
			gpu_mem[gpu_mem_id].gpuvm_aperture.base = VOID_PTR_ADD(svm.dgpu_aperture.limit, 1);
			gpu_mem[gpu_mem_id].gpuvm_aperture.limit = (void *)VOID_PTRS_SUB(svm.dgpu_aperture.limit,
					svm.dgpu_aperture.base);
			gpu_mem[gpu_mem_id].gpuvm_aperture.limit = VOID_PTR_ADD(gpu_mem[gpu_mem_id].gpuvm_aperture.limit,
				(unsigned long)gpu_mem[gpu_mem_id].gpuvm_aperture.base);
			gpu_mem[gpu_mem_id].gpuvm_aperture.align = vm_alignment;

			/* Use the first 1/4 of the dGPU aperture as
				* alternate aperture for coherent access.
				* Base and size must be 64KB aligned. */
			alt_base = (uintptr_t)svm.dgpu_aperture.base;
			alt_size = (VOID_PTRS_SUB(svm.dgpu_aperture.limit,
				svm.dgpu_aperture.base) + 1) >> 2;
			alt_base = (alt_base + 0xffff) & ~0xffffULL;
			alt_size = (alt_size + 0xffff) & ~0xffffULL;
			svm.dgpu_alt_aperture.base = (void *)alt_base;
			svm.dgpu_alt_aperture.limit = (void *)(alt_base + alt_size - 1);
			svm.dgpu_aperture.base = VOID_PTR_ADD(svm.dgpu_alt_aperture.limit, 1);
			err = fmm_set_memory_policy(gpu_mem[gpu_mem_id].gpu_id,
							KFD_IOC_CACHE_POLICY_NONCOHERENT,
							KFD_IOC_CACHE_POLICY_COHERENT,
							alt_base, alt_size);
			if (err != 0) {
				fprintf(stderr, "Error! Failed to set alt aperture for GPU [0x%x]\n", gpu_mem[gpu_mem_id].gpu_id);
				ret = HSAKMT_STATUS_ERROR;
			}
			svm.dgpu_alt_aperture.align = vm_alignment;
		}
	}

	free(process_apertures);
	return ret;

get_aperture_ioctl_failed:
invalid_gpu_id :
	free(process_apertures);
sysfs_parse_failed:
	fmm_destroy_process_apertures();
	return ret;
}

void fmm_destroy_process_apertures(void)
{
	if (gpu_mem) {
		free(gpu_mem);
		gpu_mem = NULL;
	}
	gpu_mem_count = 0;
}

HSAKMT_STATUS fmm_get_aperture_base_and_limit(aperture_type_e aperture_type, HSAuint32 gpu_id,
			HSAuint64 *aperture_base, HSAuint64 *aperture_limit)
{
	HSAKMT_STATUS err = HSAKMT_STATUS_SUCCESS;
	int32_t slot = gpu_mem_find_by_gpu_id(gpu_id);

	if (slot < 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	switch (aperture_type) {
	case FMM_GPUVM:
		if (aperture_is_valid(gpu_mem[slot].gpuvm_aperture.base,
			gpu_mem[slot].gpuvm_aperture.limit)) {
			*aperture_base = PORT_VPTR_TO_UINT64(gpu_mem[slot].gpuvm_aperture.base);
			*aperture_limit = PORT_VPTR_TO_UINT64(gpu_mem[slot].gpuvm_aperture.limit);
		}
		break;

	case FMM_SCRATCH:
		if (aperture_is_valid(gpu_mem[slot].scratch_aperture.base,
			gpu_mem[slot].scratch_aperture.limit)) {
			*aperture_base = PORT_VPTR_TO_UINT64(gpu_mem[slot].scratch_aperture.base);
			*aperture_limit = PORT_VPTR_TO_UINT64(gpu_mem[slot].scratch_aperture.limit);
		}
		break;

	case FMM_LDS:
		if (aperture_is_valid(gpu_mem[slot].lds_aperture.base,
			gpu_mem[slot].lds_aperture.limit)) {
			*aperture_base = PORT_VPTR_TO_UINT64(gpu_mem[slot].lds_aperture.base);
			*aperture_limit = PORT_VPTR_TO_UINT64(gpu_mem[slot].lds_aperture.limit);
		}
		break;

	default:
		err = HSAKMT_STATUS_ERROR;
	}

	return err;
}

static int _fmm_map_to_gpu_gtt(manageble_aperture_t *aperture,
				void *address, uint64_t size)
{
	struct kfd_ioctl_map_memory_to_gpu_new_args args;
	vm_object_t *object;

	pthread_mutex_lock(&aperture->fmm_mutex);

	/* Find the object to retrieve the handle */
	object = vm_find_object_by_address(aperture, address, 0);
	if (!object) {
		goto err_object_not_found;
	}

	args.handle = object->handle;
	if (object->device_ids_array_size > 0) {
		args.device_ids_array = object->device_ids_array;
		args.device_ids_array_size = object->device_ids_array_size;
	} else if ((object->flags & KFD_IOC_ALLOC_MEM_FLAGS_DGPU_HOST) ||
		   (object->flags & KFD_IOC_ALLOC_MEM_FLAGS_USERPTR)) {
		/* Only enable multi-GPU mapping on host memory for now */
		args.device_ids_array = all_gpu_id_array;
		args.device_ids_array_size = all_gpu_id_array_size;
	} else {
		args.device_ids_array = NULL;
		args.device_ids_array_size = 0;
	}
	if (kmtIoctl(kfd_fd, AMDKFD_IOC_MAP_MEMORY_TO_GPU_NEW, &args))
		goto err_map_ioctl_failed;

	pthread_mutex_unlock(&aperture->fmm_mutex);

	return 0;

err_map_ioctl_failed:
err_object_not_found:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	return -1;
}

static int _fmm_map_to_gpu_scratch(uint32_t gpu_id, manageble_aperture_t *aperture,
				   void *address, uint64_t size)
{
	int32_t gpu_mem_id;
	uint64_t offset;
	void *mem = NULL;
	int ret;
	bool is_debugger = 0;
	void *mmap_ret = NULL;
	uint64_t mmap_offset = 0;
	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return -1;

        if (!topology_is_dgpu(gpu_mem[gpu_mem_id].device_id))
		return 0; /* Nothing to do on APU */

	/* sanity check the address */
	if (address < aperture->base ||
	    VOID_PTR_ADD(address, size -1) > aperture->limit)
		return -1;

	ret = debug_get_reg_status(gpu_mem[gpu_mem_id].node_id, &is_debugger);
	/* allocate object within the scratch backing aperture */
	if (!ret && !is_debugger) {
		offset = VOID_PTRS_SUB(address, aperture->base);
		mem = __fmm_allocate_device(gpu_id, size, aperture, offset,
				NULL, KFD_IOC_ALLOC_MEM_FLAGS_DGPU_DEVICE);
		if (mem == NULL)
			return -1;

		if (mem != address) {
			fprintf(stderr,
				"Got unexpected address for scratch mapping.\n"
				"  expected: %p\n"
				"  got:      %p\n", address, mem);
			__fmm_release(mem, size, aperture);
			return -1;
		}
	} else {
		fmm_allocate_memory_in_device(gpu_id,
					address,
					size,
					aperture,
					&mmap_offset,
					KFD_IOC_ALLOC_MEM_FLAGS_DGPU_HOST);
		mmap_ret = mmap(address, size,
				PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_FIXED,
				kfd_fd, mmap_offset);
		if (mmap_ret == MAP_FAILED) {
			__fmm_release(mem, size, aperture);
			return -1;
		}
	}

	/* map to GPU */
	ret = _fmm_map_to_gpu_gtt(aperture, address, size);
	if (ret != 0)
		__fmm_release(mem, size, aperture);

	return ret;
}

static int _fmm_map_to_gpu(uint32_t gpu_id, manageble_aperture_t *aperture,
				void *address, uint64_t size,
				uint64_t *gpuvm_address)
{
	struct kfd_ioctl_map_memory_to_gpu_new_args args;
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
	args.device_ids_array = object->device_ids_array;
	args.device_ids_array_size = object->device_ids_array_size;
	if (kmtIoctl(kfd_fd, AMDKFD_IOC_MAP_MEMORY_TO_GPU_NEW, &args))
		goto err_map_ioctl_failed;

	pthread_mutex_unlock(&aperture->fmm_mutex);

	if (gpuvm_address) {
		*gpuvm_address = (uint64_t)object->start;
		if (!topology_is_dgpu(get_device_id_by_gpu_id(gpu_id)))
			*gpuvm_address = VOID_PTRS_SUB(object->start, aperture->base);
	}

	return 0;

err_map_ioctl_failed:
err_object_not_found:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	*gpuvm_address = 0;
	return -1;
}

static int _fmm_map_to_gpu_userptr(void *addr, uint64_t size,
				   uint64_t *gpuvm_addr)
{
	manageble_aperture_t *aperture;
	vm_object_t *obj;
	void *svm_addr;
	HSAuint64 svm_size;
	HSAuint32 page_offset = (HSAuint64)addr & (PAGE_SIZE-1);
	int ret;

	aperture = &svm.dgpu_aperture;

	/* Find the start address in SVM space for GPU mapping */
	pthread_mutex_lock(&aperture->fmm_mutex);
	obj = vm_find_object_by_userptr(aperture, addr);
	if (obj == NULL) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return HSAKMT_STATUS_ERROR;
	}
	svm_addr = obj->start;
	svm_size = obj->size;
	pthread_mutex_unlock(&aperture->fmm_mutex);

	/* Map and return the GPUVM address adjusted by the offset
	 * from the start of the page */
	ret = _fmm_map_to_gpu_gtt(aperture, svm_addr, svm_size);
	if (ret == 0 && gpuvm_addr)
		*gpuvm_addr = (uint64_t)svm_addr + page_offset;

	return ret;
}

int fmm_map_to_gpu(void *address, uint64_t size, uint64_t *gpuvm_address)
{
	uint32_t i;
	uint64_t pi;

	/* Find an aperture the requested address belongs to */
	for (i = 0; i < gpu_mem_count; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;

		if ((address >= gpu_mem[i].scratch_physical.base) &&
			(address <= gpu_mem[i].scratch_physical.limit))
			return _fmm_map_to_gpu_scratch(gpu_mem[i].gpu_id,
							&gpu_mem[i].scratch_physical,
							address, size);

		if ((address >= gpu_mem[i].gpuvm_aperture.base) &&
			(address <= gpu_mem[i].gpuvm_aperture.limit))
			/* map it */
			return _fmm_map_to_gpu(gpu_mem[i].gpu_id,
						&gpu_mem[i].gpuvm_aperture,
						address, size, gpuvm_address);
	}

	if ((address >= svm.dgpu_aperture.base) &&
		(address <= svm.dgpu_aperture.limit))
		/* map it */
		return _fmm_map_to_gpu_gtt(&svm.dgpu_aperture,
						address, size);
	else if ((address >= svm.dgpu_alt_aperture.base) &&
		(address <= svm.dgpu_alt_aperture.limit))
		/* map it */
		return _fmm_map_to_gpu_gtt(&svm.dgpu_alt_aperture,
						address, size);

	/*
	 * If address isn't an SVM memory address, we assume that this
	 * is system memory address. On dGPU we need to map it,
	 * assuming it was previously registered.
	 */
	if (is_dgpu)
		/* TODO: support mixed APU and dGPU configurations */
		return _fmm_map_to_gpu_userptr(address, size, gpuvm_address);

	/*
	 * On an APU a system memory address is accessed through
	 * IOMMU. Thus we "prefetch" it.
	 */
	for (pi = 0; pi < size / PAGE_SIZE; pi++)
		((char *) address)[pi * PAGE_SIZE] = 0;

	return 0;
}

static int _fmm_unmap_from_gpu(manageble_aperture_t *aperture, void *address)
{
	vm_object_t *object;
	struct kfd_ioctl_unmap_memory_from_gpu_new_args args;

	pthread_mutex_lock(&aperture->fmm_mutex);

	/* Find the object to retrieve the handle */
	object = vm_find_object_by_address(aperture, address, 0);
	if (!object)
		goto err;

	args.handle = object->handle;
	if (object->device_ids_array_size > 0) {
		args.device_ids_array = object->device_ids_array;
		args.device_ids_array_size = object->device_ids_array_size;
	} else if ((object->flags & KFD_IOC_ALLOC_MEM_FLAGS_DGPU_HOST) ||
		   (object->flags & KFD_IOC_ALLOC_MEM_FLAGS_USERPTR)) {
		/* Only enable multi-GPU mapping on host memory for now */
		args.device_ids_array = all_gpu_id_array;
		args.device_ids_array_size = all_gpu_id_array_size;
	} else {
		args.device_ids_array = NULL;
		args.device_ids_array_size = 0;
	}
	kmtIoctl(kfd_fd, AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU_NEW, &args);

	pthread_mutex_unlock(&aperture->fmm_mutex);

	return 0;
err:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	return -1;
}

static int _fmm_unmap_from_gpu_scratch(uint32_t gpu_id,
				       manageble_aperture_t *aperture,
				       void *address)
{
	int32_t gpu_mem_id;
	vm_object_t *object;
	uint64_t size;
	struct kfd_ioctl_unmap_memory_from_gpu_new_args args;

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return -1;

        if (!topology_is_dgpu(gpu_mem[gpu_mem_id].device_id))
		return 0; /* Nothing to do on APU */

	pthread_mutex_lock(&aperture->fmm_mutex);

	/* Find the object to retrieve the handle and size */
	object = vm_find_object_by_address(aperture, address, 0);
	if (!object)
		goto err;

	size = object->size;

	/* unmap from GPU */
	args.handle = object->handle;
	args.device_ids_array = object->device_ids_array;
	args.device_ids_array_size = object->device_ids_array_size;
	kmtIoctl(kfd_fd, AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU_NEW, &args);

	pthread_mutex_unlock(&aperture->fmm_mutex);

	/* free object in scratch backing aperture */
	__fmm_release(address, size, aperture);

	return 0;

err:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	return -1;
}

static int _fmm_unmap_from_gpu_userptr(void *addr)
{
	manageble_aperture_t *aperture;
	vm_object_t *obj;
	void *svm_addr;

	aperture = &svm.dgpu_aperture;

	/* Find the start address in SVM space for GPU unmapping */
	pthread_mutex_lock(&aperture->fmm_mutex);
	obj = vm_find_object_by_userptr(aperture, addr);
	if (obj == NULL) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return HSAKMT_STATUS_ERROR;
	}
	svm_addr = obj->start;
	pthread_mutex_unlock(&aperture->fmm_mutex);

	/* Unmap */
	return _fmm_unmap_from_gpu(aperture, svm_addr);
}

int fmm_unmap_from_gpu(void *address)
{
	uint32_t i;

	/* Find the aperture the requested address belongs to */
	for (i = 0; i < gpu_mem_count; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;

		if ((address >= gpu_mem[i].scratch_physical.base) &&
			(address <= gpu_mem[i].scratch_physical.limit))
			return _fmm_unmap_from_gpu_scratch(gpu_mem[i].gpu_id,
							&gpu_mem[i].scratch_physical,
							address);

		if ((address >= gpu_mem[i].gpuvm_aperture.base) &&
			(address <= gpu_mem[i].gpuvm_aperture.limit))
			/* unmap it */
			return _fmm_unmap_from_gpu(&gpu_mem[i].gpuvm_aperture,
							address);
	}

	if ((address >= svm.dgpu_aperture.base) &&
		(address <= svm.dgpu_aperture.limit))
		/* unmap it */
		return _fmm_unmap_from_gpu(&svm.dgpu_aperture,
							address);
	else if ((address >= svm.dgpu_alt_aperture.base) &&
		(address <= svm.dgpu_alt_aperture.limit))
		/* unmap it */
		return _fmm_unmap_from_gpu(&svm.dgpu_alt_aperture,
							address);

	/*
	 * If address isn't an SVM address, we assume that this is
	 * system memory address.
	 */
	if (is_dgpu)
		/* TODO: support mixed APU and dGPU configurations */
		return _fmm_unmap_from_gpu_userptr(address);

	return 0;
}

/* Tonga dGPU specific functions */
static bool is_dgpu_mem_init = false;

static int set_dgpu_aperture(uint32_t gpu_id, uint64_t base, uint64_t limit)
{
	struct kfd_ioctl_set_process_dgpu_aperture_args args;

	args.gpu_id = gpu_id;
	args.dgpu_base = base;
	args.dgpu_limit = limit;

	return kmtIoctl(kfd_fd, AMDKFD_IOC_SET_PROCESS_DGPU_APERTURE, &args);
}

static void *reserve_address(void *addr, long long unsigned int len)
{
	void *ret_addr;

	if (len <= 0)
		return NULL;

	ret_addr = mmap(addr, len, PROT_NONE,
				 MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
	if (addr == MAP_FAILED)
		return NULL;

	return ret_addr;
}

#define ADDRESS_RANGE_LIMIT_MASK 0xFFFFFFFFFF
#define AMDGPU_SYSFS_VM_SIZE "/sys/module/amdgpu/parameters/vm_size"

/*
 * TODO: Provide a cleaner interface via topology
 */
static HSAKMT_STATUS get_dgpu_vm_limit(uint32_t *vm_size_in_gb)
{
	FILE *fd;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	fd = fopen(AMDGPU_SYSFS_VM_SIZE, "r");
	if (!fd)
		return HSAKMT_STATUS_ERROR;
	if (fscanf(fd, "%ul", vm_size_in_gb) != 1) {
		ret = HSAKMT_STATUS_ERROR;
		goto err;
	}

err:
	fclose(fd);
	return ret;
}

static HSAKMT_STATUS dgpu_mem_init(uint32_t gpu_mem_id, void **base, void **limit)
{
	bool found;
	HSAKMT_STATUS ret;
	void *addr, *ret_addr;
	HSAuint64 len, vm_limit, max_vm_limit, min_vm_size;
	uint32_t max_vm_limit_in_gb;

	if (is_dgpu_mem_init) {
		if (base)
			*base = dgpu_shared_aperture_base;
		if (limit)
			*limit = dgpu_shared_aperture_limit;
		return HSAKMT_STATUS_SUCCESS;
	}

	ret = get_dgpu_vm_limit(&max_vm_limit_in_gb);
	if (ret != HSAKMT_STATUS_SUCCESS) {
		fprintf(stderr,
			"Unable to find vm_size for dGPU, assuming 64GB.\n");
		max_vm_limit_in_gb = 64;
	}
	max_vm_limit = ((HSAuint64)max_vm_limit_in_gb << 30) - 1;
	min_vm_size = (HSAuint64)4 << 30;

	found = false;

	for (len = max_vm_limit+1; !found && len >= min_vm_size; len >>= 1) {
		for (addr = (void *)TONGA_PAGE_SIZE, ret_addr = NULL;
		     (HSAuint64)addr + (len >> 1) < max_vm_limit;
		     addr = (void *)((HSAuint64)addr + TONGA_PAGE_SIZE)) {
			ret_addr = reserve_address(addr, len);
			if (!ret_addr)
				break;
			if ((HSAuint64)ret_addr + (len>>1) < max_vm_limit)
				/* At least half the returned address
				 * space is GPU addressable, we'll
				 * take it */
				break;
			munmap (ret_addr, len);
		}
		if (!ret_addr) {
			fprintf(stderr,
				"Failed to reserve %uGB for SVM ...\n",
				(unsigned)(len >> 30));
			continue;
		}
		if ((HSAuint64)ret_addr + min_vm_size - 1 > max_vm_limit) {
			/* addressable size is less than the minimum */
			fprintf(stderr,
				"Got %uGB for SVM at %p with only %dGB usable ...\n",
				(unsigned)(len >> 30), ret_addr,
				(int)(((HSAint64)max_vm_limit -
				       (HSAint64)ret_addr) >> 30));
			munmap(ret_addr, len);
			continue;
		} else {
			found = true;
			break;
		}
	}

	if (!found) {
		fprintf(stderr,
			"Failed to reserve SVM address range. Giving up.\n");
		return HSAKMT_STATUS_ERROR;
	}

	vm_limit = (HSAuint64)ret_addr + len - 1;
	if (vm_limit > max_vm_limit) {
		/* trim the tail that's not GPU-addressable */
		munmap((void *)(max_vm_limit + 1), vm_limit - max_vm_limit);
		vm_limit = max_vm_limit;
	}

	if (base)
		*base = ret_addr;
	dgpu_shared_aperture_base = ret_addr;
	if (limit)
		*limit = (void *)vm_limit;
	dgpu_shared_aperture_limit = (void *)vm_limit;
	is_dgpu_mem_init = true;

	return HSAKMT_STATUS_SUCCESS;
}

bool fmm_get_handle(void *address, uint64_t *handle)
{
	uint32_t i;
	manageble_aperture_t *aperture;
	vm_object_t *object;
	bool found;

	found = false;
	aperture = NULL;

	/* Find the aperture the requested address belongs to */
	for (i = 0; i < gpu_mem_count; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;

		if ((address >= gpu_mem[i].gpuvm_aperture.base) &&
			(address <= gpu_mem[i].gpuvm_aperture.limit)) {
			aperture = &gpu_mem[i].gpuvm_aperture;
			break;
		}
	}

	if (!aperture) {
		if ((address >= svm.dgpu_aperture.base) &&
			(address <= svm.dgpu_aperture.limit)) {
			aperture = &svm.dgpu_aperture;
		}
		else if ((address >= svm.dgpu_alt_aperture.base) &&
			(address <= svm.dgpu_alt_aperture.limit)) {
			aperture = &svm.dgpu_alt_aperture;
		}
	}

	if (!aperture)
		return false;

	pthread_mutex_lock(&aperture->fmm_mutex);
	/* Find the object to retrieve the handle */
	object = vm_find_object_by_address(aperture, address, 0);
	if (object && handle) {
		*handle = object->handle;
		found = true;
	}
	pthread_mutex_unlock(&aperture->fmm_mutex);


	return found;
}

static HSAKMT_STATUS fmm_register_user_memory(void *addr, HSAuint64 size, vm_object_t **obj_ret)
{
	int32_t i;
	HSAuint32 gpu_id;
	manageble_aperture_t *aperture;
	void *svm_addr = NULL;
	vm_object_t *obj;
	HSAuint32 page_offset = (HSAuint64)addr & (PAGE_SIZE-1);
	HSAuint64 aligned_addr = (HSAuint64)addr - page_offset;
	HSAuint64 aligned_size = PAGE_ALIGN_UP(page_offset + size);

	/* Find first dGPU for creating the userptr BO */
	i = find_first_dgpu(&gpu_id);
	if (i < 0)
		return HSAKMT_STATUS_ERROR;
	aperture = &svm.dgpu_aperture;

	/* Check if this address was already registered */
	pthread_mutex_lock(&aperture->fmm_mutex);
	obj = vm_find_object_by_userptr(aperture, addr);
	pthread_mutex_unlock(&aperture->fmm_mutex);
	if (obj != NULL)
		return HSAKMT_STATUS_MEMORY_ALREADY_REGISTERED;

	/* Allocate BO, userptr address is passed in mmap_offset */
	svm_addr = __fmm_allocate_device(gpu_id, aligned_size, aperture, 0,
					 &aligned_addr, KFD_IOC_ALLOC_MEM_FLAGS_USERPTR);
	if (svm_addr == NULL)
		return HSAKMT_STATUS_ERROR;

	/* Find the object and set its userptr address */
	pthread_mutex_lock(&aperture->fmm_mutex);
	obj = vm_find_object_by_address(aperture, svm_addr, size);
	if (obj == NULL) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return HSAKMT_STATUS_ERROR;
	}
	obj->userptr = addr;
	pthread_mutex_unlock(&aperture->fmm_mutex);

	if (obj_ret)
		*obj_ret = obj;
	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS fmm_register_memory(void *address, uint64_t size_in_bytes,
				  uint32_t *gpu_id_array,
				  uint32_t gpu_id_array_size)
{
	manageble_aperture_t *aperture;
	vm_object_t *object = NULL;
	HSAKMT_STATUS ret;

	if (gpu_id_array_size > 0 && gpu_id_array == NULL)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if ((address >= svm.dgpu_aperture.base) &&
	    (address <= svm.dgpu_aperture.limit))
		aperture = &svm.dgpu_aperture;
	else if ((address >= svm.dgpu_alt_aperture.base) &&
		 (address <= svm.dgpu_alt_aperture.limit))
		aperture = &svm.dgpu_alt_aperture;
	else {
		/*
		 * If address isn't SVM address, we assume that this
		 * is system memory address.
		 */
		ret = fmm_register_user_memory(address, size_in_bytes, &object);
		if (ret != HSAKMT_STATUS_SUCCESS)
			return ret;
		if (gpu_id_array_size == 0)
			return HSAKMT_STATUS_SUCCESS;
		aperture = &svm.dgpu_aperture;
		/* fall through */
	}

	if (!object) {
		pthread_mutex_lock(&aperture->fmm_mutex);
		object = vm_find_object_by_address(aperture, address, 0);
		pthread_mutex_unlock(&aperture->fmm_mutex);
	}

	if (!object)
		return HSAKMT_STATUS_NOT_SUPPORTED;
	if (object->device_ids_array_size > 0)
		return HSAKMT_STATUS_MEMORY_ALREADY_REGISTERED;

	if (gpu_id_array_size > 0) {
		object->device_ids_array = gpu_id_array;
		object->device_ids_array_size = gpu_id_array_size;
	}

	return HSAKMT_STATUS_SUCCESS;
}

static HSAKMT_STATUS fmm_deregister_user_memory(void *addr)
{
	manageble_aperture_t *aperture;
	vm_object_t *obj;
	void *svm_addr;
	HSAuint64 size;

	aperture = &svm.dgpu_aperture;

	/* Find the size and start address in SVM space */
	pthread_mutex_lock(&aperture->fmm_mutex);
	obj = vm_find_object_by_userptr(aperture, addr);
	if (obj == NULL) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return HSAKMT_STATUS_MEMORY_NOT_REGISTERED;
	}
	svm_addr = obj->start;
	size = obj->size;
	pthread_mutex_unlock(&aperture->fmm_mutex);

	/* Destroy BO */
	__fmm_release(svm_addr, size, aperture);

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS fmm_deregister_memory(void *address)
{
	manageble_aperture_t *aperture;
	vm_object_t *object = NULL;

	if ((address >= svm.dgpu_aperture.base) &&
	    (address <= svm.dgpu_aperture.limit))
		aperture = &svm.dgpu_aperture;
	else if ((address >= svm.dgpu_alt_aperture.base) &&
		 (address <= svm.dgpu_alt_aperture.limit))
		aperture = &svm.dgpu_alt_aperture;
	else {
		/*
		 * If address isn't SVM address, we assume that this
		 * is system memory address. If the userptr object had
		 * a device_ids_array, it will be freed by
		 * __fmm_release. Also the object will be
		 * removed. Therefore we can short-circuit the rest of
		 * the function below.
		 */
		return fmm_deregister_user_memory(address);
	}

	pthread_mutex_lock(&aperture->fmm_mutex);
	object = vm_find_object_by_address(aperture, address, 0);
	pthread_mutex_unlock(&aperture->fmm_mutex);

	if (!object || object->device_ids_array_size <= 0)
		return HSAKMT_STATUS_MEMORY_NOT_REGISTERED;

	if (object->userptr)
		return fmm_deregister_user_memory(object->userptr);

	free(object->device_ids_array);
	object->device_ids_array = NULL;
	object->device_ids_array_size = 0;

	return HSAKMT_STATUS_SUCCESS;
}
