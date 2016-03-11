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
#include <stdlib.h>
#include <dirent.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "libhsakmt.h"
#include "fmm.h"
#define PAGE_SIZE 4096
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
/* Number of memory banks added by thunk on top of topology */
#define NUM_OF_IGPU_HEAPS 3
#define NUM_OF_DGPU_HEAPS 2
/* SYSFS related */
#define KFD_SYSFS_PATH_GENERATION_ID "/sys/devices/virtual/kfd/kfd/topology/generation_id"
#define KFD_SYSFS_PATH_SYSTEM_PROPERTIES "/sys/devices/virtual/kfd/kfd/topology/system_properties"
#define KFD_SYSFS_PATH_NODES "/sys/devices/virtual/kfd/kfd/topology/nodes"
#define MAX_CPU_CORES	128
#define MAX_CACHES	256

typedef struct {
	uint32_t gpu_id;
	HsaNodeProperties node;
	HsaMemoryProperties *mem;     /* node->NumBanks elements */
	HsaCacheProperties *cache;
	HsaIoLinkProperties *link;
} node_t;

static HsaSystemProperties *_system = NULL;
static node_t *node = NULL;

static HSAKMT_STATUS topology_take_snapshot(void);
static HSAKMT_STATUS topology_drop_snapshot(void);
//static int get_cpu_stepping(uint16_t* stepping);

static struct hsa_gfxip_table {
	uint16_t device_id;		// Device ID
	unsigned char major;		// GFXIP Major engine version
	unsigned char minor;		// GFXIP Minor engine version
	unsigned char stepping;		// GFXIP Stepping info
	unsigned char is_dgpu;		// Predicate for dGPU devices
	const char* marketing_name;	// Marketing Name of the device
} gfxip_lookup_table[] = {
	/* Kaveri Family */
	{ 0x1304, 7, 0, 0, 0, "Spectre" },
	{ 0x1305, 7, 0, 0, 0, "Spectre" },
	{ 0x1306, 7, 0, 0, 0, "Spectre" },
	{ 0x1307, 7, 0, 0, 0, "Spectre" },
	{ 0x1309, 7, 0, 0, 0, "Spectre" },
	{ 0x130A, 7, 0, 0, 0, "Spectre" },
	{ 0x130B, 7, 0, 0, 0, "Spectre" },
	{ 0x130C, 7, 0, 0, 0, "Spectre" },
	{ 0x130D, 7, 0, 0, 0, "Spectre" },
	{ 0x130E, 7, 0, 0, 0, "Spectre" },
	{ 0x130F, 7, 0, 0, 0, "Spectre" },
	{ 0x1310, 7, 0, 0, 0, "Spectre" },
	{ 0x1311, 7, 0, 0, 0, "Spectre" },
	{ 0x1312, 7, 0, 0, 0, "Spooky" },
	{ 0x1313, 7, 0, 0, 0, "Spectre" },
	{ 0x1315, 7, 0, 0, 0, "Spectre" },
	{ 0x1316, 7, 0, 0, 0, "Spooky" },
	{ 0x1317, 7, 0, 0, 0, "Spooky" },
	{ 0x1318, 7, 0, 0, 0, "Spectre" },
	{ 0x131B, 7, 0, 0, 0, "Spectre" },
	{ 0x131C, 7, 0, 0, 0, "Spectre" },
	{ 0x131D, 7, 0, 0, 0, "Spectre" },
	/* Carrizo Family */
	{ 0x9870, 8, 0, 1, 0, "Carrizo" },
	{ 0x9874, 8, 0, 1, 0, "Carrizo" },
	{ 0x9875, 8, 0, 1, 0, "Carrizo" },
	{ 0x9876, 8, 0, 1, 0, "Carrizo" },
	{ 0x9877, 8, 0, 1, 0, "Carrizo" },
	/* Tonga Family */
	{ 0x6920, 8, 0, 2, 1, "Tonga" },
	{ 0x6921, 8, 0, 2, 1, "Tonga" },
	{ 0x6928, 8, 0, 2, 1, "Tonga" },
	{ 0x6929, 8, 0, 2, 1, "Tonga" },
	{ 0x692B, 8, 0, 2, 1, "Tonga" },
	{ 0x692F, 8, 0, 2, 1, "Tonga" },
	{ 0x6930, 8, 0, 2, 1, "Tonga" },
	{ 0x6938, 8, 0, 2, 1, "Tonga" },
	{ 0x6939, 8, 0, 2, 1, "Tonga" },
	/* Fiji */
	{ 0x7300, 8, 0, 3, 1, "Fiji" }
};

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

static void free_nodes(node_t *temp_nodes, int size)
{
	int i;
	if (temp_nodes) {
		for (i = 0; i < size; i++)
			free_node(&temp_nodes[i]);
		free(temp_nodes);
	}
}

/* num_subdirs - find the number of sub-directories in the specified path
 *	@dirpath - directory path to find sub-directories underneath
 *	@prefix - only count sub-directory names starting with prefix.
 *		Use blank string, "", to count all.
 *	Return - number of sub-directories
 */
static int num_subdirs(char *dirpath, char *prefix)
{
	int count = 0;
	DIR *dirp;
	struct dirent *dir;
	int prefix_len = strlen(prefix);

	dirp = opendir(dirpath);
	if(dirp) {
		while ((dir = readdir(dirp)) != 0) {
			if ((strcmp(dir->d_name, ".") == 0) ||
				(strcmp(dir->d_name, "..") == 0))
				continue;
			if (prefix_len &&
				strncmp(dir->d_name, prefix, prefix_len))
				continue;
			count++;
		}
		closedir(dirp);
	}

	return count;
}

/* read_file - Read the content of a file
 *	@file - file to read
 *	@buf - [OUT] buffer containing data read from the file
 *	@buf_sz - buffer size
 *	Return - data size in the returning buffer
 */
static size_t read_file(char *file, char *buf, size_t buf_sz)
{
	int fd;
	size_t len = 0;

	memset(buf, 0, buf_sz);

	if ((fd = open(file, O_RDONLY)) < 0)
		return 0;
	len = read(fd, buf, buf_sz);
	close(fd);

	return len;
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

HSAKMT_STATUS
topology_sysfs_get_system_props(HsaSystemProperties *props) {
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	long long unsigned int prop_val;
	uint32_t prog;
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
	 * Discover the number of nodes:
	 * Assuming that inside nodes folder there are only folders
	 * which represent the node numbers
	 */
	props->NumNodes = num_subdirs(KFD_SYSFS_PATH_NODES, "");

err2:
	free(read_buf);
err1:
	fclose(fd);
	return ret;
}

HSAKMT_STATUS
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

static const struct hsa_gfxip_table* find_hsa_gfxip_device(uint16_t device_id)
{
	uint32_t i, table_size;

	table_size = sizeof(gfxip_lookup_table)/sizeof(struct hsa_gfxip_table);
	for (i=0; i<table_size; i++) {
		if(gfxip_lookup_table[i].device_id == device_id)
			return &gfxip_lookup_table[i];
	}
	return NULL;
}

bool topology_is_dgpu(uint16_t device_id)
{
	const struct hsa_gfxip_table* hsa_gfxip =
				find_hsa_gfxip_device(device_id);

	if (hsa_gfxip && hsa_gfxip->is_dgpu) {
		is_dgpu = true;
		return true;
	}
	return false;
}

HSAKMT_STATUS
topology_sysfs_get_node_props(uint32_t node_id, HsaNodeProperties *props, uint32_t *gpu_id) {
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	char path[256];
	long long unsigned int  prop_val;
	uint32_t i, prog;
	uint16_t fw_version = 0;
	int read_size;
	const struct hsa_gfxip_table* hsa_gfxip;

	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(props);
	assert(gpu_id);
	/* Retrieve the GPU ID */
	ret = topology_sysfs_get_gpu_id(node_id, gpu_id);

	read_buf = malloc(PAGE_SIZE);
	if (!read_buf)
		return HSAKMT_STATUS_NO_MEMORY;

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
		goto err;
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
		else if (strcmp(prop_name,"fw_version") == 0)
			fw_version = (uint16_t)prop_val;
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
			props->LocalMemSize = prop_val;

	}

//	get_cpu_stepping(&stepping);
	props->EngineId.ui32.uCode = fw_version & 0x3ff;
	props->EngineId.ui32.Major = 0;
	props->EngineId.ui32.Minor = 0;
	props->EngineId.ui32.Stepping = 0;

	hsa_gfxip = find_hsa_gfxip_device(props->DeviceId);
	if (hsa_gfxip) {
		props->EngineId.ui32.Major = hsa_gfxip->major & 0x3f;
		props->EngineId.ui32.Minor = hsa_gfxip->minor;
		props->EngineId.ui32.Stepping = hsa_gfxip->stepping;

		if (!hsa_gfxip->marketing_name) {
			ret = HSAKMT_STATUS_ERROR;
			goto err;
		}

		/* Retrieve the marketing name of the node, convert UTF8 to UTF16 */
		for (i = 0; hsa_gfxip->marketing_name[i] != 0 && i < HSA_PUBLIC_NAME_SIZE - 1; i++)
			props->MarketingName[i] = hsa_gfxip->marketing_name[i];
		props->MarketingName[i] = 0;
	}
	if (props->NumFComputeCores)
		assert(props->EngineId.ui32.Major);

err:
	free(read_buf);
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
			props->SizeInBytes = (uint64_t)prop_val;
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


/* parse_sysfs_cache -
 *	@sys_path - cache path in sysfs
 *	@prop - [OUT] HSA cache property to fill up
 *	@apicid - an array where contains each processor's apicid
 *	Return - HSAKMT_STATUS_SUCCESS in success or an error number in failure
 */
static HSAKMT_STATUS
parse_sysfs_cache(char *sys_path, HsaCacheProperties *prop, uint32_t *apicid)
{
	char file[256], buf[256];
	int i, j, n, cpu;
	int last; /* the last valid entry in array, which is n-1 if n items */
	unsigned long map[32];
	char *token, *str;

	/* cache level */
	snprintf(file, 256, "%s/level", sys_path);
	read_file(file, buf, sizeof(buf));
	prop->CacheLevel = atoi(buf);

	/* cache size */
	snprintf(file, 256, "%s/size", sys_path);
	read_file(file, buf, sizeof(buf));
	prop->CacheSize = (atoi(buf));

	/* cache line size in bytes */
	snprintf(file, 256, "%s/coherency_line_size", sys_path);
	read_file(file, buf, sizeof(buf));
	prop->CacheLineSize = (atoi(buf));

	/* cache lines per tag */
	snprintf(file, 256, "%s/physical_line_partition", sys_path);
	read_file(file, buf, sizeof(buf));
	prop->CacheLinesPerTag = (atoi(buf));

	/* cache associativity */
	snprintf(file, 256, "%s/ways_of_associativity", sys_path);
	read_file(file, buf, sizeof(buf));
	prop->CacheAssociativity = (atoi(buf));

	/* cache type */
	prop->CacheType.ui32.CPU = 1;
	snprintf(file, 256, "%s/type", sys_path);
	read_file(file, buf, sizeof(buf));
	if (buf[0] == 'D')
		prop->CacheType.ui32.Data = 1;
	else if (buf[0] == 'I')
		prop->CacheType.ui32.Instruction = 1;

	/* sibling map */
	snprintf(file, 256, "%s/shared_cpu_map", sys_path);
	read_file(file, buf, sizeof(buf));
	/* Data in shared_cpu_map can be XXXXXXXX when the system doesn't have
	 * more than 32 processors; it also can be XXXXXXXX,XXXXXXXX,XX .... to
	 * represent more than 32 processors. We'll parse each XXXXXXXX and
	 * store them into map[].
	 * Say shared_cpu_map is "Nn-1,Nn-2,...,N2,N1,N0\n". Because strtok_r()
	 * parses Nn-1 first, map[] will store data in a reversed order:
	 * map[0]=Nn-1, map[1]=Nn-2, ... map[n-2]=N1, map[n-1]=N0
	 */
	str = (char *)&buf[0];
	for (last = 0; last < 32; last++) { /* declared map[32] */
		token = strtok_r(str, ",", &str);
		map[last] = strtol(token, NULL, 16);
		if (token[strlen(token)-1] == '\n') /* this is N0 */
			break;
	}
	if (last >= 32) {
		printf("Fail to parse shared_cpu_map. Increase map[].\n");
		return HSAKMT_STATUS_ERROR;
	}

	/* Lower processor ID doesn't always have lower apicid.
	 * Search the lowest apicid for ProcIdLow
	 */
	prop->ProcessorIdLow = 0xffffffff;
	for (i = last; i >= 0; i--) { /* N0 is stored in map[count] */
		if (!map[i])
			continue;
		j = 32;
		while (j-- > 0) {
			if (map[i] & (1<<j)) {
				cpu = 32 * (last - i) + j;
				if (apicid[cpu] < prop->ProcessorIdLow)
					prop->ProcessorIdLow = apicid[cpu];
			}
		}
	}
	/* Now fill in SiblingMap using ProcIdLow as the offset */
	for (i = last; i >= 0; i--) {
		j = 32;
		while (j-- > 0) {
			if (map[i] & (1<<j)) {
				cpu = 32 * (last - i) + j;
				/* Use the lowest-process-id item as offset */
				n = apicid[cpu] - prop->ProcessorIdLow;
				/* Use array instead of bitmask so the software
				 * is endian-worry free
				 */
				if (n < HSA_CPU_SIBLINGS)
					prop->SiblingMap[n] = 1;
				else {
					printf("Increase HSA_CPU_SIBLINGS.\n");
					return HSAKMT_STATUS_ERROR;
				}
			}
		}
	}

	return HSAKMT_STATUS_SUCCESS;
}

/* topology_get_cpu_cache_props - get CPU cache properties and fill in the
 *		cache entry of the node's table
 *	@tbl - the node table to fill up
 *	Return - HSAKMT_STATUS_SUCCESS in success or error number in failure
 */
static HSAKMT_STATUS
topology_get_cpu_cache_props(node_t *tbl)
{
	FILE *fd;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;
	char *token, *str;
	uint32_t apicid[MAX_CPU_CORES];
	int num_cpus = 0, num_caches = 0;
	char path[256], buf[256], cache_paths[MAX_CACHES][256];
	int i, j, n;
	const char SYSDIR[] = "/sys/devices/system/cpu";

	if (tbl == NULL)
		return HSAKMT_STATUS_ERROR;

	/* Get apicid info from /proc/cpuinfo for ProcessorIdLow */
	if (!(fd = fopen("/proc/cpuinfo", "r")))
		return HSAKMT_STATUS_ERROR;

	while (fgets(buf, 256, fd) != NULL) {
		/* /proc/cpuinfo lists in format - property : value */
		token = strtok_r(buf, ":", &str);
		if (strncmp(token, "apicid", 6) == 0) {
			if (num_cpus >= MAX_CPU_CORES) {
				printf("MAX_CPU_CORES %d is not enough.", MAX_CPU_CORES);
				fclose(fd);
				return HSAKMT_STATUS_ERROR;
			}
			apicid[num_cpus++] = atoi(str);
		}
	}
	fclose(fd);

	/* Get cache data from /sys/devices/system/cpu/cpuX/cache/indexY */

	/*  1. Calculate how many caches */
	for (i=0; i<num_cpus; i++) {
		snprintf(path, 256, "%s/cpu%d/cache", SYSDIR, i);
		n = num_subdirs(path, "index");
		for (j=0; j<n; j++) {
			/* One cache may be listed more than once under
			 * different CPUs if it's shared by CPUs. From
			 * shared_cpu_list we find shared CPUs.
			 */
			snprintf(path, 256,
				"%s/cpu%d/cache/index%d/shared_cpu_list",
				SYSDIR, i, j);
			read_file(path, buf, sizeof(buf));
			/* It's listed as N1,N2,... or N1-Nx if more than one
			 * CPU shares this cache. We'll only count the cache
			 * listed under CPU N1. Any cache listed at CPU N2, N3,
			 * ... Nx, is duplicated and should be ignored.
			 */
			str = strtok(buf, ",-");
			if (atoi(str) != i) /* this is not CPU N1 */
				continue; /* cache has been listed at CPU N1 */
			if (num_caches >= MAX_CACHES) {
				printf("MAX_CACHES %d is not enough!\n", MAX_CACHES);
				return HSAKMT_STATUS_ERROR;
			}
			snprintf(cache_paths[num_caches++], 256,
				"%s/cpu%d/cache/index%d", SYSDIR, i, j);
		}
	}

	/* 2. Allocate number of caches for the table */
	tbl->node.NumCaches = num_caches;
	tbl->cache = calloc(tbl->node.NumCaches * sizeof(HsaCacheProperties), 1);
	if (!tbl->cache)
		return HSAKMT_STATUS_NO_MEMORY;

	/* 3. Fill up cache properties */
	for (i=0; i<num_caches; i++) {
		ret = parse_sysfs_cache(cache_paths[i],
					&tbl->cache[i],
					&apicid[0]);
		if (ret != HSAKMT_STATUS_SUCCESS) {
			printf("Failed to parse cache properties.\n");
			free(tbl->cache);
			return ret;
		}
	}

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
	snprintf(path, 256, "%s/%d/io_links/%d/properties", KFD_SYSFS_PATH_NODES, node_id, iolink_id);
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

/* topology_get_free_io_link_slot_for_node - For the given node_id, find the next
 *   available free slot to add an io_link
 */
static HsaIoLinkProperties * topology_get_free_io_link_slot_for_node(uint32_t node_id,
			const HsaSystemProperties *sys_props, node_t *temp_nodes)
{
	HsaIoLinkProperties *props;

	if (node_id >= sys_props->NumNodes) {
		printf("Invalid node [%d]\n", node_id);
		return NULL;
	}

	props = temp_nodes[node_id].link;
	if (!props) {
		printf("Error. No io_link reported for Node [%d]\n", node_id);
		return NULL;
	}

	if (temp_nodes[node_id].node.NumIOLinks >= sys_props->NumNodes - 1) {
		printf("Error. No more space for io_link for Node [%d]\n", node_id);
		return NULL;
	}

	return &props[temp_nodes[node_id].node.NumIOLinks];
}

/* topology_add_io_link_for_node - If a free slot is available,
 *  add io_link for the given Node.
 *  TODO: Add other members of HsaIoLinkProperties
 */
static HSAKMT_STATUS topology_add_io_link_for_node(uint32_t node_id,
		const HsaSystemProperties *sys_props, node_t *temp_nodes,
		HSA_IOLINKTYPE IoLinkType, uint32_t NodeTo,
		uint32_t Weight)
{
	HsaIoLinkProperties *props;
	props = topology_get_free_io_link_slot_for_node(node_id,
		sys_props, temp_nodes);
	if (!props)
		return HSAKMT_STATUS_NO_MEMORY;

	props->IoLinkType = IoLinkType;
	props->NodeFrom = node_id;
	props->NodeTo = NodeTo;
	props->Weight = Weight;
	temp_nodes[node_id].node.NumIOLinks++;

	return HSAKMT_STATUS_SUCCESS;
}

/* topology_create_reverse_io_link - Create io_links from the given CPU
 *	NUMA node to all the GPUs attached to that node
 */
static void topology_create_reverse_io_link(uint32_t cpu_node,
			const HsaSystemProperties *sys_props, node_t *temp_nodes)
{
	unsigned int gpu_node;
	HSAKMT_STATUS ret;

	for (gpu_node = 0; gpu_node < sys_props->NumNodes; gpu_node++) {
		if (temp_nodes[gpu_node].gpu_id != 0) {
			/* Check if this GPU is connected to the give cpu_node,
			 * if so create an io_link */
			if (temp_nodes[gpu_node].link->NodeTo == cpu_node) {
				ret = topology_add_io_link_for_node(cpu_node, sys_props,
					temp_nodes, HSA_IOLINKTYPE_PCIEXPRESS,
					gpu_node, temp_nodes[gpu_node].link->Weight);
				if (ret != HSAKMT_STATUS_SUCCESS) {
					printf("Error [%d]. Failed to create reverse io_links from Node [%d]\n",
						ret, cpu_node);
					return;
				}
			}
		}
	}
}

/* topology_create_indirect_gpu_links - For the given cpu_node,
 *  find all nodes connected to it and create io_links
 *  among them */
static void topology_create_indirect_gpu_links(uint32_t cpu_node,
		const HsaSystemProperties *sys_props, node_t *temp_nodes)
{
	unsigned int i, j;
	HSAKMT_STATUS ret;
	HSA_IOLINKTYPE IoLinkType;
	HsaIoLinkProperties *props = temp_nodes[cpu_node].link;


	if (!props || temp_nodes[cpu_node].node.NumIOLinks == 0) {
		printf("CPU Node [%d] has no GPU connected\n", cpu_node);
		return;
	}

	/* props is the list of io_links cpu_node is connected to.
	 * Make an indirect io_links from props[i].NodeTo --> props[j].NodeTo
	 * and props[j].NodeTo --> props[i].NodeTo */
	for (i = 0; i < temp_nodes[cpu_node].node.NumIOLinks - 1; i++)
	{
		for (j = i + 1; j < temp_nodes[cpu_node].node.NumIOLinks; j++) {
			/* Ignore CPU <--> CPU node connected as it is handled by QPI
			 * link function */
			if (temp_nodes[props[i].NodeTo].gpu_id == 0 &&
				temp_nodes[props[j].NodeTo].gpu_id == 0)
				continue;

			/* For the given cpu_node, connect to or from the GPUs that are
			 * connected directly to it via PCIEXPRESS */
			if ((temp_nodes[props[i].NodeTo].gpu_id != 0 &&
				props[i].IoLinkType != HSA_IOLINKTYPE_PCIEXPRESS) ||
				(temp_nodes[props[j].NodeTo].gpu_id != 0 &&
				props[j].IoLinkType != HSA_IOLINKTYPE_PCIEXPRESS))
				continue;

			/* The link is from GPU to non-parent NUMA node. So set link type
			 * to HT */
			if (temp_nodes[props[i].NodeTo].gpu_id == 0 ||
				temp_nodes[props[j].NodeTo].gpu_id == 0)
				IoLinkType = HSA_IOLINKTYPE_HYPERTRANSPORT;
			else
				IoLinkType = HSA_IOLINKTYPE_PCIEXPRESS;

			ret = topology_add_io_link_for_node(props[i].NodeTo,
				sys_props, temp_nodes, IoLinkType,
				props[j].NodeTo, props[i].Weight + props[j].Weight);
			if (ret != HSAKMT_STATUS_SUCCESS)
				printf("Error [%d]. Failed to add io_link from Node [%d]->[%d]\n",
				ret, i, j);

			ret = topology_add_io_link_for_node(props[j].NodeTo,
				sys_props, temp_nodes, IoLinkType,
				props[i].NodeTo, props[i].Weight + props[j].Weight);
			if (ret != HSAKMT_STATUS_SUCCESS)
				printf("Error [%d]. Failed to add io_link from Node [%d]->[%d]\n",
				ret, j, i);
		}
	}
}

HSAKMT_STATUS
topology_take_snapshot(void)
{
	uint32_t gen_start, gen_end, i, mem_id, cache_id, link_id;
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
				free_nodes(temp_nodes, i);
				goto err;
			}
			if (temp_nodes[i].node.NumMemoryBanks) {
				temp_nodes[i].mem = calloc(temp_nodes[i].node.NumMemoryBanks * sizeof(HsaMemoryProperties), 1);
				if (!temp_nodes[i].mem) {
					ret = HSAKMT_STATUS_NO_MEMORY;
					free_nodes(temp_nodes, i + 1);
					goto err;
				}
				for (mem_id = 0; mem_id < temp_nodes[i].node.NumMemoryBanks; mem_id++) {
					ret = topology_sysfs_get_mem_props(i, mem_id, &temp_nodes[i].mem[mem_id]);
					if (ret != HSAKMT_STATUS_SUCCESS) {
						free_nodes(temp_nodes, i + 1);
						goto err;
					}
				}
			}

			if (temp_nodes[i].node.NumCaches) {
				temp_nodes[i].cache = calloc(temp_nodes[i].node.NumCaches * sizeof(HsaCacheProperties), 1);
				if (!temp_nodes[i].cache) {
					ret = HSAKMT_STATUS_NO_MEMORY;
					free_nodes(temp_nodes, i + 1);
					goto err;
				}
				for (cache_id = 0; cache_id < temp_nodes[i].node.NumCaches; cache_id++) {
					ret = topology_sysfs_get_cache_props(i, cache_id, &temp_nodes[i].cache[cache_id]);
					if (ret != HSAKMT_STATUS_SUCCESS) {
						free_nodes(temp_nodes, i + 1);
						goto err;
					}
				}
			}
			else if (!temp_nodes[i].gpu_id) { /* This is a CPU node */
				/* Get info from /proc/cpuinfo and /sys/devices/system */
				ret = topology_get_cpu_cache_props(&temp_nodes[i]);
				if (ret != HSAKMT_STATUS_SUCCESS) {
					free_nodes(temp_nodes, i + 1);
					goto err;
				}
			}

			/* To simplify, allocate maximum needed memory for io_links for each node. This
			 * removes the need for realloc when indirect and QPI links are added later */
			temp_nodes[i].link = calloc(sys_props.NumNodes - 1, sizeof(HsaIoLinkProperties));
			if (!temp_nodes[i].link) {
				ret = HSAKMT_STATUS_NO_MEMORY;
				free_nodes(temp_nodes, i + 1);
				goto err;
			}

			if (temp_nodes[i].node.NumIOLinks) {
				if (temp_nodes[i].gpu_id == 0) {
					printf("Warning. Not expecting CPU Node [%d] to have [%d] io_links.\n",
						i, temp_nodes[i].node.NumIOLinks);
				}
				for (link_id = 0; link_id < temp_nodes[i].node.NumIOLinks; link_id++) {
					ret = topology_sysfs_get_iolink_props(i, link_id, &temp_nodes[i].link[link_id]);
					if (ret != HSAKMT_STATUS_SUCCESS) {
						free_nodes(temp_nodes, i+1);
						goto err;
					}
				}
			}

		}
	}

	/* The Kernel only creates one way direct link -
	 * GPU(PCI_BUS) --> Parent NUMA Node. Create the reverse direct
	 * io_link here. [NUMA node] --> GPU */

	/* Create the reverse io_link for all the CPU nodes */
	for (i = 0; i < sys_props.NumNodes; i++) {
		if (temp_nodes[i].gpu_id == 0) {
			if (!temp_nodes[i].link) {
				printf("Unexpected NULL pointer. Node [%d].link\n", i);
				ret = HSAKMT_STATUS_NO_MEMORY;
				free_nodes(temp_nodes, i + 1);
				goto err;
			}
			topology_create_reverse_io_link(i, &sys_props, temp_nodes);
		}
	}

	/* Create In-direct links for GPUs. Connect all the (Peer-to-Peer) GPUs
	 * that belong to same NUMA node.
	 * For each CPU (NUMA) node, interconnect all the GPUs. */
	for (i = 0; i < sys_props.NumNodes; i++) {
		if (temp_nodes[i].gpu_id == 0) {
			topology_create_indirect_gpu_links(i, &sys_props, temp_nodes);
		}
	}


	ret = topology_sysfs_get_generation(&gen_end);
	if (ret != HSAKMT_STATUS_SUCCESS) {
		free_nodes(temp_nodes, sys_props.NumNodes);
		goto err;
	}

	if (gen_start != gen_end) {
		free_nodes(temp_nodes, sys_props.NumNodes);
		temp_nodes = 0;
		goto retry;
	}

	if (!_system) {
		_system = malloc(sizeof(HsaSystemProperties));
		if (!_system) {
			free_nodes(temp_nodes, sys_props.NumNodes);
			return HSAKMT_STATUS_NO_MEMORY;
		}
	}

	*_system = sys_props;
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

	if (!!_system != !!node) {
		printf("Probable inconsistency?\n");
		err = HSAKMT_STATUS_SUCCESS;
		goto out;
	}

	if (node) {
		/* Remove state */
		free_nodes(node, _system->NumNodes);
		node = NULL;
	}

	free(_system);
	_system = NULL;
	err = HSAKMT_STATUS_SUCCESS;

out:
	return err;
}

HSAKMT_STATUS
validate_nodeid(uint32_t nodeid, uint32_t *gpu_id)
{
    if (!node || !_system || _system->NumNodes <= nodeid)
		return HSAKMT_STATUS_INVALID_NODE_UNIT;
	if (gpu_id)
		*gpu_id = node[nodeid].gpu_id;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS
gpuid_to_nodeid(uint32_t gpu_id, uint32_t* node_id){
	uint64_t node_idx;
	for(node_idx = 0; node_idx < _system->NumNodes; node_idx++){
		if (node[node_idx].gpu_id == gpu_id){
			*node_id = node_idx;
			return HSAKMT_STATUS_SUCCESS;
		}
	}

	return HSAKMT_STATUS_INVALID_NODE_UNIT;

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

	assert(_system);

	*SystemProperties = *_system;
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
	if (_system == NULL) {
		err = HSAKMT_STATUS_INVALID_NODE_UNIT;
		assert(_system);
		goto out;
	}

	if (NodeId >= _system->NumNodes) {
		err = HSAKMT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	err = validate_nodeid(NodeId, &gpu_id);
	if (err != HSAKMT_STATUS_SUCCESS)
		return err;

	*NodeProperties = node[NodeId].node;
	/* For CPU only node don't add any additional GPU memory banks. */
	if (gpu_id) {
		if (topology_is_dgpu(get_device_id_by_gpu_id(gpu_id)))
			NodeProperties->NumMemoryBanks += NUM_OF_DGPU_HEAPS;
		else
			NodeProperties->NumMemoryBanks += NUM_OF_IGPU_HEAPS;
	}
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
	HSAKMT_STATUS err = HSAKMT_STATUS_SUCCESS;
	uint32_t i, gpu_id;
	HSAuint64 aperture_limit;

	if (!MemoryProperties)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_KFD_OPEN();
	pthread_mutex_lock(&hsakmt_mutex);

	/* KFD ADD page 18, snapshot protocol violation */
	if (_system == NULL) {
		err = HSAKMT_STATUS_INVALID_NODE_UNIT;
		assert(_system);
		goto out;
	}

	/* Check still necessary */
	if (NodeId >= _system->NumNodes ) {
		err = HSAKMT_STATUS_INVALID_PARAMETER;
		goto out;
	}

	err = validate_nodeid(NodeId, &gpu_id);
	if (err != HSAKMT_STATUS_SUCCESS)
		goto out;

	memset(MemoryProperties, 0, NumBanks * sizeof(HsaMemoryProperties));

	for (i = 0; i < MIN(node[NodeId].node.NumMemoryBanks, NumBanks); i++) {
		assert(node[NodeId].mem);
		MemoryProperties[i] = node[NodeId].mem[i];
	}

	/* The following memory banks does not apply to CPU only node */
	if (gpu_id == 0)
		goto out;

	/*Add LDS*/
	if (i < NumBanks &&
		fmm_get_aperture_base_and_limit(FMM_LDS, gpu_id,
				&MemoryProperties[i].VirtualBaseAddress, &aperture_limit) == HSAKMT_STATUS_SUCCESS) {
		MemoryProperties[i].HeapType = HSA_HEAPTYPE_GPU_LDS;
		MemoryProperties[i].SizeInBytes = node[NodeId].node.LDSSizeInKB * 1024;
		i++;
	}

	/* Add Local memory - HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE.
	 * For dGPU the topology node contains Local Memory and it is added by the for loop above */
	if (!topology_is_dgpu(get_device_id_by_gpu_id(gpu_id)) &&
		i < NumBanks &&
		node[NodeId].node.LocalMemSize > 0 &&
		fmm_get_aperture_base_and_limit(FMM_GPUVM, gpu_id,
				&MemoryProperties[i].VirtualBaseAddress, &aperture_limit) == HSAKMT_STATUS_SUCCESS) {
		MemoryProperties[i].HeapType = HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE;
		MemoryProperties[i].SizeInBytes = node[NodeId].node.LocalMemSize;
		i++;
	}

	/*Add SCRATCH*/
	if (i < NumBanks &&
		fmm_get_aperture_base_and_limit(FMM_SCRATCH, gpu_id,
				&MemoryProperties[i].VirtualBaseAddress, &aperture_limit) == HSAKMT_STATUS_SUCCESS) {
		MemoryProperties[i].HeapType = HSA_HEAPTYPE_GPU_SCRATCH;
		MemoryProperties[i].SizeInBytes = (aperture_limit - MemoryProperties[i].VirtualBaseAddress) + 1;
		i++;
	}

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
	if (_system == NULL) {
		err = HSAKMT_STATUS_INVALID_NODE_UNIT;
		assert(_system);
		goto out;
	}

	if (NodeId >= _system->NumNodes || NumCaches > node[NodeId].node.NumCaches) {
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
	if (_system == NULL) {
		err = HSAKMT_STATUS_INVALID_NODE_UNIT;
		assert(_system);
		goto out;
	}

	if (NodeId >= _system->NumNodes || NumIoLinks > node[NodeId].node.NumIOLinks) {
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
    if (!node || !_system || _system->NumNodes <= node_id)
        return 0;

    return node[node_id].node.DeviceId;
}

uint16_t get_device_id_by_gpu_id(HSAuint32 gpu_id)
{
	unsigned int i;
	if (!node || !_system)
		return 0;

	for (i = 0; i < _system->NumNodes; i++) {
		if (node[i].gpu_id == gpu_id)
			return node[i].node.DeviceId;
	}

	return 0;
}

HSAKMT_STATUS validate_nodeid_array(uint32_t **gpu_id_array,
		uint32_t NumberOfNodes, uint32_t *NodeArray)
{
	HSAKMT_STATUS ret;
	unsigned int i;

	if (NumberOfNodes == 0 || NodeArray == NULL || gpu_id_array == NULL)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	/* Translate Node IDs to gpu_ids */
	*gpu_id_array = malloc(NumberOfNodes * sizeof(uint32_t));
	if (*gpu_id_array == NULL)
		return HSAKMT_STATUS_NO_MEMORY;
        for (i = 0; i < NumberOfNodes; i++) {
		ret = validate_nodeid(NodeArray[i], *gpu_id_array + i);
		if (ret != HSAKMT_STATUS_SUCCESS) {
			free(*gpu_id_array);
			break;
		}
	}

        return ret;
}

#if 0
static int get_cpu_stepping(uint16_t* stepping)
{
	int ret;
	FILE* fd = fopen("/proc/cpuinfo", "r");
	if (!fd)
		return -1;

	char* read_buf = malloc(PAGE_SIZE);
	if (!read_buf) {
		ret = -1;
		goto err1;
	}

	int read_size = fread(read_buf, 1, PAGE_SIZE, fd);
	if (read_size <= 0) {
		ret = -2;
		goto err2;
	}

	/* Since we're using the buffer as a string, we make sure the string terminates */
	if(read_size >= PAGE_SIZE)
		read_size = PAGE_SIZE-1;
	read_buf[read_size] = 0;

	*stepping = 0;

	char* p = strstr(read_buf, "stepping");
	if (p)
		sscanf(p , "stepping\t: %hu\n", stepping);

err2:
	free(read_buf);
err1:
	fclose(fd);

	return ret;
}
#endif
