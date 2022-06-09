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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>

#include <numa.h>
#include <numaif.h>
#include "rbtree.h"
#ifndef MPOL_F_STATIC_NODES
/* Bug in numaif.h, this should be defined in there. Definition copied
 * from linux/mempolicy.h.
 */
#define MPOL_F_STATIC_NODES     (1 << 15)
#endif

#define NON_VALID_GPU_ID 0

#define INIT_MANAGEABLE_APERTURE(base_value, limit_value) {	\
	.base = (void *) base_value,				\
	.limit = (void *) limit_value,				\
	.align = 0,						\
	.guard_pages = 1,					\
	.vm_ranges = NULL,					\
	.fmm_mutex = PTHREAD_MUTEX_INITIALIZER,			\
	.is_cpu_accessible = false,				\
	.ops = &reserved_aperture_ops				\
	}

#define container_of(ptr, type, member) ({			\
		char *__mptr = (void *)(ptr);			\
		((type *)(__mptr - offsetof(type, member))); })

#define rb_entry(ptr, type, member)				\
		container_of(ptr, type, member)

#define vm_object_entry(n, is_userptr) ({			\
		(is_userptr) == 0 ?				\
		rb_entry(n, vm_object_t, node) :		\
		rb_entry(n, vm_object_t, user_node); })

#define vm_object_tree(app, is_userptr)				\
		((is_userptr) ? &(app)->user_tree : &(app)->tree)

struct vm_object {
	void *start;
	void *userptr;
	uint64_t userptr_size;
	uint64_t size; /* size allocated on GPU. When the user requests a random
			* size, Thunk aligns it to page size and allocates this
			* aligned size on GPU
			*/
	uint64_t handle; /* opaque */
	uint32_t node_id;
	rbtree_node_t node;
	rbtree_node_t user_node;

	HsaMemFlags mflags; /* memory allocation flags */
	/* Registered nodes to map on SVM mGPU */
	uint32_t *registered_device_id_array;
	uint32_t registered_device_id_array_size;
	uint32_t *registered_node_id_array;
	uint32_t registration_count; /* the same memory region can be registered multiple times */
	/* Nodes that mapped already */
	uint32_t *mapped_device_id_array;
	uint32_t mapped_device_id_array_size;
	uint32_t *mapped_node_id_array;
	uint32_t mapping_count;
	/* Metadata of imported graphics buffers */
	void *metadata;
	/* User data associated with the memory */
	void *user_data;
	/* Flag to indicate imported KFD buffer */
	bool is_imported_kfd_bo;
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
typedef struct manageable_aperture manageable_aperture_t;

/* Aperture management function pointers to allow different management
 * schemes.
 */
typedef struct {
	void *(*allocate_area_aligned)(manageable_aperture_t *aper, void *addr,
				       uint64_t size, uint64_t align);
	void (*release_area)(manageable_aperture_t *aper,
			     void *addr, uint64_t size);
} manageable_aperture_ops_t;

/* Reserved aperture type managed by its own address allocator */
static void *reserved_aperture_allocate_aligned(manageable_aperture_t *aper,
						void *addr,
						uint64_t size, uint64_t align);
static void reserved_aperture_release(manageable_aperture_t *aper,
				      void *addr, uint64_t size);
static const manageable_aperture_ops_t reserved_aperture_ops = {
	reserved_aperture_allocate_aligned,
	reserved_aperture_release
};

/* Unreserved aperture type using mmap to allocate virtual address space */
static void *mmap_aperture_allocate_aligned(manageable_aperture_t *aper,
					    void *addr,
					    uint64_t size, uint64_t align);
static void mmap_aperture_release(manageable_aperture_t *aper,
				  void *addr, uint64_t size);
static const manageable_aperture_ops_t mmap_aperture_ops = {
	mmap_aperture_allocate_aligned,
	mmap_aperture_release
};

struct manageable_aperture {
	void *base;
	void *limit;
	uint64_t align;
	uint32_t guard_pages;
	vm_area_t *vm_ranges;
	rbtree_t tree;
	rbtree_t user_tree;
	pthread_mutex_t fmm_mutex;
	bool is_cpu_accessible;
	const manageable_aperture_ops_t *ops;
};

typedef struct {
	void *base;
	void *limit;
} aperture_t;

typedef struct {
	uint32_t gpu_id;
	uint32_t device_id;
	uint32_t node_id;
	uint64_t local_mem_size;
	HSA_ENGINE_ID EngineId;
	aperture_t lds_aperture;
	aperture_t scratch_aperture;
	aperture_t mmio_aperture;
	manageable_aperture_t scratch_physical; /* For dGPU, scratch physical is allocated from
						 * dgpu_aperture. When requested by RT, each
						 * GPU will get a differnt range
						 */
	manageable_aperture_t gpuvm_aperture;   /* used for GPUVM on APU, outsidethe canonical address range */
	int drm_render_fd;
} gpu_mem_t;

enum svm_aperture_type {
	SVM_DEFAULT = 0,
	SVM_COHERENT,
	SVM_APERTURE_NUM
};

/* The main structure for dGPU Shared Virtual Memory Management */
typedef struct {
	/* Two apertures can have different MTypes (for coherency) */
	manageable_aperture_t apertures[SVM_APERTURE_NUM];

	/* Pointers to apertures, may point to the same aperture on
	 * GFXv9 and later, where MType is not based on apertures
	 */
	manageable_aperture_t *dgpu_aperture;
	manageable_aperture_t *dgpu_alt_aperture;

	/* whether to use userptr for paged memory */
	bool userptr_for_paged_mem;

	/* whether to check userptrs on registration */
	bool check_userptr;

	/* whether to check reserve svm on registration */
	bool reserve_svm;

	/* whether all memory is coherent (GPU cache disabled) */
	bool disable_cache;

	/* specifies the alignment size as PAGE_SIZE * 2^alignment_order */
	uint32_t alignment_order;
} svm_t;

/* The other apertures are specific to each GPU. gpu_mem_t manages GPU
 * specific memory apertures.
 */
static gpu_mem_t *gpu_mem;
static unsigned int gpu_mem_count;
static gpu_mem_t *g_first_gpu_mem;

static void *dgpu_shared_aperture_base;
static void *dgpu_shared_aperture_limit;

static svm_t svm = {
	.apertures = {INIT_MANAGEABLE_APERTURE(0, 0),
		      INIT_MANAGEABLE_APERTURE(0, 0)},
	.dgpu_aperture = NULL,
	.dgpu_alt_aperture = NULL,
	.userptr_for_paged_mem = false,
	.check_userptr = false,
	.disable_cache = false
};

/* On APU, for memory allocated on the system memory that GPU doesn't access
 * via GPU driver, they are not managed by GPUVM. cpuvm_aperture keeps track
 * of this part of memory.
 */
static manageable_aperture_t cpuvm_aperture = INIT_MANAGEABLE_APERTURE(0, 0);

/* GPU node array for default mappings */
static uint32_t all_gpu_id_array_size;
static uint32_t *all_gpu_id_array;

/* IPC structures and helper functions */
typedef enum _HSA_APERTURE {
	HSA_APERTURE_UNSUPPORTED = 0,
	HSA_APERTURE_DGPU,
	HSA_APERTURE_DGPU_ALT,
	HSA_APERTURE_GPUVM,
	HSA_APERTURE_CPUVM
} HSA_APERTURE;

typedef struct _HsaApertureInfo {
	HSA_APERTURE	type;		// Aperture type
	HSAuint32	idx;		// Aperture index
} HsaApertureInfo;

typedef struct _HsaSharedMemoryStruct {
	HSAuint32	ShareHandle[4];
	HsaApertureInfo	ApeInfo;
	HSAuint32	SizeInPages;
	HSAuint32	ExportGpuId;
} HsaSharedMemoryStruct;

static inline const HsaSharedMemoryStruct *to_const_hsa_shared_memory_struct(
			const HsaSharedMemoryHandle *SharedMemoryHandle)
{
	return (const HsaSharedMemoryStruct *)SharedMemoryHandle;
}

static inline HsaSharedMemoryStruct *to_hsa_shared_memory_struct(
			HsaSharedMemoryHandle *SharedMemoryHandle)
{
	return (HsaSharedMemoryStruct *)SharedMemoryHandle;
}

__attribute__((unused))
static inline HsaSharedMemoryHandle *to_hsa_shared_memory_handle(
			HsaSharedMemoryStruct *SharedMemoryStruct)
{
	return (HsaSharedMemoryHandle *)SharedMemoryStruct;
}

extern int debug_get_reg_status(uint32_t node_id, bool *is_debugged);
static int __fmm_release(vm_object_t *object, manageable_aperture_t *aperture);
static int _fmm_unmap_from_gpu_scratch(uint32_t gpu_id,
				       manageable_aperture_t *aperture,
				       void *address);
static void print_device_id_array(uint32_t *device_id_array, uint32_t device_id_array_size);

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
					      uint64_t handle, HsaMemFlags mflags)
{
	vm_object_t *object = (vm_object_t *) malloc(sizeof(vm_object_t));

	if (object) {
		object->start = start;
		object->userptr = NULL;
		object->userptr_size = 0;
		object->size = size;
		object->handle = handle;
		object->registered_device_id_array_size = 0;
		object->mapped_device_id_array_size = 0;
		object->registered_device_id_array = NULL;
		object->mapped_device_id_array = NULL;
		object->registered_node_id_array = NULL;
		object->mapped_node_id_array = NULL;
		object->registration_count = 0;
		object->mapping_count = 0;
		object->mflags = mflags;
		object->metadata = NULL;
		object->user_data = NULL;
		object->is_imported_kfd_bo = false;
		object->node.key = rbtree_key((unsigned long)start, size);
		object->user_node.key = rbtree_key(0, 0);
	}

	return object;
}


static void vm_remove_area(manageable_aperture_t *app, vm_area_t *area)
{
	vm_area_t *next;
	vm_area_t *prev;

	next = area->next;
	prev = area->prev;

	if (!prev) /* The first element */
		app->vm_ranges = next;
	else
		prev->next = next;

	if (next) /* If not the last element */
		next->prev = prev;

	free(area);
}

static void vm_remove_object(manageable_aperture_t *app, vm_object_t *object)
{
	/* Free allocations inside the object */
	if (object->registered_device_id_array)
		free(object->registered_device_id_array);

	if (object->mapped_device_id_array)
		free(object->mapped_device_id_array);

	if (object->metadata)
		free(object->metadata);

	if (object->registered_node_id_array)
		free(object->registered_node_id_array);
	if (object->mapped_node_id_array)
		free(object->mapped_node_id_array);

	rbtree_delete(&app->tree, &object->node);
	if (object->userptr)
		rbtree_delete(&app->user_tree, &object->user_node);

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

static void vm_split_area(manageable_aperture_t *app, vm_area_t *area,
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

static vm_object_t *vm_find_object_by_address_userptr(manageable_aperture_t *app,
					const void *address, uint64_t size, int is_userptr)
{
	vm_object_t *cur = NULL;

	rbtree_t *tree = vm_object_tree(app, is_userptr);
	rbtree_key_t key = rbtree_key((unsigned long)address, size);
	void *start;
	uint64_t s;

	/* rbtree_lookup_nearest(,,,RIGHT) will return a node with
	 * its size >= key.size and its address >= key.address
	 * if there are two nodes with format(address, size),
	 * (0x100, 16) and (0x110, 8). the key is (0x100, 0),
	 * then node (0x100, 16) will be returned.
	 */
	rbtree_node_t *n = rbtree_lookup_nearest(tree, &key, LKP_ALL, RIGHT);

	if (n) {
		cur = vm_object_entry(n, is_userptr);
		if (is_userptr == 0) {
			start = cur->start;
			s = cur->size;
		} else {
			start = cur->userptr;
			s = cur->userptr_size;
		}

		if (start != address)
			return NULL;

		if (size)
			return size == s ? cur : NULL;

		/* size is 0, make sure there is only one node whose address == key.address*/
		key = rbtree_key((unsigned long)address, (unsigned long)-1);
		rbtree_node_t *rn = rbtree_lookup_nearest(tree, &key, LKP_ALL, LEFT);

		if (rn != n)
			return NULL;
	}

	return cur; /* NULL if not found */
}


static vm_object_t *vm_find_object_by_address_userptr_range(manageable_aperture_t *app,
						    const void *address, int is_userptr)
{
	vm_object_t *cur = NULL;
	rbtree_t *tree = vm_object_tree(app, is_userptr);
	rbtree_key_t key = rbtree_key((unsigned long)address, 0);
	rbtree_node_t *ln = rbtree_lookup_nearest(tree, &key,
			LKP_ALL, LEFT);
	rbtree_node_t *rn = rbtree_lookup_nearest(tree, &key,
			LKP_ALL, RIGHT);
	void *start;
	uint64_t size;
	int bad = 0;

loop:
	while (ln) {
		cur = vm_object_entry(ln, is_userptr);
		if (is_userptr == 0) {
			start = cur->start;
			size = cur->size;
		} else {
			start = cur->userptr;
			size = cur->userptr_size;
		}

		if (address >= start &&
				(uint64_t)address < ((uint64_t)start + size))
			break;

		cur = NULL;

		if (ln == rn)
			break;

		ln = rbtree_next(tree, ln);
	}

	if (cur == NULL && bad == 0) {
		/* As there is area overlap, say, (address, size) like
		 * (0x100, 32), (0x108, 8), and the key.address is 0x118
		 * The lookup above only find (0x108, 8), but the correct node should
		 * be (0x100, 16). So try to walk though the tree to find the node.
		 */
		rn = ln;
		key = rbtree_key(0, 0);
		ln = rbtree_lookup_nearest(tree, &key, LKP_ALL, RIGHT);
		bad = 1;
		goto loop;
	}

	return cur; /* NULL if not found */
}

static vm_object_t *vm_find_object_by_address(manageable_aperture_t *app,
					const void *address, uint64_t size)
{
	return vm_find_object_by_address_userptr(app, address, size, 0);
}

static vm_object_t *vm_find_object_by_address_range(manageable_aperture_t *app,
						    const void *address)
{
	return vm_find_object_by_address_userptr_range(app, address, 0);
}

static vm_object_t *vm_find_object_by_userptr(manageable_aperture_t *app,
					const void *address, HSAuint64 size)
{
	return vm_find_object_by_address_userptr(app, address, size, 1);
}

static vm_object_t *vm_find_object_by_userptr_range(manageable_aperture_t *app,
						const void *address)
{
	return vm_find_object_by_address_userptr_range(app, address, 1);
}

static vm_area_t *vm_find(manageable_aperture_t *app, void *address)
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

/* Align size of a VM area
 *
 * Leave at least one guard page after every object to catch
 * out-of-bounds accesses with VM faults.
 */
static uint64_t vm_align_area_size(manageable_aperture_t *app, uint64_t size)
{
	return size + (uint64_t)app->guard_pages * PAGE_SIZE;
}

/*
 * Assumes that fmm_mutex is locked on entry.
 */
static void reserved_aperture_release(manageable_aperture_t *app,
				      void *address,
				      uint64_t MemorySizeInBytes)
{
	vm_area_t *area;
	uint64_t SizeOfRegion;

	MemorySizeInBytes = vm_align_area_size(app, MemorySizeInBytes);

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

	if (app->is_cpu_accessible) {
		void *mmap_ret;

		/* Reset NUMA policy */
		mbind(address, MemorySizeInBytes, MPOL_DEFAULT, NULL, 0, 0);

		/* Remove any CPU mapping, but keep the address range reserved */
		mmap_ret = mmap(address, MemorySizeInBytes, PROT_NONE,
			MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE | MAP_FIXED,
			-1, 0);
		if (mmap_ret == MAP_FAILED && errno == ENOMEM) {
			/* When mmap count reaches max_map_count, any mmap will
			 * fail. Reduce the count with munmap then map it as
			 * NORESERVE immediately.
			 */
			munmap(address, MemorySizeInBytes);
			mmap(address, MemorySizeInBytes, PROT_NONE,
				MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE | MAP_FIXED,
				-1, 0);
		}
	}
}

/*
 * returns allocated address or NULL. Assumes, that fmm_mutex is locked
 * on entry.
 */
static void *reserved_aperture_allocate_aligned(manageable_aperture_t *app,
						void *address,
						uint64_t MemorySizeInBytes,
						uint64_t align)
{
	uint64_t offset = 0, orig_align = align;
	vm_area_t *cur, *next;
	void *start;

	if (align < app->align)
		align = app->align;

	/* Align big buffers to the next power-of-2 up to huge page
	 * size for flexible fragment size TLB optimizations
	 */
	while (align < GPU_HUGE_PAGE_SIZE && MemorySizeInBytes >= (align << 1))
		align <<= 1;

	/* If no specific alignment was requested, align the end of
	 * buffers instead of the start. For fragment optimizations,
	 * aligning the start or the end achieves the same effective
	 * optimization. End alignment to the TLB cache line size is
	 * needed as a workaround for TLB issues on some older GPUs.
	 */
	if (orig_align <= (uint64_t)PAGE_SIZE)
		offset = align - (MemorySizeInBytes & (align - 1));

	MemorySizeInBytes = vm_align_area_size(app, MemorySizeInBytes);

	/* Find a big enough "hole" in the address space */
	cur = NULL;
	next = app->vm_ranges;
	start = address ? address :
		(void *)(ALIGN_UP((uint64_t)app->base, align) + offset);
	while (next) {
		if (next->start > start &&
		    VOID_PTRS_SUB(next->start, start) >= MemorySizeInBytes)
			break;

		cur = next;
		next = next->next;
		if (!address)
			start = (void *)(ALIGN_UP((uint64_t)cur->end + 1, align) + offset);
	}
	if (!next && VOID_PTRS_SUB(app->limit, start) + 1 < MemorySizeInBytes)
		/* No hole found and not enough space after the last area */
		return NULL;

	if (cur && address && address < (void *)ALIGN_UP((uint64_t)cur->end + 1, align))
		/* Required address is not free or overlaps */
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

static void *mmap_aperture_allocate_aligned(manageable_aperture_t *aper,
					    void *address,
					    uint64_t size, uint64_t align)
{
	uint64_t aligned_padded_size, guard_size;
	uint64_t alignment_size = PAGE_SIZE << svm.alignment_order;
	void *addr, *aligned_addr, *aligned_end, *mapping_end;

	if (address)
		return NULL;

	if (!aper->is_cpu_accessible) {
		pr_err("MMap Aperture must be CPU accessible\n");
		return NULL;
	}

	/* Align big buffers to the next power-of-2. By default, the max alignment
	 * size is set to 2MB. This can be modified by the env variable
	 * HSA_MAX_VA_ALIGN. This variable sets the order of the alignment size as
	 * PAGE_SIZE * 2^HSA_MAX_VA_ALIGN. Setting HSA_MAX_VA_ALIGN = 18 (1GB),
	 * improves the time for memory allocation and mapping. But it might lose
	 * performance when GFX access it, specially for big allocations (>3GB).
	 */
	while (align < alignment_size && size >= (align << 1))
		align <<= 1;

	/* Add padding to guarantee proper alignment and leave guard
	 * pages on both sides
	 */
	guard_size = (uint64_t)aper->guard_pages * PAGE_SIZE;
	aligned_padded_size = size + align +
		2*guard_size - PAGE_SIZE;

	/* Map memory */
	addr = mmap(0, aligned_padded_size, PROT_NONE,
		    MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
	if (addr == MAP_FAILED) {
		pr_err("mmap failed: %s\n", strerror(errno));
		return NULL;
	}

	/* Adjust for alignment and guard pages, range-check the reslt */
	aligned_addr = (void *)ALIGN_UP((uint64_t)addr + guard_size, align);
	if (aligned_addr < aper->base ||
	    VOID_PTR_ADD(aligned_addr, size - 1) > aper->limit) {
		pr_err("mmap returned %p, out of range %p-%p\n", aligned_addr,
		       aper->base, aper->limit);
		munmap(addr, aligned_padded_size);
		return NULL;
	}

	/* Unmap padding and guard pages */
	if (aligned_addr > addr)
		munmap(addr, VOID_PTRS_SUB(aligned_addr, addr));

	aligned_end = VOID_PTR_ADD(aligned_addr, size);
	mapping_end = VOID_PTR_ADD(addr, aligned_padded_size);
	if (mapping_end > aligned_end)
		munmap(aligned_end, VOID_PTRS_SUB(mapping_end, aligned_end));

	return aligned_addr;
}

static void mmap_aperture_release(manageable_aperture_t *aper,
				  void *addr, uint64_t size)
{
	if (!aper->is_cpu_accessible) {
		pr_err("MMap Aperture must be CPU accessible\n");
		return;
	}

	/* Reset NUMA policy */
	mbind(addr, size, MPOL_DEFAULT, NULL, 0, 0);

	/* Unmap memory */
	munmap(addr, size);
}

/* Wrapper functions to call aperture-specific VA management functions */
static void *aperture_allocate_area_aligned(manageable_aperture_t *app,
					    void *address,
					    uint64_t MemorySizeInBytes,
					    uint64_t align)
{
	return app->ops->allocate_area_aligned(app, address, MemorySizeInBytes, align);
}
static void *aperture_allocate_area(manageable_aperture_t *app, void *address,
				    uint64_t MemorySizeInBytes)
{
	return app->ops->allocate_area_aligned(app, address, MemorySizeInBytes, app->align);
}
static void aperture_release_area(manageable_aperture_t *app, void *address,
				  uint64_t MemorySizeInBytes)
{
	app->ops->release_area(app, address, MemorySizeInBytes);
}

/* returns 0 on success. Assumes, that fmm_mutex is locked on entry */
static vm_object_t *aperture_allocate_object(manageable_aperture_t *app,
					     void *new_address,
					     uint64_t handle,
					     uint64_t MemorySizeInBytes,
					     HsaMemFlags mflags)
{
	vm_object_t *new_object;

	/* Allocate new object */
	new_object = vm_create_and_init_object(new_address,
					       MemorySizeInBytes,
					       handle, mflags);
	if (!new_object)
		return NULL;

	rbtree_insert(&app->tree, &new_object->node);

	return new_object;
}

static int32_t gpu_mem_find_by_gpu_id(uint32_t gpu_id)
{
	uint32_t i;

	for (i = 0 ; i < gpu_mem_count ; i++)
		if (gpu_mem[i].gpu_id == gpu_id)
			return i;

	return -1;
}

static manageable_aperture_t *fmm_get_aperture(HsaApertureInfo info)
{
	switch (info.type) {
	case HSA_APERTURE_DGPU:
		return svm.dgpu_aperture;
	case HSA_APERTURE_DGPU_ALT:
		return svm.dgpu_alt_aperture;
	case HSA_APERTURE_GPUVM:
		return &gpu_mem[info.idx].gpuvm_aperture;
	case HSA_APERTURE_CPUVM:
		return &cpuvm_aperture;
	default:
		return NULL;
	}
}

static manageable_aperture_t *fmm_is_scratch_aperture(const void *address)
{
	uint32_t i;

	for (i = 0; i < gpu_mem_count; i++) {
		if (gpu_mem[i].gpu_id == NON_VALID_GPU_ID)
			continue;

		if ((address >= gpu_mem[i].scratch_physical.base) &&
			(address <= gpu_mem[i].scratch_physical.limit))
			return &gpu_mem[i].scratch_physical;

	}
	return NULL;
}

static manageable_aperture_t *fmm_find_aperture(const void *address,
						HsaApertureInfo *info)
{
	manageable_aperture_t *aperture = NULL;
	uint32_t i;
	HsaApertureInfo _info = { .type = HSA_APERTURE_UNSUPPORTED, .idx = 0};

	if (is_dgpu) {
		if (address >= svm.dgpu_aperture->base &&
			address <= svm.dgpu_aperture->limit) {

			aperture = fmm_is_scratch_aperture(address);
			if (!aperture) {
				aperture = svm.dgpu_aperture;
				_info.type = HSA_APERTURE_DGPU;
			}
		} else if (address >= svm.dgpu_alt_aperture->base &&
			address <= svm.dgpu_alt_aperture->limit) {
			aperture = svm.dgpu_alt_aperture;
			_info.type = HSA_APERTURE_DGPU_ALT;
		} else {
			/* Not in SVM, it can be system memory registered by userptr */
			aperture = svm.dgpu_aperture;
			_info.type = HSA_APERTURE_DGPU;
		}
	} else { /* APU */
		if (address >= svm.dgpu_aperture->base && address <= svm.dgpu_aperture->limit) {
			aperture = svm.dgpu_aperture;
			_info.type = HSA_APERTURE_DGPU;
		} else {
			/* gpuvm_aperture */
			for (i = 0; i < gpu_mem_count; i++) {
				if ((address >= gpu_mem[i].gpuvm_aperture.base) &&
					(address <= gpu_mem[i].gpuvm_aperture.limit)) {
					aperture = &gpu_mem[i].gpuvm_aperture;
					_info.type = HSA_APERTURE_GPUVM;
					_info.idx = i;
				}
			}
		}
		if (!aperture) {
			/* Not in GPUVM */
			aperture = &cpuvm_aperture;
			_info.type = HSA_APERTURE_CPUVM;
		}
	}

	if (info)
		*info = _info;

	return aperture;
}

static HsaMemFlags fmm_translate_ioc_to_hsa_flags(uint32_t ioc_flags)
{
	HsaMemFlags mflags = {0};

	if (!(ioc_flags & KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE))
		mflags.ui32.ReadOnly = 1;
	if (!(ioc_flags & KFD_IOC_ALLOC_MEM_FLAGS_COHERENT))
		mflags.ui32.CoarseGrain = 1;
	if (ioc_flags & KFD_IOC_ALLOC_MEM_FLAGS_PUBLIC)
		mflags.ui32.HostAccess = 1;
	return mflags;
}

/* After allocating the memory, return the vm_object created for this memory.
 * Return NULL if any failure.
 */
static vm_object_t *fmm_allocate_memory_object(uint32_t gpu_id, void *mem,
						uint64_t MemorySizeInBytes,
						manageable_aperture_t *aperture,
						uint64_t *mmap_offset,
						uint32_t ioc_flags)
{
	struct kfd_ioctl_alloc_memory_of_gpu_args args = {0};
	struct kfd_ioctl_free_memory_of_gpu_args free_args = {0};
	vm_object_t *vm_obj = NULL;
	HsaMemFlags mflags;

	if (!mem)
		return NULL;

	/* Allocate memory from amdkfd */
	args.gpu_id = gpu_id;
	args.size = MemorySizeInBytes;

	args.flags = ioc_flags |
		KFD_IOC_ALLOC_MEM_FLAGS_NO_SUBSTITUTE;
	args.va_addr = (uint64_t)mem;
	if (!is_dgpu &&
	    (ioc_flags & KFD_IOC_ALLOC_MEM_FLAGS_VRAM))
		args.va_addr = VOID_PTRS_SUB(mem, aperture->base);
	if (ioc_flags & KFD_IOC_ALLOC_MEM_FLAGS_USERPTR)
		args.mmap_offset = *mmap_offset;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_ALLOC_MEMORY_OF_GPU, &args))
		return NULL;

	mflags = fmm_translate_ioc_to_hsa_flags(ioc_flags);

	/* Allocate object */
	pthread_mutex_lock(&aperture->fmm_mutex);
	vm_obj = aperture_allocate_object(aperture, mem, args.handle,
				      MemorySizeInBytes, mflags);
	if (!vm_obj)
		goto err_object_allocation_failed;
	pthread_mutex_unlock(&aperture->fmm_mutex);

	if (mmap_offset)
		*mmap_offset = args.mmap_offset;

	return vm_obj;

err_object_allocation_failed:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	free_args.handle = args.handle;
	kmtIoctl(kfd_fd, AMDKFD_IOC_FREE_MEMORY_OF_GPU, &free_args);

	return NULL;
}

#ifdef DEBUG_PRINT_APERTURE
static void aperture_print(aperture_t *app)
{
	pr_info("\t Base: %p\n", app->base);
	pr_info("\t Limit: %p\n", app->limit);
}

static void manageable_aperture_print(manageable_aperture_t *app)
{
	vm_area_t *cur = app->vm_ranges;
	rbtree_node_t *n = rbtree_node_any(&app->tree, LEFT);
	vm_object_t *object;

	pr_info("\t Base: %p\n", app->base);
	pr_info("\t Limit: %p\n", app->limit);
	pr_info("\t Ranges:\n");
	while (cur) {
		pr_info("\t\t Range [%p - %p]\n", cur->start, cur->end);
		cur = cur->next;
	};
	pr_info("\t Objects:\n");
	while (n) {
		object = vm_object_entry(n, 0);
		pr_info("\t\t Object [%p - %" PRIu64 "]\n",
				object->start, object->size);
		n = rbtree_next(&app->tree, n);
	}
}

void fmm_print(uint32_t gpu_id)
{
	int32_t gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);

	if (gpu_mem_id >= 0) { /* Found */
		pr_info("LDS aperture:\n");
		aperture_print(&gpu_mem[gpu_mem_id].lds_aperture);
		pr_info("GPUVM aperture:\n");
		manageable_aperture_print(&gpu_mem[gpu_mem_id].gpuvm_aperture);
		pr_info("Scratch aperture:\n");
		aperture_print(&gpu_mem[gpu_mem_id].scratch_aperture);
		pr_info("Scratch backing memory:\n");
		manageable_aperture_print(&gpu_mem[gpu_mem_id].scratch_physical);
	}

	pr_info("dGPU aperture:\n");
	manageable_aperture_print(svm.dgpu_aperture);
	pr_info("dGPU alt aperture:\n");
	if (svm.dgpu_aperture == svm.dgpu_alt_aperture)
		pr_info("\t Alias of dGPU aperture\n");
	else
		manageable_aperture_print(svm.dgpu_alt_aperture);
}
#else
void fmm_print(uint32_t gpu_id)
{
}
#endif

/* vm_find_object - Find a VM object in any aperture
 *
 * @addr: VM address of the object
 * @size: size of the object, 0 means "don't care",
 *        UINT64_MAX means addr can match any address within the object
 * @out_aper: Aperture where the object was found
 *
 * Returns a pointer to the object if found, NULL otherwise. If an
 * object is found, this function returns with the
 * (*out_aper)->fmm_mutex locked.
 */
static vm_object_t *vm_find_object(const void *addr, uint64_t size,
				   manageable_aperture_t **out_aper)
{
	manageable_aperture_t *aper = NULL;
	bool range = (size == UINT64_MAX);
	bool userptr = false;
	vm_object_t *obj = NULL;
	uint32_t i;

	for (i = 0; i < gpu_mem_count; i++)
		if (gpu_mem[i].gpu_id != NON_VALID_GPU_ID &&
		    addr >= gpu_mem[i].gpuvm_aperture.base &&
		    addr <= gpu_mem[i].gpuvm_aperture.limit) {
			aper = &gpu_mem[i].gpuvm_aperture;
			break;
		}

	if (!aper) {
		if (!svm.dgpu_aperture)
			goto no_svm;

		if ((addr >= svm.dgpu_aperture->base) &&
		    (addr <= svm.dgpu_aperture->limit))
			aper = svm.dgpu_aperture;
		else if ((addr >= svm.dgpu_alt_aperture->base) &&
			 (addr <= svm.dgpu_alt_aperture->limit))
			aper = svm.dgpu_alt_aperture;
		else {
			aper = svm.dgpu_aperture;
			userptr = true;
		}
	}

	pthread_mutex_lock(&aper->fmm_mutex);
	if (range) {
		/* mmap_apertures can have userptrs in them. Try to
		 * look up addresses as userptrs first to sort out any
		 * ambiguity of multiple overlapping mappings at
		 * different GPU addresses.
		 */
		if (userptr || aper->ops == &mmap_aperture_ops)
			obj = vm_find_object_by_userptr_range(aper, addr);
		if (!obj && !userptr)
			obj = vm_find_object_by_address_range(aper, addr);
	} else {
		if (userptr || aper->ops == &mmap_aperture_ops)
			obj = vm_find_object_by_userptr(aper, addr, size);
		if (!obj && !userptr) {
			long page_offset = (long)addr & (PAGE_SIZE-1);
			const void *page_addr = (const uint8_t *)addr - page_offset;

			obj = vm_find_object_by_address(aper, page_addr, 0);
			/* If we find a userptr here, it's a match on
			 * the aligned GPU address. Make sure that the
			 * page offset and size match too.
			 */
			if (obj && obj->userptr &&
			    (((long)obj->userptr & (PAGE_SIZE - 1)) != page_offset ||
			     (size && size != obj->userptr_size)))
				obj = NULL;
		}
	}

no_svm:
	if (!obj && !is_dgpu) {
		/* On APUs try finding it in the CPUVM aperture */
		if (aper)
			pthread_mutex_unlock(&aper->fmm_mutex);

		aper = &cpuvm_aperture;

		pthread_mutex_lock(&aper->fmm_mutex);
		if (range)
			obj = vm_find_object_by_address_range(aper, addr);
		else
			obj = vm_find_object_by_address(aper, addr, 0);
	}

	if (obj) {
		*out_aper = aper;
		return obj;
	}

	if (aper)
		pthread_mutex_unlock(&aper->fmm_mutex);
	return NULL;
}

static HSAuint8 fmm_check_user_memory(const void *addr, HSAuint64 size)
{
	volatile const HSAuint8 *ptr = addr;
	volatile const HSAuint8 *end = ptr + size;
	HSAuint8 sum = 0;

	/* Access every page in the buffer to make sure the mapping is
	 * valid. If it's not, it will die with a segfault that's easy
	 * to debug.
	 */
	for (; ptr < end; ptr = (void *)PAGE_ALIGN_UP(ptr + 1))
		sum += *ptr;

	return sum;
}

static void fmm_release_scratch(uint32_t gpu_id)
{
	int32_t gpu_mem_id;
	uint64_t size;
	vm_object_t *obj;
	manageable_aperture_t *aperture;
	rbtree_node_t *n;

	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return;

	aperture = &gpu_mem[gpu_mem_id].scratch_physical;

	size = VOID_PTRS_SUB(aperture->limit, aperture->base) + 1;

	if (is_dgpu) {
		/* unmap and remove all remaining objects */
		pthread_mutex_lock(&aperture->fmm_mutex);
		while ((n = rbtree_node_any(&aperture->tree, MID))) {
			obj = vm_object_entry(n, 0);

			void *obj_addr = obj->start;

			pthread_mutex_unlock(&aperture->fmm_mutex);

			_fmm_unmap_from_gpu_scratch(gpu_id, aperture, obj_addr);

			pthread_mutex_lock(&aperture->fmm_mutex);
		}
		pthread_mutex_unlock(&aperture->fmm_mutex);

		/* release address space */
		pthread_mutex_lock(&svm.dgpu_aperture->fmm_mutex);
		aperture_release_area(svm.dgpu_aperture,
				      gpu_mem[gpu_mem_id].scratch_physical.base,
				      size);
		pthread_mutex_unlock(&svm.dgpu_aperture->fmm_mutex);
	} else
		/* release address space */
		munmap(gpu_mem[gpu_mem_id].scratch_physical.base, size);

	/* invalidate scratch backing aperture */
	gpu_mem[gpu_mem_id].scratch_physical.base = NULL;
	gpu_mem[gpu_mem_id].scratch_physical.limit = NULL;
}

static uint32_t fmm_translate_hsa_to_ioc_flags(HsaMemFlags flags)
{
	uint32_t ioc_flags = 0;

	if (flags.ui32.AQLQueueMemory)
		ioc_flags |= (KFD_IOC_ALLOC_MEM_FLAGS_AQL_QUEUE_MEM |
			      KFD_IOC_ALLOC_MEM_FLAGS_UNCACHED);
	if (!flags.ui32.ReadOnly)
		ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE;
	/* TODO: Since, ROCr interfaces doesn't allow caller to set page
	 * permissions, mark all user allocations with exec permission.
	 * Check for flags.ui32.ExecuteAccess once ROCr is ready.
	 */
	ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_EXECUTABLE;
	return ioc_flags;
}

#define SCRATCH_ALIGN 0x10000
void *fmm_allocate_scratch(uint32_t gpu_id, void *address, uint64_t MemorySizeInBytes)
{
	manageable_aperture_t *aperture_phy;
	struct kfd_ioctl_set_scratch_backing_va_args args = {0};
	int32_t gpu_mem_id;
	void *mem = NULL;
	uint64_t aligned_size = ALIGN_UP(MemorySizeInBytes, SCRATCH_ALIGN);

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return NULL;

	aperture_phy = &gpu_mem[gpu_mem_id].scratch_physical;
	if (aperture_phy->base || aperture_phy->limit)
		/* Scratch was already allocated for this GPU */
		return NULL;

	/* Allocate address space for scratch backing, 64KB aligned */
	if (is_dgpu) {
		pthread_mutex_lock(&svm.dgpu_aperture->fmm_mutex);
		mem = aperture_allocate_area_aligned(
			svm.dgpu_aperture, address,
			aligned_size, SCRATCH_ALIGN);
		pthread_mutex_unlock(&svm.dgpu_aperture->fmm_mutex);
	} else {
		uint64_t aligned_padded_size = aligned_size +
			SCRATCH_ALIGN - PAGE_SIZE;
		void *padded_end, *aligned_start, *aligned_end;

		if (address)
			return NULL;

		mem = mmap(0, aligned_padded_size,
			   PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
			   -1, 0);
		if (!mem)
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
	aperture_phy->is_cpu_accessible = true;

	/* Program SH_HIDDEN_PRIVATE_BASE */
	args.gpu_id = gpu_id;
	args.va_addr = ((uint64_t)mem) >> 16;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_SET_SCRATCH_BACKING_VA, &args)) {
		fmm_release_scratch(gpu_id);
		return NULL;
	}

	return mem;
}

static void *__fmm_allocate_device(uint32_t gpu_id, void *address, uint64_t MemorySizeInBytes,
		manageable_aperture_t *aperture, uint64_t *mmap_offset,
		uint32_t ioc_flags, vm_object_t **vm_obj)
{
	void *mem = NULL;
	vm_object_t *obj;

	/* Check that aperture is properly initialized/supported */
	if (!aperture_is_valid(aperture->base, aperture->limit))
		return NULL;

	/* Allocate address space */
	pthread_mutex_lock(&aperture->fmm_mutex);
	mem = aperture_allocate_area(aperture, address, MemorySizeInBytes);
	pthread_mutex_unlock(&aperture->fmm_mutex);

	/*
	 * Now that we have the area reserved, allocate memory in the device
	 * itself
	 */
	obj = fmm_allocate_memory_object(gpu_id, mem,
			MemorySizeInBytes, aperture, mmap_offset, ioc_flags);
	if (!obj) {
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
	if (vm_obj)
		*vm_obj = obj;

	return mem;
}

void *fmm_allocate_device(uint32_t gpu_id, uint32_t node_id, void *address,
			  uint64_t MemorySizeInBytes, HsaMemFlags mflags)
{
	manageable_aperture_t *aperture;
	int32_t gpu_mem_id;
	uint32_t ioc_flags = KFD_IOC_ALLOC_MEM_FLAGS_VRAM;
	uint64_t size, mmap_offset;
	void *mem;
	vm_object_t *vm_obj = NULL;

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return NULL;

	size = MemorySizeInBytes;

	if (mflags.ui32.HostAccess)
		ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_PUBLIC;

	ioc_flags |= fmm_translate_hsa_to_ioc_flags(mflags);

	if (topology_is_svm_needed(gpu_mem[gpu_mem_id].EngineId)) {
		aperture = svm.dgpu_aperture;
		if (mflags.ui32.AQLQueueMemory)
			size = MemorySizeInBytes * 2;
	} else {
		aperture = &gpu_mem[gpu_mem_id].gpuvm_aperture;
	}

	if (!mflags.ui32.CoarseGrain || svm.disable_cache)
		ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_COHERENT;

	if (mflags.ui32.Uncached || svm.disable_cache)
		ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_UNCACHED;

	mem = __fmm_allocate_device(gpu_id, address, size, aperture, &mmap_offset,
				    ioc_flags, &vm_obj);

	if (mem && vm_obj) {
		pthread_mutex_lock(&aperture->fmm_mutex);
		/* Store memory allocation flags, not ioc flags */
		vm_obj->mflags = mflags;
		gpuid_to_nodeid(gpu_id, &vm_obj->node_id);
		pthread_mutex_unlock(&aperture->fmm_mutex);
	}

	if (mem) {
		int map_fd = gpu_mem[gpu_mem_id].drm_render_fd;
		int prot = mflags.ui32.HostAccess ? PROT_READ | PROT_WRITE :
					PROT_NONE;
		int flag = mflags.ui32.HostAccess ? MAP_SHARED | MAP_FIXED :
					MAP_PRIVATE|MAP_FIXED;
		void *ret = mmap(mem, MemorySizeInBytes, prot, flag,
					map_fd, mmap_offset);

		if (ret == MAP_FAILED) {
			__fmm_release(vm_obj, aperture);
			return NULL;
		}
		/*
		 * This madvise() call is needed to avoid additional references
		 * to mapped BOs in child processes that can prevent freeing
		 * memory in the parent process and lead to out-of-memory
		 * conditions.
		 */
		madvise(mem, MemorySizeInBytes, MADV_DONTFORK);
	}

	return mem;
}

void *fmm_allocate_doorbell(uint32_t gpu_id, uint64_t MemorySizeInBytes,
			    uint64_t doorbell_mmap_offset)
{
	manageable_aperture_t *aperture;
	int32_t gpu_mem_id;
	uint32_t ioc_flags;
	void *mem;
	vm_object_t *vm_obj = NULL;

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return NULL;

	/* Use fine-grained aperture */
	aperture = svm.dgpu_alt_aperture;
	ioc_flags = KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL |
		    KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE |
		    KFD_IOC_ALLOC_MEM_FLAGS_COHERENT;

	mem = __fmm_allocate_device(gpu_id, NULL, MemorySizeInBytes, aperture, NULL,
				    ioc_flags, &vm_obj);

	if (mem && vm_obj) {
		HsaMemFlags mflags;

		/* Cook up some flags for storing in the VM object */
		mflags.Value = 0;
		mflags.ui32.NonPaged = 1;
		mflags.ui32.HostAccess = 1;
		mflags.ui32.Reserved = 0xBe1;

		pthread_mutex_lock(&aperture->fmm_mutex);
		vm_obj->mflags = mflags;
		gpuid_to_nodeid(gpu_id, &vm_obj->node_id);
		pthread_mutex_unlock(&aperture->fmm_mutex);
	}

	if (mem) {
		void *ret = mmap(mem, MemorySizeInBytes,
				 PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_FIXED, kfd_fd,
				 doorbell_mmap_offset);
		if (ret == MAP_FAILED) {
			__fmm_release(vm_obj, aperture);
			return NULL;
		}
	}

	return mem;
}

static void *fmm_allocate_host_cpu(void *address, uint64_t MemorySizeInBytes,
				HsaMemFlags mflags)
{
	void *mem = NULL;
	vm_object_t *vm_obj;
	int mmap_prot = PROT_READ;

	if (address)
		return NULL;

	if (mflags.ui32.ExecuteAccess)
		mmap_prot |= PROT_EXEC;

	if (!mflags.ui32.ReadOnly)
		mmap_prot |= PROT_WRITE;

	/* mmap will return a pointer with alignment equal to
	 * sysconf(_SC_PAGESIZE).
	 */
	mem = mmap(NULL, MemorySizeInBytes, mmap_prot,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	if (mem == MAP_FAILED)
		return NULL;

	pthread_mutex_lock(&cpuvm_aperture.fmm_mutex);
	vm_obj = aperture_allocate_object(&cpuvm_aperture, mem, 0,
				      MemorySizeInBytes, mflags);
	if (vm_obj)
		vm_obj->node_id = 0; /* APU systems only have one CPU node */
	pthread_mutex_unlock(&cpuvm_aperture.fmm_mutex);

	return mem;
}

static int bind_mem_to_numa(uint32_t node_id, void *mem,
			    uint64_t SizeInBytes, HsaMemFlags mflags)
{
	int mode = MPOL_F_STATIC_NODES;
	struct bitmask *node_mask;
	int num_node;
	long r;

	pr_debug("%s mem %p flags 0x%x size 0x%lx node_id %d\n", __func__,
		mem, mflags.Value, SizeInBytes, node_id);

	if (mflags.ui32.NoNUMABind)
		return 0;

	if (numa_available() == -1)
		return 0;

	num_node = numa_max_node() + 1;

	/* Ignore binding requests to invalid nodes IDs */
	if (node_id >= (unsigned)num_node) {
		pr_warn("node_id %d >= num_node %d\n", node_id, num_node);
		return 0;
	}

	if (num_node <= 1)
		return 0;

	node_mask = numa_bitmask_alloc(num_node);
	if (!node_mask)
		return -ENOMEM;

#ifdef __PPC64__
	numa_bitmask_setbit(node_mask, node_id * 8);
#else
	numa_bitmask_setbit(node_mask, node_id);
#endif
	mode |= mflags.ui32.NoSubstitute ? MPOL_BIND : MPOL_PREFERRED;
	r = mbind(mem, SizeInBytes, mode, node_mask->maskp, num_node + 1, 0);
	numa_bitmask_free(node_mask);

	if (r) {
		/* If applcation is running inside docker, still return
		 * ok because docker seccomp blocks mbind by default,
		 * otherwise application cannot allocate system memory.
		 */
		if (errno == EPERM) {
			pr_err_once("mbind is blocked by seccomp\n");

			return 0;
		}

		/* Ignore mbind failure if no memory available on node */
		if (!mflags.ui32.NoSubstitute)
			return 0;

		pr_warn_once("Failed to set NUMA policy for %p: %s\n", mem,
			     strerror(errno));

		return -EFAULT;
	}

	return 0;
}

static void *fmm_allocate_host_gpu(uint32_t node_id, void *address,
				   uint64_t MemorySizeInBytes, HsaMemFlags mflags)
{
	void *mem;
	manageable_aperture_t *aperture;
	uint64_t mmap_offset;
	uint32_t ioc_flags;
	uint64_t size;
	int32_t gpu_drm_fd;
	uint32_t gpu_id;
	vm_object_t *vm_obj = NULL;

	if (!g_first_gpu_mem)
		return NULL;

	gpu_id = g_first_gpu_mem->gpu_id;
	gpu_drm_fd = g_first_gpu_mem->drm_render_fd;

	size = MemorySizeInBytes;
	ioc_flags = 0;
	if (mflags.ui32.CoarseGrain)
		aperture = svm.dgpu_aperture;
	else
		aperture = svm.dgpu_alt_aperture; /* always coherent */

	if (!mflags.ui32.CoarseGrain || svm.disable_cache)
		ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_COHERENT;

	if (mflags.ui32.Uncached || svm.disable_cache)
		ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_UNCACHED;

	ioc_flags |= fmm_translate_hsa_to_ioc_flags(mflags);

	if (mflags.ui32.AQLQueueMemory)
		size = MemorySizeInBytes * 2;

	/* Paged memory is allocated as a userptr mapping, non-paged
	 * memory is allocated from KFD
	 */
	if (!mflags.ui32.NonPaged && svm.userptr_for_paged_mem) {
		/* Allocate address space */
		pthread_mutex_lock(&aperture->fmm_mutex);
		mem = aperture_allocate_area(aperture, address, size);
		pthread_mutex_unlock(&aperture->fmm_mutex);
		if (!mem)
			return NULL;

		/* Map anonymous pages */
		if (mmap(mem, MemorySizeInBytes, PROT_READ | PROT_WRITE,
			 MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0)
		    == MAP_FAILED)
			goto out_release_area;

		/* Bind to NUMA node */
		if (bind_mem_to_numa(node_id, mem, MemorySizeInBytes, mflags))
			goto out_release_area;

		/* Mappings in the DGPU aperture don't need to be copied on
		 * fork. This avoids MMU notifiers and evictions due to user
		 * memory mappings on fork.
		 */
		madvise(mem, MemorySizeInBytes, MADV_DONTFORK);

		/* Create userptr BO */
		mmap_offset = (uint64_t)mem;
		ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_USERPTR;
		vm_obj = fmm_allocate_memory_object(gpu_id, mem, size,
						       aperture, &mmap_offset,
						       ioc_flags);
		if (!vm_obj)
			goto out_release_area;
	} else {
		ioc_flags |= KFD_IOC_ALLOC_MEM_FLAGS_GTT;
		mem =  __fmm_allocate_device(gpu_id, address, size, aperture,
					     &mmap_offset, ioc_flags, &vm_obj);

		if (mem && mflags.ui32.HostAccess) {
			int map_fd = gpu_drm_fd;
			void *ret = mmap(mem, MemorySizeInBytes,
					 PROT_READ | PROT_WRITE,
					 MAP_SHARED | MAP_FIXED, map_fd, mmap_offset);
			if (ret == MAP_FAILED) {
				__fmm_release(vm_obj, aperture);
				return NULL;
			}

			madvise(ret, MemorySizeInBytes, MADV_DONTFORK);

			if (mflags.ui32.AQLQueueMemory) {
				uint64_t my_buf_size = size / 2;

				memset(ret, 0, MemorySizeInBytes);
				mmap(VOID_PTR_ADD(mem, my_buf_size), MemorySizeInBytes,
				     PROT_READ | PROT_WRITE,
				     MAP_SHARED | MAP_FIXED, map_fd, mmap_offset);

				madvise(VOID_PTR_ADD(mem, my_buf_size),
					MemorySizeInBytes,
					MADV_DONTFORK);
			}
		}
	}

	if (mem && vm_obj) {
		/* Store memory allocation flags, not ioc flags */
		pthread_mutex_lock(&aperture->fmm_mutex);
		vm_obj->mflags = mflags;
		vm_obj->node_id = node_id;
		pthread_mutex_unlock(&aperture->fmm_mutex);
	}

	return mem;

out_release_area:
	/* Release address space */
	pthread_mutex_lock(&aperture->fmm_mutex);
	aperture_release_area(aperture, mem, size);
	pthread_mutex_unlock(&aperture->fmm_mutex);

	return NULL;
}

void *fmm_allocate_host(uint32_t node_id, void *address,
			uint64_t MemorySizeInBytes, HsaMemFlags mflags)
{
	if (is_dgpu)
		return fmm_allocate_host_gpu(node_id, address, MemorySizeInBytes, mflags);
	return fmm_allocate_host_cpu(address, MemorySizeInBytes, mflags);
}

static int __fmm_release(vm_object_t *object, manageable_aperture_t *aperture)
{
	struct kfd_ioctl_free_memory_of_gpu_args args = {0};

	if (!object)
		return -EINVAL;

	pthread_mutex_lock(&aperture->fmm_mutex);

	if (object->userptr) {
		object->registration_count--;
		if (object->registration_count > 0) {
			pthread_mutex_unlock(&aperture->fmm_mutex);
			return 0;
		}
	}

	/* If memory is user memory and it's still GPU mapped, munmap
	 * would cause an eviction. If the restore happens quickly
	 * enough, restore would also fail with an error message. So
	 * free the BO before unmapping the pages.
	 */
	args.handle = object->handle;
	if (kmtIoctl(kfd_fd, AMDKFD_IOC_FREE_MEMORY_OF_GPU, &args)) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return -errno;
	}

	aperture_release_area(aperture, object->start, object->size);
	vm_remove_object(aperture, object);

	pthread_mutex_unlock(&aperture->fmm_mutex);
	return 0;
}

HSAKMT_STATUS fmm_release(void *address)
{
	manageable_aperture_t *aperture = NULL;
	vm_object_t *object = NULL;
	uint32_t i;

	/* Special handling for scratch memory */
	for (i = 0; i < gpu_mem_count; i++)
		if (gpu_mem[i].gpu_id != NON_VALID_GPU_ID &&
		    address >= gpu_mem[i].scratch_physical.base &&
		    address <= gpu_mem[i].scratch_physical.limit) {
			fmm_release_scratch(gpu_mem[i].gpu_id);
			return HSAKMT_STATUS_SUCCESS;
		}

	object = vm_find_object(address, 0, &aperture);

	if (!object)
		return HSAKMT_STATUS_MEMORY_NOT_REGISTERED;

	if (aperture == &cpuvm_aperture) {
		/* APU system memory */
		uint64_t size = 0;

		size = object->size;
		vm_remove_object(&cpuvm_aperture, object);
		pthread_mutex_unlock(&aperture->fmm_mutex);
		munmap(address, size);
	} else {
		pthread_mutex_unlock(&aperture->fmm_mutex);

		if (__fmm_release(object, aperture))
			return HSAKMT_STATUS_ERROR;

		if (!aperture->is_cpu_accessible)
			fmm_print(gpu_mem[i].gpu_id);
	}

	return HSAKMT_STATUS_SUCCESS;
}

static int fmm_set_memory_policy(uint32_t gpu_id, int default_policy, int alt_policy,
				 uintptr_t alt_base, uint64_t alt_size)
{
	struct kfd_ioctl_set_memory_policy_args args = {0};

	args.gpu_id = gpu_id;
	args.default_policy = default_policy;
	args.alternate_policy = alt_policy;
	args.alternate_aperture_base = alt_base;
	args.alternate_aperture_size = alt_size;

	return kmtIoctl(kfd_fd, AMDKFD_IOC_SET_MEMORY_POLICY, &args);
}

static uint32_t get_vm_alignment(uint32_t device_id)
{
	int page_size = 0;

	if (device_id >= 0x6920 && device_id <= 0x6939) /* Tonga */
		page_size = TONGA_PAGE_SIZE;
	else if (device_id >= 0x9870 && device_id <= 0x9877) /* Carrizo */
		page_size = TONGA_PAGE_SIZE;

	return MAX(PAGE_SIZE, page_size);
}

static HSAKMT_STATUS get_process_apertures(
	struct kfd_process_device_apertures *process_apertures,
	uint32_t *num_of_nodes)
{
	struct kfd_ioctl_get_process_apertures_new_args args_new = {0};
	struct kfd_ioctl_get_process_apertures_args args_old;

	args_new.kfd_process_device_apertures_ptr = (uintptr_t)process_apertures;
	args_new.num_of_nodes = *num_of_nodes;
	if (!kmtIoctl(kfd_fd, AMDKFD_IOC_GET_PROCESS_APERTURES_NEW,
		      (void *)&args_new)) {
		*num_of_nodes = args_new.num_of_nodes;
		return HSAKMT_STATUS_SUCCESS;
	}

	/* New IOCTL failed, try the old one in case we're running on
	 * a really old kernel */
	memset(&args_old, 0, sizeof(args_old));

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_GET_PROCESS_APERTURES,
		     (void *)&args_old))
		return HSAKMT_STATUS_ERROR;

	if (args_old.num_of_nodes < *num_of_nodes)
		*num_of_nodes = args_old.num_of_nodes;

	memcpy(process_apertures, args_old.process_apertures,
	       sizeof(*process_apertures) * *num_of_nodes);

	return HSAKMT_STATUS_SUCCESS;
}

/* The VMs from DRM render nodes are used by KFD for the lifetime of
 * the process. Therefore we have to keep using the same FDs for the
 * lifetime of the process, even when we close and reopen KFD. There
 * are up to 128 render nodes that we cache in this array.
 */
#define DRM_FIRST_RENDER_NODE 128
#define DRM_LAST_RENDER_NODE 255
static int drm_render_fds[DRM_LAST_RENDER_NODE + 1 - DRM_FIRST_RENDER_NODE];

int open_drm_render_device(int minor)
{
	char path[128];
	int index, fd;

	if (minor < DRM_FIRST_RENDER_NODE || minor > DRM_LAST_RENDER_NODE) {
		pr_err("DRM render minor %d out of range [%d, %d]\n", minor,
		       DRM_FIRST_RENDER_NODE, DRM_LAST_RENDER_NODE);
		return -EINVAL;
	}
	index = minor - DRM_FIRST_RENDER_NODE;

	/* If the render node was already opened, keep using the same FD */
	if (drm_render_fds[index])
		return drm_render_fds[index];

	sprintf(path, "/dev/dri/renderD%d", minor);
	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		if (errno != ENOENT && errno != EPERM) {
			pr_err("Failed to open %s: %s\n", path, strerror(errno));
			if (errno == EACCES)
				pr_info("Check user is in \"video\" group\n");
		}
		return -errno;
	}
	drm_render_fds[index] = fd;

	return fd;
}

static HSAKMT_STATUS acquire_vm(uint32_t gpu_id, int fd)
{
	struct kfd_ioctl_acquire_vm_args args;

	args.gpu_id = gpu_id;
	args.drm_fd = fd;
	pr_info("acquiring VM for %x using %d\n", gpu_id, fd);
	if (kmtIoctl(kfd_fd, AMDKFD_IOC_ACQUIRE_VM, (void *)&args)) {
		pr_err("AMDKFD_IOC_ACQUIRE_VM failed\n");
		return HSAKMT_STATUS_ERROR;
	}

	return HSAKMT_STATUS_SUCCESS;
}

static HSAKMT_STATUS init_mmap_apertures(HSAuint64 base, HSAuint64 limit,
					 HSAuint32 align, HSAuint32 guard_pages)
{
	void *addr;

	if (align > (HSAuint32)PAGE_SIZE) {
		/* This should never happen. Alignment constraints
		 * only apply to old GPUs that don't support 48-bit
		 * virtual addresses.
		 */
		pr_info("Falling back to reserved SVM apertures due to alignment contraints.\n");
		return HSAKMT_STATUS_ERROR;
	}

	/* Set up one SVM aperture */
	svm.apertures[SVM_DEFAULT].base  = (void *)base;
	svm.apertures[SVM_DEFAULT].limit = (void *)limit;
	svm.apertures[SVM_DEFAULT].align = align;
	svm.apertures[SVM_DEFAULT].guard_pages = guard_pages;
	svm.apertures[SVM_DEFAULT].is_cpu_accessible = true;
	svm.apertures[SVM_DEFAULT].ops = &mmap_aperture_ops;

	svm.apertures[SVM_COHERENT].base = svm.apertures[SVM_COHERENT].limit =
		NULL;

	/* Try to allocate one page. If it fails, we'll fall back to
	 * managing our own reserved address range.
	 */
	addr = aperture_allocate_area(&svm.apertures[SVM_DEFAULT], NULL, PAGE_SIZE);
	if (addr) {
		aperture_release_area(&svm.apertures[SVM_DEFAULT], addr,
				      PAGE_SIZE);

		svm.dgpu_aperture = svm.dgpu_alt_aperture =
			&svm.apertures[SVM_DEFAULT];
		pr_info("Initialized unreserved SVM apertures: %p - %p\n",
			svm.apertures[SVM_DEFAULT].base,
			svm.apertures[SVM_DEFAULT].limit);
	} else {
		pr_info("Failed to allocate unreserved SVM address space.\n");
		pr_info("Falling back to reserved SVM apertures.\n");
	}

	return addr ? HSAKMT_STATUS_SUCCESS : HSAKMT_STATUS_ERROR;
}

static void *reserve_address(void *addr, unsigned long long int len)
{
	void *ret_addr;

	if (len <= 0)
		return NULL;

	ret_addr = mmap(addr, len, PROT_NONE,
				 MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
	if (ret_addr == MAP_FAILED)
		return NULL;

	return ret_addr;
}

/* Managed SVM aperture limits: only reserve up to 40 bits (1TB, what
 * GFX8 supports). Need to find at least 4GB of usable address space.
 */
#define SVM_RESERVATION_LIMIT ((1ULL << 40) - 1)
#define SVM_MIN_VM_SIZE (4ULL << 30)
#define IS_CANONICAL_ADDR(a) ((a) < (1ULL << 47))

static HSAKMT_STATUS init_svm_apertures(HSAuint64 base, HSAuint64 limit,
					HSAuint32 align, HSAuint32 guard_pages)
{
	const HSAuint64 ADDR_INC = GPU_HUGE_PAGE_SIZE;
	HSAuint64 len, map_size, alt_base, alt_size;
	bool found = false;
	void *addr, *ret_addr;

	/* If we already have an SVM aperture initialized (from a
	 * parent process), keep using it
	 */
	if (dgpu_shared_aperture_limit)
		return HSAKMT_STATUS_SUCCESS;

	/* Align base and limit to huge page size */
	base = ALIGN_UP(base, GPU_HUGE_PAGE_SIZE);
	limit = ((limit + 1) & ~(HSAuint64)(GPU_HUGE_PAGE_SIZE - 1)) - 1;

	/* If the limit is greater or equal 47-bits of address space,
	 * it means we have GFXv9 or later GPUs only. We don't need
	 * apertures to determine the MTYPE and the virtual address
	 * space of the GPUs covers the full CPU address range (on
	 * x86_64) or at least mmap is unlikely to run out of
	 * addresses the GPUs can handle.
	 */
	if (limit >= (1ULL << 47) - 1 && !svm.reserve_svm) {
		HSAKMT_STATUS status = init_mmap_apertures(base, limit, align,
							   guard_pages);

		if (status == HSAKMT_STATUS_SUCCESS)
			return status;
		/* fall through: fall back to reserved address space */
	}

	if (limit > SVM_RESERVATION_LIMIT)
		limit = SVM_RESERVATION_LIMIT;
	if (base >= limit) {
		pr_err("No SVM range compatible with all GPU and software constraints\n");
		return HSAKMT_STATUS_ERROR;
	}

	/* Try to reserve address space for SVM.
	 *
	 * Inner loop: try start addresses in huge-page increments up
	 * to half the VM size we're trying to reserve
	 *
	 * Outer loop: reduce size of the allocation by factor 2 at a
	 * time and print a warning for every reduction
	 */
	for (len = limit - base + 1; !found && len >= SVM_MIN_VM_SIZE;
	     len = (len + 1) >> 1) {
		for (addr = (void *)base, ret_addr = NULL;
		     (HSAuint64)addr + ((len + 1) >> 1) - 1 <= limit;
		     addr = (void *)((HSAuint64)addr + ADDR_INC)) {
			HSAuint64 top = MIN((HSAuint64)addr + len, limit+1);

			map_size = (top - (HSAuint64)addr) &
				~(HSAuint64)(PAGE_SIZE - 1);
			if (map_size < SVM_MIN_VM_SIZE)
				break;

			ret_addr = reserve_address(addr, map_size);
			if (!ret_addr)
				break;
			if ((HSAuint64)ret_addr + ((len + 1) >> 1) - 1 <= limit)
				/* At least half the returned address
				 * space is GPU addressable, we'll
				 * take it
				 */
				break;
			munmap(ret_addr, map_size);
			ret_addr = NULL;
		}
		if (!ret_addr) {
			pr_warn("Failed to reserve %uGB for SVM ...\n",
				(unsigned int)(len >> 30));
			continue;
		}
		if ((HSAuint64)ret_addr + SVM_MIN_VM_SIZE - 1 > limit) {
			/* addressable size is less than the minimum */
			pr_warn("Got %uGB for SVM at %p with only %dGB usable ...\n",
				(unsigned int)(map_size >> 30), ret_addr,
				(int)((limit - (HSAint64)ret_addr) >> 30));
			munmap(ret_addr, map_size);
			ret_addr = NULL;
			continue;
		} else {
			found = true;
			break;
		}
	}

	if (!found) {
		pr_err("Failed to reserve SVM address range. Giving up.\n");
		return HSAKMT_STATUS_ERROR;
	}

	base = (HSAuint64)ret_addr;
	if (base + map_size - 1 > limit)
		/* trim the tail that's not GPU-addressable */
		munmap((void *)(limit + 1), base + map_size - 1 - limit);
	else
		limit = base + map_size - 1;

	/* init two apertures for non-coherent and coherent memory */
	svm.apertures[SVM_DEFAULT].base  = dgpu_shared_aperture_base  = ret_addr;
	svm.apertures[SVM_DEFAULT].limit = dgpu_shared_aperture_limit = (void *)limit;
	svm.apertures[SVM_DEFAULT].align = align;
	svm.apertures[SVM_DEFAULT].guard_pages = guard_pages;
	svm.apertures[SVM_DEFAULT].is_cpu_accessible = true;
	svm.apertures[SVM_DEFAULT].ops = &reserved_aperture_ops;

	/* Use the first 1/4 of the dGPU aperture as
	 * alternate aperture for coherent access.
	 * Base and size must be 64KB aligned.
	 */
	alt_base = (HSAuint64)svm.apertures[SVM_DEFAULT].base;
	alt_size = (VOID_PTRS_SUB(svm.apertures[SVM_DEFAULT].limit,
				  svm.apertures[SVM_DEFAULT].base) + 1) >> 2;
	alt_base = (alt_base + 0xffff) & ~0xffffULL;
	alt_size = (alt_size + 0xffff) & ~0xffffULL;
	svm.apertures[SVM_COHERENT].base = (void *)alt_base;
	svm.apertures[SVM_COHERENT].limit = (void *)(alt_base + alt_size - 1);
	svm.apertures[SVM_COHERENT].align = align;
	svm.apertures[SVM_COHERENT].guard_pages = guard_pages;
	svm.apertures[SVM_COHERENT].is_cpu_accessible = true;
	svm.apertures[SVM_COHERENT].ops = &reserved_aperture_ops;

	svm.apertures[SVM_DEFAULT].base = VOID_PTR_ADD(svm.apertures[SVM_COHERENT].limit, 1);

	pr_info("SVM alt (coherent): %12p - %12p\n",
		svm.apertures[SVM_COHERENT].base, svm.apertures[SVM_COHERENT].limit);
	pr_info("SVM (non-coherent): %12p - %12p\n",
		svm.apertures[SVM_DEFAULT].base, svm.apertures[SVM_DEFAULT].limit);

	svm.dgpu_aperture = &svm.apertures[SVM_DEFAULT];
	svm.dgpu_alt_aperture = &svm.apertures[SVM_COHERENT];

	return HSAKMT_STATUS_SUCCESS;
}

static void fmm_init_rbtree(void)
{
	static int once;
	int i = gpu_mem_count;

	if (once++ == 0) {
		rbtree_init(&svm.apertures[SVM_DEFAULT].tree);
		rbtree_init(&svm.apertures[SVM_DEFAULT].user_tree);
		rbtree_init(&svm.apertures[SVM_COHERENT].tree);
		rbtree_init(&svm.apertures[SVM_COHERENT].user_tree);
		rbtree_init(&cpuvm_aperture.tree);
		rbtree_init(&cpuvm_aperture.user_tree);
	}

	while (i--) {
		rbtree_init(&gpu_mem[i].scratch_physical.tree);
		rbtree_init(&gpu_mem[i].scratch_physical.user_tree);
		rbtree_init(&gpu_mem[i].gpuvm_aperture.tree);
		rbtree_init(&gpu_mem[i].gpuvm_aperture.user_tree);
	}
}

static void *map_mmio(uint32_t node_id, uint32_t gpu_id, int mmap_fd)
{
	void *mem;
	manageable_aperture_t *aperture = svm.dgpu_alt_aperture;
	uint32_t ioc_flags;
	vm_object_t *vm_obj = NULL;
	HsaMemFlags mflags;
	void *ret;
	uint64_t mmap_offset;

	/* Allocate physical memory and vm object*/
	ioc_flags = KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP |
		KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE |
		KFD_IOC_ALLOC_MEM_FLAGS_COHERENT;
	mem = __fmm_allocate_device(gpu_id, NULL, PAGE_SIZE, aperture,
			&mmap_offset, ioc_flags, &vm_obj);

	if (!mem || !vm_obj)
		return NULL;

	mflags.Value = 0;
	mflags.ui32.NonPaged = 1;
	mflags.ui32.HostAccess = 1;
	mflags.ui32.Reserved = 0;
	pthread_mutex_lock(&aperture->fmm_mutex);
	vm_obj->mflags = mflags;
	vm_obj->node_id = node_id;
	pthread_mutex_unlock(&aperture->fmm_mutex);

	/* Map for CPU access*/
	ret = mmap(mem, PAGE_SIZE,
			 PROT_READ | PROT_WRITE,
			 MAP_SHARED | MAP_FIXED, mmap_fd,
			 mmap_offset);
	if (ret == MAP_FAILED) {
		__fmm_release(vm_obj, aperture);
		return NULL;
	}

	/* Map for GPU access*/
	if (fmm_map_to_gpu(mem, PAGE_SIZE, NULL)) {
		__fmm_release(vm_obj, aperture);
		return NULL;
	}

	return mem;
}

static void release_mmio(void)
{
	uint32_t gpu_mem_id;

	for (gpu_mem_id = 0; (uint32_t)gpu_mem_id < gpu_mem_count; gpu_mem_id++) {
		if (!gpu_mem[gpu_mem_id].mmio_aperture.base)
			continue;
		fmm_unmap_from_gpu(gpu_mem[gpu_mem_id].mmio_aperture.base);
		munmap(gpu_mem[gpu_mem_id].mmio_aperture.base, PAGE_SIZE);
		fmm_release(gpu_mem[gpu_mem_id].mmio_aperture.base);
	}
}

HSAKMT_STATUS fmm_init_process_apertures(unsigned int NumNodes)
{
	uint32_t i;
	int32_t gpu_mem_id = 0;
	uint32_t gpu_id;
	HsaNodeProperties props;
	struct kfd_process_device_apertures *process_apertures;
	uint32_t num_of_sysfs_nodes;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;
	char *disableCache, *pagedUserptr, *checkUserptr, *guardPagesStr, *reserveSvm;
	char *maxVaAlignStr;
	unsigned int guardPages = 1;
	uint64_t svm_base = 0, svm_limit = 0;
	uint32_t svm_alignment = 0;

	/* If HSA_DISABLE_CACHE is set to a non-0 value, disable caching */
	disableCache = getenv("HSA_DISABLE_CACHE");
	svm.disable_cache = (disableCache && strcmp(disableCache, "0"));

	/* If HSA_USERPTR_FOR_PAGED_MEM is not set or set to a non-0
	 * value, enable userptr for all paged memory allocations
	 */
	pagedUserptr = getenv("HSA_USERPTR_FOR_PAGED_MEM");
	svm.userptr_for_paged_mem = (!pagedUserptr || strcmp(pagedUserptr, "0"));

	/* If HSA_CHECK_USERPTR is set to a non-0 value, check all userptrs
	 * when they are registered
	 */
	checkUserptr = getenv("HSA_CHECK_USERPTR");
	svm.check_userptr = (checkUserptr && strcmp(checkUserptr, "0"));

	/* If HSA_RESERVE_SVM is set to a non-0 value,
	 * enable packet capture and replay mode.
	 */
	reserveSvm = getenv("HSA_RESERVE_SVM");
	svm.reserve_svm = (reserveSvm && strcmp(reserveSvm, "0"));

	/* Specify number of guard pages for SVM apertures, default is 1 */
	guardPagesStr = getenv("HSA_SVM_GUARD_PAGES");
	if (!guardPagesStr || sscanf(guardPagesStr, "%u", &guardPages) != 1)
		guardPages = 1;

	/* Sets the max VA alignment order size during mapping. By default the order
	 * size is set to 9(2MB)
	 */
	maxVaAlignStr = getenv("HSA_MAX_VA_ALIGN");
	if (!maxVaAlignStr || sscanf(maxVaAlignStr, "%u", &svm.alignment_order) != 1)
		svm.alignment_order = 9;

	gpu_mem_count = 0;
	g_first_gpu_mem = NULL;

	/* Trade off - NumNodes includes GPU nodes + CPU Node. So in
	 * systems with CPU node, slightly more memory is allocated than
	 * necessary
	 */
	gpu_mem = (gpu_mem_t *)calloc(NumNodes, sizeof(gpu_mem_t));
	if (!gpu_mem)
		return HSAKMT_STATUS_NO_MEMORY;

	/* Initialize gpu_mem[] from sysfs topology. Rest of the members are
	 * set to 0 by calloc. This is necessary because this function
	 * gets called before hsaKmtAcquireSystemProperties() is called.
	 */

	is_dgpu = false;

	for (i = 0; i < NumNodes; i++) {
		memset(&props, 0, sizeof(props));
		ret = topology_sysfs_get_node_props(i, &props, &gpu_id, NULL, NULL);
		if (ret != HSAKMT_STATUS_SUCCESS)
			goto sysfs_parse_failed;

		topology_setup_is_dgpu_param(&props);

		/* Skip non-GPU nodes */
		if (gpu_id != 0) {
			int fd = open_drm_render_device(props.DrmRenderMinor);
			if (fd <= 0) {
				ret = HSAKMT_STATUS_ERROR;
				goto sysfs_parse_failed;
			}

			gpu_mem[gpu_mem_count].EngineId.ui32.Major = props.EngineId.ui32.Major;
			gpu_mem[gpu_mem_count].EngineId.ui32.Minor = props.EngineId.ui32.Minor;
			gpu_mem[gpu_mem_count].EngineId.ui32.Stepping = props.EngineId.ui32.Stepping;

			gpu_mem[gpu_mem_count].drm_render_fd = fd;
			gpu_mem[gpu_mem_count].gpu_id = gpu_id;
			gpu_mem[gpu_mem_count].local_mem_size = props.LocalMemSize;
			gpu_mem[gpu_mem_count].device_id = props.DeviceId;
			gpu_mem[gpu_mem_count].node_id = i;

			gpu_mem[gpu_mem_count].scratch_physical.align = PAGE_SIZE;
			gpu_mem[gpu_mem_count].scratch_physical.ops = &reserved_aperture_ops;
			pthread_mutex_init(&gpu_mem[gpu_mem_count].scratch_physical.fmm_mutex, NULL);

			gpu_mem[gpu_mem_count].gpuvm_aperture.align =
				get_vm_alignment(props.DeviceId);
			gpu_mem[gpu_mem_count].gpuvm_aperture.guard_pages = guardPages;
			gpu_mem[gpu_mem_count].gpuvm_aperture.ops = &reserved_aperture_ops;
			pthread_mutex_init(&gpu_mem[gpu_mem_count].gpuvm_aperture.fmm_mutex, NULL);

			if (!g_first_gpu_mem)
				g_first_gpu_mem = &gpu_mem[gpu_mem_count];

			gpu_mem_count++;
		}
	}

	/* The ioctl will also return Number of Nodes if
	 * args.kfd_process_device_apertures_ptr is set to NULL. This is not
	 * required since Number of nodes is already known. Kernel will fill in
	 * the apertures in kfd_process_device_apertures_ptr
	 */
	num_of_sysfs_nodes = get_num_sysfs_nodes();
	if (num_of_sysfs_nodes < gpu_mem_count) {
		ret = HSAKMT_STATUS_ERROR;
		goto sysfs_parse_failed;
	}

	process_apertures = calloc(num_of_sysfs_nodes, sizeof(struct kfd_process_device_apertures));
	if (!process_apertures) {
		ret = HSAKMT_STATUS_NO_MEMORY;
		goto sysfs_parse_failed;
	}

	/* GPU Resource management can disable some of the GPU nodes.
	 * The Kernel driver could be not aware of this.
	 * Get from Kernel driver information of all the nodes and then filter it.
	 */
	ret = get_process_apertures(process_apertures, &num_of_sysfs_nodes);
	if (ret != HSAKMT_STATUS_SUCCESS)
		goto get_aperture_ioctl_failed;

	all_gpu_id_array_size = 0;
	all_gpu_id_array = NULL;
	if (num_of_sysfs_nodes > 0) {
		all_gpu_id_array = malloc(sizeof(uint32_t) * gpu_mem_count);
		if (!all_gpu_id_array) {
			ret = HSAKMT_STATUS_NO_MEMORY;
			goto get_aperture_ioctl_failed;
		}
	}

	for (i = 0 ; i < num_of_sysfs_nodes ; i++) {
		/* Map Kernel process device data node i <--> gpu_mem_id which
		 * indexes into gpu_mem[] based on gpu_id
		 */
		gpu_mem_id = gpu_mem_find_by_gpu_id(process_apertures[i].gpu_id);
		if (gpu_mem_id < 0)
			continue;

		if (all_gpu_id_array_size == gpu_mem_count) {
			ret = HSAKMT_STATUS_ERROR;
			goto invalid_gpu_id;
		}
		all_gpu_id_array[all_gpu_id_array_size++] = process_apertures[i].gpu_id;

		gpu_mem[gpu_mem_id].lds_aperture.base =
			PORT_UINT64_TO_VPTR(process_apertures[i].lds_base);
		gpu_mem[gpu_mem_id].lds_aperture.limit =
			PORT_UINT64_TO_VPTR(process_apertures[i].lds_limit);

		gpu_mem[gpu_mem_id].scratch_aperture.base =
			PORT_UINT64_TO_VPTR(process_apertures[i].scratch_base);
		gpu_mem[gpu_mem_id].scratch_aperture.limit =
			PORT_UINT64_TO_VPTR(process_apertures[i].scratch_limit);

		if (IS_CANONICAL_ADDR(process_apertures[i].gpuvm_limit)) {
			uint64_t vm_alignment = get_vm_alignment(
				gpu_mem[gpu_mem_id].device_id);

			/* Set proper alignment for scratch backing aperture */
			gpu_mem[gpu_mem_id].scratch_physical.align = vm_alignment;

			/* Non-canonical per-ASIC GPUVM aperture does
			 * not exist on dGPUs in GPUVM64 address mode
			 */
			gpu_mem[gpu_mem_id].gpuvm_aperture.base = NULL;
			gpu_mem[gpu_mem_id].gpuvm_aperture.limit = NULL;

			/* Update SVM aperture limits and alignment */
			if (process_apertures[i].gpuvm_base > svm_base)
				svm_base = process_apertures[i].gpuvm_base;
			if (process_apertures[i].gpuvm_limit < svm_limit ||
			    svm_limit == 0)
				svm_limit = process_apertures[i].gpuvm_limit;
			if (vm_alignment > svm_alignment)
				svm_alignment = vm_alignment;
		} else {
			gpu_mem[gpu_mem_id].gpuvm_aperture.base =
				PORT_UINT64_TO_VPTR(process_apertures[i].gpuvm_base);
			gpu_mem[gpu_mem_id].gpuvm_aperture.limit =
				PORT_UINT64_TO_VPTR(process_apertures[i].gpuvm_limit);
			/* Reserve space at the start of the
			 * aperture. After subtracting the base, we
			 * don't want valid pointers to become NULL.
			 */
			aperture_allocate_area(
				&gpu_mem[gpu_mem_id].gpuvm_aperture,
				NULL,
				gpu_mem[gpu_mem_id].gpuvm_aperture.align);
		}

		/* Acquire the VM from the DRM render node for KFD use */
		ret = acquire_vm(gpu_mem[gpu_mem_id].gpu_id,
				 gpu_mem[gpu_mem_id].drm_render_fd);
		if (ret != HSAKMT_STATUS_SUCCESS)
			goto acquire_vm_failed;
	}
	all_gpu_id_array_size *= sizeof(uint32_t);

	if (svm_limit) {
		/* At least one GPU uses GPUVM in canonical address
		 * space. Set up SVM apertures shared by all such GPUs
		 */
		ret = init_svm_apertures(svm_base, svm_limit, svm_alignment,
					 guardPages);
		if (ret != HSAKMT_STATUS_SUCCESS)
			goto init_svm_failed;

		for (i = 0 ; i < num_of_sysfs_nodes ; i++) {
			uintptr_t alt_base;
			uint64_t alt_size;
			int err;

			if (!IS_CANONICAL_ADDR(process_apertures[i].gpuvm_limit))
				continue;

			/* Set memory policy to match the SVM apertures */
			alt_base = (uintptr_t)svm.dgpu_alt_aperture->base;
			alt_size = VOID_PTRS_SUB(svm.dgpu_alt_aperture->limit,
				svm.dgpu_alt_aperture->base) + 1;
			err = fmm_set_memory_policy(process_apertures[i].gpu_id,
						    svm.disable_cache ?
						    KFD_IOC_CACHE_POLICY_COHERENT :
						    KFD_IOC_CACHE_POLICY_NONCOHERENT,
						    KFD_IOC_CACHE_POLICY_COHERENT,
						    alt_base, alt_size);
			if (err) {
				pr_err("Failed to set mem policy for GPU [0x%x]\n",
				       process_apertures[i].gpu_id);
				ret = HSAKMT_STATUS_ERROR;
				goto set_memory_policy_failed;
			}
		}
	}

	cpuvm_aperture.align = PAGE_SIZE;
	cpuvm_aperture.limit = (void *)0x7FFFFFFFFFFF; /* 2^47 - 1 */

	fmm_init_rbtree();

	for (gpu_mem_id = 0; (uint32_t)gpu_mem_id < gpu_mem_count; gpu_mem_id++) {
		if (!topology_is_svm_needed(gpu_mem[gpu_mem_id].EngineId))
			continue;
		gpu_mem[gpu_mem_id].mmio_aperture.base = map_mmio(
				gpu_mem[gpu_mem_id].node_id,
				gpu_mem[gpu_mem_id].gpu_id,
				kfd_fd);
		if (gpu_mem[gpu_mem_id].mmio_aperture.base)
			gpu_mem[gpu_mem_id].mmio_aperture.limit = (void *)
			((char *)gpu_mem[gpu_mem_id].mmio_aperture.base +
			 PAGE_SIZE - 1);
		else
			pr_err("Failed to map remapped mmio page on gpu_mem %d\n",
					gpu_mem_id);
	}

	free(process_apertures);
	return ret;

invalid_gpu_id:
init_svm_failed:
acquire_vm_failed:
set_memory_policy_failed:
	free(all_gpu_id_array);
	all_gpu_id_array = NULL;
get_aperture_ioctl_failed:
	free(process_apertures);
sysfs_parse_failed:
	fmm_destroy_process_apertures();
	return ret;
}

void fmm_destroy_process_apertures(void)
{
	release_mmio();
	if (gpu_mem) {
		free(gpu_mem);
		gpu_mem = NULL;
	}
	gpu_mem_count = 0;
}

HSAKMT_STATUS fmm_get_aperture_base_and_limit(aperture_type_e aperture_type, HSAuint32 gpu_id,
			HSAuint64 *aperture_base, HSAuint64 *aperture_limit)
{
	HSAKMT_STATUS err = HSAKMT_STATUS_ERROR;
	int32_t slot = gpu_mem_find_by_gpu_id(gpu_id);

	if (slot < 0)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	switch (aperture_type) {
	case FMM_GPUVM:
		if (aperture_is_valid(gpu_mem[slot].gpuvm_aperture.base,
			gpu_mem[slot].gpuvm_aperture.limit)) {
			*aperture_base = PORT_VPTR_TO_UINT64(gpu_mem[slot].gpuvm_aperture.base);
			*aperture_limit = PORT_VPTR_TO_UINT64(gpu_mem[slot].gpuvm_aperture.limit);
			err = HSAKMT_STATUS_SUCCESS;
		}
		break;

	case FMM_SCRATCH:
		if (aperture_is_valid(gpu_mem[slot].scratch_aperture.base,
			gpu_mem[slot].scratch_aperture.limit)) {
			*aperture_base = PORT_VPTR_TO_UINT64(gpu_mem[slot].scratch_aperture.base);
			*aperture_limit = PORT_VPTR_TO_UINT64(gpu_mem[slot].scratch_aperture.limit);
			err = HSAKMT_STATUS_SUCCESS;
		}
		break;

	case FMM_LDS:
		if (aperture_is_valid(gpu_mem[slot].lds_aperture.base,
			gpu_mem[slot].lds_aperture.limit)) {
			*aperture_base = PORT_VPTR_TO_UINT64(gpu_mem[slot].lds_aperture.base);
			*aperture_limit = PORT_VPTR_TO_UINT64(gpu_mem[slot].lds_aperture.limit);
			err = HSAKMT_STATUS_SUCCESS;
		}
		break;

	case FMM_SVM:
		/* Report single SVM aperture, starting at base of
		 * fine-grained, ending at limit of coarse-grained
		 */
		if (aperture_is_valid(svm.dgpu_alt_aperture->base,
				      svm.dgpu_aperture->limit)) {
			*aperture_base = PORT_VPTR_TO_UINT64(svm.dgpu_alt_aperture->base);
			*aperture_limit = PORT_VPTR_TO_UINT64(svm.dgpu_aperture->limit);
			err = HSAKMT_STATUS_SUCCESS;
		}
		break;

	case FMM_MMIO:
		if (aperture_is_valid(gpu_mem[slot].mmio_aperture.base,
			gpu_mem[slot].mmio_aperture.limit)) {
			*aperture_base = PORT_VPTR_TO_UINT64(gpu_mem[slot].mmio_aperture.base);
			*aperture_limit = PORT_VPTR_TO_UINT64(gpu_mem[slot].mmio_aperture.limit);
			err = HSAKMT_STATUS_SUCCESS;
		}
		break;

	default:
		break;
	}

	return err;
}

static bool id_in_array(uint32_t id, uint32_t *ids_array,
		uint32_t ids_array_size)
{
	uint32_t i;

	for (i = 0; i < ids_array_size/sizeof(uint32_t); i++) {
		if (id == ids_array[i])
			return true;
	}
	return false;
}

/* Helper function to remove ids_array from
 * obj->mapped_device_id_array
 */
static void remove_device_ids_from_mapped_array(vm_object_t *obj,
		uint32_t *ids_array, uint32_t ids_array_size)
{
	uint32_t i = 0, j = 0;

	if (obj->mapped_device_id_array == ids_array)
		goto set_size_and_free;

	for (i = 0; i < obj->mapped_device_id_array_size/
			sizeof(uint32_t); i++) {
		if (!id_in_array(obj->mapped_device_id_array[i],
					ids_array, ids_array_size))
			obj->mapped_device_id_array[j++] =
				obj->mapped_device_id_array[i];
	}

set_size_and_free:
	obj->mapped_device_id_array_size = j*sizeof(uint32_t);
	if (!j) {
		if (obj->mapped_device_id_array)
			free(obj->mapped_device_id_array);

		obj->mapped_device_id_array = NULL;
	}
}

/* Helper function to add ids_array to
 * obj->mapped_device_id_array
 */
static void add_device_ids_to_mapped_array(vm_object_t *obj,
		uint32_t *ids_array, uint32_t ids_array_size)
{
	uint32_t new_array_size;

	/* Remove any potential duplicated ids */
	remove_device_ids_from_mapped_array(obj, ids_array, ids_array_size);
	new_array_size = obj->mapped_device_id_array_size
		+ ids_array_size;

	obj->mapped_device_id_array = (uint32_t *)realloc(
			obj->mapped_device_id_array, new_array_size);

	memcpy(&obj->mapped_device_id_array
			[obj->mapped_device_id_array_size/sizeof(uint32_t)],
			ids_array, ids_array_size);

	obj->mapped_device_id_array_size = new_array_size;
}


/* If nodes_to_map is not NULL, map the nodes specified; otherwise map all. */
static int _fmm_map_to_gpu(manageable_aperture_t *aperture,
			void *address, uint64_t size, vm_object_t *obj,
			uint32_t *nodes_to_map, uint32_t nodes_array_size)
{
	struct kfd_ioctl_map_memory_to_gpu_args args = {0};
	vm_object_t *object;
	int ret = 0;

	if (!obj)
		pthread_mutex_lock(&aperture->fmm_mutex);

	object = obj;
	if (!object) {
		/* Find the object to retrieve the handle */
		object = vm_find_object_by_address(aperture, address, 0);
		if (!object) {
			ret = -EINVAL;
			goto err_object_not_found;
		}
	}

	/* For a memory region that is registered by user pointer, changing
	 * mapping nodes is not allowed, so we don't need to check the mapping
	 * nodes or map if it's already mapped. Just increase the reference.
	 */
	if (object->userptr && object->mapping_count) {
		++object->mapping_count;
		goto exit_ok;
	}

	args.handle = object->handle;
	if (nodes_to_map) {
	/* If specified, map the requested */
		args.device_ids_array_ptr = (uint64_t)nodes_to_map;
		args.n_devices = nodes_array_size / sizeof(uint32_t);
	} else if (object->registered_device_id_array_size > 0) {
	/* otherwise map all registered */
		args.device_ids_array_ptr =
			(uint64_t)object->registered_device_id_array;
		args.n_devices = object->registered_device_id_array_size /
			sizeof(uint32_t);
	} else {
	/* not specified, not registered: map all GPUs */
		args.device_ids_array_ptr = (uint64_t)all_gpu_id_array;
		args.n_devices = all_gpu_id_array_size / sizeof(uint32_t);
	}
	args.n_success = 0;

	ret = kmtIoctl(kfd_fd, AMDKFD_IOC_MAP_MEMORY_TO_GPU, &args);
	if (ret) {
		pr_err("GPU mapping failed (%d) for obj at %p, userptr %p, size %lu",
		       ret, object->start, object->userptr, object->size);
		goto err_map_failed;
	}

	add_device_ids_to_mapped_array(object,
				(uint32_t *)args.device_ids_array_ptr,
				args.n_success * sizeof(uint32_t));
	print_device_id_array((uint32_t *)object->mapped_device_id_array,
			      object->mapped_device_id_array_size);

	object->mapping_count = 1;
	/* Mapping changed and lifecycle of object->mapped_node_id_array
	 * terminates here. Free it and allocate on next query
	 */
	if (object->mapped_node_id_array) {
		free(object->mapped_node_id_array);
		object->mapped_node_id_array = NULL;
	}

exit_ok:
err_object_not_found:
err_map_failed:
	if (!obj)
		pthread_mutex_unlock(&aperture->fmm_mutex);

	return ret;
}

static int _fmm_map_to_gpu_scratch(uint32_t gpu_id, manageable_aperture_t *aperture,
				   void *address, uint64_t size)
{
	int32_t gpu_mem_id;
	int ret;
	bool is_debugger = 0;
	void *mmap_ret = NULL;
	uint64_t mmap_offset = 0;
	int map_fd;
	vm_object_t *obj;

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return -1;

	if (!is_dgpu)
		return 0; /* Nothing to do on APU */

	/* sanity check the address */
	if (address < aperture->base ||
	    VOID_PTR_ADD(address, size - 1) > aperture->limit)
		return -1;

	ret = debug_get_reg_status(gpu_mem[gpu_mem_id].node_id, &is_debugger);
	/* allocate object within the scratch backing aperture */
	if (!ret && !is_debugger) {
		obj = fmm_allocate_memory_object(
			gpu_id, address, size, aperture, &mmap_offset,
			KFD_IOC_ALLOC_MEM_FLAGS_VRAM |
			KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE);
		if (!obj)
			return -1;
		/* Create a CPU mapping for the debugger */
		map_fd = gpu_mem[gpu_mem_id].drm_render_fd;
		mmap_ret = mmap(address, size, PROT_NONE,
				MAP_PRIVATE | MAP_FIXED, map_fd, mmap_offset);
		if (mmap_ret == MAP_FAILED) {
			__fmm_release(obj, aperture);
			return -1;
		}
	} else {
		obj = fmm_allocate_memory_object(
			gpu_id, address, size, aperture, &mmap_offset,
			KFD_IOC_ALLOC_MEM_FLAGS_GTT |
			KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE);
		map_fd = gpu_mem[gpu_mem_id].drm_render_fd;
		mmap_ret = mmap(address, size,
				PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_FIXED, map_fd, mmap_offset);
		if (mmap_ret == MAP_FAILED) {
			__fmm_release(obj, aperture);
			return -1;
		}
	}
	madvise(mmap_ret, size, MADV_DONTFORK);

	/* map to GPU */
	ret = _fmm_map_to_gpu(aperture, address, size, NULL, &gpu_id, sizeof(uint32_t));
	if (ret != 0)
		__fmm_release(obj, aperture);

	return ret;
}

static int _fmm_map_to_gpu_userptr(void *addr, uint64_t size,
				   uint64_t *gpuvm_addr, vm_object_t *object)
{
	manageable_aperture_t *aperture;
	void *svm_addr;
	HSAuint64 svm_size;
	HSAuint32 page_offset = (HSAuint64)addr & (PAGE_SIZE-1);
	int ret;

	aperture = svm.dgpu_aperture;

	svm_addr = object->start;
	svm_size = object->size;

	/* Map and return the GPUVM address adjusted by the offset
	 * from the start of the page
	 */
	ret = _fmm_map_to_gpu(aperture, svm_addr, svm_size, object, NULL, 0);
	if (ret == 0 && gpuvm_addr)
		*gpuvm_addr = (uint64_t)svm_addr + page_offset;

	return ret;
}

int fmm_map_to_gpu(void *address, uint64_t size, uint64_t *gpuvm_address)
{
	manageable_aperture_t *aperture;
	vm_object_t *object;
	uint32_t i;
	int ret;

	/* Special handling for scratch memory */
	for (i = 0; i < gpu_mem_count; i++)
		if (gpu_mem[i].gpu_id != NON_VALID_GPU_ID &&
		    address >= gpu_mem[i].scratch_physical.base &&
		    address <= gpu_mem[i].scratch_physical.limit)
			return _fmm_map_to_gpu_scratch(gpu_mem[i].gpu_id,
							&gpu_mem[i].scratch_physical,
							address, size);

	object = vm_find_object(address, size, &aperture);
	if (!object) {
		if (!is_dgpu) {
			/* Prefetch memory on APUs with dummy-reads */
			fmm_check_user_memory(address, size);
			return 0;
		}
		pr_err("Object not found at %p\n", address);
		return -EINVAL;
	}
	/* Successful vm_find_object returns with the aperture locked */

	if (aperture == &cpuvm_aperture) {
		/* Prefetch memory on APUs with dummy-reads */
		fmm_check_user_memory(address, size);
		ret = 0;
	} else if (object->userptr) {
		ret = _fmm_map_to_gpu_userptr(address, size, gpuvm_address, object);
	} else {
		ret = _fmm_map_to_gpu(aperture, address, size, object, NULL, 0);
		/* Update alternate GPUVM address only for
		 * CPU-invisible apertures on old APUs
		 */
		if (!ret && gpuvm_address && !aperture->is_cpu_accessible)
			*gpuvm_address = VOID_PTRS_SUB(object->start, aperture->base);
	}

	pthread_mutex_unlock(&aperture->fmm_mutex);
	return ret;
}

static void print_device_id_array(uint32_t *device_id_array, uint32_t device_id_array_size)
{
#ifdef DEBUG_PRINT_APERTURE
	device_id_array_size /= sizeof(uint32_t);

	pr_info("device id array size %d\n", device_id_array_size);

	for (uint32_t i = 0 ; i < device_id_array_size; i++)
		pr_info("%d . 0x%x\n", (i+1), device_id_array[i]);
#endif
}

static int _fmm_unmap_from_gpu(manageable_aperture_t *aperture, void *address,
		uint32_t *device_ids_array, uint32_t device_ids_array_size,
		vm_object_t *obj)
{
	vm_object_t *object;
	int ret = 0;
	struct kfd_ioctl_unmap_memory_from_gpu_args args = {0};
	HSAuint32 page_offset = (HSAint64)address & (PAGE_SIZE - 1);

	if (!obj)
		pthread_mutex_lock(&aperture->fmm_mutex);

	/* Find the object to retrieve the handle */
	object = obj;
	if (!object) {
		object = vm_find_object_by_address(aperture,
					VOID_PTR_SUB(address, page_offset), 0);
		if (!object) {
			ret = -1;
			goto out;
		}
	}

	if (object->userptr && object->mapping_count > 1) {
		--object->mapping_count;
		goto out;
	}

	args.handle = object->handle;
	if (device_ids_array && device_ids_array_size > 0) {
		args.device_ids_array_ptr = (uint64_t)device_ids_array;
		args.n_devices = device_ids_array_size / sizeof(uint32_t);
	} else if (object->mapped_device_id_array_size > 0) {
		args.device_ids_array_ptr = (uint64_t)object->mapped_device_id_array;
		args.n_devices = object->mapped_device_id_array_size /
			sizeof(uint32_t);
	} else {
		/*
		 * When unmap exits here it should return failing error code as the user tried to
		 * unmap already unmapped buffer. Currently we returns success as KFDTEST and RT
		 * need to deploy the change on there side before thunk fails on this case.
		 */
		ret = 0;
		goto out;
	}
	args.n_success = 0;

	print_device_id_array((void *)args.device_ids_array_ptr,
			      args.n_devices * sizeof(uint32_t));

	ret = kmtIoctl(kfd_fd, AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU, &args);

	remove_device_ids_from_mapped_array(object,
			(uint32_t *)args.device_ids_array_ptr,
			args.n_success * sizeof(uint32_t));

	if (object->mapped_node_id_array)
		free(object->mapped_node_id_array);
	object->mapped_node_id_array = NULL;
	object->mapping_count = 0;

out:
	if (!obj)
		pthread_mutex_unlock(&aperture->fmm_mutex);
	return ret;
}

static int _fmm_unmap_from_gpu_scratch(uint32_t gpu_id,
				       manageable_aperture_t *aperture,
				       void *address)
{
	int32_t gpu_mem_id;
	vm_object_t *object;
	struct kfd_ioctl_unmap_memory_from_gpu_args args = {0};
	int ret;

	/* Retrieve gpu_mem id according to gpu_id */
	gpu_mem_id = gpu_mem_find_by_gpu_id(gpu_id);
	if (gpu_mem_id < 0)
		return -1;

	if (!is_dgpu)
		return 0; /* Nothing to do on APU */

	pthread_mutex_lock(&aperture->fmm_mutex);

	/* Find the object to retrieve the handle and size */
	object = vm_find_object_by_address(aperture, address, 0);
	if (!object) {
		ret = -EINVAL;
		goto err;
	}

	if (!object->mapped_device_id_array ||
			object->mapped_device_id_array_size == 0) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return 0;
	}

	/* unmap from GPU */
	args.handle = object->handle;
	args.device_ids_array_ptr = (uint64_t)object->mapped_device_id_array;
	args.n_devices = object->mapped_device_id_array_size / sizeof(uint32_t);
	args.n_success = 0;
	ret = kmtIoctl(kfd_fd, AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU, &args);

	/* unmap from CPU while keeping the address space reserved */
	mmap(address, object->size, PROT_NONE,
	     MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE | MAP_FIXED,
	     -1, 0);

	remove_device_ids_from_mapped_array(object,
			(uint32_t *)args.device_ids_array_ptr,
			args.n_success * sizeof(uint32_t));

	if (object->mapped_node_id_array)
		free(object->mapped_node_id_array);
	object->mapped_node_id_array = NULL;

	if (ret)
		goto err;

	pthread_mutex_unlock(&aperture->fmm_mutex);

	/* free object in scratch backing aperture */
	return __fmm_release(object, aperture);

err:
	pthread_mutex_unlock(&aperture->fmm_mutex);
	return ret;
}

int fmm_unmap_from_gpu(void *address)
{
	manageable_aperture_t *aperture;
	vm_object_t *object;
	uint32_t i;
	int ret;

	/* Special handling for scratch memory */
	for (i = 0; i < gpu_mem_count; i++)
		if (gpu_mem[i].gpu_id != NON_VALID_GPU_ID &&
		    address >= gpu_mem[i].scratch_physical.base &&
		    address <= gpu_mem[i].scratch_physical.limit)
			return _fmm_unmap_from_gpu_scratch(gpu_mem[i].gpu_id,
							&gpu_mem[i].scratch_physical,
							address);

	object = vm_find_object(address, 0, &aperture);
	if (!object)
		/* On APUs GPU unmapping of system memory is a no-op */
		return is_dgpu ? -EINVAL : 0;
	/* Successful vm_find_object returns with the aperture locked */

	if (aperture == &cpuvm_aperture)
		/* On APUs GPU unmapping of system memory is a no-op */
		ret = 0;
	else
		ret = _fmm_unmap_from_gpu(aperture, address, NULL, 0, object);

	pthread_mutex_unlock(&aperture->fmm_mutex);

	return ret;
}

bool fmm_get_handle(void *address, uint64_t *handle)
{
	uint32_t i;
	manageable_aperture_t *aperture;
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
		if ((address >= svm.dgpu_aperture->base) &&
			(address <= svm.dgpu_aperture->limit)) {
			aperture = svm.dgpu_aperture;
		} else if ((address >= svm.dgpu_alt_aperture->base) &&
			(address <= svm.dgpu_alt_aperture->limit)) {
			aperture = svm.dgpu_alt_aperture;
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

static HSAKMT_STATUS fmm_register_user_memory(void *addr, HSAuint64 size,
				  vm_object_t **obj_ret, bool coarse_grain)
{
	manageable_aperture_t *aperture = svm.dgpu_aperture;
	HSAuint32 page_offset = (HSAuint64)addr & (PAGE_SIZE-1);
	HSAuint64 aligned_addr = (HSAuint64)addr - page_offset;
	HSAuint64 aligned_size = PAGE_ALIGN_UP(page_offset + size);
	void *svm_addr;
	HSAuint32 gpu_id;
	vm_object_t *obj, *exist_obj;

	/* Find first GPU for creating the userptr BO */
	if (!g_first_gpu_mem)
		return HSAKMT_STATUS_ERROR;

	gpu_id = g_first_gpu_mem->gpu_id;

	/* Optionally check that the CPU mapping is valid */
	if (svm.check_userptr)
		fmm_check_user_memory(addr, size);

	/* Allocate BO, userptr address is passed in mmap_offset */
	svm_addr = __fmm_allocate_device(gpu_id, NULL, aligned_size, aperture,
			 &aligned_addr, KFD_IOC_ALLOC_MEM_FLAGS_USERPTR |
			 KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE |
			 KFD_IOC_ALLOC_MEM_FLAGS_EXECUTABLE |
			 (coarse_grain ? 0 : KFD_IOC_ALLOC_MEM_FLAGS_COHERENT), &obj);
	if (!svm_addr)
		return HSAKMT_STATUS_ERROR;

	if (!obj)
		return HSAKMT_STATUS_ERROR;

	pthread_mutex_lock(&aperture->fmm_mutex);

	/* catch the race condition where some other thread added the userptr
	 * object already after the vm_find_object.
	 */
	exist_obj = vm_find_object_by_userptr(aperture, addr, size);
	if (exist_obj) {
		++exist_obj->registration_count;
	} else {
		obj->userptr = addr;
		gpuid_to_nodeid(gpu_id, &obj->node_id);
		obj->userptr_size = size;
		obj->registration_count = 1;
		obj->user_node.key = rbtree_key((unsigned long)addr, size);
		rbtree_insert(&aperture->user_tree, &obj->user_node);
	}
	pthread_mutex_unlock(&aperture->fmm_mutex);

	if (exist_obj)
		__fmm_release(obj, aperture);

	if (obj_ret)
		*obj_ret = exist_obj ? exist_obj : obj;
	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS fmm_register_memory(void *address, uint64_t size_in_bytes,
				  uint32_t *gpu_id_array,
				  uint32_t gpu_id_array_size,
				  bool coarse_grain)
{
	manageable_aperture_t *aperture = NULL;
	vm_object_t *object = NULL;
	HSAKMT_STATUS ret;

	if (gpu_id_array_size > 0 && !gpu_id_array)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	object = vm_find_object(address, size_in_bytes, &aperture);
	if (!object) {
		if (!is_dgpu)
			/* System memory registration on APUs is a no-op */
			return HSAKMT_STATUS_SUCCESS;

		/* Register a new user ptr */
		ret = fmm_register_user_memory(address, size_in_bytes, &object, coarse_grain);
		if (ret != HSAKMT_STATUS_SUCCESS)
			return ret;
		if (gpu_id_array_size == 0)
			return HSAKMT_STATUS_SUCCESS;
		aperture = svm.dgpu_aperture;
		pthread_mutex_lock(&aperture->fmm_mutex);
		/* fall through for registered device ID array setup */
	} else if (object->userptr) {
		/* Update an existing userptr */
		++object->registration_count;
	}
	/* Successful vm_find_object returns with aperture locked */

	if (object->registered_device_id_array_size > 0) {
		/* Multiple registration is allowed, but not changing nodes */
		if ((gpu_id_array_size != object->registered_device_id_array_size)
			|| memcmp(object->registered_device_id_array,
					gpu_id_array, gpu_id_array_size)) {
			pr_err("Cannot change nodes in a registered addr.\n");
			pthread_mutex_unlock(&aperture->fmm_mutex);
			return HSAKMT_STATUS_MEMORY_ALREADY_REGISTERED;
		} else {
			/* Delete the new array, keep the existing one. */
			if (gpu_id_array)
				free(gpu_id_array);

			pthread_mutex_unlock(&aperture->fmm_mutex);
			return HSAKMT_STATUS_SUCCESS;
		}
	}

	if (gpu_id_array_size > 0) {
		object->registered_device_id_array = gpu_id_array;
		object->registered_device_id_array_size = gpu_id_array_size;
		/* Registration of object changed. Lifecycle of object->
		 * registered_node_id_array terminates here. Free old one
		 * and re-allocate on next query
		 */
		if (object->registered_node_id_array) {
			free(object->registered_node_id_array);
			object->registered_node_id_array = NULL;
		}
	}

	pthread_mutex_unlock(&aperture->fmm_mutex);
	return HSAKMT_STATUS_SUCCESS;
}

#define GRAPHICS_METADATA_DEFAULT_SIZE 64
HSAKMT_STATUS fmm_register_graphics_handle(HSAuint64 GraphicsResourceHandle,
					   HsaGraphicsResourceInfo *GraphicsResourceInfo,
					   uint32_t *gpu_id_array,
					   uint32_t gpu_id_array_size)
{
	struct kfd_ioctl_get_dmabuf_info_args infoArgs = {0};
	struct kfd_ioctl_import_dmabuf_args importArgs = {0};
	struct kfd_ioctl_free_memory_of_gpu_args freeArgs = {0};
	manageable_aperture_t *aperture;
	HsaMemFlags mflags;
	vm_object_t *obj;
	void *metadata;
	void *mem, *aperture_base;
	int32_t gpu_mem_id;
	int r;
	HSAKMT_STATUS status = HSAKMT_STATUS_ERROR;
	static const uint64_t IMAGE_ALIGN = 256*1024;

	if (gpu_id_array_size > 0 && !gpu_id_array)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	infoArgs.dmabuf_fd = GraphicsResourceHandle;
	infoArgs.metadata_size = GRAPHICS_METADATA_DEFAULT_SIZE;
	metadata = calloc(infoArgs.metadata_size, 1);
	if (!metadata)
		return HSAKMT_STATUS_NO_MEMORY;
	infoArgs.metadata_ptr = (uint64_t)metadata;
	r = kmtIoctl(kfd_fd, AMDKFD_IOC_GET_DMABUF_INFO, (void *)&infoArgs);
	if (r && infoArgs.metadata_size > GRAPHICS_METADATA_DEFAULT_SIZE) {
		/* Try again with bigger metadata */
		free(metadata);
		metadata = calloc(infoArgs.metadata_size, 1);
		if (!metadata)
			return HSAKMT_STATUS_NO_MEMORY;
		infoArgs.metadata_ptr = (uint64_t)metadata;
		r = kmtIoctl(kfd_fd, AMDKFD_IOC_GET_DMABUF_INFO, (void *)&infoArgs);
	}

	if (r)
		goto error_free_metadata;

	/* Choose aperture based on GPU and allocate virtual address */
	gpu_mem_id = gpu_mem_find_by_gpu_id(infoArgs.gpu_id);
	if (gpu_mem_id < 0)
		goto error_free_metadata;
	if (topology_is_svm_needed(gpu_mem[gpu_mem_id].EngineId)) {
		aperture = svm.dgpu_aperture;
		aperture_base = NULL;
	} else {
		aperture = &gpu_mem[gpu_mem_id].gpuvm_aperture;
		aperture_base = aperture->base;
	}
	if (!aperture_is_valid(aperture->base, aperture->limit))
		goto error_free_metadata;
	pthread_mutex_lock(&aperture->fmm_mutex);
	mem = aperture_allocate_area_aligned(aperture, NULL, infoArgs.size,
					     IMAGE_ALIGN);
	pthread_mutex_unlock(&aperture->fmm_mutex);
	if (!mem)
		goto error_free_metadata;

	/* Import DMA buffer */
	importArgs.va_addr = VOID_PTRS_SUB(mem, aperture_base);
	importArgs.gpu_id = infoArgs.gpu_id;
	importArgs.dmabuf_fd = GraphicsResourceHandle;
	r = kmtIoctl(kfd_fd, AMDKFD_IOC_IMPORT_DMABUF, (void *)&importArgs);
	if (r)
		goto error_release_aperture;

	pthread_mutex_lock(&aperture->fmm_mutex);
	mflags = fmm_translate_ioc_to_hsa_flags(infoArgs.flags);
	mflags.ui32.CoarseGrain = 1;
	obj = aperture_allocate_object(aperture, mem, importArgs.handle,
				       infoArgs.size, mflags);
	if (obj) {
		obj->metadata = metadata;
		obj->registered_device_id_array = gpu_id_array;
		obj->registered_device_id_array_size = gpu_id_array_size;
		gpuid_to_nodeid(infoArgs.gpu_id, &obj->node_id);
	}
	pthread_mutex_unlock(&aperture->fmm_mutex);
	if (!obj)
		goto error_release_buffer;

	GraphicsResourceInfo->MemoryAddress = mem;
	GraphicsResourceInfo->SizeInBytes = infoArgs.size;
	GraphicsResourceInfo->Metadata = (void *)(unsigned long)infoArgs.metadata_ptr;
	GraphicsResourceInfo->MetadataSizeInBytes = infoArgs.metadata_size;
	GraphicsResourceInfo->Reserved = 0;

	return HSAKMT_STATUS_SUCCESS;

error_release_buffer:
	freeArgs.handle = importArgs.handle;
	kmtIoctl(kfd_fd, AMDKFD_IOC_FREE_MEMORY_OF_GPU, &freeArgs);
error_release_aperture:
	aperture_release_area(aperture, mem, infoArgs.size);
error_free_metadata:
	free(metadata);

	return status;
}

HSAKMT_STATUS fmm_share_memory(void *MemoryAddress,
				HSAuint64 SizeInBytes,
				HsaSharedMemoryHandle *SharedMemoryHandle)
{
	int r = 0;
	HSAuint32 gpu_id = 0;
	vm_object_t *obj = NULL;
	manageable_aperture_t *aperture = NULL;
	struct kfd_ioctl_ipc_export_handle_args exportArgs = {0};
	HsaApertureInfo ApeInfo;
	HsaSharedMemoryStruct *SharedMemoryStruct =
		to_hsa_shared_memory_struct(SharedMemoryHandle);

	if (SizeInBytes >= (1ULL << ((sizeof(HSAuint32) * 8) + PAGE_SHIFT)))
		return HSAKMT_STATUS_INVALID_PARAMETER;

	aperture = fmm_find_aperture(MemoryAddress, &ApeInfo);
	if (!aperture)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	pthread_mutex_lock(&aperture->fmm_mutex);
	obj = vm_find_object_by_address(aperture, MemoryAddress, 0);
	pthread_mutex_unlock(&aperture->fmm_mutex);
	if (!obj)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	r = validate_nodeid(obj->node_id, &gpu_id);
	if (r != HSAKMT_STATUS_SUCCESS)
		return r;
	if (!gpu_id && is_dgpu) {
		/* Sharing non paged system memory. Use first GPU which was
		 * used during allocation. See fmm_allocate_host_gpu()
		 */
		if (!g_first_gpu_mem)
			return HSAKMT_STATUS_ERROR;

		gpu_id = g_first_gpu_mem->gpu_id;
	}
	exportArgs.handle = obj->handle;
	exportArgs.gpu_id = gpu_id;
	exportArgs.flags = obj->mflags.Value;

	r = kmtIoctl(kfd_fd, AMDKFD_IOC_IPC_EXPORT_HANDLE, (void *)&exportArgs);
	if (r)
		return HSAKMT_STATUS_ERROR;

	memcpy(SharedMemoryStruct->ShareHandle, exportArgs.share_handle,
			sizeof(SharedMemoryStruct->ShareHandle));
	SharedMemoryStruct->ApeInfo = ApeInfo;
	SharedMemoryStruct->SizeInPages = (HSAuint32) (SizeInBytes >> PAGE_SHIFT);
	SharedMemoryStruct->ExportGpuId = gpu_id;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS fmm_register_shared_memory(const HsaSharedMemoryHandle *SharedMemoryHandle,
						HSAuint64 *SizeInBytes,
						void **MemoryAddress,
						uint32_t *gpu_id_array,
						uint32_t gpu_id_array_size)
{
	int r = 0;
	HSAKMT_STATUS err = HSAKMT_STATUS_ERROR;
	vm_object_t *obj = NULL;
	void *reservedMem = NULL;
	manageable_aperture_t *aperture;
	struct kfd_ioctl_ipc_import_handle_args importArgs = {0};
	struct kfd_ioctl_free_memory_of_gpu_args freeArgs = {0};
	const HsaSharedMemoryStruct *SharedMemoryStruct =
		to_const_hsa_shared_memory_struct(SharedMemoryHandle);
	HSAuint64 SizeInPages = SharedMemoryStruct->SizeInPages;
	HsaMemFlags mflags;

	if (gpu_id_array_size > 0 && !gpu_id_array)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	memcpy(importArgs.share_handle, SharedMemoryStruct->ShareHandle,
			sizeof(importArgs.share_handle));
	importArgs.gpu_id = SharedMemoryStruct->ExportGpuId;

	aperture = fmm_get_aperture(SharedMemoryStruct->ApeInfo);

	pthread_mutex_lock(&aperture->fmm_mutex);
	reservedMem = aperture_allocate_area(aperture, NULL,
			(SizeInPages << PAGE_SHIFT));
	pthread_mutex_unlock(&aperture->fmm_mutex);
	if (!reservedMem) {
		err = HSAKMT_STATUS_NO_MEMORY;
		goto err_free_buffer;
	}

	importArgs.va_addr = (uint64_t)reservedMem;
	r = kmtIoctl(kfd_fd, AMDKFD_IOC_IPC_IMPORT_HANDLE, (void *)&importArgs);
	if (r) {
		err = HSAKMT_STATUS_ERROR;
		goto err_import;
	}

	pthread_mutex_lock(&aperture->fmm_mutex);
	mflags.Value = importArgs.flags;
	obj = aperture_allocate_object(aperture, reservedMem, importArgs.handle,
				       (SizeInPages << PAGE_SHIFT), mflags);
	if (!obj) {
		err = HSAKMT_STATUS_NO_MEMORY;
		goto err_free_mem;
	}
	pthread_mutex_unlock(&aperture->fmm_mutex);

	if (importArgs.mmap_offset) {
		int32_t gpu_mem_id = gpu_mem_find_by_gpu_id(importArgs.gpu_id);
		int map_fd;
		void *ret;

		if (gpu_mem_id < 0) {
			err = HSAKMT_STATUS_ERROR;
			goto err_free_obj;
		}
		obj->node_id = gpu_mem[gpu_mem_id].node_id;
		map_fd = gpu_mem[gpu_mem_id].drm_render_fd;
		ret = mmap(reservedMem, (SizeInPages << PAGE_SHIFT),
			   PROT_READ | PROT_WRITE,
			   MAP_SHARED | MAP_FIXED, map_fd, importArgs.mmap_offset);
		if (ret == MAP_FAILED) {
			err = HSAKMT_STATUS_ERROR;
			goto err_free_obj;
		}
		madvise(ret, (SizeInPages << PAGE_SHIFT), MADV_DONTFORK);
	}

	*MemoryAddress = reservedMem;
	*SizeInBytes = (SizeInPages << PAGE_SHIFT);

	if (gpu_id_array_size > 0) {
		obj->registered_device_id_array = gpu_id_array;
		obj->registered_device_id_array_size = gpu_id_array_size;
	}
	obj->is_imported_kfd_bo = true;

	return HSAKMT_STATUS_SUCCESS;
err_free_obj:
	pthread_mutex_lock(&aperture->fmm_mutex);
	vm_remove_object(aperture, obj);
err_free_mem:
	aperture_release_area(aperture, reservedMem, (SizeInPages << PAGE_SHIFT));
	pthread_mutex_unlock(&aperture->fmm_mutex);
err_free_buffer:
	freeArgs.handle = importArgs.handle;
	kmtIoctl(kfd_fd, AMDKFD_IOC_FREE_MEMORY_OF_GPU, &freeArgs);
err_import:
	return err;
}

HSAKMT_STATUS fmm_deregister_memory(void *address)
{
	manageable_aperture_t *aperture;
	vm_object_t *object;

	object = vm_find_object(address, 0, &aperture);
	if (!object)
		/* On APUs we assume it's a random system memory address
		 * where registration and dergistration is a no-op
		 */
		return is_dgpu ?
			HSAKMT_STATUS_MEMORY_NOT_REGISTERED :
			HSAKMT_STATUS_SUCCESS;
	/* Successful vm_find_object returns with aperture locked */

	if (aperture == &cpuvm_aperture) {
		/* API-allocated system memory on APUs, deregistration
		 * is a no-op
		 */
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return HSAKMT_STATUS_SUCCESS;
	}

	if (object->metadata || object->userptr || object->is_imported_kfd_bo) {
		/* An object with metadata is an imported graphics
		 * buffer. Deregistering imported graphics buffers or
		 * userptrs means releasing the BO.
		 */
		pthread_mutex_unlock(&aperture->fmm_mutex);
		__fmm_release(object, aperture);
		return HSAKMT_STATUS_SUCCESS;
	}

	if (!object->registered_device_id_array ||
		object->registered_device_id_array_size <= 0) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return HSAKMT_STATUS_MEMORY_NOT_REGISTERED;
	}

	if (object->registered_device_id_array) {
		free(object->registered_device_id_array);
		object->registered_device_id_array = NULL;
		object->registered_device_id_array_size = 0;
	}
	if (object->registered_node_id_array)
		free(object->registered_node_id_array);
	object->registered_node_id_array = NULL;
	object->registration_count = 0;

	pthread_mutex_unlock(&aperture->fmm_mutex);

	return HSAKMT_STATUS_SUCCESS;
}

/*
 * This function unmaps all nodes on current mapped nodes list that are not included on nodes_to_map
 * and maps nodes_to_map
 */

HSAKMT_STATUS fmm_map_to_gpu_nodes(void *address, uint64_t size,
		uint32_t *nodes_to_map, uint64_t num_of_nodes,
		uint64_t *gpuvm_address)
{
	manageable_aperture_t *aperture;
	vm_object_t *object;
	uint32_t i;
	uint32_t *registered_node_id_array, registered_node_id_array_size;
	HSAKMT_STATUS ret = HSAKMT_STATUS_ERROR;
	int retcode = 0;

	if (!num_of_nodes || !nodes_to_map || !address)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	object = vm_find_object(address, size, &aperture);
	if (!object)
		return HSAKMT_STATUS_ERROR;
	/* Successful vm_find_object returns with aperture locked */

	/* APU memory is not supported by this function */
	if (aperture == &cpuvm_aperture || !aperture->is_cpu_accessible) {
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return HSAKMT_STATUS_ERROR;
	}

	/* For userptr, we ignore the nodes array and map all registered nodes.
	 * This is to simply the implementation of allowing the same memory
	 * region to be registered multiple times.
	 */
	if (object->userptr) {
		retcode = _fmm_map_to_gpu_userptr(address, size,
					gpuvm_address, object);
		pthread_mutex_unlock(&aperture->fmm_mutex);
		return retcode ? HSAKMT_STATUS_ERROR : HSAKMT_STATUS_SUCCESS;
	}

	/* Verify that all nodes to map are registered already */
	registered_node_id_array = all_gpu_id_array;
	registered_node_id_array_size = all_gpu_id_array_size;
	if (object->registered_device_id_array_size > 0 &&
			object->registered_device_id_array) {
		registered_node_id_array = object->registered_device_id_array;
		registered_node_id_array_size = object->registered_device_id_array_size;
	}
	for (i = 0 ; i < num_of_nodes; i++) {
		if (!id_in_array(nodes_to_map[i], registered_node_id_array,
					registered_node_id_array_size)) {
			pthread_mutex_unlock(&aperture->fmm_mutex);
			return HSAKMT_STATUS_ERROR;
		}
	}

	/* Unmap buffer from all nodes that have this buffer mapped that are not included on nodes_to_map array */
	if (object->mapped_device_id_array_size > 0) {
		uint32_t temp_node_id_array[object->mapped_device_id_array_size];
		uint32_t temp_node_id_array_size = 0;

		for (i = 0 ; i < object->mapped_device_id_array_size / sizeof(uint32_t); i++) {
			if (!id_in_array(object->mapped_device_id_array[i],
					nodes_to_map,
					num_of_nodes*sizeof(uint32_t)))
				temp_node_id_array[temp_node_id_array_size++] =
					object->mapped_device_id_array[i];
		}
		temp_node_id_array_size *= sizeof(uint32_t);

		if (temp_node_id_array_size) {
			ret = _fmm_unmap_from_gpu(aperture, address,
					temp_node_id_array,
					temp_node_id_array_size,
					object);
			if (ret != HSAKMT_STATUS_SUCCESS) {
				pthread_mutex_unlock(&aperture->fmm_mutex);
				return ret;
			}
		}
	}

	/* Remove already mapped nodes from nodes_to_map
	 * to generate the final map list
	 */
	uint32_t map_node_id_array[num_of_nodes];
	uint32_t map_node_id_array_size = 0;

	for (i = 0; i < num_of_nodes; i++) {
		if (!id_in_array(nodes_to_map[i],
				object->mapped_device_id_array,
				object->mapped_device_id_array_size))
			map_node_id_array[map_node_id_array_size++] =
				nodes_to_map[i];
	}

	if (map_node_id_array_size)
		retcode = _fmm_map_to_gpu(aperture, address, size, object,
				map_node_id_array,
				map_node_id_array_size * sizeof(uint32_t));

	pthread_mutex_unlock(&aperture->fmm_mutex);

	if (retcode != 0)
		return HSAKMT_STATUS_ERROR;

	return 0;
}

HSAKMT_STATUS fmm_get_mem_info(const void *address, HsaPointerInfo *info)
{
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;
	uint32_t i;
	manageable_aperture_t *aperture;
	vm_object_t *vm_obj;

	memset(info, 0, sizeof(HsaPointerInfo));

	vm_obj = vm_find_object(address, UINT64_MAX, &aperture);
	if (!vm_obj) {
		info->Type = HSA_POINTER_UNKNOWN;
		return HSAKMT_STATUS_ERROR;
	}
	/* Successful vm_find_object returns with the aperture locked */

	if (vm_obj->is_imported_kfd_bo)
		info->Type = HSA_POINTER_REGISTERED_SHARED;
	else if (vm_obj->metadata)
		info->Type = HSA_POINTER_REGISTERED_GRAPHICS;
	else if (vm_obj->userptr)
		info->Type = HSA_POINTER_REGISTERED_USER;
	else
		info->Type = HSA_POINTER_ALLOCATED;

	info->Node = vm_obj->node_id;
	info->GPUAddress = (HSAuint64)vm_obj->start;
	info->SizeInBytes = vm_obj->size;
	/* registered nodes */
	info->NRegisteredNodes =
		vm_obj->registered_device_id_array_size / sizeof(uint32_t);
	if (info->NRegisteredNodes && !vm_obj->registered_node_id_array) {
		vm_obj->registered_node_id_array = (uint32_t *)
			(uint32_t *)malloc(vm_obj->registered_device_id_array_size);
		/* vm_obj->registered_node_id_array allocated here will be
		 * freed whenever the registration is changed (deregistration or
		 * register to new nodes) or the memory being freed
		 */
		for (i = 0; i < info->NRegisteredNodes; i++)
			gpuid_to_nodeid(vm_obj->registered_device_id_array[i],
				&vm_obj->registered_node_id_array[i]);
	}
	info->RegisteredNodes = vm_obj->registered_node_id_array;
	/* mapped nodes */
	info->NMappedNodes =
		vm_obj->mapped_device_id_array_size / sizeof(uint32_t);
	if (info->NMappedNodes && !vm_obj->mapped_node_id_array) {
		vm_obj->mapped_node_id_array =
			(uint32_t *)malloc(vm_obj->mapped_device_id_array_size);
		/* vm_obj->mapped_node_id_array allocated here will be
		 * freed whenever the mapping is changed (unmapped or map
		 * to new nodes) or memory being freed
		 */
		for (i = 0; i < info->NMappedNodes; i++)
			gpuid_to_nodeid(vm_obj->mapped_device_id_array[i],
				&vm_obj->mapped_node_id_array[i]);
	}
	info->MappedNodes = vm_obj->mapped_node_id_array;
	info->UserData = vm_obj->user_data;

	info->MemFlags = vm_obj->mflags;

	if (info->Type == HSA_POINTER_REGISTERED_USER) {
		info->CPUAddress = vm_obj->userptr;
		info->SizeInBytes = vm_obj->userptr_size;
		info->GPUAddress += ((HSAuint64)info->CPUAddress & (PAGE_SIZE - 1));
	} else if (info->Type == HSA_POINTER_ALLOCATED) {
		info->CPUAddress = vm_obj->start;
	}

	pthread_mutex_unlock(&aperture->fmm_mutex);
	return ret;
}

HSAKMT_STATUS fmm_set_mem_user_data(const void *mem, void *usr_data)
{
	manageable_aperture_t *aperture;
	vm_object_t *vm_obj;

	vm_obj = vm_find_object(mem, 0, &aperture);
	if (!vm_obj)
		return HSAKMT_STATUS_ERROR;

	vm_obj->user_data = usr_data;

	pthread_mutex_unlock(&aperture->fmm_mutex);
	return HSAKMT_STATUS_SUCCESS;
}

static void fmm_clear_aperture(manageable_aperture_t *app)
{
	rbtree_node_t *n;

	pthread_mutex_init(&app->fmm_mutex, NULL);

	while ((n = rbtree_node_any(&app->tree, MID)))
		vm_remove_object(app, vm_object_entry(n, 0));

	while (app->vm_ranges)
		vm_remove_area(app, app->vm_ranges);

}

/* This is a special funcion that should be called only from the child process
 * after a fork(). This will clear all vm_objects and mmaps duplicated from
 * the parent.
 */
void fmm_clear_all_mem(void)
{
	uint32_t i;
	void *map_addr;

	/* Close render node FDs. The child process needs to open new ones */
	for (i = 0; i <= DRM_LAST_RENDER_NODE - DRM_FIRST_RENDER_NODE; i++)
		if (drm_render_fds[i]) {
			close(drm_render_fds[i]);
			drm_render_fds[i] = 0;
		}

	fmm_clear_aperture(&cpuvm_aperture);
	fmm_clear_aperture(&svm.apertures[SVM_DEFAULT]);
	fmm_clear_aperture(&svm.apertures[SVM_COHERENT]);

	if (dgpu_shared_aperture_limit) {
		/* Use the same dgpu range as the parent. If failed, then set
		 * is_dgpu_mem_init to false. Later on dgpu_mem_init will try
		 * to get a new range
		 */
		map_addr = mmap(dgpu_shared_aperture_base, (HSAuint64)(dgpu_shared_aperture_limit)-
			(HSAuint64)(dgpu_shared_aperture_base) + 1, PROT_NONE,
			MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE | MAP_FIXED, -1, 0);

		if (map_addr == MAP_FAILED) {
			munmap(dgpu_shared_aperture_base,
				   (HSAuint64)(dgpu_shared_aperture_limit) -
				   (HSAuint64)(dgpu_shared_aperture_base) + 1);

			dgpu_shared_aperture_base = NULL;
			dgpu_shared_aperture_limit = NULL;
		}
	}

	if (all_gpu_id_array)
		free(all_gpu_id_array);

	all_gpu_id_array_size = 0;
	all_gpu_id_array = NULL;

	/* Nothing is initialized. */
	if (!gpu_mem)
		return;

	for (i = 0; i < gpu_mem_count; i++) {
		fmm_clear_aperture(&gpu_mem[i].gpuvm_aperture);
		fmm_clear_aperture(&gpu_mem[i].scratch_physical);
	}

	gpu_mem_count = 0;
	free(gpu_mem);
	gpu_mem = NULL;
}
