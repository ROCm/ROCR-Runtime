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

#include <assert.h>
#include <stdio.h>
#include <dirent.h>
#include <malloc.h>
#include <string.h>

#include "libhsakmt.h"
#include "fmm.h"
#define PAGE_SIZE 4096
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define NUM_OF_HEAPS 2
/* SYSFS related */
#define KFD_SYSFS_PATH_GENERATION_ID "/sys/devices/virtual/kfd/kfd/topology/generation_id"
#define KFD_SYSFS_PATH_SYSTEM_PROPERTIES "/sys/devices/virtual/kfd/kfd/topology/system_properties"
#define KFD_SYSFS_PATH_NODES "/sys/devices/virtual/kfd/kfd/topology/nodes"

typedef struct {
	uint32_t gpu_id;
	HsaNodeProperties node;
	HsaMemoryProperties *mem;     /* node->NumBanks elements */
	HsaCacheProperties *cache;
	HsaIoLinkProperties *link;
} node_t;

static HsaSystemProperties *system = NULL;
static node_t *node = NULL;

static HSAKMT_STATUS topology_take_snapshot(void);
static HSAKMT_STATUS topology_drop_snapshot(void);

static void
free_node(node_t *n)
{
	assert(n);

	if (n == NULL)
		return;

	if ((n)->mem)
		free((n)->mem);
	if ((n)->cache)
		free((n)->cache);
	if ((n)->link)
		free((n)->link);
}

static HSAKMT_STATUS
topology_sysfs_get_generation(uint32_t *gen) {
	FILE *fd;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(gen);
	fd = fopen(KFD_SYSFS_PATH_GENERATION_ID, "r");
	if (!fd)
		return HSAKMT_STATUS_ERROR;
	if (fscanf(fd, "%ul", gen) != 1) {
		ret = HSAKMT_STATUS_ERROR;
		goto err;
	}

err:
	fclose(fd);
	return ret;
}

static HSAKMT_STATUS
topology_sysfs_get_system_props(HsaSystemProperties *props) {
	FILE *fd;
	DIR *dirp;
	char *read_buf, *p;
	char prop_name[256];
	long long unsigned int prop_val;
	uint32_t node_count, prog;
	struct dirent *dir;
    int read_size;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;


	assert(props);
	fd = fopen(KFD_SYSFS_PATH_SYSTEM_PROPERTIES, "r");
	if (!fd)
		return HSAKMT_STATUS_ERROR;

	read_buf = malloc(PAGE_SIZE);
	if (!read_buf) {
		ret = HSAKMT_STATUS_NO_MEMORY;
		goto err1;
	}

    read_size = fread(read_buf, 1, PAGE_SIZE, fd);
    if (read_size <= 0) {
		ret = HSAKMT_STATUS_ERROR;
		goto err2;
	}

    /* Since we're using the buffer as a string, we make sure the string terminates */
    if(read_size >= PAGE_SIZE)
        read_size = PAGE_SIZE-1;
    read_buf[read_size] = 0;

	/*
	 * Read the system properties
	 */
	prog = 0;
	p = read_buf;
	while(sscanf(p+=prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name,"platform_oem") == 0)
			props->PlatformOem = (uint32_t)prop_val;
		else if (strcmp(prop_name,"platform_id") == 0)
			props->PlatformId = (uint32_t)prop_val;
		else if (strcmp(prop_name,"platform_rev") == 0)
			props->PlatformRev = (uint32_t)prop_val;
	}

	/*
	 * Discover the number of nodes
	 */
	node_count = 0;
	dirp = opendir(KFD_SYSFS_PATH_NODES);
	if(dirp) {
		/*
		 * Assuming that inside nodes folder there are only folders
		 * which represent the node numbers
		 */
		while ((dir = readdir(dirp)) != 0) {
			if ((strcmp(dir->d_name, ".") == 0) ||
					(strcmp(dir->d_name, "..") == 0))
				continue;
			node_count++;
		}
		closedir(dirp);
	}
	props->NumNodes = node_count;


err2:
	free(read_buf);
err1:
	fclose(fd);
	return ret;
}

static HSAKMT_STATUS
topology_sysfs_get_gpu_id(uint32_t node_id, uint32_t *gpu_id) {
	FILE *fd;
	char path[256];
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(gpu_id);
	snprintf(path, 256, "%s/%d/gpu_id", KFD_SYSFS_PATH_NODES, node_id);
	fd = fopen(path, "r");
	if (!fd)
		return HSAKMT_STATUS_ERROR;
	if (fscanf(fd, "%ul", gpu_id) != 1) {
		ret = HSAKMT_STATUS_ERROR;
	}
	fclose(fd);

	return ret;
}

static HSAKMT_STATUS
topology_sysfs_get_node_props(uint32_t node_id, HsaNodeProperties *props, uint32_t *gpu_id) {
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	char path[256];
	long long unsigned int  prop_val;
	uint32_t i, prog;
    int read_size;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(props);
	assert(gpu_id);
	/* Retrieve the GPU ID */
	ret = topology_sysfs_get_gpu_id(node_id, gpu_id);

	/* Retrieve the marketing name of the node */
	snprintf(path, 256, "%s/%d/name", KFD_SYSFS_PATH_NODES, node_id);
	fd = fopen(path, "r");
	if (!fd)
		return HSAKMT_STATUS_ERROR;

	read_buf = malloc(PAGE_SIZE);
	if (!read_buf) {
		ret = HSAKMT_STATUS_NO_MEMORY;
		goto err1;
	}

    read_size = fread(read_buf, 1, PAGE_SIZE, fd);
    if (read_size <= 0) {
		ret = HSAKMT_STATUS_ERROR;
		goto err2;
	}
    p = memchr(read_buf, '\n', read_size);
	if ((!p) || ((p-read_buf) > HSA_PUBLIC_NAME_SIZE)) {
		ret = HSAKMT_STATUS_ERROR;
		goto err2;
	}
	/*
	 * Convert UTF8 to UTF16
	 */
	for (i = 0; (i < HSA_PUBLIC_NAME_SIZE) && (read_buf[i] != '\n'); i++)
		props->MarketingName[i] = read_buf[i];
	props->MarketingName[i] = 0;
	fclose(fd);

	/* Retrieve the node properties */
	snprintf(path, 256, "%s/%d/properties", KFD_SYSFS_PATH_NODES, node_id);
	fd = fopen(path, "r");
	if (!fd) {
		free(read_buf);
        return HSAKMT_STATUS_ERROR;
	}

    read_size = fread(read_buf, 1, PAGE_SIZE, fd);
    if (read_size <= 0) {
		ret = HSAKMT_STATUS_ERROR;
		goto err2;
	}

    /* Since we're using the buffer as a string, we make sure the string terminates */
    if(read_size >= PAGE_SIZE)
        read_size = PAGE_SIZE-1;
    read_buf[read_size] = 0;

	/*
     * Read the node properties
	 */
	prog = 0;
	p = read_buf;
	while(sscanf(p+=prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name,"cpu_cores_count") == 0)
			props->NumCPUCores = (uint32_t)prop_val;
		else if (strcmp(prop_name,"simd_count") == 0)
			props->NumFComputeCores = (uint32_t)prop_val;
		else if (strcmp(prop_name,"mem_banks_count") == 0)
			props->NumMemoryBanks = (uint32_t)prop_val;
		else if (strcmp(prop_name,"caches_count") == 0)
			props->NumCaches = (uint32_t)prop_val;
		else if (strcmp(prop_name,"io_links_count") == 0)
			props->NumIOLinks = (uint32_t)prop_val;
		else if (strcmp(prop_name,"cpu_core_id_base") == 0)
			props->CComputeIdLo = (uint32_t)prop_val;
		else if (strcmp(prop_name,"simd_id_base") == 0)
			props->FComputeIdLo = (uint32_t)prop_val;
		else if (strcmp(prop_name,"capability") == 0)
			props->Capability.Value = (uint32_t)prop_val;
		else if (strcmp(prop_name,"max_waves_per_simd") == 0)
			props->MaxWavesPerSIMD = (uint32_t)prop_val;
		else if (strcmp(prop_name,"lds_size_in_kb") == 0)
			props->LDSSizeInKB = (uint32_t)prop_val;
		else if (strcmp(prop_name,"gds_size_in_kb") == 0)
			props->GDSSizeInKB = (uint32_t)prop_val;
		else if (strcmp(prop_name,"wave_front_size") == 0)
			props->WaveFrontSize = (uint32_t)prop_val;
		else if (strcmp(prop_name,"array_count") == 0)
			props->NumShaderBanks = (uint32_t)prop_val;
		else if (strcmp(prop_name,"simd_arrays_per_engine") == 0)
			props->NumArrays = (uint32_t)prop_val;
		else if (strcmp(prop_name,"cu_per_simd_array") == 0)
			props->NumCUPerArray = (uint32_t)prop_val;
		else if (strcmp(prop_name,"simd_per_cu") == 0)
			props->NumSIMDPerCU = (uint32_t)prop_val;
		else if (strcmp(prop_name,"max_slots_scratch_cu") == 0)
			props->MaxSlotsScratchCU = (uint32_t)prop_val;
		else if (strcmp(prop_name,"engine_id") == 0)
			props->EngineId = (uint32_t)prop_val;
		else if (strcmp(prop_name,"vendor_id") == 0)
			props->VendorId = (uint32_t)prop_val;
		else if (strcmp(prop_name,"device_id") == 0)
			props->DeviceId = (uint32_t)prop_val;
		else if (strcmp(prop_name,"location_id") == 0)
			props->LocationId = (uint32_t)prop_val;
		else if (strcmp(prop_name,"max_engine_clk_fcompute") == 0)
			props->MaxEngineClockMhzFCompute = (uint32_t)prop_val;
		else if (strcmp(prop_name,"max_engine_clk_ccompute") == 0)
			props->MaxEngineClockMhzCCompute = (uint32_t)prop_val;
		else if (strcmp(prop_name,"local_mem_size") == 0)
			props->LocalMemSize = (uint32_t)prop_val;
	}

err2:
	free(read_buf);
err1:
	fclose(fd);
	return ret;
}

static HSAKMT_STATUS
topology_sysfs_get_mem_props(uint32_t node_id, uint32_t mem_id, HsaMemoryProperties *props) {
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	char path[256];
	long long unsigned int  prop_val;
	uint32_t prog;
    int read_size;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(props);
	snprintf(path, 256, "%s/%d/mem_banks/%d/properties", KFD_SYSFS_PATH_NODES, node_id, mem_id);
	fd = fopen(path, "r");
	if (!fd) {
		return HSAKMT_STATUS_ERROR;
	}
	read_buf = malloc(PAGE_SIZE);
	if (!read_buf) {
		ret = HSAKMT_STATUS_NO_MEMORY;
		goto err1;
	}

    read_size = fread(read_buf, 1, PAGE_SIZE, fd);
    if (read_size <= 0) {
		ret = HSAKMT_STATUS_ERROR;
		goto err2;
	}

    /* Since we're using the buffer as a string, we make sure the string terminates */
    if(read_size >= PAGE_SIZE)
        read_size = PAGE_SIZE-1;
    read_buf[read_size] = 0;

	prog = 0;
	p = read_buf;
	while(sscanf(p+=prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name,"heap_type") == 0)
			props->HeapType = (uint32_t)prop_val;
		else if (strcmp(prop_name,"size_in_bytes") == 0)
			props->SizeInBytes = prop_val;
		else if (strcmp(prop_name,"flags") == 0)
			props->Flags.MemoryProperty = (uint32_t)prop_val;
		else if (strcmp(prop_name,"width") == 0)
			props->Width = (uint32_t)prop_val;
		else if (strcmp(prop_name,"mem_clk_max") == 0)
			props->MemoryClockMax = (uint32_t)prop_val;
	}

err2:
	free(read_buf);
err1:
	fclose(fd);
	return ret;
}

static HSAKMT_STATUS
topology_sysfs_get_cache_props(uint32_t node_id, uint32_t cache_id, HsaCacheProperties *props) {
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	char path[256];
	long long unsigned int  prop_val;
	uint32_t i, prog;
    int read_size;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(props);
	snprintf(path, 256, "%s/%d/caches/%d/properties", KFD_SYSFS_PATH_NODES, node_id, cache_id);
	fd = fopen(path, "r");
	if (!fd) {
		return HSAKMT_STATUS_ERROR;
	}
	read_buf = malloc(PAGE_SIZE);
	if (!read_buf) {
		ret = HSAKMT_STATUS_NO_MEMORY;
		goto err1;
	}

    read_size = fread(read_buf, 1, PAGE_SIZE, fd);
    if (read_size <= 0) {
		ret = HSAKMT_STATUS_ERROR;
		goto err2;
	}

    /* Since we're using the buffer as a string, we make sure the string terminates */
    if(read_size >= PAGE_SIZE)
        read_size = PAGE_SIZE-1;
    read_buf[read_size] = 0;

	prog = 0;
	p = read_buf;
	while(sscanf(p+=prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name,"processor_id_low") == 0)
			props->ProcessorIdLow = (uint32_t)prop_val;
		else if (strcmp(prop_name,"level") == 0)
			props->CacheLevel = (uint32_t)prop_val;
		else if (strcmp(prop_name,"size") == 0)
			props->CacheSize = (uint32_t)prop_val;
		else if (strcmp(prop_name,"cache_line_size") == 0)
			props->CacheLineSize = (uint32_t)prop_val;
		else if (strcmp(prop_name,"cache_lines_per_tag") == 0)
			props->CacheLinesPerTag = (uint32_t)prop_val;
		else if (strcmp(prop_name,"association") == 0)
			props->CacheAssociativity = (uint32_t)prop_val;
		else if (strcmp(prop_name,"latency") == 0)
			props->CacheLatency = (uint32_t)prop_val;
		else if (strcmp(prop_name,"type") == 0)
			props->CacheType.Value = (uint32_t)prop_val;
		else if (strcmp(prop_name, "sibling_map") == 0)
			break;
	}

	prog = 0;
	if ((sscanf(p, "sibling_map %n", &prog)) == 0 && prog) {
		i = 0;
		while ((i < HSA_CPU_SIBLINGS) &&
			(sscanf(p+=prog, "%u%*[,\n]%n", &props->SiblingMap[i++],
					&prog) == 1));
	}

err2:
	free(read_buf);
err1:
	fclose(fd);
	return ret;
}

static HSAKMT_STATUS
topology_sysfs_get_iolink_props(uint32_t node_id, uint32_t iolink_id, HsaIoLinkProperties *props) {
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	char path[256];
	long long unsigned int  prop_val;
	uint32_t prog;
    int read_size;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(props);
	snprintf(path, 256, "%s/%d/io_link/%d/properties", KFD_SYSFS_PATH_NODES, node_id, iolink_id);
	fd = fopen(path, "r");
	if (!fd) {
		return HSAKMT_STATUS_ERROR;
	}
	read_buf = malloc(PAGE_SIZE);
	if (!read_buf) {
		ret = HSAKMT_STATUS_NO_MEMORY;
		goto err1;
	}

    read_size = fread(read_buf, 1, PAGE_SIZE, fd);
    if (read_size <= 0) {
		ret = HSAKMT_STATUS_ERROR;
		goto err2;
	}

    /* Since we're using the buffer as a string, we make sure the string terminates */
    if(read_size >= PAGE_SIZE)
        read_size = PAGE_SIZE-1;
    read_buf[read_size] = 0;

	prog = 0;
	p = read_buf;
	while(sscanf(p+=prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name,"type") == 0)
			props->IoLinkType = (uint32_t)prop_val;
		else if (strcmp(prop_name,"version_major") == 0)
			props->VersionMajor = (uint32_t)prop_val;
		else if (strcmp(prop_name,"version_minor") == 0)
			props->VersionMinor = (uint32_t)prop_val;
		else if (strcmp(prop_name,"node_from") == 0)
			props->NodeFrom = (uint32_t)prop_val;
		else if (strcmp(prop_name,"node_to") == 0)
			props->NodeTo = (uint32_t)prop_val;
		else if (strcmp(prop_name,"weight") == 0)
			props->Weight = (uint32_t)prop_val;
		else if (strcmp(prop_name,"min_latency") == 0)
			props->MinimumLatency = (uint32_t)prop_val;
		else if (strcmp(prop_name,"max_latency") == 0)
			props->MaximumLatency = (uint32_t)prop_val;
		else if (strcmp(prop_name,"min_bandwidth") == 0)
			props->MinimumBandwidth = (uint32_t)prop_val;
		else if (strcmp(prop_name,"max_bandwidth") == 0)
			props->MaximumBandwidth = (uint32_t)prop_val;
		else if (strcmp(prop_name,"recommended_transfer_size") == 0)
			props->RecTransferSize = (uint32_t)prop_val;
		else if (strcmp(prop_name,"flags") == 0)
			props->Flags.LinkProperty = (uint32_t)prop_val;
	}


err2:
	free(read_buf);
err1:
	fclose(fd);
	return ret;
}

HSAKMT_STATUS
topology_take_snapshot(void)
{
	uint32_t gen_start, gen_end, i, j, mem_id, cache_id, link_id;
	HsaSystemProperties sys_props;
	node_t *temp_nodes = 0;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

retry:
	ret = topology_sysfs_get_generation(&gen_start);
	if (ret != HSAKMT_STATUS_SUCCESS)
		return ret;
	ret = topology_sysfs_get_system_props(&sys_props);
	if (ret != HSAKMT_STATUS_SUCCESS)
		return ret;
	if(sys_props.NumNodes > 0) {
		temp_nodes = calloc(sys_props.NumNodes * sizeof(node_t),1);
		if (!temp_nodes)
			return HSAKMT_STATUS_NO_MEMORY;
		for (i = 0; i < sys_props.NumNodes; i++) {
			ret = topology_sysfs_get_node_props(i,
					&temp_nodes[i].node,
					&temp_nodes[i].gpu_id);
			if (ret != HSAKMT_STATUS_SUCCESS) {
				for (j=0; j < i; j++)
					free_node(&temp_nodes[j]);
				free(temp_nodes);
				goto err;
			}
			if (temp_nodes[i].node.NumMemoryBanks) {
				temp_nodes[i].mem = calloc(temp_nodes[i].node.NumMemoryBanks * sizeof(HsaMemoryProperties), 1);
				if (!temp_nodes[i].mem) {
					ret = HSAKMT_STATUS_NO_MEMORY;
					for (j=0; j <= i; j++)
						free_node(&temp_nodes[j]);
					free(temp_nodes);
					goto err;
				}
				for (mem_id = 0; mem_id < temp_nodes[i].node.NumMemoryBanks; mem_id++) {
					ret = topology_sysfs_get_mem_props(i, mem_id, &temp_nodes[i].mem[mem_id]);
					if (ret != HSAKMT_STATUS_SUCCESS) {
						for (j=0; j <= i; j++)
							free_node(&temp_nodes[j]);
						free(temp_nodes);
						goto err;
					}
				}
			}

			if (temp_nodes[i].node.NumCaches) {
				temp_nodes[i].cache = calloc(temp_nodes[i].node.NumCaches * sizeof(HsaCacheProperties), 1);
				if (!temp_nodes[i].cache) {
					ret = HSAKMT_STATUS_NO_MEMORY;
					for (j=0; j <= i; j++)
						free_node(&temp_nodes[j]);
					free(temp_nodes);
					goto err;
				}
				for (cache_id = 0; cache_id < temp_nodes[i].node.NumCaches; cache_id++) {
					ret = topology_sysfs_get_cache_props(i, cache_id, &temp_nodes[i].cache[cache_id]);
					if (ret != HSAKMT_STATUS_SUCCESS) {
						for (j=0; j <= i; j++)
							free_node(&temp_nodes[j]);
						free(temp_nodes);
						goto err;
					}
				}
			}

			if (temp_nodes[i].node.NumIOLinks) {
				temp_nodes[i].link = calloc(temp_nodes[i].node.NumIOLinks * sizeof(HsaIoLinkProperties), 1);
				if (!temp_nodes[i].link) {
					ret = HSAKMT_STATUS_NO_MEMORY;
					for (j=0; j <= i; j++)
						free_node(&temp_nodes[j]);
					free(temp_nodes);
					goto err;
				}
				for (link_id = 0; link_id < temp_nodes[i].node.NumIOLinks; link_id++) {
					ret = topology_sysfs_get_iolink_props(i, link_id, &temp_nodes[i].link[link_id]);
					if (ret != HSAKMT_STATUS_SUCCESS) {
						for (j=0; j <= i; j++)
							free_node(&temp_nodes[j]);
						free(temp_nodes);
						goto err;
					}
				}
			}

		}
	}

	ret = topology_sysfs_get_generation(&gen_end);
	if (ret != HSAKMT_STATUS_SUCCESS) {
		if (temp_nodes) {
			for (j=0; j < sys_props.NumNodes; j++)
				free_node(&temp_nodes[j]);
			free(temp_nodes);
		}
		goto err;
	}

	if (gen_start != gen_end) {
		if (temp_nodes) {
			for (j=0; j < sys_props.NumNodes; j++)
				free_node(&temp_nodes[j]);
			free(temp_nodes);
			temp_nodes = 0;
		}
		goto retry;
	}

	if (!system) {
		system = malloc(sizeof(HsaSystemProperties));
		if (!system) {
			if (temp_nodes) {
				for (j=0; j < sys_props.NumNodes; j++)
					free_node(&temp_nodes[j]);
				free(temp_nodes);
			}
			return HSAKMT_STATUS_NO_MEMORY;
		}
	}

	*system = sys_props;
	if (node)
		free(node);
	node = temp_nodes;
err:

	return ret;
}

/*
 * Drop the Snashot of the HSA topology information.
 * Assume lock is held.
 */
HSAKMT_STATUS
topology_drop_snapshot(void)
{
	HSAKMT_STATUS err;

	if (!!system != !!node) {
		printf("Probable inconsistency?\n");
		err = HSAKMT_STATUS_SUCCESS;
		goto out;
	}

	if (node) {
		uint64_t nodeid;

		/* Remove state */
		for (nodeid = 0; nodeid < system->NumNodes; nodeid++) {
			free_node(&node[nodeid]);
		}

		free(node);
		node = NULL;
	}

	free(system);
	system = NULL;
	err = HSAKMT_STATUS_SUCCESS;

out:
	return err;
}

HSAKMT_STATUS
validate_nodeid(uint32_t nodeid, uint32_t *gpu_id)
{
    if (nodeid >= MAX_NODES || !node || !system || system->NumNodes <= nodeid)
		return HSAKMT_STATUS_INVALID_NODE_UNIT;
	if (gpu_id)
		*gpu_id = node[nodeid].gpu_id;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtAcquireSystemProperties(
    HsaSystemProperties*  SystemProperties    //OUT
    )
{
	HSAKMT_STATUS err;
	CHECK_KFD_OPEN();

	if (!SystemProperties)
			return HSAKMT_STATUS_INVALID_PARAMETER;

	pthread_mutex_lock(&hsakmt_mutex);

	err = topology_take_snapshot();
	if (err != HSAKMT_STATUS_SUCCESS)
		goto out;

	assert(system);

	*SystemProperties = *system;
	err = HSAKMT_STATUS_SUCCESS;

out:
	pthread_mutex_unlock(&hsakmt_mutex);
	return err;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtReleaseSystemProperties(void)
{
	CHECK_KFD_OPEN();

	HSAKMT_STATUS err;

	pthread_mutex_lock(&hsakmt_mutex);

	err = topology_drop_snapshot();

	pthread_mutex_unlock(&hsakmt_mutex);

	return err;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetNodeProperties(
    HSAuint32               NodeId,            //IN
    HsaNodeProperties*      NodeProperties     //OUT
    )
{
	HSAKMT_STATUS err;
	uint32_t gpu_id;

	if (!NodeProperties)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_KFD_OPEN();
	pthread_mutex_lock(&hsakmt_mutex);

	/* KFD ADD page 18, snapshot protocol violation */
	if (system == NULL) {
		err = HSAKMT_STATUS_INVALID_NODE_UNIT;
		assert(system);
		goto out;
	}

	if (NodeId >= system->NumNodes) {
		err = HSAKMT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	err = validate_nodeid(NodeId, &gpu_id);
	if (err != HSAKMT_STATUS_SUCCESS)
		return err;

	*NodeProperties = node[NodeId].node;
	NodeProperties->NumMemoryBanks += NUM_OF_HEAPS;

	err = HSAKMT_STATUS_SUCCESS;

out:
	pthread_mutex_unlock(&hsakmt_mutex);
	return err;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetNodeMemoryProperties(
    HSAuint32             NodeId,             //IN
    HSAuint32             NumBanks,           //IN
    HsaMemoryProperties*  MemoryProperties    //OUT
    )
{
	HSAKMT_STATUS err;
	uint32_t i, gpu_id;

	if (!MemoryProperties)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_KFD_OPEN();
	pthread_mutex_lock(&hsakmt_mutex);

	/* KFD ADD page 18, snapshot protocol violation */
	if (system == NULL) {
		err = HSAKMT_STATUS_INVALID_NODE_UNIT;
		assert(system);
		goto out;
	}

	/* Check still necessary */
	if (NodeId >= system->NumNodes ) {
		err = HSAKMT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	err = validate_nodeid(NodeId, &gpu_id);
	if (err != HSAKMT_STATUS_SUCCESS)
		return err;

	for (i = 0; i < MIN(node[NodeId].node.NumMemoryBanks, NumBanks); i++) {
		assert(node[NodeId].mem);
		MemoryProperties[i] = node[NodeId].mem[i];
	}

	/*Add LDS*/
	if (i < NumBanks){
		MemoryProperties[i].HeapType = HSA_HEAPTYPE_GPU_LDS;
		MemoryProperties[i].SizeInBytes = node[NodeId].node.LDSSizeInKB * 1024;
		MemoryProperties[i].VirtualBaseAddress = fmm_get_aperture_base(FMM_LDS, gpu_id);
		i++;
	}

	/*Add Local memory - HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE*/
	if (i < NumBanks){
		MemoryProperties[i].HeapType = HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE;
		MemoryProperties[i].SizeInBytes = node[NodeId].node.LocalMemSize;
		MemoryProperties[i].VirtualBaseAddress = fmm_get_aperture_base(FMM_GPUVM, gpu_id);
		i++;
	}

	err = HSAKMT_STATUS_SUCCESS;

out:
	pthread_mutex_unlock(&hsakmt_mutex);
	return err;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetNodeCacheProperties(
    HSAuint32           NodeId,         //IN
    HSAuint32           ProcessorId,    //IN
    HSAuint32           NumCaches,      //IN
    HsaCacheProperties* CacheProperties //OUT
    )
{
	HSAKMT_STATUS err;
	uint32_t i;

	if (!CacheProperties)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_KFD_OPEN();
	pthread_mutex_lock(&hsakmt_mutex);

	/* KFD ADD page 18, snapshot protocol violation */
	if (system == NULL) {
		err = HSAKMT_STATUS_INVALID_NODE_UNIT;
		assert(system);
		goto out;
	}

	if (NodeId >= system->NumNodes || NumCaches > node[NodeId].node.NumCaches) {
		err = HSAKMT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	for (i = 0; i < MIN(node[NodeId].node.NumCaches, NumCaches); i++) {
		assert(node[NodeId].cache);
		CacheProperties[i] = node[NodeId].cache[i];
	}

	err = HSAKMT_STATUS_SUCCESS;

out:
	pthread_mutex_unlock(&hsakmt_mutex);
	return err;
}

HSAKMT_STATUS
HSAKMTAPI
hsaKmtGetNodeIoLinkProperties(
    HSAuint32            NodeId,            //IN
    HSAuint32            NumIoLinks,        //IN
    HsaIoLinkProperties* IoLinkProperties  //OUT
    )
{
	HSAKMT_STATUS err;
	uint32_t i;

	if (!IoLinkProperties)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_KFD_OPEN();

	pthread_mutex_lock(&hsakmt_mutex);

	/* KFD ADD page 18, snapshot protocol violation */
	if (system == NULL) {
		err = HSAKMT_STATUS_INVALID_NODE_UNIT;
		assert(system);
		goto out;
	}

	if (NodeId >= system->NumNodes || NumIoLinks > node[NodeId].node.NumIOLinks) {
		err = HSAKMT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	for (i = 0; i < MIN(node[NodeId].node.NumIOLinks, NumIoLinks); i++) {
		assert(node[NodeId].link);
		IoLinkProperties[i] = node[NodeId].link[i];
	}

	err = HSAKMT_STATUS_SUCCESS;

out:
	pthread_mutex_unlock(&hsakmt_mutex);
	return err;
}

uint16_t get_device_id_by_node(HSAuint32 node_id)
{
    if (!node || !system || system->NumNodes <= node_id)
        return 0;

    return node[node_id].node.DeviceId;
}
