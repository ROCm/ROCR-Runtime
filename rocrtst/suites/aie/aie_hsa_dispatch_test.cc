// Copyright 2024 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <sys/mman.h>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"

namespace {

hsa_status_t get_agent(hsa_agent_t agent, std::vector<hsa_agent_t> *agents,
                       hsa_device_type_t requested_dev_type) {
  if (!agents || !(requested_dev_type == HSA_DEVICE_TYPE_AIE ||
                   requested_dev_type == HSA_DEVICE_TYPE_GPU ||
                   requested_dev_type == HSA_DEVICE_TYPE_CPU)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  hsa_device_type_t device_type;
  hsa_status_t ret =
      hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);

  if (ret != HSA_STATUS_SUCCESS) {
    return ret;
  }

  if (device_type == requested_dev_type) {
    agents->push_back(agent);
  }

  return ret;
}

hsa_status_t get_aie_agents(hsa_agent_t agent, void *data) {
  if (!data) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  auto *aie_agents = reinterpret_cast<std::vector<hsa_agent_t> *>(data);
  return get_agent(agent, aie_agents, HSA_DEVICE_TYPE_AIE);
}

hsa_status_t get_coarse_global_mem_pool(hsa_amd_memory_pool_t pool, void *data,
                                        bool kernarg) {
  hsa_amd_segment_t segment_type;
  auto ret = hsa_amd_memory_pool_get_info(
      pool, HSA_AMD_MEMORY_POOL_INFO_SEGMENT, &segment_type);
  if (ret != HSA_STATUS_SUCCESS) {
    return ret;
  }

  if (segment_type == HSA_AMD_SEGMENT_GLOBAL) {
    hsa_amd_memory_pool_global_flag_t global_pool_flags;
    ret = hsa_amd_memory_pool_get_info(
        pool, HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &global_pool_flags);
    if (ret != HSA_STATUS_SUCCESS) {
      return ret;
    }

    if (kernarg) {
      if ((global_pool_flags &
           HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED) &&
          (global_pool_flags & HSA_REGION_GLOBAL_FLAG_KERNARG)) {
        *static_cast<hsa_amd_memory_pool_t *>(data) = pool;
      }
    } else {
      if ((global_pool_flags &
           HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED) &&
          !(global_pool_flags & HSA_REGION_GLOBAL_FLAG_KERNARG)) {
        *static_cast<hsa_amd_memory_pool_t *>(data) = pool;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t get_coarse_global_dev_mem_pool(hsa_amd_memory_pool_t pool,
                                            void *data) {
  return get_coarse_global_mem_pool(pool, data, false);
}

hsa_status_t get_coarse_global_kernarg_mem_pool(hsa_amd_memory_pool_t pool,
                                                void *data) {
  return get_coarse_global_mem_pool(pool, data, true);
}

void load_pdi_file(hsa_amd_memory_pool_t mem_pool, const std::string &file_name,
                   void **buf) {
  std::ifstream bin_file(file_name,
                         std::ios::binary | std::ios::ate | std::ios::in);

  assert(bin_file.fail() == false);

  auto size(bin_file.tellg());

  bin_file.seekg(0, std::ios::beg);
  auto r = hsa_amd_memory_pool_allocate(mem_pool, size, 0, buf);
  assert(r == HSA_STATUS_SUCCESS);
  bin_file.read(reinterpret_cast<char *>(*buf), size);
}

void load_instr_file(hsa_amd_memory_pool_t mem_pool, const std::string &file_name,
                   void **buf, uint32_t &num_instr) {
  std::ifstream bin_file(file_name,
                         std::ios::binary | std::ios::ate | std::ios::in);

  assert(bin_file.fail() == false);

  auto size(bin_file.tellg());
  bin_file.seekg(0, std::ios::beg);
  std::vector<uint32_t> pdi_vec;
  std::string val;

  while (bin_file >> val) {
    pdi_vec.push_back(std::stoul(val, nullptr, 16));
  }
  auto r = hsa_amd_memory_pool_allocate(mem_pool, size, 0, buf);
  assert(r == HSA_STATUS_SUCCESS);
  std::memcpy(*buf, pdi_vec.data(), pdi_vec.size() * sizeof(uint32_t));
  num_instr = pdi_vec.size();
}

}  // namespace

int main(int argc, char **argv) {
  std::filesystem::path sourcePath(argv[1]);
  // List of AIE agents in the system.
  std::vector<hsa_agent_t> aie_agents;
  // For creating a queue on an AIE agent.
  hsa_queue_t *aie_queue(nullptr);
  // Memory pool for allocating device-mapped memory. Used for PDI/DPU
  // instructions.
  hsa_amd_memory_pool_t global_dev_mem_pool{0};
  // System memory pool. Used for allocating kernel argument data.
  hsa_amd_memory_pool_t global_kernarg_mem_pool{0};
  const std::string instr_inst_file_name(sourcePath / "add_one_insts.txt");
  const std::string pdi_file_name(sourcePath / "add_one.pdi");
  uint32_t *instr_inst_buf(nullptr);
  uint64_t *pdi_buf(nullptr);

  assert(aie_agents.empty());
  assert(global_dev_mem_pool.handle == 0);
  assert(global_kernarg_mem_pool.handle == 0);

  // Initialize the runtime.
  auto r = hsa_init();
  assert(r == HSA_STATUS_SUCCESS);

  assert(sizeof(hsa_kernel_dispatch_packet_s) ==
         sizeof(hsa_amd_aie_ert_packet_s));

  // Test a launch of an AIE kernel using the HSA API.
  // Find the AIE agents in the system.
  r = hsa_iterate_agents(get_aie_agents, &aie_agents);
  assert(r == HSA_STATUS_SUCCESS);
  assert(aie_agents.size() == 1);

  const auto &aie_agent = aie_agents.front();

  // Create a queue on the first agent.
  r = hsa_queue_create(aie_agent, 64, HSA_QUEUE_TYPE_SINGLE, nullptr, nullptr,
                       0, 0, &aie_queue);
  assert(r == HSA_STATUS_SUCCESS);
  assert(aie_queue);
  assert(aie_queue->base_address);

  // Find a pool for DEV BOs. This is a global system memory pool that is
  // mapped to the device. Will be used for PDIs and DPU instructions.
  r = hsa_amd_agent_iterate_memory_pools(
      aie_agent, get_coarse_global_dev_mem_pool, &global_dev_mem_pool);
  assert(r == HSA_STATUS_SUCCESS);

  // Find a pool that supports kernel args. This is just normal system memory.
  // It will be used for commands and input data.
  r = hsa_amd_agent_iterate_memory_pools(
      aie_agent, get_coarse_global_kernarg_mem_pool, &global_kernarg_mem_pool);
  assert(r == HSA_STATUS_SUCCESS);
  assert(global_kernarg_mem_pool.handle);

  // Getting the maximum size of the queue so we can submit that many consecutive
  // packets.
  uint32_t aie_max_queue_size;
  r = hsa_agent_get_info(aie_agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &aie_max_queue_size);
  assert(r == HSA_STATUS_SUCCESS);
  int num_pkts = aie_max_queue_size;

  // Load the DPU and PDI files into a global pool that doesn't support kernel
  // args (DEV BO).
  uint32_t num_instr;
  load_instr_file(global_dev_mem_pool, instr_inst_file_name,
                reinterpret_cast<void **>(&instr_inst_buf), num_instr);
  uint32_t instr_handle = 0;
  r = hsa_amd_get_handle_from_vaddr(instr_inst_buf, &instr_handle);
  assert(r == HSA_STATUS_SUCCESS);
  assert(instr_handle != 0);

  load_pdi_file(global_dev_mem_pool, pdi_file_name,
                reinterpret_cast<void **>(&pdi_buf));
  uint32_t pdi_handle = 0;
  r = hsa_amd_get_handle_from_vaddr(pdi_buf, &pdi_handle);
  assert(r == HSA_STATUS_SUCCESS);
  assert(pdi_handle != 0);

  hsa_amd_aie_ert_hw_ctx_cu_config_t cu_config{.cu_config_bo = pdi_handle,
                                               .cu_func = 0};

  hsa_amd_aie_ert_hw_ctx_config_cu_param_t config_cu_args{
      .num_cus = 1, .cu_configs = &cu_config};

  // Configure the queue's hardware context.
  r = hsa_amd_queue_hw_ctx_config(
      aie_queue, HSA_AMD_QUEUE_AIE_ERT_HW_CXT_CONFIG_CU, &config_cu_args);
  assert(r == HSA_STATUS_SUCCESS);

  // create inputs / outputs
  constexpr std::size_t num_data_elements = 1024;
  constexpr std::size_t data_buffer_size =
      num_data_elements * sizeof(std::uint32_t);

  std::vector<uint32_t *> input(num_pkts);
  std::vector<uint32_t *> output(num_pkts);
  std::vector<hsa_amd_aie_ert_start_kernel_data_t *> cmd_payloads(num_pkts);
  std::vector<uint32_t> input_handle(num_pkts);
  std::vector<uint32_t> output_handle(num_pkts);

  uint64_t wr_idx = 0;
  uint64_t packet_id = 0;

  for (int pkt_iter = 0; pkt_iter < num_pkts; pkt_iter++) {
    r = hsa_amd_memory_pool_allocate(global_kernarg_mem_pool, data_buffer_size, 0,
                                     reinterpret_cast<void **>(&input[pkt_iter]));
    assert(r == HSA_STATUS_SUCCESS);
    r = hsa_amd_get_handle_from_vaddr(input[pkt_iter], &input_handle[pkt_iter]);
    assert(r == HSA_STATUS_SUCCESS);
    assert(input_handle[pkt_iter] != 0);

    r = hsa_amd_memory_pool_allocate(global_kernarg_mem_pool, data_buffer_size, 0,
                                     reinterpret_cast<void **>(&output[pkt_iter]));
    assert(r == HSA_STATUS_SUCCESS);
    r = hsa_amd_get_handle_from_vaddr(output[pkt_iter], &output_handle[pkt_iter]);
    assert(r == HSA_STATUS_SUCCESS);
    assert(output_handle[pkt_iter] != 0);

    for (std::size_t i = 0; i < num_data_elements; i++) {
      *(input[pkt_iter] + i) = i * (pkt_iter + 1);
      *(output[pkt_iter] + i) = 0xDEFACE;
    }

    // Getting a slot in the queue
    wr_idx = hsa_queue_add_write_index_relaxed(aie_queue, 1);
    packet_id = wr_idx % aie_queue->size;

    // Creating a packet to store the command
    hsa_amd_aie_ert_packet_t *cmd_pkt = static_cast<hsa_amd_aie_ert_packet_t *>(
        aie_queue->base_address) + packet_id;
    assert(r == HSA_STATUS_SUCCESS);
    cmd_pkt->state = HSA_AMD_AIE_ERT_STATE_NEW;
    cmd_pkt->count = 0xA;  // # of arguments to put in command
    cmd_pkt->opcode = HSA_AMD_AIE_ERT_START_CU;
    cmd_pkt->header.AmdFormat = HSA_AMD_PACKET_TYPE_AIE_ERT;
    cmd_pkt->header.header = HSA_PACKET_TYPE_VENDOR_SPECIFIC
                             << HSA_PACKET_HEADER_TYPE;

    // Creating the payload for the packet
    hsa_amd_aie_ert_start_kernel_data_t *cmd_payload = NULL;
    assert(r == HSA_STATUS_SUCCESS);
    r = hsa_amd_memory_pool_allocate(global_kernarg_mem_pool, 64, 0,
                                     reinterpret_cast<void **>(&cmd_payload));
    assert(r == HSA_STATUS_SUCCESS);
    // Selecting the PDI to use with this command
    cmd_payload->cu_mask = 0x1;
    // Transaction opcode
    cmd_payload->data[0] = 0x3;
    cmd_payload->data[1] = 0x0;
    cmd_payload->data[2] = instr_handle;
    cmd_payload->data[3] = 0x0;
    cmd_payload->data[4] = num_instr;
    cmd_payload->data[5] = input_handle[pkt_iter];
    cmd_payload->data[6] = 0;
    cmd_payload->data[7] = output_handle[pkt_iter];
    cmd_payload->data[8] = 0;
    cmd_pkt->payload_data = reinterpret_cast<uint64_t>(cmd_payload);

    // Keeping track of payloads so we can free them at the end
    cmd_payloads[pkt_iter] = cmd_payload;
  }

  // Ringing the doorbell to dispatch each packet we added to
  // the queue
  hsa_signal_store_screlease(aie_queue->doorbell_signal, wr_idx);

  for (int pkt_iter = 0; pkt_iter < num_pkts; pkt_iter++) {
    for (std::size_t i = 0; i < num_data_elements; i++) {
      const auto expected = *(input[pkt_iter] + i) + 1;
      const auto result = *(output[pkt_iter] + i);
      assert(result == expected);
    }

    r = hsa_amd_memory_pool_free(output[pkt_iter]);
    assert(r == HSA_STATUS_SUCCESS);
    r = hsa_amd_memory_pool_free(input[pkt_iter]);
    assert(r == HSA_STATUS_SUCCESS);
    r = hsa_amd_memory_pool_free(cmd_payloads[pkt_iter]);
    assert(r == HSA_STATUS_SUCCESS);
  }

  r = hsa_queue_destroy(aie_queue);
  assert(r == HSA_STATUS_SUCCESS);

  r = hsa_amd_memory_pool_free(pdi_buf);
  assert(r == HSA_STATUS_SUCCESS);
  r = hsa_amd_memory_pool_free(instr_inst_buf);
  assert(r == HSA_STATUS_SUCCESS);

  r = hsa_shut_down();
  assert(r == HSA_STATUS_SUCCESS);
  std::cout << "PASS\n";
}
