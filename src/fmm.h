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

#ifndef FMM_H_
#define FMM_H_

#include "hsakmttypes.h"
#include <stddef.h>

typedef enum {
	FMM_FIRST_APERTURE_TYPE = 0,
	FMM_GPUVM = FMM_FIRST_APERTURE_TYPE,
	FMM_LDS,
	FMM_SCRATCH,
	FMM_SVM,
	FMM_LAST_APERTURE_TYPE
} aperture_type_e;

typedef struct {
	aperture_type_e app_type;
	uint64_t size;
	void* start_address;
} aperture_properties_t;

HSAKMT_STATUS fmm_init_process_apertures(unsigned int NumNodes);
void fmm_destroy_process_apertures(void);

/*
 * Memory interface
 */
void* fmm_allocate_scratch(uint32_t gpu_id, uint64_t MemorySizeInBytes);
void* fmm_allocate_device(uint32_t gpu_id, uint64_t MemorySizeInBytes, HsaMemFlags flags);
void* fmm_allocate_doorbell(uint32_t gpu_id, uint64_t MemorySizeInBytes, uint64_t doorbell_offset);
void* fmm_allocate_host(uint32_t node_id, uint64_t MemorySizeInBytes,
			HsaMemFlags flags);
void* fmm_open_graphic_handle(uint32_t gpu_id,
        int32_t graphic_device_handle,
        uint32_t graphic_handle,
        uint64_t MemorySizeInBytes);
void fmm_print(uint32_t node);
bool fmm_is_inside_some_aperture(void* address);
void fmm_release(void* address);
int fmm_map_to_gpu(void *address, uint64_t size, uint64_t *gpuvm_address);
int fmm_unmap_from_gpu(void *address);
bool fmm_get_handle(void *address, uint64_t *handle);
HSAKMT_STATUS fmm_get_mem_info(const void *address, HsaPointerInfo *info);
HSAKMT_STATUS fmm_set_mem_user_data(const void *mem, void *usr_data);

/* Topology interface*/
HSAKMT_STATUS fmm_node_added(HSAuint32 gpu_id);
HSAKMT_STATUS fmm_node_removed(HSAuint32 gpu_id);
HSAKMT_STATUS fmm_get_aperture_base_and_limit(aperture_type_e aperture_type, HSAuint32 gpu_id,
		HSAuint64 *aperture_base, HSAuint64 *aperture_limit);

HSAKMT_STATUS fmm_register_memory(void *address, uint64_t size_in_bytes,
                                  uint32_t *gpu_id_array,
                                  uint32_t gpu_id_array_size);
HSAKMT_STATUS fmm_register_graphics_handle(HSAuint64 GraphicsResourceHandle,
					   HsaGraphicsResourceInfo *GraphicsResourceInfo,
					   uint32_t *gpu_id_array,
					   uint32_t gpu_id_array_size);
HSAKMT_STATUS fmm_deregister_memory(void *address);
HSAKMT_STATUS fmm_share_memory(void* MemoryAddress,
			       HSAuint64 SizeInBytes,
			       HsaSharedMemoryHandle *SharedMemoryHandle);
HSAKMT_STATUS fmm_register_shared_memory(const HsaSharedMemoryHandle *SharedMemoryHandle,
					 HSAuint64 *SizeInBytes,
					 void **MemoryAddress,
					 uint32_t *gpu_id_array,
					 uint32_t gpu_id_array_size);
HSAKMT_STATUS fmm_map_to_gpu_nodes(void *address, uint64_t size,
		uint32_t *nodes_to_map, uint32_t nodes_to_map_size, uint64_t *gpuvm_address);
#endif /* FMM_H_ */
