////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2024-2025, Advanced Micro Devices, Inc. All rights reserved.
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
#ifndef HSA_RUNTIME_CORE_INC_AMD_XDNA_DRIVER_H_
#define HSA_RUNTIME_CORE_INC_AMD_XDNA_DRIVER_H_

#include <map>
#include <memory>
#include <unordered_map>

#include "core/driver/xdna/uapi/amdxdna_accel.h"
#include "core/inc/amd_aie_agent.h"
#include "core/inc/driver.h"
#include "core/inc/memory_region.h"

/// @brief struct amdxdna_cmd_chain - Interpretation of data payload for
/// ERT_CMD_CHAIN
struct amdxdna_cmd_chain {
  /// Number of commands in chain
  __u32 command_count;
  /// Index of last successfully submitted command in chain
  __u32 submit_index;
  /// Index of failing command if cmd status is not completed
  __u32 error_index;
  __u32 reserved[3];
  /// Address of each command in chain
  __u64 data[] __counted_by(command_count);
};

/// @brief struct amdxdna_cmd - Exec buffer command header format
struct amdxdna_cmd {
  union {
    struct {
      /// Current state of a command
      __u32 state : 4;
      __u32 unused : 6;
      /// Extra CU masks in addition to mandatory mask
      __u32 extra_cu_masks : 2;
      /// Number of words in payload (data)
      __u32 count : 11;
      /// Opcode identifying specific command
      __u32 opcode : 5;
      __u32 reserved : 4;
    };
    __u32 header;
  };
  /// Count number of words representing packet payload
  __u32 data[] __counted_by(count);
};

namespace rocr {
namespace core {
class Queue;
}

namespace AMD {

class AieAqlQueue;

/// @brief: The number of arguments in the packet payload before we start passing operands
constexpr uint32_t NON_OPERAND_COUNT = 6;

// @brief: Used to transform an address into a device address
constexpr uint32_t DEV_ADDR_BASE = 0x04000000;
constexpr uint32_t DEV_ADDR_OFFSET_MASK = 0x02FFFFFF;

/// @brief: The driver places a structure before each command in a command chain.
/// Need to increase the size of the command by the size of this structure.
/// In the following xdna driver source can see where this is implemented:
/// Commit hash: eddd92c0f61592c576a500f16efa24eb23667c23
/// https://github.com/amd/xdna-driver/blob/main/src/driver/amdxdna/aie2_msg_priv.h#L387-L391
/// https://github.com/amd/xdna-driver/blob/main/src/driver/amdxdna/aie2_message.c#L637
constexpr uint32_t CMD_COUNT_SIZE_INCREASE = 3;

/// @brief: The size of an instruction in bytes
constexpr uint32_t INSTR_SIZE_BYTES = 4;

/// @brief: Index of command payload where the instruction sequence
/// address is located
constexpr uint32_t CMD_PKT_PAYLOAD_INSTRUCTION_SEQUENCE_IDX = 2;
constexpr uint32_t CMD_PKT_PAYLOAD_INSTRUCTION_SEQUENCE_SIZE_IDX = 4;

/// @brief Environment variable to define job submission timeout
constexpr uint32_t DEFAULT_TIMEOUT_VAL = 50;

/// @brief Defacult CU_FUNC value we use when configuring a CU
constexpr uint32_t DEFAULT_CU_FUNC = 0;

/// @brief: Calculates the number of operands in a packet
/// given the number of arguments in the packet
/// @param: arg_count(Input), Number of arguments in the packet
/// @return: uint32_t, The number of operands in the packet
inline uint32_t GetOperandCount(uint32_t arg_count) {
  return ((arg_count - NON_OPERAND_COUNT) / 2);
}

class XdnaDriver final : public core::Driver {
  /// @brief BO handle information.
  struct BOHandle {
    /// Mapped address.
    void* vaddr = nullptr;
    /// Handle returned by xdna.
    uint32_t handle = AMDXDNA_INVALID_BO_HANDLE;
    /// Size in bytes.
    size_t size = 0;

    constexpr BOHandle() = default;
    constexpr BOHandle(void* vaddr, uint32_t handle, size_t size)
        : vaddr{vaddr}, handle{handle}, size{size} {}
    constexpr bool IsValid() const { return handle != AMDXDNA_INVALID_BO_HANDLE; }
  };

public:
  XdnaDriver(std::string devnode_name);

  static hsa_status_t DiscoverDriver(std::unique_ptr<core::Driver>& driver);

  /// @brief Returns the size of the system memory heap in bytes.
  static uint64_t GetSystemMemoryByteSize();

  /// @brief Returns the size of the dev heap in bytes.
  static uint64_t GetDevHeapByteSize();

  hsa_status_t Init() override;
  hsa_status_t ShutDown() override;
  hsa_status_t QueryKernelModeDriver(core::DriverQuery query) override;

  hsa_status_t Open() override;
  hsa_status_t Close() override;
  hsa_status_t GetSystemProperties(HsaSystemProperties& sys_props) const override;
  hsa_status_t GetNodeProperties(HsaNodeProperties& node_props, uint32_t node_id) const override;
  hsa_status_t GetEdgeProperties(std::vector<HsaIoLinkProperties>& io_link_props,
                                 uint32_t node_id) const override;
  hsa_status_t GetAgentProperties(core::Agent &agent) const override;
  hsa_status_t
  GetMemoryProperties(uint32_t node_id,
                      core::MemoryRegion &mem_region) const override;
  hsa_status_t AllocateMemory(const core::MemoryRegion &mem_region,
                              core::MemoryRegion::AllocateFlags alloc_flags,
                              void **mem, size_t size,
                              uint32_t node_id) override;
  hsa_status_t FreeMemory(void *mem, size_t size) override;
  hsa_status_t CreateQueue(core::Queue &queue) const override;
  hsa_status_t DestroyQueue(core::Queue &queue) const override;
  hsa_status_t ExportDMABuf(void *mem, size_t size, int *dmabuf_fd,
                            size_t *offset) override;
  hsa_status_t ImportDMABuf(int dmabuf_fd, core::Agent &agent,
                            core::ShareableHandle &handle) override;
  hsa_status_t Map(core::ShareableHandle handle, void *mem, size_t offset,
                   size_t size, hsa_access_permission_t perms) override;
  hsa_status_t Unmap(core::ShareableHandle handle, void *mem, size_t offset,
                     size_t size) override;
  hsa_status_t ReleaseShareableHandle(core::ShareableHandle &handle) override;

  /// @brief Submits @p num_pkts packets in a command chain.
  hsa_status_t SubmitCmdChain(hsa_amd_aie_ert_packet_t* first_pkt, uint32_t num_pkts,
                              AieAqlQueue& aie_queue);

 private:
  /// @brief Finds the BO associated with the address.
  BOHandle FindBOHandle(void* mem) const;

  /// @brief Creates a new hardware context with the correct CUs
  hsa_status_t ConfigHwCtxNewCUs(const std::vector<BOHandle>& new_cus, AieAqlQueue& aie_queue);

  hsa_status_t QueryDriverVersion();

  /// @brief Allocate device accesible heap space.
  ///
  /// Allocate and map a buffer object (BO) that the AIE device can access.
  hsa_status_t InitDeviceHeap();
  hsa_status_t FreeDeviceHeap();

  /// @brief Creates a command BO and returns it to @p bo_info.
  ///
  /// @param size size of memory to allocate
  /// @param bo_info allocated BO
  hsa_status_t CreateCmdBO(uint32_t size, BOHandle& bo_info);

  /// @brief Finds all BOs in a command packet payload.
  ///
  /// @param count Number of entries in the command
  /// @param cmd_pkt_payload A pointer to the payload of the command
  /// @param bo_handles vector that contains all BO handles
  hsa_status_t FindBOs(uint32_t count, hsa_amd_aie_ert_start_kernel_data_t* cmd_pkt_payload,
                       std::vector<uint32_t>& bo_handles);

  /// @brief Executes a command and waits for its completion
  ///
  /// @param cmd_chain_bo_handle command to execute
  /// @param bo_handles handles associated with the command
  /// @param aie_queue queue to submit to
  hsa_status_t ExecCmdAndWait(const BOHandle& cmd_chain_bo_handle,
                              const std::vector<uint32_t>& bo_handles, AieAqlQueue& aie_queue);

  /// TODO: Remove this in the future and rely on the core Runtime
  /// object to track handle allocations. Using the VMEM API for mapping XDNA
  /// driver handles requires a bit more refactoring. So rely on the XDNA driver
  /// to manage some of this for now.
  std::map<void*, BOHandle> vmem_addr_mappings;

  /// @brief Storing cached CUs that have already been added to the hardware context.
  /// This maps the CU BO to the cu_mask in the hardware context
  std::unordered_map<uint32_t, uint32_t> handle_cu_mappings;

  /// @brief Virtual address range allocated for the device heap.
  ///
  /// Allocate a large enough space so we can carve out the device heap in
  /// this range and ensure it is aligned to 64MB. Currently, npu1 supports
  /// 64MB device heap and it must be aligned to 64MB.
  BOHandle dev_heap_handle;

  /// @brief The aligned device heap.
  void *dev_heap_aligned = nullptr;
  static constexpr size_t dev_heap_size = 64 * 1024 * 1024;
  static constexpr size_t dev_heap_align = 64 * 1024 * 1024;
};

} // namespace AMD
} // namespace rocr

#endif // header guard
