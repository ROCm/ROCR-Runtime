// Copyright (C) 2023, Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#ifndef AMDAIR_IOCTL_H_
#define AMDAIR_IOCTL_H_

#define AMDAIR_IOCTL_MAJOR_VERSION 1
#define AMDAIR_IOCTL_MINOR_VERSION 0

struct amdair_get_version_args {
	uint32_t major_version; /* from driver */
	uint32_t minor_version; /* from driver */
};

enum amdair_queue_type {
	AMDAIR_QUEUE_DEVICE /* queue is in device memory */
};

struct amdair_create_queue_args {
	uint64_t doorbell_offset; /* out */
	uint64_t queue_offset; /* out */
	uint64_t queue_buf_offset; /* out */
	uint64_t dram_heap_vaddr; /* To AMDAIR driver */

	uint32_t ring_size_bytes; /* in: ring buffer size in bytes*/
	uint32_t device_id; /* in: which device/card consumes queue entries */
	uint32_t queue_type; /* in: see amdair_queue_type */
	uint32_t queue_id; /* out: globally unique queue id */
	uint32_t doorbell_id; /* out: doorbell ID within the process */
};

/**
 * struct amdair_destroy_queue_args - Destroy a queue and free its resources.
 *
 * @device_id: ID of the device on which the queue resides.
 *
 * @queue_id: The ID of queue being destroyed.
 *
 * @doorbell_id: The ID of the doorbell associated with the queue being
 *               destroyed.
 */
struct amdair_destroy_queue_args {
	uint32_t device_id; /* To AMDAIR driver */
	uint32_t queue_id; /* To AMDAIR driver */
	uint32_t doorbell_id; /* To AMDAIR driver */
};

/**
 * enum amdair_ioc_alloc_mem_flags - Allocation flags for heap type.
 *
 * @AMDAIR_IOC_ALLOC_MEM_HEAP_TYPE_BRAM: The memory will be allocated in BRAM.
 *
 * @AMDAIR_IOC_ALLOC_MEM_HEAP_TYPE_DRAM: The memory will be allocated in on-chip
 *                                       DRAM.
 */
enum amdair_ioc_alloc_mem_flags {
	AMDAIR_IOC_ALLOC_MEM_HEAP_TYPE_BRAM = (1 << 0),
	AMDAIR_IOC_ALLOC_MEM_HEAP_TYPE_DRAM = (1 << 1)
};

/**
 * struct amdair_alloc_device_memory_args - Allocate memory on the device.
 *
 * @handle: Unique buffer object handle that is used to refer to the buffer
 *          object for mapping, unmapping, and freeing.
 *
 * @device_id: The ID of the device on which the buffer object resides.
 *
 * @size: Size of the allocation in bytes.
 *
 * @flags: Memory type and other attributes. See amdair_ioc_alloc_mem_flags.
 */
struct amdair_alloc_device_memory_args {
	int handle; /* From AMDAIR driver */
	uint64_t mmap_offset; /* From AMDAIR driver */
	uint32_t device_id; /* To AMDAIR driver */
	uint64_t size; /* To AMDAIR driver */
	uint32_t flags; /* To AMDAIR driver */
};

/**
 * struct amdair_free_memory_device_memory_args - Free memory on the device.
 *
 * @handle: Unique buffer object handle that is used to free the buffer object.
 *
 * @device_id: The ID of the device on which the buffer object resides.
 */
struct amdair_free_device_memory_args {
	int handle; /* To AMDAIR driver */
	uint32_t device_id; /* To AMDAIR driver */
};

#define AMDAIR_COMMAND_START 0x1
#define AMDAIR_COMMAND_END 0x5

#define AMDAIR_IOCTL_BASE 'Y'
#define AMDAIR_IO(nr) _IO(AMDAIR_IOCTL_BASE, nr)
#define AMDAIR_IOR(nr, type) _IOR(AMDAIR_IOCTL_BASE, nr, type)
#define AMDAIR_IOW(nr, type) _IOW(AMDAIR_IOCTL_BASE, nr, type)
#define AMDAIR_IOWR(nr, type) _IOWR(AMDAIR_IOCTL_BASE, nr, type)

#define AMDAIR_IOC_GET_VERSION AMDAIR_IOR(0x01, struct amdair_get_version_args)

#define AMDAIR_IOC_CREATE_QUEUE \
	AMDAIR_IOWR(0x02, struct amdair_create_queue_args)

#define AMDAIR_IOC_DESTROY_QUEUE \
	AMDAIR_IOWR(0x03, struct amdair_destroy_queue_args)

#define AMDAIR_IOC_ALLOC_DEVICE_MEMORY \
	AMDAIR_IOWR(0x04, struct amdair_alloc_device_memory_args)

#define AMDAIR_IOC_FREE_DEVICE_MEMORY \
	AMDAIR_IOWR(0x05, struct amdair_free_device_memory_args)

#endif /* AMDAIR_IOCTL_H_ */
