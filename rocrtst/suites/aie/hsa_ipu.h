// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "amdxdna_accel.h"

// want to mmap the file

#define MAX_NUM_INSTRUCTIONS 1024  // Maximum number of dpu or pdi instructions.

// Dummy packet defines

int map_doorbell(int fd, uint64_t *doorbell) {
  // Mmap the mailbox.
  int32_t page_size = 4096;
  *doorbell = (uint64_t)mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
  if (doorbell != MAP_FAILED) {
    printf("Doorbell mapped\n");
    return 0;
  }

  printf("[ERROR] doorbell mmap failed: %s\n", strerror(errno));
  return errno;
}

void ring_doorbell(uint64_t doorbell) {
  int32_t curr_tail = *((int32_t *)doorbell);
  *((uint32_t *)doorbell) = curr_tail + 0x94;
}

int get_driver_version(int fd, __u32 *major, __u32 *minor) {
  int ret;
  amdxdna_drm_query_aie_version version;

  amdxdna_drm_get_info info_params = {
      .param = DRM_AMDXDNA_QUERY_AIE_VERSION,
      .buffer_size = sizeof(version),
      .buffer = (__u64)&version,
  };

  ret = ioctl(fd, DRM_IOCTL_AMDXDNA_GET_INFO, &info_params);
  if (ret == 0) {
    *major = version.major;
    *minor = version.minor;
  }

  return ret;
}

/*
        Allocates a heap on the device by creating a BO of type dev heap
*/
int alloc_heap(int fd, __u32 size, __u32 *handle) {
  int ret;
  void *heap_buf = NULL;
  const size_t alignment = 64 * 1024 * 1024;
  ret = posix_memalign(&heap_buf, alignment, size);
  if (ret != 0 || heap_buf == NULL) {
    printf("[ERROR] Failed to allocate heap buffer of size %d\n", size);
  }

  void *dev_heap_parent = mmap(0, alignment * 2 - 1, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (dev_heap_parent == MAP_FAILED) {
    dev_heap_parent = nullptr;
    return -1;
  }

  amdxdna_drm_create_bo create_bo_params = {
      .type = AMDXDNA_BO_DEV_HEAP,
      .size = size,
  };

  ret = ioctl(fd, DRM_IOCTL_AMDXDNA_CREATE_BO, &create_bo_params);
  if (ret == 0 && handle) {
    *handle = create_bo_params.handle;
  }

  amdxdna_drm_get_bo_info get_bo_info = {.handle = create_bo_params.handle};
  ret = ioctl(fd, DRM_IOCTL_AMDXDNA_GET_BO_INFO, &get_bo_info);
  if (ret != 0) {
    perror("Failed to get BO info");
    return -2;
  }

  // Need to free the heap buf but still use the address so we can
  // ensure alignment
  free(heap_buf);
  heap_buf = (void *)mmap(heap_buf, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                          fd, get_bo_info.map_offset);
  printf("Heap buffer @:                  %p\n", heap_buf);

  return ret;
}

/*
        Creates a dev bo which is carved out of the heap bo.
*/
int create_dev_bo(int fd, uint64_t *vaddr, uint64_t *sram_vaddr, __u32 *handle,
                  __u64 size_in_bytes) {
  amdxdna_drm_create_bo create_bo = {
      .type = AMDXDNA_BO_DEV,
      .size = size_in_bytes,
  };
  int ret = ioctl(fd, DRM_IOCTL_AMDXDNA_CREATE_BO, &create_bo);
  if (ret != 0) {
    perror("Failed to create BO");
    return -1;
  }

  amdxdna_drm_get_bo_info get_bo_info = {.handle = create_bo.handle};
  ret = ioctl(fd, DRM_IOCTL_AMDXDNA_GET_BO_INFO, &get_bo_info);
  if (ret != 0) {
    perror("Failed to get BO info");
    return -2;
  }

  *vaddr = get_bo_info.vaddr;
  *sram_vaddr = get_bo_info.xdna_addr;
  *handle = create_bo.handle;
  return 0;
}

/*
        Creates a shmem bo
*/
int create_shmem_bo(int fd, uint64_t *vaddr, uint64_t *sram_vaddr,
                    __u32 *handle, __u64 size_in_bytes) {
  const size_t alignment = 64 * 1024 * 1024;
  void *shmem_create = NULL;
  int ret = posix_memalign(&shmem_create, alignment, size_in_bytes);
  if (ret != 0) {
    printf("[ERROR] Failed to allocate shmem bo of size %lld\n", size_in_bytes);
  }

  // Touching buffer to map page
  *(uint32_t *)shmem_create = 0xDEADBEEF;

  printf("Shmem BO @:                     %p\n", shmem_create);

  amdxdna_drm_create_bo create_bo = {.type = AMDXDNA_BO_SHMEM,
                                     .vaddr = (__u64)shmem_create,
                                     .size = size_in_bytes};
  ret = ioctl(fd, DRM_IOCTL_AMDXDNA_CREATE_BO, &create_bo);
  if (ret != 0) {
    perror("Failed to create BO");
    return -1;
  }

  amdxdna_drm_get_bo_info get_bo_info = {.handle = create_bo.handle};
  ret = ioctl(fd, DRM_IOCTL_AMDXDNA_GET_BO_INFO, &get_bo_info);
  if (ret != 0) {
    perror("Failed to get BO info");
    return -2;
  }

  *vaddr = (__u64)shmem_create;
  *sram_vaddr = get_bo_info.xdna_addr;
  *handle = create_bo.handle;
  return 0;
}

/*
  Wrapper around synch bo ioctl.
*/
int sync_bo(int fd, __u32 handle) {
  amdxdna_drm_sync_bo sync_params = {.handle = handle};
  int ret = ioctl(fd, DRM_IOCTL_AMDXDNA_SYNC_BO, &sync_params);
  if (ret != 0) {
    printf("Synch bo ioctl failed for handle %d\n", handle);
  }
  return ret;
}

/*
  Create a BO_DEV and populate it with a PDI
*/

int load_pdi(int fd, uint64_t *vaddr, uint64_t *sram_addr, __u32 *handle,
             const char *path) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    perror("Failed to open instructions file.");
    return -1;
  }

  fseek(file, 0L, SEEK_END);
  ssize_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);

  printf("Pdi file size: %ld\n", file_size);

  fclose(file);

  // Mmaping the file
  int pdi_fd = open(path, O_RDONLY);
  uint64_t *file_data =
      (uint64_t *)mmap(0, file_size, PROT_READ, MAP_PRIVATE, pdi_fd, 0);

  // Creating a BO_DEV bo to store the pdi file.
  int ret = create_dev_bo(fd, vaddr, sram_addr, handle, file_size);
  if (ret != 0) {
    perror("Failed to create pdi BO");
    return -1;
  }

  // copy the file into Bo dev
  uint64_t *bo = (uint64_t *)*vaddr;
  memcpy(bo, file_data, file_size);

  close(pdi_fd);
  return 0;
}

/*
  Create a BO DEV and populate it with instructions whose virtual address is
  passed to the driver via an HSA packet.
*/
int load_instructions(int fd, uint64_t *vaddr, uint64_t *sram_addr,
                      __u32 *handle, const char *path, __u32 *num_inst) {
  // read dpu instructions into an array
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    perror("Failed to open instructions file.");
    return -1;
  }

  char *line = NULL;
  size_t len = 0;
  __u32 inst_array[MAX_NUM_INSTRUCTIONS];
  __u32 inst_counter = 0;
  while (getline(&line, &len, file) != -1) {
    inst_array[inst_counter++] = strtoul(line, NULL, 16);
    if (inst_counter >= MAX_NUM_INSTRUCTIONS) {
      perror("Instruction array overflowed.");
      return -2;
    }
  }
  fclose(file);

  // Creating a BO_DEV bo to store the instruction.
  int ret =
      create_dev_bo(fd, vaddr, sram_addr, handle, inst_counter * sizeof(__u32));
  if (ret != 0) {
    perror("Failed to create dpu BO");
    return -3;
  }

  *num_inst = inst_counter;

  memcpy((__u32 *)*vaddr, inst_array, inst_counter * sizeof(__u32));
  return ret;
}
