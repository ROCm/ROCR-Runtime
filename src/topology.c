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

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sched.h>
#include <pci/pci.h>

#include "libhsakmt.h"
#include "fmm.h"

/* Number of memory banks added by thunk on top of topology */
#define NUM_OF_IGPU_HEAPS 3
#define NUM_OF_DGPU_HEAPS 3
/* SYSFS related */
#define KFD_SYSFS_PATH_GENERATION_ID "/sys/devices/virtual/kfd/kfd/topology/generation_id"
#define KFD_SYSFS_PATH_SYSTEM_PROPERTIES "/sys/devices/virtual/kfd/kfd/topology/system_properties"
#define KFD_SYSFS_PATH_NODES "/sys/devices/virtual/kfd/kfd/topology/nodes"
#define PROC_CPUINFO_PATH "/proc/cpuinfo"

typedef struct {
	uint32_t gpu_id;
	HsaNodeProperties node;
	HsaMemoryProperties *mem;     /* node->NumBanks elements */
	HsaCacheProperties *cache;
	HsaIoLinkProperties *link;
	int drm_render_fd;
} node_t;

static HsaSystemProperties *_system = NULL;
static node_t *node = NULL;
static int is_valgrind;

static int processor_vendor;
/* Supported System Vendors */
enum SUPPORTED_PROCESSOR_VENDORS {
	GENUINE_INTEL = 0,
	AUTHENTIC_AMD
};
/* Adding newline to make the search easier */
static const char *supported_processor_vendor_name[] = {
	"GenuineIntel\n",
	"AuthenticAMD\n"
};

static HSAKMT_STATUS topology_take_snapshot(void);
static HSAKMT_STATUS topology_drop_snapshot(void);

static struct hsa_gfxip_table {
	uint16_t device_id;		// Device ID
	unsigned char major;		// GFXIP Major engine version
	unsigned char minor;		// GFXIP Minor engine version
	unsigned char stepping;		// GFXIP Stepping info
	unsigned char is_dgpu;		// Predicate for dGPU devices
	const char *amd_name;		// CALName of the device
	enum asic_family_type asic_family;
} gfxip_lookup_table[] = {
	/* Kaveri Family */
	{ 0x1304, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x1305, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x1306, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x1307, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x1309, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x130A, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x130B, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x130C, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x130D, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x130E, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x130F, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x1310, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x1311, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x1312, 7, 0, 0, 0, "Spooky", CHIP_KAVERI },
	{ 0x1313, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x1315, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x1316, 7, 0, 0, 0, "Spooky", CHIP_KAVERI },
	{ 0x1317, 7, 0, 0, 0, "Spooky", CHIP_KAVERI },
	{ 0x1318, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x131B, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x131C, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	{ 0x131D, 7, 0, 0, 0, "Spectre", CHIP_KAVERI },
	/* Hawaii Family */
	{ 0x67A0, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67A1, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67A2, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67A8, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67A9, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67AA, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67B0, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67B1, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67B8, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67B9, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67BA, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	{ 0x67BE, 7, 0, 1, 1, "Hawaii", CHIP_HAWAII },
	/* Carrizo Family */
	{ 0x9870, 8, 0, 1, 0, "Carrizo", CHIP_CARRIZO },
	{ 0x9874, 8, 0, 1, 0, "Carrizo", CHIP_CARRIZO },
	{ 0x9875, 8, 0, 1, 0, "Carrizo", CHIP_CARRIZO },
	{ 0x9876, 8, 0, 1, 0, "Carrizo", CHIP_CARRIZO },
	{ 0x9877, 8, 0, 1, 0, "Carrizo", CHIP_CARRIZO },
	/* Tonga Family */
	{ 0x6920, 8, 0, 2, 1, "Tonga", CHIP_TONGA },
	{ 0x6921, 8, 0, 2, 1, "Tonga", CHIP_TONGA },
	{ 0x6928, 8, 0, 2, 1, "Tonga", CHIP_TONGA },
	{ 0x6929, 8, 0, 2, 1, "Tonga", CHIP_TONGA },
	{ 0x692B, 8, 0, 2, 1, "Tonga", CHIP_TONGA },
	{ 0x692F, 8, 0, 2, 1, "Tonga", CHIP_TONGA },
	{ 0x6930, 8, 0, 2, 1, "Tonga", CHIP_TONGA },
	{ 0x6938, 8, 0, 2, 1, "Tonga", CHIP_TONGA },
	{ 0x6939, 8, 0, 2, 1, "Tonga", CHIP_TONGA },
	/* Fiji */
	{ 0x7300, 8, 0, 3, 1, "Fiji", CHIP_FIJI },
	{ 0x730F, 8, 0, 3, 1, "Fiji", CHIP_FIJI },
	/* Polaris10 */
	{ 0x67C0, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67C1, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67C2, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67C4, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67C7, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67C8, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67C9, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67CA, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67CC, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67CF, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67D0, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	{ 0x67DF, 8, 0, 3, 1, "Polaris10", CHIP_POLARIS10 },
	/* Polaris11 */
	{ 0x67E0, 8, 0, 3, 1, "Polaris11", CHIP_POLARIS11 },
	{ 0x67E1, 8, 0, 3, 1, "Polaris11", CHIP_POLARIS11 },
	{ 0x67E3, 8, 0, 3, 1, "Polaris11", CHIP_POLARIS11 },
	{ 0x67E7, 8, 0, 3, 1, "Polaris11", CHIP_POLARIS11 },
	{ 0x67E8, 8, 0, 3, 1, "Polaris11", CHIP_POLARIS11 },
	{ 0x67E9, 8, 0, 3, 1, "Polaris11", CHIP_POLARIS11 },
	{ 0x67EB, 8, 0, 3, 1, "Polaris11", CHIP_POLARIS11 },
	{ 0x67EF, 8, 0, 3, 1, "Polaris11", CHIP_POLARIS11 },
	{ 0x67FF, 8, 0, 3, 1, "Polaris11", CHIP_POLARIS11 },
	/* Vega10 */
	{ 0x6860, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
	{ 0x6861, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
	{ 0x6862, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
	{ 0x6863, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
	{ 0x6864, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
	{ 0x6867, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
	{ 0x6868, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
	{ 0x686C, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
	{ 0x687F, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
	/* Raven */
	{ 0x15DD, 9, 0, 2, 0, "Raven", CHIP_RAVEN },
	/* Vega20 on emulator, treat it as vega10 */
	{ 0x66A0, 9, 0, 0, 1, "Vega10", CHIP_VEGA10 },
};

enum cache_type {
	CACHE_TYPE_NULL = 0,
	CACHE_TYPE_DATA = 1,
	CACHE_TYPE_INST = 2,
	CACHE_TYPE_UNIFIED = 3
};

typedef struct cacheinfo {
	HsaCacheProperties hsa_cache_prop;
	uint32_t num_threads_sharing; /* how many CPUs share this cache */
} cacheinfo_t;

/* CPU cache table for all CPUs on the system. Each entry has the relative CPU
 * info and caches connected to that CPU.
 */
typedef struct cpu_cacheinfo {
	uint32_t len; /* length of the table -> number of online procs */
	uint32_t num_caches; /* number of caches connected to this cpu */
	uint32_t num_duplicated_caches; /* to count caches being shared */
	uint32_t apicid; /* this cpu's apic id */
	uint32_t max_num_apicid; /* max number of addressable IDs */
	cacheinfo_t *cache_info; /* an array for cache information */
} cpu_cacheinfo_t;

/* Deterministic Cache Parameters Leaf in cpuid */
union _cpuid_leaf_eax { /* Register EAX */
	struct {
		enum cache_type	type:5;
		uint32_t	level:3;
		uint32_t	is_self_initializing:1;
		uint32_t	is_fully_associative:1;
		uint32_t	reserved:4;
		uint32_t	num_threads_sharing:12;
		uint32_t	num_cores_on_die:6;
	} split;
	uint32_t full;
};

union _cpuid_leaf_ebx { /* Register EBX */
	struct {
		uint32_t	coherency_line_size:12;
		uint32_t	physical_line_partition:10;
		uint32_t	ways_of_associativity:10;
	} split;
	uint32_t full;
};

static void
free_node(node_t *n)
{
	assert(n);

	if (!n)
		return;

	if ((n)->mem)
		free((n)->mem);
	if ((n)->cache)
		free((n)->cache);
	if ((n)->link)
		free((n)->link);
	if ((n)->drm_render_fd > 0)
		close((n)->drm_render_fd);
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
	if (dirp) {
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

#if defined(__x86_64__) || defined(__i386__)
/* cpuid instruction returns processor identification and feature information
 * to the EAX, EBX, ECX, and EDX registers, as determined by input entered in
 * EAX (in some cases, ECX as well).
 */
static inline void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
			 uint32_t *edx)
{
	__asm__ __volatile__(
		"cpuid;"
		: "=a" (*eax),
		  "=b" (*ebx),
		  "=c" (*ecx),
		  "=d" (*edx)
		: "0" (*eax), "2" (*ecx)
		: "memory"
	);
}

/* In cases ECX is also used as an input for cpuid, i.e. cache leaf */
static void cpuid_count(uint32_t op, int count, uint32_t *eax, uint32_t *ebx,
			uint32_t *ecx, uint32_t *edx)
{
	*eax = op;
	*ecx = count;
	cpuid(eax, ebx, ecx, edx);
}

/* Lock current process to the specified processor */
static int lock_to_processor(int processor)
{
	cpu_set_t cpuset;

	memset(&cpuset, 0, sizeof(cpu_set_t));
	CPU_SET(processor, &cpuset);
	/* 0: this process */
	return sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

/* Get count's order of 2. In other words, 2^rtn_value = count
 * When count is not an order of 2, round it up to the closest.
 */
static int get_count_order(unsigned int count)
{
	int bit;
	uint32_t num;

	for (bit = 31; bit >= 0; bit--) {
		num = 1 << bit;
		if (count >= num)
			break;
	}
	if (count & (count - 1))
		++bit;

	return bit;
}

/* cpuid_find_num_cache_leaves - Use cpuid instruction to find out how many
 *		cache leaves the CPU has.
 *	@op - cpuid opcode to get cache information
 *	Return - the number of cache leaves
 */
static int cpuid_find_num_cache_leaves(uint32_t op)
{
	union _cpuid_leaf_eax eax;
	union _cpuid_leaf_ebx ebx;
	unsigned int ecx;
	unsigned int edx;
	int idx = -1;

	do {
		++idx;
		cpuid_count(op, idx, &eax.full, &ebx.full, &ecx, &edx);
		/* Modern systems have cache levels up to 3. */
	} while (eax.split.type != CACHE_TYPE_NULL && idx < 4);
	return idx;
}

/* cpuid_get_cpu_cache_info - Use cpuid instruction to get cache information
 *	@op - cpuid opcode to get cache information
 *	@cpu_ci - this parameter is an input and also an output.
 *		  [IN] cpu_ci->num_caches: the number of caches of this cpu
 *		  [OUT] cpu_ci->cache_info: to store cache info collected
 */
static void cpuid_get_cpu_cache_info(uint32_t op, cpu_cacheinfo_t *cpu_ci)
{
	union _cpuid_leaf_eax eax;
	union _cpuid_leaf_ebx ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t index;
	cacheinfo_t *this_leaf;

	for (index = 0; index < cpu_ci->num_caches; index++) {
		cpuid_count(op, index, &eax.full, &ebx.full, &ecx, &edx);
		this_leaf = cpu_ci->cache_info + index;
		this_leaf->hsa_cache_prop.ProcessorIdLow = cpu_ci->apicid;
		this_leaf->num_threads_sharing =
				eax.split.num_threads_sharing + 1;
		this_leaf->hsa_cache_prop.CacheLevel = eax.split.level;
		this_leaf->hsa_cache_prop.CacheType.ui32.CPU = 1;
		if (eax.split.type & CACHE_TYPE_DATA)
			this_leaf->hsa_cache_prop.CacheType.ui32.Data = 1;
		if (eax.split.type & CACHE_TYPE_INST)
			this_leaf->hsa_cache_prop.CacheType.ui32.Instruction = 1;
		this_leaf->hsa_cache_prop.CacheLineSize =
				ebx.split.coherency_line_size + 1;
		this_leaf->hsa_cache_prop.CacheAssociativity =
				ebx.split.ways_of_associativity + 1;
		this_leaf->hsa_cache_prop.CacheLinesPerTag =
				ebx.split.physical_line_partition + 1;
		this_leaf->hsa_cache_prop.CacheSize = (ecx + 1) *
				(ebx.split.coherency_line_size	   + 1) *
				(ebx.split.physical_line_partition + 1) *
				(ebx.split.ways_of_associativity   + 1);
	}
}

/* find_cpu_cache_siblings - In the cache list, some caches may be listed more
 *	than once if they are shared by multiple CPUs. Identify the cache's CPU
 *	siblings, record it to SiblingMap[], then remove the duplicated cache by
 *	changing the cache size to 0.
 */
static void find_cpu_cache_siblings(cpu_cacheinfo_t *cpu_ci_list)
{
	cacheinfo_t *this_leaf, *leaf2;
	uint32_t n, j, idx_msb, apicid1, apicid2;
	cpu_cacheinfo_t *this_cpu, *cpu2;
	uint32_t index;

	/* FixMe: cpuid under Valgrind doesn't return data from the processor we set
	 * affinity to. We can't use that data to calculate siblings.
	 */
	if (is_valgrind)
		return;

	for (n = 0; n < cpu_ci_list->len; n++) {
		this_cpu = cpu_ci_list + n;
		for (index = 0; index < this_cpu->num_caches; index++) {
			this_leaf = this_cpu->cache_info + index;
			/* CacheSize 0 means an invalid cache */
			if (!this_leaf->hsa_cache_prop.CacheSize)
				continue;
			if (this_leaf->num_threads_sharing == 1) // no siblings
				continue;
			idx_msb = get_count_order(this_leaf->num_threads_sharing);
			for (j = n + 1; j < cpu_ci_list->len; j++) {
				cpu2 = cpu_ci_list + j;
				leaf2 = cpu2->cache_info + index;
				apicid1 = this_leaf->hsa_cache_prop.ProcessorIdLow;
				apicid2 = leaf2->hsa_cache_prop.ProcessorIdLow;
				if ((apicid2 >> idx_msb) != (apicid1 >> idx_msb))
					continue;
				/* A sibling leaf is found. Cache properties
				 * use ProcIdLow as offset to represent siblings
				 * in SiblingMap, so keep the lower apicid and
				 * delete the other by changing CacheSize to 0.
				 */
				if (apicid1 < apicid2) {
					this_leaf->hsa_cache_prop.SiblingMap[0] = 1;
					this_leaf->hsa_cache_prop.SiblingMap[apicid2 - apicid1] = 1;
					leaf2->hsa_cache_prop.CacheSize = 0;
					cpu2->num_duplicated_caches++;
				} else {
					leaf2->hsa_cache_prop.SiblingMap[0] = 1;
					leaf2->hsa_cache_prop.SiblingMap[apicid1 - apicid2] = 1;
					this_leaf->hsa_cache_prop.CacheSize = 0;
					this_cpu->num_duplicated_caches++;
				}
			}
		}
	}
}
#endif /* X86 platform */

static HSAKMT_STATUS topology_sysfs_get_generation(uint32_t *gen)
{
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

HSAKMT_STATUS topology_sysfs_get_system_props(HsaSystemProperties *props)
{
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	unsigned long long prop_val;
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
	if (read_size >= PAGE_SIZE)
		read_size = PAGE_SIZE - 1;
	read_buf[read_size] = 0;

	/* Read the system properties */
	prog = 0;
	p = read_buf;
	while (sscanf(p += prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name, "platform_oem") == 0)
			props->PlatformOem = (uint32_t)prop_val;
		else if (strcmp(prop_name, "platform_id") == 0)
			props->PlatformId = (uint32_t)prop_val;
		else if (strcmp(prop_name, "platform_rev") == 0)
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

HSAKMT_STATUS topology_sysfs_get_gpu_id(uint32_t node_id, uint32_t *gpu_id)
{
	FILE *fd;
	char path[256];
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(gpu_id);
	snprintf(path, 256, "%s/%d/gpu_id", KFD_SYSFS_PATH_NODES, node_id);
	fd = fopen(path, "r");
	if (!fd)
		return HSAKMT_STATUS_ERROR;
	if (fscanf(fd, "%ul", gpu_id) != 1)
		ret = HSAKMT_STATUS_ERROR;
	fclose(fd);

	return ret;
}

static const struct hsa_gfxip_table *find_hsa_gfxip_device(uint16_t device_id)
{
	uint32_t i, table_size;

	table_size = sizeof(gfxip_lookup_table)/sizeof(struct hsa_gfxip_table);
	for (i = 0; i < table_size; i++) {
		if (gfxip_lookup_table[i].device_id == device_id)
			return &gfxip_lookup_table[i];
	}
	return NULL;
}

HSAKMT_STATUS topology_get_asic_family(uint16_t device_id,
					enum asic_family_type *asic)
{
	const struct hsa_gfxip_table *hsa_gfxip =
				find_hsa_gfxip_device(device_id);

	if (!hsa_gfxip)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	*asic = hsa_gfxip->asic_family;
	return HSAKMT_STATUS_SUCCESS;
}

bool topology_is_dgpu(uint16_t device_id)
{
	const struct hsa_gfxip_table *hsa_gfxip =
				find_hsa_gfxip_device(device_id);

	if (hsa_gfxip && hsa_gfxip->is_dgpu) {
		is_dgpu = true;
		return true;
	}
	is_dgpu = false;
	return false;
}

bool topology_is_svm_needed(uint16_t device_id)
{
	const struct hsa_gfxip_table *hsa_gfxip;

	if (topology_is_dgpu(device_id))
		return true;

	hsa_gfxip = find_hsa_gfxip_device(device_id);

	if (hsa_gfxip && hsa_gfxip->asic_family >= CHIP_VEGA10)
		return true;

	return false;
}

static HSAKMT_STATUS topology_get_cpu_model_name(HsaNodeProperties *props,
						 bool is_apu)
{
	FILE *fd;
	char read_buf[256], cpu_model_name[HSA_PUBLIC_NAME_SIZE];
	const char *p;
	uint32_t i = 0, apic_id = 0;

	if (!props)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	fd = fopen(PROC_CPUINFO_PATH, "r");
	if (!fd) {
		pr_err("Failed to open [%s]. Unable to get CPU Model Name",
			PROC_CPUINFO_PATH);
		return HSAKMT_STATUS_ERROR;
	}

	while (fgets(read_buf, sizeof(read_buf), fd)) {
		/* Get the model name first, in case matching
		 * apic IDs are also present in the file
		 */
		if (!strncmp("model name", read_buf, sizeof("model name") - 1)) {
			p = strrchr(read_buf, ':');
			if (!p)
				goto err;

			p++; // remove separator ':'
			for (; isspace(*p); p++)
				; /* remove white space */

			/* Extract model name from string */
			for (i = 0; i < sizeof(cpu_model_name) - 1 && p[i] != '\n'; i++)
				cpu_model_name[i] = p[i];
			cpu_model_name[i] = '\0';
		}

		if (!strncmp("apicid", read_buf, sizeof("apicid") - 1)) {
			p = strrchr(read_buf, ':');
			if (!p)
				goto err;

			p++; // remove separator ':'
			for (; isspace(*p); p++)
				; /* remove white space */

			/* Extract apic_id from remaining chars */
			apic_id = atoi(p);

			/* Set CPU model name only if corresponding apic id */
			if (props->CComputeIdLo == apic_id) {
				/* Retrieve the CAL name of CPU node */
				if (!is_apu)
					strncpy((char *)props->AMDName, cpu_model_name, sizeof(props->AMDName));
				/* Convert from UTF8 to UTF16 */
				for (i = 0; cpu_model_name[i] != '\0' && i < HSA_PUBLIC_NAME_SIZE - 1; i++)
					props->MarketingName[i] = cpu_model_name[i];
				props->MarketingName[i] = '\0';
			}
		}
	}
	fclose(fd);
	return HSAKMT_STATUS_SUCCESS;
err:
	fclose(fd);
	return HSAKMT_STATUS_ERROR;
}

static int topology_search_processor_vendor(const char *processor_name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_LEN(supported_processor_vendor_name); i++) {
		if (!strcmp(processor_name, supported_processor_vendor_name[i]))
			return i;
	}
	return -1;
}

/* topology_set_processor_vendor - Parse /proc/cpuinfo and
 *  to find processor vendor and set global variable processor_vendor
 *
 *  cat /proc/cpuinfo format is - "token       : Value"
 *  where token = "vendor_id" and
 *        Value = indicates System Vendor
 */
static void topology_set_processor_vendor(void)
{
	FILE *fd;
	char read_buf[256];
	const char *p;

	fd = fopen(PROC_CPUINFO_PATH, "r");
	if (!fd) {
		pr_err("Failed to open [%s]. Setting Processor Vendor to %s",
			PROC_CPUINFO_PATH, supported_processor_vendor_name[GENUINE_INTEL]);
		processor_vendor = GENUINE_INTEL;
		return;
	}

	while (fgets(read_buf, sizeof(read_buf), fd)) {
		if (!strncmp("vendor_id", read_buf, sizeof("vendor_id") - 1)) {
			p = strrchr(read_buf, ':');
			p++; // remove separator ':'
			for (; *p && isspace(*p); p++)
				;	/* remove white space */
			processor_vendor = topology_search_processor_vendor(p);
			if (processor_vendor != -1) {
				fclose(fd);
				return;
			}
		}
	}
	fclose(fd);
	pr_err("Failed to get Processor Vendor. Setting to %s",
		supported_processor_vendor_name[GENUINE_INTEL]);
	processor_vendor = GENUINE_INTEL;
}

HSAKMT_STATUS topology_sysfs_get_node_props(uint32_t node_id,
					    HsaNodeProperties *props,
					    uint32_t *gpu_id,
					    struct pci_access *pacc)
{
	FILE *fd;
	char *read_buf, *p, *envvar, dummy;
	char prop_name[256];
	char path[256];
	unsigned long long prop_val;
	uint32_t i, prog, major, minor, step;
	uint16_t fw_version = 0;
	int read_size;
	const struct hsa_gfxip_table *hsa_gfxip;
	char namebuf[HSA_PUBLIC_NAME_SIZE];
	const char *name;

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
	if (read_size >= PAGE_SIZE)
		read_size = PAGE_SIZE - 1;
	read_buf[read_size] = 0;

	/* Read the node properties */
	prog = 0;
	p = read_buf;
	while (sscanf(p += prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name, "cpu_cores_count") == 0)
			props->NumCPUCores = (uint32_t)prop_val;
		else if (strcmp(prop_name, "simd_count") == 0)
			props->NumFComputeCores = (uint32_t)prop_val;
		else if (strcmp(prop_name, "mem_banks_count") == 0)
			props->NumMemoryBanks = (uint32_t)prop_val;
		else if (strcmp(prop_name, "caches_count") == 0)
			props->NumCaches = (uint32_t)prop_val;
		else if (strcmp(prop_name, "io_links_count") == 0)
			props->NumIOLinks = (uint32_t)prop_val;
		else if (strcmp(prop_name, "cpu_core_id_base") == 0)
			props->CComputeIdLo = (uint32_t)prop_val;
		else if (strcmp(prop_name, "simd_id_base") == 0)
			props->FComputeIdLo = (uint32_t)prop_val;
		else if (strcmp(prop_name, "capability") == 0)
			props->Capability.Value = (uint32_t)prop_val;
		else if (strcmp(prop_name, "max_waves_per_simd") == 0)
			props->MaxWavesPerSIMD = (uint32_t)prop_val;
		else if (strcmp(prop_name, "lds_size_in_kb") == 0)
			props->LDSSizeInKB = (uint32_t)prop_val;
		else if (strcmp(prop_name, "gds_size_in_kb") == 0)
			props->GDSSizeInKB = (uint32_t)prop_val;
		else if (strcmp(prop_name, "wave_front_size") == 0)
			props->WaveFrontSize = (uint32_t)prop_val;
		else if (strcmp(prop_name, "array_count") == 0)
			props->NumShaderBanks = (uint32_t)prop_val;
		else if (strcmp(prop_name, "simd_arrays_per_engine") == 0)
			props->NumArrays = (uint32_t)prop_val;
		else if (strcmp(prop_name, "cu_per_simd_array") == 0)
			props->NumCUPerArray = (uint32_t)prop_val;
		else if (strcmp(prop_name, "simd_per_cu") == 0)
			props->NumSIMDPerCU = (uint32_t)prop_val;
		else if (strcmp(prop_name, "max_slots_scratch_cu") == 0)
			props->MaxSlotsScratchCU = (uint32_t)prop_val;
		else if (strcmp(prop_name, "fw_version") == 0)
			fw_version = (uint16_t)prop_val;
		else if (strcmp(prop_name, "vendor_id") == 0)
			props->VendorId = (uint32_t)prop_val;
		else if (strcmp(prop_name, "device_id") == 0)
			props->DeviceId = (uint32_t)prop_val;
		else if (strcmp(prop_name, "location_id") == 0)
			props->LocationId = (uint32_t)prop_val;
		else if (strcmp(prop_name, "max_engine_clk_fcompute") == 0)
			props->MaxEngineClockMhzFCompute = (uint32_t)prop_val;
		else if (strcmp(prop_name, "max_engine_clk_ccompute") == 0)
			props->MaxEngineClockMhzCCompute = (uint32_t)prop_val;
		else if (strcmp(prop_name, "local_mem_size") == 0)
			props->LocalMemSize = prop_val;
		else if (strcmp(prop_name, "drm_render_minor") == 0)
			props->DrmRenderMinor = (int32_t)prop_val;

	}

	props->EngineId.ui32.uCode = fw_version & 0x3ff;
	props->EngineId.ui32.Major = 0;
	props->EngineId.ui32.Minor = 0;
	props->EngineId.ui32.Stepping = 0;

	hsa_gfxip = find_hsa_gfxip_device(props->DeviceId);
	if (hsa_gfxip) {
		envvar = getenv("HSA_OVERRIDE_GFX_VERSION");
		if (envvar) {
			/* HSA_OVERRIDE_GFX_VERSION=major.minor.stepping */
			if ((sscanf(envvar, "%u.%u.%u%c",
					&major, &minor, &step, &dummy) != 3) ||
				(major > 63 || minor > 255 || step > 255)) {
				pr_err("HSA_OVERRIDE_GFX_VERSION %s is invalid\n",
					envvar);
				ret = HSAKMT_STATUS_ERROR;
				goto err;
			}
			props->EngineId.ui32.Major = major & 0x3f;
			props->EngineId.ui32.Minor = minor & 0xff;
			props->EngineId.ui32.Stepping = step & 0xff;
		} else {
			props->EngineId.ui32.Major = hsa_gfxip->major & 0x3f;
			props->EngineId.ui32.Minor = hsa_gfxip->minor;
			props->EngineId.ui32.Stepping = hsa_gfxip->stepping;
		}

		if (!hsa_gfxip->amd_name) {
			ret = HSAKMT_STATUS_ERROR;
			goto err;
		}

		/* Retrieve the CAL name of the node */
		strncpy((char *)props->AMDName, hsa_gfxip->amd_name, sizeof(props->AMDName));
		if (props->NumCPUCores) {
			/* Is APU node */
			ret = topology_get_cpu_model_name(props, true);
			if (ret != HSAKMT_STATUS_SUCCESS) {
				pr_err("Failed to get APU Model Name from %s\n", PROC_CPUINFO_PATH);
				ret = HSAKMT_STATUS_SUCCESS; /* No hard error, continue regardless */
			}
		} else {
			/* Is dGPU Node
			 * Retrieve the marketing name of the node using pcilib,
			 * convert UTF8 to UTF16
			 */
			name = pci_lookup_name(pacc, namebuf, sizeof(namebuf), PCI_LOOKUP_DEVICE,
								   props->VendorId, props->DeviceId);
			for (i = 0; name[i] != 0 && i < HSA_PUBLIC_NAME_SIZE - 1; i++)
				props->MarketingName[i] = name[i];
			props->MarketingName[i] = '\0';
		}
	} else {
		/* Is CPU Node */
		if (!props->NumFComputeCores || !props->DeviceId) {
			ret = topology_get_cpu_model_name(props, false);
			if (ret != HSAKMT_STATUS_SUCCESS) {
				pr_err("Failed to get CPU Model Name from %s\n", PROC_CPUINFO_PATH);
				ret = HSAKMT_STATUS_SUCCESS; /* No hard error, continue regardless */
			}
		} else {
			ret = HSAKMT_STATUS_ERROR;
			goto err;
		}
	}
	if (props->NumFComputeCores)
		assert(props->EngineId.ui32.Major);

err:
	free(read_buf);
	fclose(fd);
	return ret;
}

static HSAKMT_STATUS topology_sysfs_get_mem_props(uint32_t node_id,
						  uint32_t mem_id,
						  HsaMemoryProperties *props)
{
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	char path[256];
	unsigned long long prop_val;
	uint32_t prog;
	int read_size;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(props);
	snprintf(path, 256, "%s/%d/mem_banks/%d/properties", KFD_SYSFS_PATH_NODES, node_id, mem_id);
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

	/* Since we're using the buffer as a string, we make sure the string terminates */
	if (read_size >= PAGE_SIZE)
		read_size = PAGE_SIZE - 1;
	read_buf[read_size] = 0;

	prog = 0;
	p = read_buf;
	while (sscanf(p += prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name, "heap_type") == 0)
			props->HeapType = (uint32_t)prop_val;
		else if (strcmp(prop_name, "size_in_bytes") == 0)
			props->SizeInBytes = (uint64_t)prop_val;
		else if (strcmp(prop_name, "flags") == 0)
			props->Flags.MemoryProperty = (uint32_t)prop_val;
		else if (strcmp(prop_name, "width") == 0)
			props->Width = (uint32_t)prop_val;
		else if (strcmp(prop_name, "mem_clk_max") == 0)
			props->MemoryClockMax = (uint32_t)prop_val;
	}

err2:
	free(read_buf);
err1:
	fclose(fd);
	return ret;
}

#if defined(__x86_64__) || defined(__i386__)
/* topology_destroy_temp_cpu_cache_list - Free the memory allocated in
 *		topology_create_temp_cpu_cache_list().
 */
static void topology_destroy_temp_cpu_cache_list(void *temp_cpu_ci_list)
{
	uint32_t n;
	cpu_cacheinfo_t *p_temp_cpu_ci_list = (cpu_cacheinfo_t *)temp_cpu_ci_list;
	cpu_cacheinfo_t *this_cpu;

	if (p_temp_cpu_ci_list) {
		for (n = 0; n < p_temp_cpu_ci_list->len; n++) {
			this_cpu = p_temp_cpu_ci_list + n;
			if (this_cpu->cache_info)
				free(this_cpu->cache_info);
		}
		free(p_temp_cpu_ci_list);
	}

	p_temp_cpu_ci_list = NULL;
}

/* topology_create_temp_cpu_cache_list - Create a temporary cpu-cache list to
 *		store cpu cache information. This list will be used to copy
 *		cache information to each CPU node. Must call
 *		topology_destroy_temp_cpu_cache_list to free the memory after
 *		the information is copied.
 *	@temp_cpu_ci_list - [OUT] temporary cpu-cache-info list to store data
 *	Return - HSAKMT_STATUS_SUCCESS in success or error number in failure
 */
static HSAKMT_STATUS topology_create_temp_cpu_cache_list(void **temp_cpu_ci_list)
{
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;
	void *p_temp_cpu_ci_list;
	int procs_online;
	cpu_set_t orig_cpuset;
	int i;
	uint32_t cpuid_op_cache;
	uint32_t eax, ebx, ecx = 0, edx; /* cpuid registers */
	cpu_cacheinfo_t *cpu_ci_list, *this_cpu;

	if (!temp_cpu_ci_list) {
		ret = HSAKMT_STATUS_ERROR;
		goto exit;
	}
	*temp_cpu_ci_list = NULL;

	procs_online = (int)sysconf(_SC_NPROCESSORS_ONLN);
	if (procs_online <= 0) {
		ret = HSAKMT_STATUS_ERROR;
		goto exit;
	}

	p_temp_cpu_ci_list = calloc(sizeof(cpu_cacheinfo_t) * procs_online, 1);
	if (!p_temp_cpu_ci_list) {
		ret = HSAKMT_STATUS_NO_MEMORY;
		goto exit;
	}

	cpu_ci_list = (cpu_cacheinfo_t *)p_temp_cpu_ci_list;
	cpu_ci_list->len = procs_online;

	if (processor_vendor == AUTHENTIC_AMD)
		cpuid_op_cache = 0x8000001d;
	else
		cpuid_op_cache = 0x4;

	/* lock_to_processor() changes the affinity. Save the current affinity
	 * so we can restore it after cpuid is done.
	 */
	CPU_ZERO(&orig_cpuset);
	if (sched_getaffinity(0, sizeof(cpu_set_t), &orig_cpuset) != 0) {
		pr_err("Failed to get CPU affinity\n");
		free(p_temp_cpu_ci_list);
		ret = HSAKMT_STATUS_ERROR;
		goto exit;
	}

	for (i = 0; i < procs_online; i++) {
		this_cpu = cpu_ci_list + i;
		lock_to_processor(i); /* so cpuid is executed in correct cpu */

		eax = 0x1;
		cpuid(&eax, &ebx, &ecx, &edx);
		this_cpu->apicid = (ebx >> 24) & 0xff;
		this_cpu->max_num_apicid = (ebx >> 16) & 0x0FF;
		this_cpu->num_caches = cpuid_find_num_cache_leaves(cpuid_op_cache);
		this_cpu->num_duplicated_caches = 0;
		this_cpu->cache_info = calloc(
				sizeof(cacheinfo_t) * this_cpu->num_caches, 1);
		if (!this_cpu->cache_info) {
			ret = HSAKMT_STATUS_NO_MEMORY;
			goto err;
		}
		cpuid_get_cpu_cache_info(cpuid_op_cache, this_cpu);
	}

	find_cpu_cache_siblings(cpu_ci_list);
	*temp_cpu_ci_list = p_temp_cpu_ci_list;

err:
	/* restore affinity to original */
	sched_setaffinity(0, sizeof(cpu_set_t), &orig_cpuset);
exit:
	if (ret != HSAKMT_STATUS_SUCCESS) {
		pr_warn("Topology fails to create cpu cache list\n");
		topology_destroy_temp_cpu_cache_list(*temp_cpu_ci_list);
	}
	return ret;
}

/* topology_get_cpu_cache_props - Read CPU cache information from the temporary
 *		cache list and put them to the node's cache properties entry.
 *	@tbl - the node table to fill up
 *	@cpu_ci_list - the cpu cache information list to look up cache info
 *	Return - HSAKMT_STATUS_SUCCESS in success or error number in failure
 */
static HSAKMT_STATUS topology_get_cpu_cache_props(node_t *tbl,
						  cpu_cacheinfo_t *cpu_ci_list)
{
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;
	uint32_t apicid_low = tbl->node.CComputeIdLo, apicid_max = 0;
	uint32_t n, cache_cnt, idx;
	cpu_cacheinfo_t *this_cpu;
	cacheinfo_t *this_leaf;

	/* CPU cache info list contains all CPUs. Find out CPUs belonging to
	 * this node and number of caches under, so we can allocate the cache
	 * properties in the node.
	 */
	tbl->node.NumCaches = 0;
	for (n = 0; n < cpu_ci_list->len; n++) {
		this_cpu = cpu_ci_list + n;
		if (this_cpu->apicid == apicid_low)
			/* found the first cpu in the node */
			apicid_max = apicid_low + this_cpu->max_num_apicid - 1;

		if ((this_cpu->apicid < apicid_low) ||
			(this_cpu->apicid > apicid_max))
			continue; /* this cpu doesn't belong to the node */
		tbl->node.NumCaches +=
			this_cpu->num_caches - this_cpu->num_duplicated_caches;
	}

	/* FixMe: cpuid under Valgrind doesn't return data from the processor we set
	 * affinity to. All the data come from one specific processor. We'll report
	 * this one processor's cache and ignore others.
	 */
	if (is_valgrind) {
		this_cpu = cpu_ci_list;
		tbl->node.NumCaches = this_cpu->num_caches;
		apicid_low = apicid_max = this_cpu->apicid;
	}

	tbl->cache = calloc(
			sizeof(HsaCacheProperties) * tbl->node.NumCaches, 1);
	if (!tbl->cache) {
		ret = HSAKMT_STATUS_NO_MEMORY;
		goto exit;
	}

	/* Now fill in the information to cache properties. */
	cache_cnt = 0;
	for (n = 0; n < cpu_ci_list->len; n++) {
		this_cpu = cpu_ci_list + n;
		if ((this_cpu->apicid < apicid_low) || this_cpu->apicid > apicid_max)
			continue; /* this cpu doesn't belong to the node */
		for (idx = 0; idx < this_cpu->num_caches; idx++) {
			this_leaf = this_cpu->cache_info + idx;
			if (this_leaf->hsa_cache_prop.CacheSize > 0)
				memcpy(&tbl->cache[cache_cnt++], &this_leaf->hsa_cache_prop, sizeof(HsaCacheProperties));
			if (cache_cnt >= tbl->node.NumCaches)
				goto exit;
		}
	}

exit:
	return ret;
}
#else /* not X86 */
static void topology_destroy_temp_cpu_cache_list(void *temp_cpu_ci_list)
{
}

static HSAKMT_STATUS topology_create_temp_cpu_cache_list(void **temp_cpu_ci_list)
{
	return HSAKMT_STATUS_SUCCESS;
}

static HSAKMT_STATUS topology_get_cpu_cache_props(node_t *tbl,
						  cpu_cacheinfo_t *cpu_ci_list)
{
	return HSAKMT_STATUS_SUCCESS;
}
#endif

static HSAKMT_STATUS topology_sysfs_get_cache_props(uint32_t node_id,
						    uint32_t cache_id,
						    HsaCacheProperties *props)
{
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	char path[256];
	unsigned long long prop_val;
	uint32_t i, prog;
	int read_size;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(props);
	snprintf(path, 256, "%s/%d/caches/%d/properties", KFD_SYSFS_PATH_NODES, node_id, cache_id);
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

	/* Since we're using the buffer as a string, we make sure the string terminates */
	if (read_size >= PAGE_SIZE)
		read_size = PAGE_SIZE - 1;
	read_buf[read_size] = 0;

	prog = 0;
	p = read_buf;
	while (sscanf(p += prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name, "processor_id_low") == 0)
			props->ProcessorIdLow = (uint32_t)prop_val;
		else if (strcmp(prop_name, "level") == 0)
			props->CacheLevel = (uint32_t)prop_val;
		else if (strcmp(prop_name, "size") == 0)
			props->CacheSize = (uint32_t)prop_val;
		else if (strcmp(prop_name, "cache_line_size") == 0)
			props->CacheLineSize = (uint32_t)prop_val;
		else if (strcmp(prop_name, "cache_lines_per_tag") == 0)
			props->CacheLinesPerTag = (uint32_t)prop_val;
		else if (strcmp(prop_name, "association") == 0)
			props->CacheAssociativity = (uint32_t)prop_val;
		else if (strcmp(prop_name, "latency") == 0)
			props->CacheLatency = (uint32_t)prop_val;
		else if (strcmp(prop_name, "type") == 0)
			props->CacheType.Value = (uint32_t)prop_val;
		else if (strcmp(prop_name, "sibling_map") == 0)
			break;
	}

	prog = 0;
	if ((sscanf(p, "sibling_map %n", &prog)) == 0 && prog) {
		i = 0;
		while ((i < HSA_CPU_SIBLINGS) &&
			(sscanf(p += prog, "%u%*[,\n]%n", &props->SiblingMap[i++], &prog) == 1))
			continue;
	}

err2:
	free(read_buf);
err1:
	fclose(fd);
	return ret;
}

static HSAKMT_STATUS topology_sysfs_get_iolink_props(uint32_t node_id,
						     uint32_t iolink_id,
						     HsaIoLinkProperties *props)
{
	FILE *fd;
	char *read_buf, *p;
	char prop_name[256];
	char path[256];
	unsigned long long prop_val;
	uint32_t prog;
	int read_size;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;

	assert(props);
	snprintf(path, 256, "%s/%d/io_links/%d/properties", KFD_SYSFS_PATH_NODES, node_id, iolink_id);
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

	/* Since we're using the buffer as a string, we make sure the string terminates */
	if (read_size >= PAGE_SIZE)
		read_size = PAGE_SIZE - 1;
	read_buf[read_size] = 0;

	prog = 0;
	p = read_buf;
	while (sscanf(p += prog, "%s %llu\n%n", prop_name, &prop_val, &prog) == 2) {
		if (strcmp(prop_name, "type") == 0)
			props->IoLinkType = (uint32_t)prop_val;
		else if (strcmp(prop_name, "version_major") == 0)
			props->VersionMajor = (uint32_t)prop_val;
		else if (strcmp(prop_name, "version_minor") == 0)
			props->VersionMinor = (uint32_t)prop_val;
		else if (strcmp(prop_name, "node_from") == 0)
			props->NodeFrom = (uint32_t)prop_val;
		else if (strcmp(prop_name, "node_to") == 0)
			props->NodeTo = (uint32_t)prop_val;
		else if (strcmp(prop_name, "weight") == 0)
			props->Weight = (uint32_t)prop_val;
		else if (strcmp(prop_name, "min_latency") == 0)
			props->MinimumLatency = (uint32_t)prop_val;
		else if (strcmp(prop_name, "max_latency") == 0)
			props->MaximumLatency = (uint32_t)prop_val;
		else if (strcmp(prop_name, "min_bandwidth") == 0)
			props->MinimumBandwidth = (uint32_t)prop_val;
		else if (strcmp(prop_name, "max_bandwidth") == 0)
			props->MaximumBandwidth = (uint32_t)prop_val;
		else if (strcmp(prop_name, "recommended_transfer_size") == 0)
			props->RecTransferSize = (uint32_t)prop_val;
		else if (strcmp(prop_name, "flags") == 0)
			props->Flags.LinkProperty = (uint32_t)prop_val;
	}


err2:
	free(read_buf);
err1:
	fclose(fd);
	return ret;
}

/* topology_get_free_io_link_slot_for_node - For the given node_id, find the
 * next available free slot to add an io_link
 */
static HsaIoLinkProperties *topology_get_free_io_link_slot_for_node(uint32_t node_id,
								    const HsaSystemProperties *sys_props,
								    node_t *nodes)
{
	HsaIoLinkProperties *props;

	if (node_id >= sys_props->NumNodes) {
		pr_err("Invalid node [%d]\n", node_id);
		return NULL;
	}

	props = nodes[node_id].link;
	if (!props) {
		pr_err("No io_link reported for Node [%d]\n", node_id);
		return NULL;
	}

	if (nodes[node_id].node.NumIOLinks >= sys_props->NumNodes - 1) {
		pr_err("No more space for io_link for Node [%d]\n", node_id);
		return NULL;
	}

	return &props[nodes[node_id].node.NumIOLinks];
}

/* topology_add_io_link_for_node - If a free slot is available,
 * add io_link for the given Node. If bi_directional is true, set up two
 * links for both directions.
 * TODO: Add other members of HsaIoLinkProperties
 */
static HSAKMT_STATUS topology_add_io_link_for_node(uint32_t node_id,
						   const HsaSystemProperties *sys_props,
						   node_t *nodes,
						   HSA_IOLINKTYPE IoLinkType,
						   uint32_t NodeTo,
						   uint32_t Weight, bool bi_dir)
{
	HsaIoLinkProperties *props;
	/* If bi-directional is set true, it's two links to add. */
	uint32_t i, num_links = (bi_dir == true) ? 2 : 1;
	uint32_t node_from = node_id, node_to = NodeTo;

	for (i = 0; i < num_links; i++) {
		props = topology_get_free_io_link_slot_for_node(node_from,
			sys_props, nodes);
		if (!props)
			return HSAKMT_STATUS_NO_MEMORY;

		props->IoLinkType = IoLinkType;
		props->NodeFrom = node_from;
		props->NodeTo = node_to;
		props->Weight = Weight;
		nodes[node_from].node.NumIOLinks++;
		/* switch direction on the 2nd link when num_links=2 */
		node_from = NodeTo;
		node_to = node_id;
	}

	return HSAKMT_STATUS_SUCCESS;
}

/* Find the CPU that this GPU (gpu_node) directly connects to */
static int32_t gpu_get_direct_link_cpu(uint32_t gpu_node, node_t *nodes)
{
	HsaIoLinkProperties *props = nodes[gpu_node].link;
	uint32_t i;

	if (!nodes[gpu_node].gpu_id || !props ||
			nodes[gpu_node].node.NumIOLinks == 0)
		return -1;

	for (i = 0; i < nodes[gpu_node].node.NumIOLinks; i++)
		if (props[i].IoLinkType == HSA_IOLINKTYPE_PCIEXPRESS &&
			props[i].Weight <= 20) /* >20 is GPU->CPU->GPU */
			return props[i].NodeTo;

	return -1;
}

/* Get node1->node2 IO link information. This should be a direct link that has
 * been created in the kernel.
 */
static HSAKMT_STATUS get_direct_iolink_info(uint32_t node1, uint32_t node2,
					    node_t *nodes, HSAuint32 *weight,
					    HSA_IOLINKTYPE *type)
{
	HsaIoLinkProperties *props = nodes[node1].link;
	uint32_t i;

	if (!props)
		return HSAKMT_STATUS_INVALID_NODE_UNIT;

	for (i = 0; i < nodes[node1].node.NumIOLinks; i++)
		if (props[i].NodeTo == node2) {
			if (weight)
				*weight = props[i].Weight;
			if (type)
				*type = props[i].IoLinkType;
			return HSAKMT_STATUS_SUCCESS;
		}

	return HSAKMT_STATUS_INVALID_PARAMETER;
}

static HSAKMT_STATUS get_indirect_iolink_info(uint32_t node1, uint32_t node2,
					      node_t *nodes, HSAuint32 *weight,
					      HSA_IOLINKTYPE *type)
{
	int32_t dir_cpu1 = -1, dir_cpu2 = -1;
	HSAuint32 weight1 = 0, weight2 = 0, weight3 = 0;
	HSAKMT_STATUS ret;

	*weight = 0;
	*type = HSA_IOLINKTYPE_UNDEFINED;

	if (node1 == node2)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	/* CPU->CPU is not an indirect link */
	if (!nodes[node1].gpu_id && !nodes[node2].gpu_id)
		return HSAKMT_STATUS_INVALID_NODE_UNIT;

	if (nodes[node1].gpu_id)
		dir_cpu1 = gpu_get_direct_link_cpu(node1, nodes);
	if (nodes[node2].gpu_id)
		dir_cpu2 = gpu_get_direct_link_cpu(node2, nodes);

	if (dir_cpu1 < 0 && dir_cpu2 < 0)
		return HSAKMT_STATUS_ERROR;

	/* Possible topology:
	 *   GPU --(weight1) -- CPU -- (weight2) -- GPU
	 *   GPU --(weight1) -- CPU -- (weight2) -- CPU -- (weight3) -- GPU
	 *   GPU --(weight1) -- CPU -- (weight2) -- CPU
	 *   CPU -- (weight2) -- CPU -- (weight3) -- GPU
	 */
	if (dir_cpu1 >= 0) { /* GPU->CPU ... */
		if (dir_cpu2 >= 0) {
			if (dir_cpu1 == dir_cpu2) /* GPU->CPU->GPU*/ {
				ret = get_direct_iolink_info(node1, dir_cpu1,
						nodes, &weight1, NULL);
				if (ret != HSAKMT_STATUS_SUCCESS)
					return ret;
				ret = get_direct_iolink_info(dir_cpu1, node2,
						nodes, &weight2, type);
			} else /* GPU->CPU->CPU->GPU*/ {
				ret = get_direct_iolink_info(node1, dir_cpu1,
						nodes, &weight1, NULL);
				if (ret != HSAKMT_STATUS_SUCCESS)
					return ret;
				ret = get_direct_iolink_info(dir_cpu1, dir_cpu2,
						nodes, &weight2, type);
				if (ret != HSAKMT_STATUS_SUCCESS)
					return ret;
				/* On QPI interconnection, GPUs can't access
				 * each other if they are attached to different
				 * CPU sockets. CPU<->CPU weight larger than 20
				 * means the two CPUs are in different sockets.
				 */
				if (*type == HSA_IOLINK_TYPE_QPI_1_1
					&& weight2 > 20)
					return HSAKMT_STATUS_NOT_SUPPORTED;
				ret = get_direct_iolink_info(dir_cpu2, node2,
						nodes, &weight3, NULL);
			}
		} else /* GPU->CPU->CPU */ {
			ret = get_direct_iolink_info(node1, dir_cpu1, nodes,
							&weight1, NULL);
			if (ret != HSAKMT_STATUS_SUCCESS)
				return ret;
			ret = get_direct_iolink_info(dir_cpu1, node2, nodes,
							&weight2, type);
		}
	} else { /* CPU->CPU->GPU */
		ret = get_direct_iolink_info(node1, dir_cpu2, nodes, &weight2,
					type);
		if (ret != HSAKMT_STATUS_SUCCESS)
			return ret;
		ret = get_direct_iolink_info(dir_cpu2, node2, nodes, &weight3,
						NULL);
	}

	if (ret != HSAKMT_STATUS_SUCCESS)
		return ret;

	*weight = weight1 + weight2 + weight3;
	return HSAKMT_STATUS_SUCCESS;
}

static void topology_create_indirect_gpu_links(const HsaSystemProperties *sys_props,
					       node_t *nodes)
{

	uint32_t i, j;
	HSAuint32 weight;
	HSA_IOLINKTYPE type;

	for (i = 0; i < sys_props->NumNodes - 1; i++) {
		for (j = i + 1; j < sys_props->NumNodes; j++) {
			get_indirect_iolink_info(i, j, nodes, &weight, &type);
			if (!weight)
				continue;
			if (topology_add_io_link_for_node(i, sys_props, nodes,
				type, j, weight, true) != HSAKMT_STATUS_SUCCESS)
				pr_err("Fail to add IO link %d->%d\n", i, j);
		}
	}
}


static void open_drm_render_device(node_t *n)
{
	int minor = n->node.DrmRenderMinor;
	char path[128];

	sprintf(path, "/dev/dri/renderD%d", minor);
	n->drm_render_fd = open(path, O_RDWR | O_CLOEXEC);
}

HSAKMT_STATUS topology_take_snapshot(void)
{
	uint32_t gen_start, gen_end, i, mem_id, cache_id, link_id;
	HsaSystemProperties sys_props;
	node_t *temp_nodes = 0;
	void *cpu_ci_list = NULL;
	HSAKMT_STATUS ret = HSAKMT_STATUS_SUCCESS;
	struct pci_access *pacc;
	char *envvar;

	topology_set_processor_vendor();
	envvar = getenv("HSA_RUNNING_UNDER_VALGRIND");
	if (envvar && !strcmp(envvar, "1"))
		is_valgrind = 1;
	else
		is_valgrind = 0;

retry:
	ret = topology_sysfs_get_generation(&gen_start);
	if (ret != HSAKMT_STATUS_SUCCESS)
		return ret;
	ret = topology_sysfs_get_system_props(&sys_props);
	if (ret != HSAKMT_STATUS_SUCCESS)
		return ret;
	if (sys_props.NumNodes > 0) {
		topology_create_temp_cpu_cache_list(&cpu_ci_list);
		temp_nodes = calloc(sys_props.NumNodes * sizeof(node_t), 1);
		if (!temp_nodes)
			return HSAKMT_STATUS_NO_MEMORY;
		pacc = pci_alloc();
		pci_init(pacc);
		for (i = 0; i < sys_props.NumNodes; i++) {
			ret = topology_sysfs_get_node_props(i,
					&temp_nodes[i].node,
					&temp_nodes[i].gpu_id, pacc);
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
			} else if (!temp_nodes[i].gpu_id) { /* a CPU node */
				ret = topology_get_cpu_cache_props(
						&temp_nodes[i], cpu_ci_list);
				if (ret != HSAKMT_STATUS_SUCCESS) {
					free_nodes(temp_nodes, i + 1);
					goto err;
				}
			}

			/* To simplify, allocate maximum needed memory for io_links for each node. This
			 * removes the need for realloc when indirect and QPI links are added later
			 */
			temp_nodes[i].link = calloc(sys_props.NumNodes - 1, sizeof(HsaIoLinkProperties));
			if (!temp_nodes[i].link) {
				ret = HSAKMT_STATUS_NO_MEMORY;
				free_nodes(temp_nodes, i + 1);
				goto err;
			}

			if (temp_nodes[i].node.NumIOLinks) {
				for (link_id = 0; link_id < temp_nodes[i].node.NumIOLinks; link_id++) {
					ret = topology_sysfs_get_iolink_props(i, link_id, &temp_nodes[i].link[link_id]);
					if (ret != HSAKMT_STATUS_SUCCESS) {
						free_nodes(temp_nodes, i+1);
						goto err;
					}
				}
			}
			open_drm_render_device(&temp_nodes[i]);
		}
		pci_cleanup(pacc);
	}

	/* All direct IO links are created in the kernel. Here we need to
	 * connect GPU<->GPU or GPU<->CPU indirect IO links.
	 */
	topology_create_indirect_gpu_links(&sys_props, temp_nodes);

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
	topology_destroy_temp_cpu_cache_list(cpu_ci_list);
	return ret;
}

/* Drop the Snashot of the HSA topology information. Assume lock is held. */
HSAKMT_STATUS topology_drop_snapshot(void)
{
	HSAKMT_STATUS err;

	if (!!_system != !!node) {
		pr_warn("Probably inconsistency?\n");
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

HSAKMT_STATUS validate_nodeid(uint32_t nodeid, uint32_t *gpu_id)
{
	if (!node || !_system || _system->NumNodes <= nodeid)
		return HSAKMT_STATUS_INVALID_NODE_UNIT;
	if (gpu_id)
		*gpu_id = node[nodeid].gpu_id;

	return HSAKMT_STATUS_SUCCESS;
}

HSAKMT_STATUS gpuid_to_nodeid(uint32_t gpu_id, uint32_t *node_id)
{
	uint64_t node_idx;

	for (node_idx = 0; node_idx < _system->NumNodes; node_idx++) {
		if (node[node_idx].gpu_id == gpu_id) {
			*node_id = node_idx;
			return HSAKMT_STATUS_SUCCESS;
		}
	}

	return HSAKMT_STATUS_INVALID_NODE_UNIT;

}

HSAKMT_STATUS HSAKMTAPI hsaKmtAcquireSystemProperties(HsaSystemProperties *SystemProperties)
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

HSAKMT_STATUS HSAKMTAPI hsaKmtReleaseSystemProperties(void)
{
	CHECK_KFD_OPEN();

	HSAKMT_STATUS err;

	pthread_mutex_lock(&hsakmt_mutex);

	err = topology_drop_snapshot();

	pthread_mutex_unlock(&hsakmt_mutex);

	return err;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtGetNodeProperties(HSAuint32 NodeId,
						HsaNodeProperties *NodeProperties)
{
	HSAKMT_STATUS err;
	uint32_t gpu_id;

	if (!NodeProperties)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_KFD_OPEN();
	pthread_mutex_lock(&hsakmt_mutex);

	/* KFD ADD page 18, snapshot protocol violation */
	if (!_system) {
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

HSAKMT_STATUS HSAKMTAPI hsaKmtGetNodeMemoryProperties(HSAuint32 NodeId,
						      HSAuint32 NumBanks,
						      HsaMemoryProperties *MemoryProperties)
{
	HSAKMT_STATUS err = HSAKMT_STATUS_SUCCESS;
	uint32_t i, gpu_id;
	HSAuint64 aperture_limit;
	bool nodeIsDGPU;

	if (!MemoryProperties)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_KFD_OPEN();
	pthread_mutex_lock(&hsakmt_mutex);

	/* KFD ADD page 18, snapshot protocol violation */
	if (!_system) {
		err = HSAKMT_STATUS_INVALID_NODE_UNIT;
		assert(_system);
		goto out;
	}

	/* Check still necessary */
	if (NodeId >= _system->NumNodes) {
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

	nodeIsDGPU = topology_is_dgpu(get_device_id_by_gpu_id(gpu_id));

	/*Add LDS*/
	if (i < NumBanks &&
		fmm_get_aperture_base_and_limit(FMM_LDS, gpu_id,
				&MemoryProperties[i].VirtualBaseAddress, &aperture_limit) == HSAKMT_STATUS_SUCCESS) {
		MemoryProperties[i].HeapType = HSA_HEAPTYPE_GPU_LDS;
		MemoryProperties[i].SizeInBytes = node[NodeId].node.LDSSizeInKB * 1024;
		i++;
	}

	/* Add Local memory - HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE.
	 * For dGPU the topology node contains Local Memory and it is added by
	 * the for loop above
	 */
	if (!nodeIsDGPU && i < NumBanks && node[NodeId].node.LocalMemSize > 0 &&
		fmm_get_aperture_base_and_limit(FMM_GPUVM, gpu_id,
				&MemoryProperties[i].VirtualBaseAddress, &aperture_limit) == HSAKMT_STATUS_SUCCESS) {
		MemoryProperties[i].HeapType = HSA_HEAPTYPE_FRAME_BUFFER_PRIVATE;
		MemoryProperties[i].SizeInBytes = node[NodeId].node.LocalMemSize;
		i++;
	}

	/* Add SCRATCH */
	if (i < NumBanks &&
		fmm_get_aperture_base_and_limit(FMM_SCRATCH, gpu_id,
				&MemoryProperties[i].VirtualBaseAddress, &aperture_limit) == HSAKMT_STATUS_SUCCESS) {
		MemoryProperties[i].HeapType = HSA_HEAPTYPE_GPU_SCRATCH;
		MemoryProperties[i].SizeInBytes = (aperture_limit - MemoryProperties[i].VirtualBaseAddress) + 1;
		i++;
	}

	/* On dGPUs add SVM aperture */
	if (nodeIsDGPU && i < NumBanks &&
	    fmm_get_aperture_base_and_limit(
		    FMM_SVM, gpu_id, &MemoryProperties[i].VirtualBaseAddress,
		    &aperture_limit) == HSAKMT_STATUS_SUCCESS) {
		MemoryProperties[i].HeapType = HSA_HEAPTYPE_DEVICE_SVM;
		MemoryProperties[i].SizeInBytes = (aperture_limit - MemoryProperties[i].VirtualBaseAddress) + 1;
		i++;
	}

out:
	pthread_mutex_unlock(&hsakmt_mutex);
	return err;
}

HSAKMT_STATUS HSAKMTAPI hsaKmtGetNodeCacheProperties(HSAuint32 NodeId,
						     HSAuint32 ProcessorId,
						     HSAuint32 NumCaches,
						     HsaCacheProperties *CacheProperties)
{
	HSAKMT_STATUS err;
	uint32_t i;

	if (!CacheProperties)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_KFD_OPEN();
	pthread_mutex_lock(&hsakmt_mutex);

	/* KFD ADD page 18, snapshot protocol violation */
	if (!_system) {
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

HSAKMT_STATUS HSAKMTAPI hsaKmtGetNodeIoLinkProperties(HSAuint32 NodeId,
						      HSAuint32 NumIoLinks,
						      HsaIoLinkProperties *IoLinkProperties)
{
	HSAKMT_STATUS err;
	uint32_t i;

	if (!IoLinkProperties)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	CHECK_KFD_OPEN();

	pthread_mutex_lock(&hsakmt_mutex);

	/* KFD ADD page 18, snapshot protocol violation */
	if (!_system) {
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

int get_drm_render_fd_by_gpu_id(HSAuint32 gpu_id)
{
	unsigned int i;

	if (!node || !_system)
		return 0;

	for (i = 0; i < _system->NumNodes; i++) {
		if (node[i].gpu_id == gpu_id)
			return node[i].drm_render_fd;
	}

	return -1;
}

HSAKMT_STATUS validate_nodeid_array(uint32_t **gpu_id_array,
		uint32_t NumberOfNodes, uint32_t *NodeArray)
{
	HSAKMT_STATUS ret;
	unsigned int i;

	if (NumberOfNodes == 0 || !NodeArray || !gpu_id_array)
		return HSAKMT_STATUS_INVALID_PARAMETER;

	/* Translate Node IDs to gpu_ids */
	*gpu_id_array = malloc(NumberOfNodes * sizeof(uint32_t));
	if (!(*gpu_id_array))
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
