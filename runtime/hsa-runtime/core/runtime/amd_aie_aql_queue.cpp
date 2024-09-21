////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2023, Advanced Micro Devices, Inc. All rights reserved.
//
// Developed by:
//
//                 AMD Research and AMD HSA Software Development
//
//                 Advanced Micro Devices, Inc.
//
//                 www.amd.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#include "core/inc/amd_aie_aql_queue.h"
#include "core/inc/amd_xdna_driver.h"

#ifdef __linux__
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cstring>

#include "core/inc/queue.h"
#include "core/inc/runtime.h"
#include "core/inc/signal.h"
#include "core/util/utils.h"

// The number of arguments in the packet payload before we start passing operands
constexpr int NON_OPERAND_COUNT = 6;

// Used to transform an address into a device address
constexpr int DEV_ADDR_BASE = 0x04000000;
constexpr int DEV_ADDR_OFFSET_MASK = 0x02FFFFFF;

// The driver places a structure before each command in a command chain.
// Need to increase the size of the command by the size of this structure.
// In the following xdna driver source can see where this is implemented:
// Commit hash: eddd92c0f61592c576a500f16efa24eb23667c23
// https://github.com/amd/xdna-driver/blob/main/src/driver/amdxdna/aie2_msg_priv.h#L387-L391
// https://github.com/amd/xdna-driver/blob/main/src/driver/amdxdna/aie2_message.c#L637
constexpr int CMD_COUNT_SIZE_INCREASE = 3;

// Index of command payload where the instruction sequence
// address is located
constexpr int CMD_PKT_PAYLOAD_INSTRUCTION_SEQUENCE_IDX = 2;

// Environment variable to define job submission timeout
constexpr const char *TIMEOUT_ENV_VAR = "ROCR_AIE_TIMEOUT";
constexpr int DEFAULT_TIMEOUT_VAL = 50;
char *timeout_env_var_ptr = getenv(TIMEOUT_ENV_VAR);
int timeout_val = timeout_env_var_ptr == nullptr ? DEFAULT_TIMEOUT_VAL : atoi(timeout_env_var_ptr);

namespace rocr {
namespace AMD {

AieAqlQueue::AieAqlQueue(AieAgent *agent, size_t req_size_pkts,
                         uint32_t node_id)
    : Queue(0, 0), LocalSignal(0, false), DoorbellSignal(signal()),
      agent_(*agent), active_(false) {
  if (agent_.device_type() != core::Agent::DeviceType::kAmdAieDevice) {
    throw AMD::hsa_exception(
        HSA_STATUS_ERROR_INVALID_AGENT,
        "Attempting to create an AIE queue on a non-AIE agent.");
  }
  queue_size_bytes_ = req_size_pkts * sizeof(core::AqlPacket);
  ring_buf_ = agent_.system_allocator()(queue_size_bytes_, 4096,
                                        core::MemoryRegion::AllocateNoFlags);

  if (!ring_buf_) {
    throw AMD::hsa_exception(
        HSA_STATUS_ERROR_INVALID_QUEUE_CREATION,
        "Could not allocate a ring buffer for an AIE queue.");
  }

  // Populate hsa_queue_t fields.
  amd_queue_.hsa_queue.type = HSA_QUEUE_TYPE_SINGLE;
  amd_queue_.hsa_queue.id = INVALID_QUEUEID;
  amd_queue_.hsa_queue.doorbell_signal = Signal::Convert(this);
  amd_queue_.hsa_queue.size = req_size_pkts;
  amd_queue_.hsa_queue.base_address = ring_buf_;
  // Populate AMD queue fields.
  amd_queue_.write_dispatch_id = 0;
  amd_queue_.read_dispatch_id = 0;

  signal_.hardware_doorbell_ptr = nullptr;
  signal_.kind = AMD_SIGNAL_KIND_DOORBELL;
  signal_.queue_ptr = &amd_queue_;
  active_ = true;

  core::Runtime::runtime_singleton_->AgentDriver(agent_.driver_type)
      .CreateQueue(*this);
}

AieAqlQueue::~AieAqlQueue() {
  AieAqlQueue::Inactivate();
  if (ring_buf_) {
    agent_.system_deallocator()(ring_buf_);
  }
}

hsa_status_t AieAqlQueue::Inactivate() {
  bool active(active_.exchange(false, std::memory_order_relaxed));
  hsa_status_t status(HSA_STATUS_SUCCESS);

  if (active) {
    status = core::Runtime::runtime_singleton_->AgentDriver(agent_.driver_type)
                 .DestroyQueue(*this);
    hw_ctx_handle_ = std::numeric_limits<uint32_t>::max();
  }

  return status;
}

hsa_status_t AieAqlQueue::SetPriority(HSA_QUEUE_PRIORITY priority) {
  return HSA_STATUS_SUCCESS;
}

void AieAqlQueue::Destroy() { delete this; }

// Atomic Reads/Writes
uint64_t AieAqlQueue::LoadReadIndexRelaxed() {
  return atomic::Load(&amd_queue_.read_dispatch_id, std::memory_order_relaxed);
}

uint64_t AieAqlQueue::LoadReadIndexAcquire() {
  return atomic::Load(&amd_queue_.read_dispatch_id, std::memory_order_acquire);
}

uint64_t AieAqlQueue::LoadWriteIndexRelaxed() {
  return atomic::Load(&amd_queue_.write_dispatch_id, std::memory_order_relaxed);
}

uint64_t AieAqlQueue::LoadWriteIndexAcquire() {
  return atomic::Load(&amd_queue_.write_dispatch_id, std::memory_order_acquire);
}

void AieAqlQueue::StoreWriteIndexRelaxed(uint64_t value) {
  atomic::Store(&amd_queue_.write_dispatch_id, value,
                std::memory_order_relaxed);
}

void AieAqlQueue::StoreWriteIndexRelease(uint64_t value) {
  atomic::Store(&amd_queue_.write_dispatch_id, value,
                std::memory_order_release);
}

uint64_t AieAqlQueue::CasWriteIndexRelaxed(uint64_t expected, uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_relaxed);
}

uint64_t AieAqlQueue::CasWriteIndexAcquire(uint64_t expected, uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_acquire);
}

uint64_t AieAqlQueue::CasWriteIndexRelease(uint64_t expected, uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_release);
}

uint64_t AieAqlQueue::CasWriteIndexAcqRel(uint64_t expected, uint64_t value) {
  return atomic::Cas(&amd_queue_.write_dispatch_id, value, expected,
                     std::memory_order_acq_rel);
}

uint64_t AieAqlQueue::AddWriteIndexRelaxed(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_relaxed);
}

uint64_t AieAqlQueue::AddWriteIndexAcquire(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_acquire);
}

uint64_t AieAqlQueue::AddWriteIndexRelease(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_release);
}

uint64_t AieAqlQueue::AddWriteIndexAcqRel(uint64_t value) {
  return atomic::Add(&amd_queue_.write_dispatch_id, value,
                     std::memory_order_acq_rel);
}

void AieAqlQueue::StoreRelaxed(hsa_signal_value_t value) {
  std::unordered_map<uint32_t, void*> vmem_handle_mappings;

  auto &driver = static_cast<XdnaDriver &>(
      core::Runtime::runtime_singleton_->AgentDriver(agent_.driver_type));
  if (driver.GetHandleMappings(vmem_handle_mappings) != HSA_STATUS_SUCCESS) {
    return;
  }

  int fd = 0;
  if (driver.GetFd(fd) != HSA_STATUS_SUCCESS) {
    return;
  }

  SubmitCmd(hw_ctx_handle_, fd, amd_queue_.hsa_queue.base_address,
            amd_queue_.read_dispatch_id, amd_queue_.write_dispatch_id,
            vmem_handle_mappings);
}

hsa_status_t AieAqlQueue::SyncBos(std::vector<uint32_t> &bo_args, int fd) {
  for (unsigned int bo_arg : bo_args) {
    amdxdna_drm_sync_bo sync_params = {};
    sync_params.handle = bo_arg;
    if (ioctl(fd, DRM_IOCTL_AMDXDNA_SYNC_BO, &sync_params))
      return HSA_STATUS_ERROR;
  }

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AieAqlQueue::ExecCmdAndWait(amdxdna_drm_exec_cmd *exec_cmd,
                                         uint32_t hw_ctx_handle, int fd) {
  // Submit the cmd
  if (ioctl(fd, DRM_IOCTL_AMDXDNA_EXEC_CMD, exec_cmd))
    return HSA_STATUS_ERROR;

  // Waiting for command to finish
  amdxdna_drm_wait_cmd wait_cmd = {};
  wait_cmd.hwctx = hw_ctx_handle;
  wait_cmd.timeout = timeout_val;
  wait_cmd.seq = exec_cmd->seq;

  if (ioctl(fd, DRM_IOCTL_AMDXDNA_WAIT_CMD, &wait_cmd))
    return HSA_STATUS_ERROR;

  return HSA_STATUS_SUCCESS;
}

void AieAqlQueue::RegisterCmdBOs(
    uint32_t count, std::vector<uint32_t> &bo_args,
    hsa_amd_aie_ert_start_kernel_data_t *cmd_pkt_payload,
    std::unordered_map<uint32_t, void *> &vmem_handle_mappings) {
  // This is the index where the operand addresses start in a command
  const int operand_starting_index = 5;

  // Counting the number of operands in the command payload.
  // Operands are 64-bits so we need to divide by two
  uint32_t num_operands = (count - NON_OPERAND_COUNT) / 2;

  // Keep track of the handles before we submit the packet
  bo_args.push_back(
      cmd_pkt_payload->data[CMD_PKT_PAYLOAD_INSTRUCTION_SEQUENCE_IDX]);

  // Going through all of the operands in the command, keeping track of the
  // handles and turning the handles into addresses. The starting index of
  // the operands in a command is `operand_starting_index` and the fields
  // are 32-bits we need to iterate over every two
  for (int operand_iter = 0; operand_iter < num_operands; operand_iter++) {
    bo_args.push_back(
        cmd_pkt_payload->data[operand_starting_index + 2 * operand_iter]);
    // clang-format off
    cmd_pkt_payload->data[operand_starting_index + 2 * operand_iter + 1] =
        (uint64_t)vmem_handle_mappings[cmd_pkt_payload->data[operand_starting_index + 2 * operand_iter]] >> 32 & 0xFFFFFFFF;
    cmd_pkt_payload->data[operand_starting_index + 2 * operand_iter] =
        (uint64_t)vmem_handle_mappings[cmd_pkt_payload->data[operand_starting_index + 2 * operand_iter]] & 0xFFFFFFFF;
    // clang-format on
  }

  // Transform the instruction sequence address into device address
  cmd_pkt_payload->data[CMD_PKT_PAYLOAD_INSTRUCTION_SEQUENCE_IDX] =
      DEV_ADDR_BASE |
      (reinterpret_cast<uint64_t>(
           vmem_handle_mappings
               [cmd_pkt_payload
                    ->data[CMD_PKT_PAYLOAD_INSTRUCTION_SEQUENCE_IDX]]) &
       DEV_ADDR_OFFSET_MASK);
}

hsa_status_t AieAqlQueue::CreateCmd(uint32_t size, uint32_t *handle,
                                    amdxdna_cmd **cmd, int fd) {
  // Creating the command
  amdxdna_drm_create_bo create_cmd_bo = {};
  create_cmd_bo.type = AMDXDNA_BO_CMD,
  create_cmd_bo.size = size;
  if (ioctl(fd, DRM_IOCTL_AMDXDNA_CREATE_BO, &create_cmd_bo))
    return HSA_STATUS_ERROR;

  amdxdna_drm_get_bo_info cmd_bo_get_bo_info = {};
  cmd_bo_get_bo_info.handle = create_cmd_bo.handle;
  if (ioctl(fd, DRM_IOCTL_AMDXDNA_GET_BO_INFO, &cmd_bo_get_bo_info))
    return HSA_STATUS_ERROR;

  *cmd = static_cast<amdxdna_cmd *>(mmap(nullptr, create_cmd_bo.size,
                                         PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                         cmd_bo_get_bo_info.map_offset));
  *handle = create_cmd_bo.handle;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t AieAqlQueue::SubmitCmd(
    uint32_t hw_ctx_handle, int fd, void *queue_base, uint64_t read_dispatch_id,
    uint64_t write_dispatch_id,
    std::unordered_map<uint32_t, void *> &vmem_handle_mappings) {
  uint64_t cur_id = read_dispatch_id;
  while (cur_id < write_dispatch_id) {
    hsa_amd_aie_ert_packet_t *pkt =
        static_cast<hsa_amd_aie_ert_packet_t *>(queue_base) + cur_id;

    // Get the packet header information
    if (pkt->header.header != HSA_PACKET_TYPE_VENDOR_SPECIFIC ||
        pkt->header.AmdFormat != HSA_AMD_PACKET_TYPE_AIE_ERT)
      return HSA_STATUS_ERROR;

    // Get the payload information
    switch (pkt->opcode) {
      case HSA_AMD_AIE_ERT_START_CU: {
        std::vector<uint32_t> bo_args;
        std::vector<uint32_t> cmd_handles;
        std::vector<uint32_t> cmd_sizes;
        std::vector<amdxdna_cmd *> cmds;

        // Iterating over future packets and seeing how many contiguous HSA_AMD_AIE_ERT_START_CU
        // packets there are. All can be combined into a single chain.
        int num_cont_start_cu_pkts = 1;
        for (int peak_pkt_id = cur_id + 1; peak_pkt_id < write_dispatch_id; peak_pkt_id++) {
          if (pkt->opcode != HSA_AMD_AIE_ERT_START_CU) {
            break;
          }
          num_cont_start_cu_pkts++;
        }

        // Iterating over all the contiguous HSA_AMD_AIE_ERT_CMD_CHAIN packets
        for (int pkt_iter = cur_id; pkt_iter < cur_id + num_cont_start_cu_pkts; pkt_iter++) {

          // Getting the current command packet
          hsa_amd_aie_ert_packet_t *pkt =
              static_cast<hsa_amd_aie_ert_packet_t *>(queue_base) + pkt_iter;
          hsa_amd_aie_ert_start_kernel_data_t *cmd_pkt_payload =
              reinterpret_cast<hsa_amd_aie_ert_start_kernel_data_t *>(
                  pkt->payload_data);

          // Add the handles for all of the BOs to bo_args as well as rewrite
          // the command payload handles to contain the actual virtual addresses
          RegisterCmdBOs(pkt->count, bo_args, cmd_pkt_payload, vmem_handle_mappings);

          // Creating a packet that contains the command to execute the kernel
          uint32_t cmd_bo_handle = 0;
          amdxdna_cmd *cmd = nullptr;
          uint32_t cmd_size = sizeof(amdxdna_cmd) + pkt->count * sizeof(uint32_t);
          if (CreateCmd(cmd_size, &cmd_bo_handle, &cmd, fd))
            return HSA_STATUS_ERROR;

          // Filling in the fields of the command
          cmd->state = pkt->state;
          cmd->extra_cu_masks = 0;

          // The driver places a structure before each command in a command chain.
          // Need to increase the size of the command by the size of this structure.
          cmd->count = pkt->count + CMD_COUNT_SIZE_INCREASE;
          cmd->opcode = pkt->opcode;
          cmd->data[0] = cmd_pkt_payload->cu_mask;
          memcpy((cmd->data + 1),  cmd_pkt_payload->data, 4 * pkt->count);

          // Keeping track of the handle
          cmd_handles.push_back(cmd_bo_handle);
          cmds.push_back(cmd);
          cmd_sizes.push_back(cmd_size);
        }

        // Creating a packet that contains the command chain
        uint32_t cmd_chain_bo_handle = 0;
        amdxdna_cmd *cmd_chain = nullptr;
        int cmd_chain_size = (cmd_handles.size() + 1) * sizeof(uint32_t);
        if (CreateCmd(cmd_chain_size, &cmd_chain_bo_handle, &cmd_chain, fd))
          return HSA_STATUS_ERROR;

        // Writing information to the command buffer
        amdxdna_cmd_chain *cmd_chain_payload = reinterpret_cast<amdxdna_cmd_chain *>(cmd_chain->data);

        // Creating a command chain
        cmd_chain->state = HSA_AMD_AIE_ERT_STATE_NEW;
        cmd_chain->extra_cu_masks = 0;
        cmd_chain->count = sizeof(amdxdna_cmd_chain) + cmd_handles.size() * sizeof(uint64_t);
        cmd_chain->opcode = HSA_AMD_AIE_ERT_CMD_CHAIN;
        cmd_chain_payload->command_count = cmd_handles.size();
        cmd_chain_payload->submit_index = 0;
        cmd_chain_payload->error_index = 0;
        for (int i = 0; i < cmd_handles.size(); i++) {
          cmd_chain_payload->data[i] = cmd_handles[i];
        }

        // Syncing BOs before we execute the command
        if (SyncBos(bo_args, fd))
          return HSA_STATUS_ERROR;

        // Removing duplicates in the bo container. The driver will report
        // an error if we provide the same BO handle multiple times.
        // This can happen if any of the BOs are the same across jobs
        std::sort(bo_args.begin(), bo_args.end());
        bo_args.erase(std::unique(bo_args.begin(), bo_args.end()), bo_args.end());

        // Filling in the fields to execute the command chain
        amdxdna_drm_exec_cmd exec_cmd_0 = {};
        exec_cmd_0.ext = 0;
        exec_cmd_0.ext_flags = 0;
        exec_cmd_0.hwctx = hw_ctx_handle;
        exec_cmd_0.type = AMDXDNA_CMD_SUBMIT_EXEC_BUF;
        exec_cmd_0.cmd_handles = cmd_chain_bo_handle;
        exec_cmd_0.args = (uint64_t)bo_args.data();
        exec_cmd_0.cmd_count = 1;
        exec_cmd_0.arg_count = bo_args.size();

        // Executing all commands in the command chain
        ExecCmdAndWait(&exec_cmd_0, hw_ctx_handle, fd);

        // Unmapping and closing the cmd BOs
        drm_gem_close close_bo_args{0};
        for (int i = 0; i < cmd_handles.size(); i++) {
          munmap(cmds[i], cmd_sizes[i]);
          close_bo_args.handle = cmd_handles[i];
          ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close_bo_args);
        }

        // Unmapping and closing the cmd_chain BO
        munmap(cmd_chain, cmd_chain_size);
        close_bo_args.handle = cmd_chain_bo_handle;
        ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close_bo_args);

        // Syncing BOs after we execute the command
        if (SyncBos(bo_args, fd))
          return HSA_STATUS_ERROR;

        cur_id += num_cont_start_cu_pkts;
        break;
      }
      default: {
        return HSA_STATUS_ERROR;
      }
    }
  }

  return HSA_STATUS_SUCCESS;
}

void AieAqlQueue::StoreRelease(hsa_signal_value_t value) {
  std::atomic_thread_fence(std::memory_order_release);
  StoreRelaxed(value);
}

hsa_status_t AieAqlQueue::GetInfo(hsa_queue_info_attribute_t attribute,
                                  void *value) {
  switch (attribute) {
    case HSA_AMD_QUEUE_INFO_AGENT:
      *static_cast<hsa_agent_t *>(value) = agent_.public_handle();
      break;
    case HSA_AMD_QUEUE_INFO_DOORBELL_ID:
      // Hardware doorbell supports AQL semantics.
      *static_cast<uint64_t *>(value) =
          reinterpret_cast<uint64_t>(signal_.hardware_doorbell_ptr);
      break;
    default:
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t AieAqlQueue::GetCUMasking(uint32_t num_cu_mask_count,
                                       uint32_t *cu_mask) {
  assert(false && "AIE AQL queue does not support CU masking.");
  return HSA_STATUS_ERROR;
}

hsa_status_t AieAqlQueue::SetCUMasking(uint32_t num_cu_mask_count,
                                       const uint32_t *cu_mask) {
  assert(false && "AIE AQL queue does not support CU masking.");
  return HSA_STATUS_ERROR;
}

void AieAqlQueue::ExecutePM4(uint32_t *cmd_data, size_t cmd_size_b,
                             hsa_fence_scope_t acquireFence,
                             hsa_fence_scope_t releaseFence,
                             hsa_signal_t *signal) {
  assert(false && "AIE AQL queue does not support PM4 packets.");
}

} // namespace AMD
} // namespace rocr
