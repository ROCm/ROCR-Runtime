/*
 * Copyright 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef KFD_IOCTL_H_INCLUDED
#define KFD_IOCTL_H_INCLUDED

#include <linux/types.h>
#include <linux/ioctl.h>

#define KFD_IOCTL_MAJOR_VERSION 1
#define KFD_IOCTL_MINOR_VERSION 2

struct kfd_ioctl_get_version_args {
	uint32_t major_version;	/* from KFD */
	uint32_t minor_version;	/* from KFD */
};

/* For kfd_ioctl_create_queue_args.queue_type. */
#define KFD_IOC_QUEUE_TYPE_COMPUTE	0
#define KFD_IOC_QUEUE_TYPE_SDMA		1
#define KFD_IOC_QUEUE_TYPE_COMPUTE_AQL	2

#define KFD_MAX_QUEUE_PERCENTAGE	100
#define KFD_MAX_QUEUE_PRIORITY		15

struct kfd_ioctl_create_queue_args {
	uint64_t ring_base_address;	/* to KFD */
	uint64_t write_pointer_address;	/* from KFD */
	uint64_t read_pointer_address;	/* from KFD */
	uint64_t doorbell_offset;	/* from KFD */

	uint32_t ring_size;		/* to KFD */
	uint32_t gpu_id;		/* to KFD */
	uint32_t queue_type;		/* to KFD */
	uint32_t queue_percentage;	/* to KFD */
	uint32_t queue_priority;	/* to KFD */
	uint32_t queue_id;		/* from KFD */

	uint64_t eop_buffer_address;	/* to KFD */
	uint64_t eop_buffer_size;	/* to KFD */
	uint64_t ctx_save_restore_address; /* to KFD */
	uint32_t ctx_save_restore_size;	/* to KFD */
	uint32_t ctl_stack_size; 	/* to KFD */
};

struct kfd_ioctl_destroy_queue_args {
	uint32_t queue_id;		/* to KFD */
	uint32_t pad;
};

struct kfd_ioctl_update_queue_args {
	uint64_t ring_base_address;	/* to KFD */

	uint32_t queue_id;		/* to KFD */
	uint32_t ring_size;		/* to KFD */
	uint32_t queue_percentage;	/* to KFD */
	uint32_t queue_priority;	/* to KFD */
};

struct kfd_ioctl_set_cu_mask_args {
	uint32_t queue_id;		/* to KFD */
	uint32_t num_cu_mask;		/* to KFD */
	uint64_t cu_mask_ptr;		/* to KFD */
};

/* For kfd_ioctl_set_memory_policy_args.default_policy and alternate_policy */
#define KFD_IOC_CACHE_POLICY_COHERENT 0
#define KFD_IOC_CACHE_POLICY_NONCOHERENT 1

struct kfd_ioctl_set_memory_policy_args {
	uint64_t alternate_aperture_base;	/* to KFD */
	uint64_t alternate_aperture_size;	/* to KFD */

	uint32_t gpu_id;			/* to KFD */
	uint32_t default_policy;		/* to KFD */
	uint32_t alternate_policy;		/* to KFD */
	uint32_t pad;
};

struct kfd_ioctl_set_trap_handler_args {
	uint64_t tba_addr;
	uint64_t tma_addr;
	uint32_t gpu_id;			/* to KFD */
	uint32_t pad;
};

/*
 * All counters are monotonic. They are used for profiling of compute jobs.
 * The profiling is done by userspace.
 *
 * In case of GPU reset, the counter should not be affected.
 */

struct kfd_ioctl_get_clock_counters_args {
	uint64_t gpu_clock_counter;	/* from KFD */
	uint64_t cpu_clock_counter;	/* from KFD */
	uint64_t system_clock_counter;	/* from KFD */
	uint64_t system_clock_freq;	/* from KFD */

	uint32_t gpu_id;		/* to KFD */
	uint32_t pad;
};

#define NUM_OF_SUPPORTED_GPUS 7

struct kfd_process_device_apertures {
	uint64_t lds_base;		/* from KFD */
	uint64_t lds_limit;		/* from KFD */
	uint64_t scratch_base;		/* from KFD */
	uint64_t scratch_limit;		/* from KFD */
	uint64_t gpuvm_base;		/* from KFD */
	uint64_t gpuvm_limit;		/* from KFD */
	uint32_t gpu_id;		/* from KFD */
	uint32_t pad;
};

struct kfd_ioctl_get_process_apertures_args {
	struct kfd_process_device_apertures
			process_apertures[NUM_OF_SUPPORTED_GPUS];/* from KFD */

	/* from KFD, should be in the range [1 - NUM_OF_SUPPORTED_GPUS] */
	uint32_t num_of_nodes;
	uint32_t pad;
};

#define MAX_ALLOWED_NUM_POINTS    100
#define MAX_ALLOWED_AW_BUFF_SIZE 4096
#define MAX_ALLOWED_WAC_BUFF_SIZE  128

struct kfd_ioctl_dbg_register_args {
	uint32_t gpu_id;		/* to KFD */
	uint32_t pad;
};

struct kfd_ioctl_dbg_unregister_args {
	uint32_t gpu_id;		/* to KFD */
	uint32_t pad;
};

struct kfd_ioctl_dbg_address_watch_args {
	uint32_t gpu_id;		/* to KFD */
	uint32_t buf_size_in_bytes;	/*including gpu_id and buf_size */
	void	*content_ptr;	/* a pointer to the actual content */
};

struct kfd_ioctl_dbg_wave_control_args {
	uint32_t gpu_id;		/* to KFD */
	uint32_t buf_size_in_bytes;	/*including gpu_id and buf_size */
	void	*content_ptr;	/* a pointer to the actual content */
};

/* Matching HSA_EVENTTYPE */
#define KFD_IOC_EVENT_SIGNAL		0
#define KFD_IOC_EVENT_NODECHANGE	1
#define KFD_IOC_EVENT_DEVICESTATECHANGE	2
#define KFD_IOC_EVENT_HW_EXCEPTION	3
#define KFD_IOC_EVENT_SYSTEM_EVENT	4
#define KFD_IOC_EVENT_DEBUG_EVENT	5
#define KFD_IOC_EVENT_PROFILE_EVENT	6
#define KFD_IOC_EVENT_QUEUE_EVENT	7
#define KFD_IOC_EVENT_MEMORY		8

#define KFD_IOC_WAIT_RESULT_COMPLETE	0
#define KFD_IOC_WAIT_RESULT_TIMEOUT	1
#define KFD_IOC_WAIT_RESULT_FAIL	2

/*
 * The added 512 is because, currently, 8*(4096/256) signal events are
 * reserved for debugger events, and we want to provide at least 4K signal
 * events for EOP usage.
 * We add 512 to make the allocated size (KFD_SIGNAL_EVENT_LIMIT * 8) be
 * page aligned.
 */
#define KFD_SIGNAL_EVENT_LIMIT		(4096 + 512)

struct kfd_ioctl_create_event_args {
	uint64_t event_page_offset;	/* from KFD */
	uint32_t event_trigger_data;	/* from KFD - signal events only */
	uint32_t event_type;		/* to KFD */
	uint32_t auto_reset;		/* to KFD */
	uint32_t node_id;		/* to KFD - only valid for certain event types */
	uint32_t event_id;		/* from KFD */
	uint32_t event_slot_index;	/* from KFD */
};

struct kfd_ioctl_destroy_event_args {
	uint32_t event_id;		/* to KFD */
	uint32_t pad;
};

struct kfd_ioctl_set_event_args {
	uint32_t event_id;		/* to KFD */
	uint32_t pad;
};

struct kfd_ioctl_reset_event_args {
	uint32_t event_id;		/* to KFD */
	uint32_t pad;
};


struct kfd_memory_exception_failure {
	bool NotPresent;  /* Page not present or supervisor privilege */
	bool ReadOnly;  /* Write access to a read-only page */
	bool NoExecute;  /* Execute access to a page marked NX */
	bool pad;
};

/* memory exception data*/
struct kfd_hsa_memory_exception_data {
	uint64_t va;
	uint32_t gpu_id;
	struct kfd_memory_exception_failure failure;
};

/* Event data*/
struct kfd_event_data {
	union {
		struct kfd_hsa_memory_exception_data memory_exception_data;
	};				/* From KFD */
	uint64_t kfd_event_data_ext;	/* pointer to an extension structure
	 	 	 	 	   for future exception types */
	uint32_t event_id;		/* to KFD */
	uint32_t pad;
};

struct kfd_ioctl_wait_events_args {
	uint64_t events_ptr;		/* pointed to struct
					   kfd_event_data array, to KFD */
	uint32_t num_events;		/* to KFD */
	uint32_t wait_for_all;		/* to KFD */
	uint32_t timeout;		/* to KFD */
	uint32_t wait_result;		/* from KFD */
};

struct kfd_ioctl_alloc_memory_of_gpu_args {
	uint64_t va_addr;	/* to KFD */
	uint64_t size;		/* to KFD */
	uint64_t handle;	/* from KFD */
	uint32_t gpu_id;	/* to KFD */
	uint64_t mmap_offset;   /* from KFD */
};

struct kfd_ioctl_free_memory_of_gpu_args {
	uint64_t handle;	/* to KFD */
};

struct kfd_ioctl_map_memory_to_gpu_args {
	uint64_t handle;	/* to KFD */
};

struct kfd_ioctl_unmap_memory_from_gpu_args {
	uint64_t handle;	/* to KFD */
};

struct kfd_ioctl_open_graphic_handle_args {
	uint64_t va_addr;		/* to KFD */
	uint64_t handle;		/* from KFD */
	uint32_t gpu_id;		/* to KFD */
	int graphic_device_fd;		/* to KFD */
	uint32_t graphic_handle;	/* to KFD */
	uint32_t pad;
};

struct kfd_ioctl_set_process_dgpu_aperture_args {
	uint32_t node_id;
	uint64_t dgpu_base;
	uint64_t dgpu_limit;
};

/*
 * GPU Memory allocation flags
 */
#define KFD_IOC_ALLOC_MEM_FLAGS_DGPU_HOST 	(1 << 0)
#define KFD_IOC_ALLOC_MEM_FLAGS_DGPU_DEVICE	(1 << 1)
#define KFD_IOC_ALLOC_MEM_FLAGS_DGPU_SCRATCH	(1 << 2)
#define KFD_IOC_ALLOC_MEM_FLAGS_APU_DEVICE	(1 << 3)
#define KFD_IOC_ALLOC_MEM_FLAGS_APU_SCRATCH	(1 << 4)

struct kfd_ioctl_alloc_memory_of_gpu_new_args {
	uint64_t va_addr;	/* to KFD */
	uint64_t size;		/* to KFD */
	uint64_t handle;	/* from KFD */
	uint32_t gpu_id;	/* to KFD */
	uint64_t mmap_offset;   /* from KFD */
	uint32_t flags;
};

#define AMDKFD_IOCTL_BASE 'K'
#define AMDKFD_IO(nr)			_IO(AMDKFD_IOCTL_BASE, nr)
#define AMDKFD_IOR(nr, type)		_IOR(AMDKFD_IOCTL_BASE, nr, type)
#define AMDKFD_IOW(nr, type)		_IOW(AMDKFD_IOCTL_BASE, nr, type)
#define AMDKFD_IOWR(nr, type)		_IOWR(AMDKFD_IOCTL_BASE, nr, type)

#define AMDKFD_IOC_GET_VERSION			\
		AMDKFD_IOR(0x01, struct kfd_ioctl_get_version_args)

#define AMDKFD_IOC_CREATE_QUEUE			\
		AMDKFD_IOWR(0x02, struct kfd_ioctl_create_queue_args)

#define AMDKFD_IOC_DESTROY_QUEUE		\
		AMDKFD_IOWR(0x03, struct kfd_ioctl_destroy_queue_args)

#define AMDKFD_IOC_SET_MEMORY_POLICY		\
		AMDKFD_IOW(0x04, struct kfd_ioctl_set_memory_policy_args)

#define AMDKFD_IOC_GET_CLOCK_COUNTERS		\
		AMDKFD_IOWR(0x05, struct kfd_ioctl_get_clock_counters_args)

#define AMDKFD_IOC_GET_PROCESS_APERTURES	\
		AMDKFD_IOR(0x06, struct kfd_ioctl_get_process_apertures_args)

#define AMDKFD_IOC_UPDATE_QUEUE			\
		AMDKFD_IOW(0x07, struct kfd_ioctl_update_queue_args)

#define AMDKFD_IOC_CREATE_EVENT			\
		AMDKFD_IOWR(0x08, struct kfd_ioctl_create_event_args)

#define AMDKFD_IOC_DESTROY_EVENT		\
		AMDKFD_IOW(0x09, struct kfd_ioctl_destroy_event_args)

#define AMDKFD_IOC_SET_EVENT			\
		AMDKFD_IOW(0x0A, struct kfd_ioctl_set_event_args)

#define AMDKFD_IOC_RESET_EVENT			\
		AMDKFD_IOW(0x0B, struct kfd_ioctl_reset_event_args)

#define AMDKFD_IOC_WAIT_EVENTS			\
		AMDKFD_IOWR(0x0C, struct kfd_ioctl_wait_events_args)

#define AMDKFD_IOC_DBG_REGISTER			\
		AMDKFD_IOW(0x0D, struct kfd_ioctl_dbg_register_args)

#define AMDKFD_IOC_DBG_UNREGISTER		\
		AMDKFD_IOW(0x0E, struct kfd_ioctl_dbg_unregister_args)

#define AMDKFD_IOC_DBG_ADDRESS_WATCH		\
		AMDKFD_IOW(0x0F, struct kfd_ioctl_dbg_address_watch_args)

#define AMDKFD_IOC_DBG_WAVE_CONTROL		\
		AMDKFD_IOW(0x10, struct kfd_ioctl_dbg_wave_control_args)

#define AMDKFD_IOC_ALLOC_MEMORY_OF_GPU		\
		AMDKFD_IOWR(0x11, struct kfd_ioctl_alloc_memory_of_gpu_args)

#define AMDKFD_IOC_FREE_MEMORY_OF_GPU		\
		AMDKFD_IOWR(0x12, struct kfd_ioctl_free_memory_of_gpu_args)

#define AMDKFD_IOC_MAP_MEMORY_TO_GPU		\
		AMDKFD_IOWR(0x13, struct kfd_ioctl_map_memory_to_gpu_args)

#define AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU	\
		AMDKFD_IOWR(0x14, struct kfd_ioctl_unmap_memory_from_gpu_args)

#define AMDKFD_IOC_OPEN_GRAPHIC_HANDLE		\
		AMDKFD_IOWR(0x15, struct kfd_ioctl_open_graphic_handle_args)

#define AMDKFD_IOC_ALLOC_MEMORY_OF_SCRATCH	\
		AMDKFD_IOWR(0x16, struct kfd_ioctl_alloc_memory_of_gpu_args)

#define AMDKFD_IOC_SET_CU_MASK		\
		AMDKFD_IOW(0x17, struct kfd_ioctl_set_cu_mask_args)

#define AMDKFD_IOC_SET_PROCESS_DGPU_APERTURE   \
		AMDKFD_IOW(0x18, struct kfd_ioctl_set_process_dgpu_aperture_args)

#define AMDKFD_IOC_ALLOC_MEMORY_OF_GPU_NEW		\
		AMDKFD_IOWR(0x19, struct kfd_ioctl_alloc_memory_of_gpu_new_args)


#define AMDKFD_IOC_SET_TRAP_HANDLER		\
		AMDKFD_IOW(0x1a, struct kfd_ioctl_set_trap_handler_args)

#define AMDKFD_COMMAND_START		0x01
#define AMDKFD_COMMAND_END		0x1b

#endif
