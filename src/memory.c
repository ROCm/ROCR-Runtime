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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "fmm.h"

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetMemoryPolicy(
	HSAuint32	Node,
	HSAuint32	DefaultPolicy,
	HSAuint32	AlternatePolicy,
	void		*MemoryAddressAlternate,
	HSAuint64	MemorySizeInBytes
)
{
	struct kfd_ioctl_set_memory_policy_args args;
	HSAKMT_STATUS result;
	uint32_t gpu_id;

	CHECK_KFD_OPEN();

	if (is_dgpu)
		/* This is a legacy API useful on Kaveri only. On dGPU
		 * the alternate aperture is setup and used
		 * automatically for coherent allocations. Don't let
		 * app override it. */
		return HSAKMT_STATUS_NOT_IMPLEMENTED;

	result = validate_nodeid(Node, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	/*
	 * We accept any legal policy and alternate address location.
	 * You get CC everywhere anyway.
	 */
	if ((DefaultPolicy != HSA_CACHING_CACHED &&
		DefaultPolicy != HSA_CACHING_NONCACHED) ||
			(AlternatePolicy != HSA_CACHING_CACHED &&
			AlternatePolicy != HSA_CACHING_NONCACHED))
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_PAGE_MULTIPLE(MemoryAddressAlternate);
	CHECK_PAGE_MULTIPLE(MemorySizeInBytes);

	memset(&args, 0, sizeof(args));

	args.gpu_id = gpu_id;
	args.default_policy = (DefaultPolicy == HSA_CACHING_CACHED) ?
					KFD_IOC_CACHE_POLICY_COHERENT :
					KFD_IOC_CACHE_POLICY_NONCOHERENT;

	args.alternate_policy = (AlternatePolicy == HSA_CACHING_CACHED) ?
					KFD_IOC_CACHE_POLICY_COHERENT :
					KFD_IOC_CACHE_POLICY_NONCOHERENT;

	args.alternate_aperture_base = (uintptr_t) MemoryAddressAlternate;
	args.alternate_aperture_size = MemorySizeInBytes;

	int err = kmtIoctl(kfd_fd, AMDKFD_IOC_SET_MEMORY_POLICY, &args);

	return (err == -1) ? HSAKMT_STATUS_ERROR : HSAKMT_STATUS_SUCCESS;
}

HSAuint32 PageSizeFromFlags(unsigned int pageSizeFlags)
{
	switch (pageSizeFlags) {
	case HSA_PAGE_SIZE_4KB: return 4*1024;
	case HSA_PAGE_SIZE_64KB: return 64*1024;
	case HSA_PAGE_SIZE_2MB: return 2*1024*1024;
	case HSA_PAGE_SIZE_1GB: return 1024*1024*1024;
	default:
		assert(false);
		return 4*1024;
	}
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtAllocMemory(
	HSAuint32	PreferredNode,	/* IN */
	HSAuint64	SizeInBytes,	/* IN  (multiple of page size) */
	HsaMemFlags	MemFlags,	/* IN */
	void		**MemoryAddress	/* OUT (page-aligned) */
)
{
	HSAKMT_STATUS result;
	uint32_t gpu_id;
	HSAuint64 page_size;

	CHECK_KFD_OPEN();

	result = validate_nodeid(PreferredNode, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	page_size = PageSizeFromFlags(MemFlags.ui32.PageSize);

	if ((!MemoryAddress) || (!SizeInBytes) ||
	    (SizeInBytes & (page_size-1))) {
		return HSAKMT_STATUS_INVALID_PARAMETER;
	}

	if (gpu_id == 0 && !MemFlags.ui32.Scratch) {
		*MemoryAddress = fmm_allocate_host(PreferredNode, SizeInBytes,
						MemFlags);

		if (*MemoryAddress == NULL)
			return HSAKMT_STATUS_ERROR;

		return HSAKMT_STATUS_SUCCESS;
	}

	if (gpu_id && MemFlags.ui32.NonPaged && !MemFlags.ui32.Scratch) {
		*MemoryAddress = fmm_allocate_device(gpu_id, SizeInBytes, MemFlags);

		if (*MemoryAddress == NULL)
			return HSAKMT_STATUS_NO_MEMORY;

		return HSAKMT_STATUS_SUCCESS;
	}
	if (MemFlags.ui32.Scratch ) {
		*MemoryAddress = fmm_allocate_scratch(gpu_id, SizeInBytes);

		if (*MemoryAddress == NULL)
			return HSAKMT_STATUS_NO_MEMORY;

		return HSAKMT_STATUS_SUCCESS;
	}

	/* Backwards compatibility hack: Allocate system memory if app
	 * asks for paged memory from a GPU node. */
	if (gpu_id && !MemFlags.ui32.NonPaged && !MemFlags.ui32.Scratch) {
		*MemoryAddress = fmm_allocate_host(PreferredNode, SizeInBytes,
						MemFlags);

		if (*MemoryAddress == NULL)
			return HSAKMT_STATUS_ERROR;

		return HSAKMT_STATUS_SUCCESS;
	}

	return HSAKMT_STATUS_INVALID_PARAMETER;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtFreeMemory(
	void		*MemoryAddress,	/* IN (page-aligned) */
	HSAuint64	SizeInBytes	/* IN */
)
{
	CHECK_KFD_OPEN();

	if (MemoryAddress == NULL) {
		fprintf(stderr, "FIXME: freeing NULL pointer\n");
		return HSAKMT_STATUS_ERROR;
	}

	fmm_release(MemoryAddress);
	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterMemory(
	void		*MemoryAddress,		/* IN (cache-aligned) */
	HSAuint64	MemorySizeInBytes	/* IN (cache-aligned) */
)
{
	CHECK_KFD_OPEN();

	if (!is_dgpu)
		/* TODO: support mixed APU and dGPU configurations */
		return HSAKMT_STATUS_SUCCESS;

	return fmm_register_memory(MemoryAddress, MemorySizeInBytes,
                                   NULL, 0);
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterMemoryToNodes(
	void		*MemoryAddress,		/* IN (cache-aligned) */
	HSAuint64	MemorySizeInBytes,	/* IN (cache-aligned) */
	HSAuint64	NumberOfNodes,		/* IN */
	HSAuint32*	NodeArray		/* IN */
)
{
	CHECK_KFD_OPEN();
	uint32_t *gpu_id_array;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	if (!is_dgpu)
		/* TODO: support mixed APU and dGPU configurations */
		return HSAKMT_STATUS_NOT_SUPPORTED;

	ret = validate_nodeid_array(&gpu_id_array,
			NumberOfNodes, NodeArray);

	if (ret == HSAKMT_STATUS_SUCCESS) {
		ret = fmm_register_memory(MemoryAddress, MemorySizeInBytes,
					  gpu_id_array,
					  NumberOfNodes*sizeof(uint32_t));
		if (ret != HSAKMT_STATUS_SUCCESS)
			free(gpu_id_array);
	}

	return ret;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterGraphicsHandleToNodes(
	HSAuint64	GraphicsResourceHandle,	       /* IN */
	HsaGraphicsResourceInfo *GraphicsResourceInfo, /* OUT */
	HSAuint64	NumberOfNodes,		       /* IN */
	HSAuint32*	NodeArray		       /* IN */
)
{
	CHECK_KFD_OPEN();
	uint32_t *gpu_id_array;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	ret = validate_nodeid_array(&gpu_id_array,
			NumberOfNodes, NodeArray);

	if (ret == HSAKMT_STATUS_SUCCESS) {
		ret = fmm_register_graphics_handle(
			GraphicsResourceHandle, GraphicsResourceInfo,
			gpu_id_array,NumberOfNodes*sizeof(uint32_t));
		if (ret != HSAKMT_STATUS_SUCCESS)
			free(gpu_id_array);
	}

	return ret;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtShareMemory(
	void                  *MemoryAddress,     /* IN */
	HSAuint64             SizeInBytes,        /* IN */
	HsaSharedMemoryHandle *SharedMemoryHandle /* OUT */
)
{
	CHECK_KFD_OPEN();

	if (!SharedMemoryHandle)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	return fmm_share_memory(MemoryAddress, SizeInBytes, SharedMemoryHandle);
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterSharedHandle(
	const HsaSharedMemoryHandle *SharedMemoryHandle, /* IN */
	void                        **MemoryAddress,     /* OUT */
	HSAuint64                   *SizeInBytes         /* OUT */
)
{
	CHECK_KFD_OPEN();

	return hsaKmtRegisterSharedHandleToNodes(SharedMemoryHandle,
						 MemoryAddress,
						 SizeInBytes,
						 0,
						 NULL);
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtRegisterSharedHandleToNodes(
	const HsaSharedMemoryHandle *SharedMemoryHandle, /* IN */
	void                        **MemoryAddress,     /* OUT */
	HSAuint64                   *SizeInBytes,        /* OUT */
	HSAuint64                   NumberOfNodes,       /* OUT */
	HSAuint32*                  NodeArray            /* OUT */
)
{
	CHECK_KFD_OPEN();

	uint32_t *gpu_id_array = NULL;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	if (!SharedMemoryHandle)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	if (NodeArray) {
		ret = validate_nodeid_array(&gpu_id_array, NumberOfNodes, NodeArray);
		if (ret != HSAKMT_STATUS_SUCCESS)
			goto error;
	}

	ret = fmm_register_shared_memory(SharedMemoryHandle,
					 SizeInBytes,
					 MemoryAddress,
					 gpu_id_array,
					 NumberOfNodes*sizeof(uint32_t));
	if (ret != HSAKMT_STATUS_SUCCESS)
		goto error;

	return ret;

error:
	if (gpu_id_array)
		free(gpu_id_array);
	return ret;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtDeregisterMemory(
	void		*MemoryAddress		/* IN */
)
{
	CHECK_KFD_OPEN();

	return fmm_deregister_memory(MemoryAddress);
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtMapMemoryToGPU(
	void		*MemoryAddress,		/* IN (page-aligned) */
	HSAuint64	MemorySizeInBytes,	/* IN (page-aligned) */
	HSAuint64	*AlternateVAGPU		/* OUT (page-aligned) */
)
{
	CHECK_KFD_OPEN();

	if (MemoryAddress == NULL) {
		fprintf(stderr, "FIXME: mapping NULL pointer\n");
		return HSAKMT_STATUS_ERROR;
	}

	if (AlternateVAGPU)
		*AlternateVAGPU = 0;

	if (!fmm_map_to_gpu(MemoryAddress, MemorySizeInBytes, AlternateVAGPU))
		return HSAKMT_STATUS_SUCCESS;
	else
		return HSAKMT_STATUS_ERROR;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtMapMemoryToGPUNodes(
    void*           MemoryAddress,         //IN (page-aligned)
    HSAuint64       MemorySizeInBytes,     //IN (page-aligned)
    HSAuint64*      AlternateVAGPU,        //OUT (page-aligned)
    HsaMemMapFlags  MemMapFlags,           //IN
    HSAuint64       NumberOfNodes,         //IN
    HSAuint32*      NodeArray              //IN
)
{
	uint32_t *gpu_id_array;
	HSAKMT_STATUS ret;

	if (MemoryAddress == NULL) {
		fprintf(stderr, "FIXME: mapping NULL pointer\n");
		return HSAKMT_STATUS_ERROR;
	}

	if (!is_dgpu && NumberOfNodes == 1)
		return hsaKmtMapMemoryToGPU(MemoryAddress,
				MemorySizeInBytes,
				AlternateVAGPU);

	ret = validate_nodeid_array(&gpu_id_array,
				NumberOfNodes, NodeArray);
	if (ret != HSAKMT_STATUS_SUCCESS)
		return ret;

	return fmm_map_to_gpu_nodes(MemoryAddress, MemorySizeInBytes,
			gpu_id_array, NumberOfNodes * sizeof(uint32_t), AlternateVAGPU);
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtUnmapMemoryToGPU(
	void		*MemoryAddress	/* IN (page-aligned) */
)
{
	CHECK_KFD_OPEN();

	if (MemoryAddress == NULL) {
		/* Workaround for runtime bug */
		fprintf(stderr, "FIXME: Unmapping NULL pointer\n");
		return HSAKMT_STATUS_SUCCESS;
	}

	if (!fmm_unmap_from_gpu(MemoryAddress))
		return HSAKMT_STATUS_SUCCESS;
	else
		return HSAKMT_STATUS_ERROR;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtMapGraphicHandle(
	HSAuint32	NodeId,			/* IN */
	HSAuint64	GraphicDeviceHandle,	/* IN */
	HSAuint64	GraphicResourceHandle,	/* IN */
	HSAuint64	GraphicResourceOffset,	/* IN */
	HSAuint64	GraphicResourceSize,	/* IN */
	HSAuint64	*FlatMemoryAddress	/* OUT */
)
{

	CHECK_KFD_OPEN();
	HSAKMT_STATUS result;
	uint32_t gpu_id;
	void *graphic_handle;

	if (GraphicResourceOffset != 0)
		return HSAKMT_STATUS_NOT_IMPLEMENTED;

	result = validate_nodeid(NodeId, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	graphic_handle = fmm_open_graphic_handle(gpu_id,
						GraphicDeviceHandle,
						GraphicResourceHandle,
						GraphicResourceSize);

	*FlatMemoryAddress = PORT_VPTR_TO_UINT64(graphic_handle);

	if (*FlatMemoryAddress)
		return HSAKMT_STATUS_SUCCESS;
	else
		return HSAKMT_STATUS_NO_MEMORY;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtUnmapGraphicHandle(
	HSAuint32	NodeId,			/* IN */
	HSAuint64	FlatMemoryAddress,	/* IN */
	HSAuint64	SizeInBytes		/* IN */
)
{

	return hsaKmtUnmapMemoryToGPU(PORT_UINT64_TO_VPTR(FlatMemoryAddress));
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetTileConfig(
	HSAuint32		NodeId,	/* IN */
	HsaGpuTileConfig	*config	/* IN & OUT */
)
{
	struct kfd_ioctl_get_tile_config_args args;
	uint32_t gpu_id;
	HSAKMT_STATUS result;

	result = validate_nodeid(NodeId, &gpu_id);
	if (result != HSAKMT_STATUS_SUCCESS)
		return result;

	args.gpu_id = gpu_id;
	args.tile_config_ptr = (uint64_t)config->TileConfig;
	args.macro_tile_config_ptr = (uint64_t)config->MacroTileConfig;
	args.num_tile_configs = config->NumTileConfigs;
	args.num_macro_tile_configs = config->NumMacroTileConfigs;

	if (kmtIoctl(kfd_fd, AMDKFD_IOC_GET_TILE_CONFIG, &args) != 0) {
		return HSAKMT_STATUS_ERROR;
	}

	config->NumTileConfigs = args.num_tile_configs;
	config->NumMacroTileConfigs = args.num_macro_tile_configs;

	config->GbAddrConfig = args.gb_addr_config;

	config->NumBanks = args.num_banks;
	config->NumRanks = args.num_ranks;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtQueryPointerInfo(
	const void	*Pointer,	/* IN */
	HsaPointerInfo	*PointerInfo	/* OUT */
)
{
	if (!PointerInfo)
		return HSAKMT_STATUS_INVALID_PARAMETER;
	return fmm_get_mem_info(Pointer, PointerInfo);
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtSetMemoryUserData(
	const void	*Pointer,	/* IN */
	void		*UserData	/* IN */
)
{
	return fmm_set_mem_user_data(Pointer, UserData);
}
