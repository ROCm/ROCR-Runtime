// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cstdint>
#include <cstdlib>
#include <fstream>

#include "amdxdna_accel.h"
#include "hsa_ipu.h"

#define DATA_BUFFER_SIZE (1024 * 4)

/*
 * Interpretation of the beginning of data payload for ERT_CMD_CHAIN in
 * amdxdna_cmd. The rest of the payload in amdxdna_cmd is cmd BO handles.
 */
struct amdxdna_cmd_chain {
  uint32_t command_count;
  uint32_t submit_index;
  uint32_t error_index;
  uint32_t reserved[3];
  uint64_t data[] __counted_by(command_count);
};

/* Exec buffer command header format */
struct amdxdna_cmd {
  union {
    struct {
      uint32_t state : 4;
      uint32_t unused : 6;
      uint32_t extra_cu_masks : 2;
      uint32_t count : 11;
      uint32_t opcode : 5;
      uint32_t reserved : 4;
    };
    uint32_t header;
  };
  uint32_t data[] __counted_by(count);
};

// These packets are variable width but using this as a
// maximum size for now
#define PACKET_SIZE 64

int main(int argc, char **argv) {
  int drv_fd;
  int ret;
  const char drv_path[] = "/dev/accel/accel0";
  std::string test_dir(argv[1]);
  std::string inst_path = test_dir + "/add_one_insts.txt";
  std::string pdi_path_str = test_dir + "/add_one.pdi";
  const char *dpu_inst_path = inst_path.c_str();
  const char *pdi_path = pdi_path_str.c_str();  // Add one kernel
  uint32_t heap_handle;
  uint32_t major, minor;

  // open the driver
  drv_fd = open(drv_path, O_RDWR);

  if (drv_fd < 0) {
    printf("Error %i opening %s\n", drv_fd, drv_path);
    return -1;
  }

  printf("%s open\n", drv_path);

  // get driver version
  if (get_driver_version(drv_fd, &major, &minor) < 0) {
    printf("Error getting driver version\n");
    printf("Closing\n");
    close(drv_fd);
    printf("Done\n");
    return -1;
  }

  printf("Driver version %u.%u\n", major, minor);

  /////////////////////////////////////////////////////////////////////////////////
  // Step 0: Allocate the necessary BOs. This includes:
  // 1. The operands for the two kernels that will be launched
  // 2. A heap which contains:
  //  a. A PDI for the design that will be run
  //  b. Instruction sequences for both runs

  // reserve some device memory for the heap
  if (alloc_heap(drv_fd, 48 * 1024 * 1024, &heap_handle) < 0) {
    perror("Error allocating device heap");
    printf("Closing\n");
    close(drv_fd);
    printf("Done\n");
    return -1;
  }

  uint64_t pdi_vaddr;
  uint64_t pdi_sram_vaddr;
  uint32_t pdi_handle;
  printf("Loading pdi\n");
  ret = load_pdi(drv_fd, &pdi_vaddr, &pdi_sram_vaddr, &pdi_handle, pdi_path);
  if (ret < 0) {
    printf("Error %i loading pdi\n", ret);
    printf("Closing\n");
    close(drv_fd);
    printf("Done\n");
    return -1;
  }

  uint64_t dpu_0_vaddr;
  uint64_t dpu_0_sram_vaddr;
  uint32_t dpu_0_handle;
  uint32_t num_dpu_0_insts;
  printf("Loading dpu inst\n");
  ret = load_instructions(drv_fd, &dpu_0_vaddr, &dpu_0_sram_vaddr,
                          &dpu_0_handle, dpu_inst_path, &num_dpu_0_insts);
  if (ret < 0) {
    printf("Error %i loading dpu instructions\n", ret);
    printf("Closing\n");
    close(drv_fd);
    printf("Done\n");
    return -1;
  }

  uint64_t dpu_1_vaddr;
  uint64_t dpu_1_sram_vaddr;
  uint32_t dpu_1_handle;
  uint32_t num_dpu_1_insts;
  printf("Loading dpu inst\n");
  ret = load_instructions(drv_fd, &dpu_1_vaddr, &dpu_1_sram_vaddr,
                          &dpu_1_handle, dpu_inst_path, &num_dpu_1_insts);
  if (ret < 0) {
    printf("Error %i loading dpu instructions\n", ret);
    printf("Closing\n");
    close(drv_fd);
    printf("Done\n");
    return -1;
  }

  printf("DPU 0 instructions @:             %p\n", (void *)dpu_0_vaddr);
  printf("DPU 1 instructions @:             %p\n", (void *)dpu_1_vaddr);
  printf("PDI file @:                     %p\n", (void *)pdi_vaddr);
  printf("PDI handle @:                     %d\n", pdi_handle);

  uint64_t input_0;
  uint64_t input_0_sram_vaddr;
  uint32_t input_0_handle;
  ret = create_dev_bo(drv_fd, &input_0, &input_0_sram_vaddr, &input_0_handle,
                      DATA_BUFFER_SIZE);
  printf("Input @:             %p\n", (void *)input_0);
  if (ret < 0) {
    printf("Error %i creating data 0\n", ret);
    printf("Closing\n");
    close(drv_fd);
    printf("Done\n");
    return -1;
  }

  uint64_t output_0;
  uint64_t output_0_sram_vaddr;
  uint32_t output_0_handle;
  ret = create_dev_bo(drv_fd, &output_0, &output_0_sram_vaddr, &output_0_handle,
                      DATA_BUFFER_SIZE);
  printf("Output @:             %p\n", (void *)output_0);
  if (ret < 0) {
    printf("Error %i creating data 1\n", ret);
    printf("Closing\n");
    close(drv_fd);
    printf("Done\n");
    return -1;
  }

  uint64_t input_1;
  uint64_t input_1_sram_vaddr;
  uint32_t input_1_handle;
  ret = create_dev_bo(drv_fd, &input_1, &input_1_sram_vaddr, &input_1_handle,
                      DATA_BUFFER_SIZE);
  printf("Input @:             %p\n", (void *)input_1);
  if (ret < 0) {
    printf("Error %i creating data 0\n", ret);
    printf("Closing\n");
    close(drv_fd);
    printf("Done\n");
    return -1;
  }

  uint64_t output_1;
  uint64_t output_1_sram_vaddr;
  uint32_t output_1_handle;
  ret = create_dev_bo(drv_fd, &output_1, &output_1_sram_vaddr, &output_1_handle,
                      DATA_BUFFER_SIZE);
  printf("Output @:             %p\n", (void *)output_1);
  if (ret < 0) {
    printf("Error %i creating data 1\n", ret);
    printf("Closing\n");
    close(drv_fd);
    printf("Done\n");
    return -1;
  }

  for (int i = 0; i < DATA_BUFFER_SIZE / sizeof(uint32_t); i++) {
    *((uint32_t *)input_0 + i) = i;
    *((uint32_t *)input_1 + i) = i + 0xFEEDED1E;
    *((uint32_t *)output_0 + i) = 0xDEFACE;
    *((uint32_t *)output_1 + i) = 0xDEADBEEF;
  }

  // Writing the user buffers
  sync_bo(drv_fd, input_0_handle);
  sync_bo(drv_fd, output_0_handle);
  sync_bo(drv_fd, input_1_handle);
  sync_bo(drv_fd, output_1_handle);

  // Performing a sync on the queue descriptor, completion signal, queue buffer
  // and config cu bo.
  sync_bo(drv_fd, dpu_0_handle);
  sync_bo(drv_fd, dpu_1_handle);
  sync_bo(drv_fd, pdi_handle);
  sync_bo(drv_fd, input_0_handle);
  sync_bo(drv_fd, output_0_handle);

  /////////////////////////////////////////////////////////////////////////////////
  // Step 1: Create a user mode queue
  // This is going to be where we create a queue where we:
  // 1. Create and configure a hardware context
  // 2. Allocate the queue buffer as a user-mode queue

  // Allocating a structure to store QOS information
  amdxdna_qos_info *qos =
      (struct amdxdna_qos_info *)malloc(sizeof(struct amdxdna_qos_info));
  qos->gops = 0;
  qos->fps = 0;
  qos->dma_bandwidth = 0;
  qos->latency = 0;
  qos->frame_exec_time = 0;
  qos->priority = 0;

  // This is the structure that we pass
  amdxdna_drm_create_hwctx create_hw_ctx = {
      .ext = 0,
      .ext_flags = 0,
      .qos_p = (uint64_t)qos,
      .umq_bo = 0,
      .log_buf_bo = 0,
      .max_opc = 0x800,  // Not sure what this is but this was the value used
      .num_tiles = 4,
      .mem_size = 0,
      .umq_doorbell = 0,
  };
  ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_CREATE_HWCTX, &create_hw_ctx);
  if (ret != 0) {
    perror("Failed to create hwctx");
    return -1;
  }

  // Creating a structure to configure the CU
  amdxdna_cu_config cu_config = {
      .cu_bo = pdi_handle,
      .cu_func = 0,
  };

  // Creating a structure to configure the hardware context
  amdxdna_hwctx_param_config_cu param_config_cu;
  param_config_cu.num_cus = 1;
  param_config_cu.cu_configs[0] = cu_config;

  printf("Size of param_config_cu: 0x%lx\n", sizeof(param_config_cu));

  // Configuring the hardware context with the PDI
  amdxdna_drm_config_hwctx config_hw_ctx = {
      .handle = create_hw_ctx.handle,
      .param_type = DRM_AMDXDNA_HWCTX_CONFIG_CU,
      // Pass in the pointer to the param value
      .param_val = (uint64_t)&param_config_cu,
      // Size of param config CU is 16B
      .param_val_size = 0x10,
  };
  ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_CONFIG_HWCTX, &config_hw_ctx);
  if (ret != 0) {
    perror("Failed to config hwctx");
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////////
  // Step 2: Configuring the CMD BOs with the different instruction sequences
  amdxdna_drm_create_bo create_cmd_bo_0 = {
      .type = AMDXDNA_BO_CMD,
      .size = PACKET_SIZE,
  };
  int cmd_bo_ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_CREATE_BO, &create_cmd_bo_0);
  if (cmd_bo_ret != 0) {
    perror("Failed to create cmd_0");
    return -1;
  }

  amdxdna_drm_get_bo_info cmd_bo_0_get_bo_info = {.handle =
                                                      create_cmd_bo_0.handle};
  ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_GET_BO_INFO, &cmd_bo_0_get_bo_info);
  if (ret != 0) {
    perror("Failed to get cmd BO 0 info");
    return -2;
  }

  // Writing the first packet to the queue
  amdxdna_cmd *cmd_0 = (struct amdxdna_cmd *)mmap(
      0, PACKET_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, drv_fd,
      cmd_bo_0_get_bo_info.map_offset);
  cmd_0->state = 1;  // ERT_CMD_STATE_NEW;
  cmd_0->extra_cu_masks = 0;
  cmd_0->count = 0xF;    // NOTE: For some reason this needs to be larger
  cmd_0->opcode = 0x0;   // ERT_START_CU;
  cmd_0->data[0] = 0x3;  // NOTE: This one seems to be skipped
  cmd_0->data[1] = 0x3;  // Transaction opcode
  cmd_0->data[2] = 0x0;
  cmd_0->data[3] = dpu_0_sram_vaddr;
  cmd_0->data[4] = 0x0;
  cmd_0->data[5] = 0x44;                           // Size of DPU instruction
  cmd_0->data[6] = input_0 & 0xFFFFFFFF;           // Input low
  cmd_0->data[7] = (input_0 >> 32) & 0xFFFFFFFF;   // Input high
  cmd_0->data[8] = output_0 & 0xFFFFFFFF;          // Output low
  cmd_0->data[9] = (output_0 >> 32) & 0xFFFFFFFF;  // Output high

  // Writing to the second packet of the queue
  amdxdna_drm_create_bo create_cmd_bo_1 = {
      .type = AMDXDNA_BO_CMD,
      .size = PACKET_SIZE,
  };
  cmd_bo_ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_CREATE_BO, &create_cmd_bo_1);
  if (cmd_bo_ret != 0) {
    perror("Failed to create cmd_1");
    return -1;
  }

  amdxdna_drm_get_bo_info cmd_bo_1_get_bo_info = {.handle =
                                                      create_cmd_bo_1.handle};
  ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_GET_BO_INFO, &cmd_bo_1_get_bo_info);
  if (ret != 0) {
    perror("Failed to get cmd BO 0 info");
    return -2;
  }

  amdxdna_cmd *cmd_1 = (struct amdxdna_cmd *)mmap(
      0, PACKET_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, drv_fd,
      cmd_bo_1_get_bo_info.map_offset);
  cmd_1->state = 1;  // ERT_CMD_STATE_NEW;
  cmd_1->extra_cu_masks = 0;
  cmd_1->count = 10;     // Number of commands
  cmd_1->opcode = 0x0;   // ERT_START_CU;
  cmd_1->data[0] = 0x3;  // This one seems to be skipped
  cmd_1->data[1] = 0x3;  // Transaction opcode
  cmd_1->data[2] = 0x0;
  cmd_1->data[3] = dpu_1_sram_vaddr;
  cmd_1->data[4] = 0x0;
  cmd_1->data[5] = 0x44;                           // Size of DPU instruction
  cmd_1->data[6] = input_1 & 0xFFFFFFFF;           // Input low
  cmd_1->data[7] = (input_1 >> 32) & 0xFFFFFFFF;   // Input high
  cmd_1->data[8] = output_1 & 0xFFFFFFFF;          // Output low
  cmd_1->data[9] = (output_1 >> 32) & 0xFFFFFFFF;  // Output high

  /////////////////////////////////////////////////////////////////////////////////
  // Step 3: Submit commands -- This requires creating a BO_EXEC that contains
  // the command chain that points to the instruction sequences just created

  // Allocate a command chain
  void *bo_cmd_chain_buf = nullptr;
  cmd_bo_ret = posix_memalign(&bo_cmd_chain_buf, 4096, 4096);
  if (cmd_bo_ret != 0 || bo_cmd_chain_buf == nullptr) {
    printf("[ERROR] Failed to allocate cmd_bo buffer of size %d\n", 4096);
  }

  amdxdna_drm_create_bo create_cmd_chain_bo = {
      .type = AMDXDNA_BO_CMD,
      .size = 4096,
  };
  cmd_bo_ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_CREATE_BO, &create_cmd_chain_bo);
  if (cmd_bo_ret != 0) {
    perror("Failed to create command chain BO");
    return -1;
  }

  amdxdna_drm_get_bo_info cmd_chain_bo_get_bo_info = {
      .handle = create_cmd_chain_bo.handle};
  ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_GET_BO_INFO, &cmd_chain_bo_get_bo_info);
  if (ret != 0) {
    perror("Failed to get cmd BO 0 info");
    return -2;
  }

  amdxdna_cmd *cmd_chain =
      (struct amdxdna_cmd *)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED,
                                 drv_fd, cmd_chain_bo_get_bo_info.map_offset);

  // Writing information to the command buffer
  amdxdna_cmd_chain *cmd_chain_payload =
      (struct amdxdna_cmd_chain *)(cmd_chain->data);
  cmd_chain->state = 1;  // ERT_CMD_STATE_NEW;
  cmd_chain->extra_cu_masks = 0;
  cmd_chain->count = 0xA;    // TODO: Why is this the value?
  cmd_chain->opcode = 0x13;  // ERT_CMD_CHAIN
  cmd_chain_payload->command_count = 2;
  cmd_chain_payload->submit_index = 0;
  cmd_chain_payload->error_index = 0;
  cmd_chain_payload->data[0] = create_cmd_bo_0.handle;
  cmd_chain_payload->data[1] = create_cmd_bo_1.handle;

  // Reading the user buffers
  sync_bo(drv_fd, create_cmd_chain_bo.handle);
  sync_bo(drv_fd, create_cmd_bo_0.handle);
  sync_bo(drv_fd, create_cmd_bo_1.handle);

  // Perform a submit cmd
  uint32_t bo_args[6] = {dpu_0_handle,    dpu_1_handle,   input_0_handle,
                         output_0_handle, input_1_handle, output_1_handle};
  amdxdna_drm_exec_cmd exec_cmd_0 = {
      .ext = 0,
      .ext_flags = 0,
      .hwctx = create_hw_ctx.handle,
      .type = AMDXDNA_CMD_SUBMIT_EXEC_BUF,
      .cmd_handles = create_cmd_chain_bo.handle,
      .args = (uint64_t)bo_args,
      .cmd_count = 1,
      .arg_count = sizeof(bo_args) / sizeof(uint32_t),
  };
  ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_EXEC_CMD, &exec_cmd_0);
  if (ret != 0) {
    perror("Failed to submit work");
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////////
  // Step 4: Wait for the output
  // Use the wait IOCTL to wait for our submission to complete
  amdxdna_drm_wait_cmd wait_cmd = {
      .hwctx = create_hw_ctx.handle,
      .timeout = 50,  // 50ms timeout
      .seq = exec_cmd_0.seq,
  };

  ret = ioctl(drv_fd, DRM_IOCTL_AMDXDNA_WAIT_CMD, &wait_cmd);
  if (ret != 0) {
    perror("Failed to wait");
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////////
  // Step 5: Verify output

  // Reading the user buffers
  sync_bo(drv_fd, input_0_handle);
  sync_bo(drv_fd, output_0_handle);
  sync_bo(drv_fd, input_1_handle);
  sync_bo(drv_fd, output_1_handle);

  int errors = 0;
  printf("Checking run 0:\n");
  for (int i = 0; i < DATA_BUFFER_SIZE / sizeof(uint32_t); i++) {
    uint32_t src = *((uint32_t *)input_0 + i);
    uint32_t dst = *((uint32_t *)output_0 + i);
    if (src + 1 != dst) {
      printf("[ERROR] %d: %d + 1 != %d\n", i, src, dst);
      errors++;
    }
  }

  printf("Checking run 1:\n");
  for (int i = 0; i < DATA_BUFFER_SIZE / sizeof(uint32_t); i++) {
    uint32_t src = *((uint32_t *)input_1 + i);
    uint32_t dst = *((uint32_t *)output_1 + i);
    if (src + 1 != dst) {
      printf("[ERROR] %d: %d + 1 != %d\n", i, src, dst);
      errors++;
    }
  }

  if (!errors) {
    printf("PASS!\n");
  } else {
    printf("FAIL! %d/2048\n", errors);
  }

  printf("Closing\n");
  close(drv_fd);
  printf("Done\n");
  return 0;
}