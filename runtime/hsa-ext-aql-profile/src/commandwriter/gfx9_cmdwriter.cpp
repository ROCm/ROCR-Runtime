#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "gfx9_cmdwriter.h"

template <class T>
static void PrintPm4Packet(const T& command, const char* name) {
#if ! defined(NDEBUG)
  uint32_t * cmd = (uint32_t*)&command;
  uint32_t size = sizeof(command) / sizeof(uint32_t);
  std::ostringstream oss;
  oss << "'" << name << "' size(" << std::dec << size << ")";
  std::clog << std::setw(40) << std::left << oss.str() << ":";
  for (uint32_t idx = 0; idx < size; idx++) {
    std::clog << " " << std::hex << std::setw(8) << std::setfill('0') << cmd[idx];
  }
  std::clog << std::setfill(' ') << std::endl;
#endif
}

#define APPEND_COMMAND_WRAPPER(cmdbuf, command) \
  PrintPm4Packet(command, __FUNCTION__); \
  AppendCommand(cmdbuf, command);

namespace pm4_profile {
namespace gfx9 {

template <class T> void Gfx9CmdWriter::AppendCommand(CmdBuf* cmdbuf, const T& command) {
  cmdbuf->AppendCommand(&command, sizeof(command));
}

void Gfx9CmdWriter::InitializeLaunchTemplate() {
  memset(&launch_template_, 0, sizeof(launch_template_));
  GenerateCmdHeader(&launch_template_.indirect_buffer, IT_INDIRECT_BUFFER);
}

void Gfx9CmdWriter::InitializeAtomicTemplate() {
  memset(&atomic_template_.atomic, 0, sizeof(atomic_template_));
  GenerateCmdHeader(&atomic_template_.atomic, IT_ATOMIC_MEM);

  // Specify the micro engine and cache policies
  PM4MEC_ATOMIC_MEM* atomicCmd = &atomic_template_.atomic;
  atomicCmd->bitfields2.cache_policy = cache_policy__mec_atomic_mem__stream;
}

void Gfx9CmdWriter::InitializeBarrierTemplate() {
  memset(&pending_dispatch_template_, 0, sizeof(pending_dispatch_template_));
  GenerateCmdHeader(&pending_dispatch_template_.event_write, IT_EVENT_WRITE);

  MEC_EVENT_WRITE_event_index_enum index;
  index = event_index__mec_event_write__cs_partial_flush;
  pending_dispatch_template_.event_write.bitfields2.event_index = index;
  pending_dispatch_template_.event_write.bitfields2.event_type = CS_PARTIAL_FLUSH;
}

void Gfx9CmdWriter::InitializeAcquireMemTemplate() {
  memset(&invalidate_cache_template_, 0, sizeof(invalidate_cache_template_));
  GenerateCmdHeader(&invalidate_cache_template_.acquire_mem, IT_ACQUIRE_MEM);

  // Specify the CP module which will process this packet
  PM4MEC_ACQUIRE_MEM* acquire_mem = &invalidate_cache_template_.acquire_mem;

  // Specify the size of memory to invalidate. Size is
  // specified in terms of 256 byte chunks. A coher_size
  // of 0xFFFFFFFF actually specified 0xFFFFFFFF00 (40 bits)
  // of memory. The field coher_size_hi specifies memory from
  // bits 40-64 for a total of 256 TB.
  acquire_mem->coher_size = 0xFFFFFFFF;
  acquire_mem->bitfields4.coher_size_hi = 0xFFFFFF;

  // Specify the address of memory to invalidate. The
  // address must be 256 byte aligned.
  acquire_mem->coher_base_lo = 0x00;
  acquire_mem->bitfields6.coher_base_hi = 0x00;

  // Specify the poll interval for determing if operation is complete
  acquire_mem->bitfields7.poll_interval = 0x04;
}

void Gfx9CmdWriter::InitializeWaitRegMemTemplate() {
  memset(&wait_reg_mem_template_, 0, sizeof(wait_reg_mem_template_));
  GenerateCmdHeader(&wait_reg_mem_template_.wait_reg_mem, IT_WAIT_REG_MEM);

  PM4MEC_WAIT_REG_MEM* wait_reg_mem = &wait_reg_mem_template_.wait_reg_mem;

  wait_reg_mem->bitfields7.poll_interval = 0x04;
  wait_reg_mem->bitfields2.operation = operation__mec_wait_reg_mem__wait_reg_mem;
}

void Gfx9CmdWriter::InitializeWriteDataTemplate(PM4MEC_WRITE_DATA* write_data, bool bit32) {
  // Initialize the header of command packet by adjusting the
  // size of payload - one 32bit DWord or two 32bit DWords
  uint32_t cmd_size = (bit32) ? 1 : 2;
  memset(write_data, 0, sizeof(PM4MEC_WRITE_DATA));
  cmd_size = cmd_size + (sizeof(PM4MEC_WRITE_DATA) / sizeof(uint32_t));
  write_data->ordinal1 = PM4_TYPE3_HDR(IT_WRITE_DATA, cmd_size);

  // Set the bit to confirm the write operation and cache policy
  write_data->bitfields2.wr_confirm = wr_confirm__mec_write_data__wait_for_write_confirmation;
  write_data->bitfields2.cache_policy = cache_policy__mec_write_data__stream;

  // Specify the command to increment address if writing more than one DWord
  write_data->bitfields2.addr_incr = addr_incr__mec_write_data__increment_address;

  // Specify the class to which the write destination belongs
  write_data->bitfields2.dst_sel = dst_sel__mec_write_data__memory;
}

void Gfx9CmdWriter::InitializeWriteDataTemplate() {
  InitializeWriteDataTemplate(&write_data_template_.write_data, true);
}

void Gfx9CmdWriter::InitializeWriteData64Template() {
  InitializeWriteDataTemplate(&write_data64_template_.write_data, false);
}

void Gfx9CmdWriter::InitializeConditionalTemplate() {
  /*
  memset(&conditional_template_.conditional, 0, sizeof(conditional_template_));
  GenerateCmdHeader(&conditional_template_.conditional, IT_COND_EXEC);

  if (atc_support_) {
    const uint32_t kAtcShift = 24;
    conditional_template_.conditional.ordinal4 |= 1 << kAtcShift;
  }
  */
}

void Gfx9CmdWriter::InitializeEndOfKernelNotifyTemplate() {
  memset(&notify_template_, 0, sizeof(notify_template_));
  GenerateCmdHeader(&notify_template_.release_mem, IT_RELEASE_MEM);

  // Set the event type to be bottom of pipe and cache policy
  PM4MEC_RELEASE_MEM* rel_mem;
  rel_mem = &notify_template_.release_mem;
  rel_mem->bitfields2.event_type = BOTTOM_OF_PIPE_TS;
  rel_mem->bitfields2.cache_policy = cache_policy__mec_release_mem__stream;
  rel_mem->bitfields2.event_index = event_index__mec_release_mem__end_of_pipe;

  // Specify the attributes of source and destinations of data
  rel_mem->bitfields3.int_sel = int_sel__mec_release_mem__none;
  rel_mem->bitfields3.data_sel = data_sel__mec_release_mem__none;
  rel_mem->bitfields3.dst_sel = dst_sel__mec_release_mem__memory_controller;
}

Gfx9CmdWriter::Gfx9CmdWriter(bool atc_support, bool pcie_atomic_support) {
  // Initialize various state variables related to
  // atomic operations and atc support
  this->atc_support_ = atc_support;
  this->pcie_atomic_support_ = pcie_atomic_support;

  // Initialize various command templates
  InitializeLaunchTemplate();
  InitializeAtomicTemplate();
  InitializeBarrierTemplate();
  InitializeAcquireMemTemplate();
  InitializeWaitRegMemTemplate();
  InitializeWriteDataTemplate();
  InitializeWriteData64Template();
  InitializeConditionalTemplate();
  InitializeEndOfKernelNotifyTemplate();
}

void Gfx9CmdWriter::BuildIndirectBufferCmd(CmdBuf* cmdbuf, const void* cmd_addr,
                                           std::size_t cmd_size) {
  // Verify the address is 4-byte aligned
  uint64_t addr = uintptr_t(cmd_addr);
  assert(!(addr & 0x3) && "IndirectBuffer address must be 4 byte aligned");

  // Specify the address of indirect buffer encoding cmd stream
  LaunchTemplate launch = launch_template_;

  launch.indirect_buffer.bitfields2.ib_base_lo = (PtrLow32(cmd_addr) >> 2);
  launch.indirect_buffer.ib_base_hi = PtrHigh32(cmd_addr);

  // Specify the size of indirect buffer and cache policy to set
  // upon executing the cmds of indirect buffer
  launch.indirect_buffer.bitfields4.priv = 0;
  launch.indirect_buffer.bitfields4.valid = 1;
  launch.indirect_buffer.bitfields4.ib_size = cmd_size / sizeof(uint32_t);
  launch.indirect_buffer.bitfields4.cache_policy = cache_policy__mec_indirect_buffer__stream;

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, launch);
}

void Gfx9CmdWriter::BuildAtomicPacket(CmdBuf* cmdbuf, AtomicType atomic_op, volatile uint32_t* addr,
                                      uint32_t value, uint32_t compare) {
  AtomicTemplate atomicTemplate = atomic_template_;
  PM4MEC_ATOMIC_MEM* atomicCmd = &atomicTemplate.atomic;

  // make sure the destination adddress is aligned
  uint32_t address_low = PtrLow32((void*)addr);
  uint32_t address_high = PtrHigh32((void*)addr);
  assert(!(address_low & 0x7) && "destination address must be 8 byte aligned");
  atomicCmd->addr_lo = address_low;
  atomicCmd->addr_hi = address_high;

  switch (atomic_op) {
    case CommandWriter::kAtomicTypeIncrement:
      assert(!(value != 0x01) && "Atomic Increment value should be 1");
    case CommandWriter::kAtomicAdd:
      atomicCmd->src_data_lo = value;
      atomicCmd->bitfields2.atomic = TC_OP_ATOMIC_ADD_RTN_32;
      break;
    case CommandWriter::kAtomicTypeDecrement:
      assert(!(value != 0x01) && "Atomic Decrement value should be 1");
    case CommandWriter::kAtomicSubtract:
      atomicCmd->src_data_lo = value;
      atomicCmd->bitfields2.atomic = TC_OP_ATOMIC_SUB_RTN_32;
      break;
    case CommandWriter::kAtomicTypeBlockingCompareAndSwap:
      atomicCmd->bitfields9.loop_interval = 128;
      atomicCmd->bitfields2.command = command__mec_atomic_mem__loop_until_compare_satisfied;
    case CommandWriter::kAtomicTypeCompareAndSwap:
      atomicCmd->src_data_lo = value;
      atomicCmd->cmp_data_lo = compare;
      atomicCmd->bitfields2.atomic = TC_OP_ATOMIC_CMPSWAP_RTN_32;
      break;
    case CommandWriter::kAtomicSwap:
      atomicCmd->src_data_lo = value;
      atomicCmd->bitfields2.atomic = TC_OP_ATOMIC_SWAP_RTN_32;
      break;
    default:
      assert((false) && "Atomic operation id is invalid");
  }

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, atomicTemplate);
}

void Gfx9CmdWriter::BuildAtomicPacket64(CmdBuf* cmdbuf, AtomicType atomic_op,
                                        volatile uint64_t* addr, uint64_t value, uint64_t compare) {
  AtomicTemplate atomicTemplate = atomic_template_;
  PM4MEC_ATOMIC_MEM* atomicCmd = &atomicTemplate.atomic;

  // make sure the destination adddress is aligned
  uint32_t address_low = PtrLow32((void*)addr);
  uint32_t address_high = PtrHigh32((void*)addr);
  assert(!(address_low & 0x7) && "destination address must be 8 byte aligned");
  atomicCmd->addr_lo = address_low;
  atomicCmd->addr_hi = address_high;

  switch (atomic_op) {
    case CommandWriter::kAtomicTypeIncrement:
      assert(!(value != 0x01) && "Atomic Increment value should be 1");
    case CommandWriter::kAtomicAdd:
      atomicCmd->src_data_lo = Low32(value);
      atomicCmd->src_data_hi = High32(value);
      atomicCmd->bitfields2.atomic = TC_OP_ATOMIC_ADD_RTN_64;
      break;
    case CommandWriter::kAtomicTypeDecrement:
      assert(!(value != 0x01) && "Atomic Decrement value should be 1");
    case CommandWriter::kAtomicSubtract:
      atomicCmd->src_data_lo = Low32(value);
      atomicCmd->src_data_hi = High32(value);
      atomicCmd->bitfields2.atomic = TC_OP_ATOMIC_SUB_RTN_64;
      break;
    case CommandWriter::kAtomicTypeBlockingCompareAndSwap:
      atomicCmd->bitfields9.loop_interval = 128;
      atomicCmd->bitfields2.command = command__mec_atomic_mem__loop_until_compare_satisfied;
    case CommandWriter::kAtomicTypeCompareAndSwap:
      atomicCmd->src_data_lo = Low32(value);
      atomicCmd->src_data_hi = High32(value);
      atomicCmd->cmp_data_lo = Low32(compare);
      atomicCmd->cmp_data_hi = High32(compare);
      atomicCmd->bitfields2.atomic = TC_OP_ATOMIC_CMPSWAP_RTN_64;
      break;
    case CommandWriter::kAtomicSwap:
      atomicCmd->src_data_lo = Low32(value);
      atomicCmd->src_data_hi = High32(value);
      atomicCmd->bitfields2.atomic = TC_OP_ATOMIC_SWAP_RTN_64;
      break;
    default:
      assert((false) && "Atomic operation id is invalid");
  }

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, atomicTemplate);
}

void Gfx9CmdWriter::BuildBarrierCommand(CmdBuf* cmdBuf) {
  APPEND_COMMAND_WRAPPER(cmdBuf, pending_dispatch_template_);
}

void Gfx9CmdWriter::BuildWriteDataCommand(CmdBuf* cmdbuf, uint32_t* write_addr,
                                          uint32_t write_value) {
  // Copy the initialized command packet and its payload
  WriteDataTemplate command = write_data_template_;

  // Encode the user specified address to write to
  uint64_t addr = uintptr_t(write_addr);
  assert(!(addr & 0x3) && "WriteData address must be 4 byte aligned");

  // Specify the value to write
  command.write_data_value = write_value;

  // Test Code to see if this makes a difference
  command.write_data.dst_mem_addr_hi = PtrHigh32(write_addr);
  command.write_data.bitfields3c.dst_mem_addr_lo = (PtrLow32(write_addr) >> 2);

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, command);
}

void Gfx9CmdWriter::BuildWriteData64Command(CmdBuf* cmdbuf, uint64_t* write_addr,
                                            uint64_t write_value) {
  // Copy the initialized command packet and its payload
  WriteData64Template command = write_data64_template_;

  // Encode the user specified address to write to
  uint64_t addr = uintptr_t(write_addr);
  assert(!(addr & 0x3) && "WriteData address must be 4 byte aligned");

  command.write_data.bitfields3c.dst_mem_addr_lo = (PtrLow32(write_addr) >> 2);
  command.write_data.dst_mem_addr_hi = PtrHigh32(write_addr);

  // Specify the value to write
  command.write_data_value = write_value;

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, command);
}

void Gfx9CmdWriter::BuildWaitRegMemCommand(CmdBuf* cmdbuf, bool mem_space, uint64_t wait_addr,
                                           bool func_eq, uint32_t mask_val, uint32_t wait_val) {
  WaitRegMemTemplate wait_cmd = wait_reg_mem_template_;

  // Apply the space to which addr belongs
  if (mem_space) {
    wait_cmd.wait_reg_mem.bitfields2.mem_space = mem_space__mec_wait_reg_mem__memory_space;
  } else {
    wait_cmd.wait_reg_mem.bitfields2.mem_space = mem_space__mec_wait_reg_mem__register_space;
  }

  // Apply the function - equal / not equal desired by user
  if (func_eq) {
    wait_cmd.wait_reg_mem.bitfields2.function =
        function__mec_wait_reg_mem__equal_to_the_reference_value;
  } else {
    wait_cmd.wait_reg_mem.bitfields2.function =
        function__mec_wait_reg_mem__not_equal_reference_value;
  }

  // Value to use in applying equal / not equal function
  wait_cmd.wait_reg_mem.reference = wait_val;

  // Apply the mask on value at address/register
  wait_cmd.wait_reg_mem.mask = mask_val;

  // The address to poll should be DWord (4 byte) aligned
  // Update upper 32 bit address if addr is not a register
  if (mem_space) {
    assert(!(wait_addr & 0x3) && "WaitRegMem address must be 4 byte aligned");
  }
  wait_cmd.wait_reg_mem.bitfields3a.mem_poll_addr_lo = (Low32(wait_addr) >> 2);
  if (mem_space) {
    wait_cmd.wait_reg_mem.mem_poll_addr_hi = High32(wait_addr);
  }

  // Append the command to cmd stream
  APPEND_COMMAND_WRAPPER(cmdbuf, wait_cmd);
}

void Gfx9CmdWriter::BuildConditionalExecute(CmdBuf* cmdbuf, uint32_t* signal, uint16_t count) {
  assert(false && "BuildConditionalExecute method is not implemented");
  /*
  ConditionalExecuteTemplate conditional = conditional_template_;

  uint32_t address_low = PtrLow32(signal);
  uint32_t address_high = PtrHigh32(signal);
  assert(!(address_low & 0x7) && "destination address must be 8 byte aligned");

  conditional.conditional.boolAddrLo = address_low;
  conditional.conditional.boolAddrHi = address_high;
  conditional.conditional.execCount = count;

  APPEND_COMMAND_WRAPPER(cmdbuf, conditional);
  */
}

void Gfx9CmdWriter::BuildUpdateHostAddress(CmdBuf* cmdbuf, uint64_t* addr, int64_t value) {
  // If Atomics are supported, use it
  if (pcie_atomic_support_) {
    BuildAtomicPacket64(cmdbuf, CommandWriter::AtomicType::kAtomicSwap, (volatile uint64_t*)addr,
                        value);
    return;
  }

  BuildWriteData64Command(cmdbuf, addr, value);
  return;
}

void Gfx9CmdWriter::BuildBOPNotifyCmd(CmdBuf* cmdbuf, const void* write_addr, uint32_t write_value,
                                      bool interrupt) {
  // Initialize the command including its header
  EndofKernelNotifyTemplate eop = notify_template_;
  PM4MEC_RELEASE_MEM* rel_mem = &eop.release_mem;

  // Program CP to perform various cache operations
  // before issuing the write operation commences
  rel_mem->bitfields2.tc_action_ena = true;
  rel_mem->bitfields2.tc_wb_action_ena = true;

  // Update cmd to write a user specified 32-bit value
  rel_mem->data_lo = write_value;
  rel_mem->bitfields3.data_sel = data_sel__mec_release_mem__send_32_bit_low;

  // Update cmd with user specified address to write to
  rel_mem->address_hi = High32(uint64_t(write_addr));
  rel_mem->bitfields4b.address_lo_64b = (Low32(uint64_t(write_addr) >> 3));

  // Update cmd to issue interrupt if user has requested it
  if (interrupt) {
    rel_mem->bitfields3.int_sel = int_sel__mec_release_mem__send_interrupt_after_write_confirm;
  }

  // Serialize the command as stream of Dwords
  APPEND_COMMAND_WRAPPER(cmdbuf, eop);
}

void Gfx9CmdWriter::BuildBarrierFenceCommands(CmdBuf* cmdbuf) {
  // TODO: temporarily remove the check because some OpenCL tests
  // (test_buffers, test_relationals) are failing.
  //    if (using_cc_memory_policy_)
  //        return;
  AcquireMemTemplate invalidate_src_caches = invalidate_cache_template_;

  // wbINVL2 by default writes-back and invalidates both L1 and L2
  invalidate_src_caches.acquire_mem.bitfields2.coher_cntl = CP_COHER_CNTL__TC_ACTION_ENA_MASK;
  invalidate_src_caches.acquire_mem.bitfields2.coher_cntl |= CP_COHER_CNTL__TC_WB_ACTION_ENA_MASK;

  APPEND_COMMAND_WRAPPER(cmdbuf, invalidate_src_caches);
}

/*
// PM4 packet for profilers
#define PM4_PACKET3 (0xC0000000)
#define PM4_PACKET3_CMD_SHIFT 8
#define PM4_PACKET3_COUNT_SHIFT 16

#define PACKET3(cmd, count)                                 \
  (PM4_PACKET3 | (((count)-1) << PM4_PACKET3_COUNT_SHIFT) | \
   ((cmd) << PM4_PACKET3_CMD_SHIFT))
*/

// Structure to store the event PM4 packet
typedef struct WriteRegPacket_ { uint32_t item[3]; } WriteRegPacket;

void Gfx9CmdWriter::BuildWriteEventPacket(CmdBuf* cmdbuf, uint32_t event) {
  PM4MEC_EVENT_WRITE cp_event_initiator;
  memset(&cp_event_initiator, 0, sizeof(PM4MEC_EVENT_WRITE));
  cp_event_initiator.ordinal1 =
      PM4_TYPE3_HDR(IT_EVENT_WRITE, (sizeof(PM4MEC_EVENT_WRITE) / sizeof(uint32_t)));
  cp_event_initiator.ordinal2 = 0;

  VGT_EVENT_TYPE eventType = Reserved_0x00;
  switch (event) {
    case kPerfCntrsStart:
      eventType = PERFCOUNTER_START;
      break;
    case kPerfCntrsStop:
      eventType = PERFCOUNTER_STOP;
      break;
    case kPerfCntrsSample:
      eventType = PERFCOUNTER_SAMPLE;
      break;
    default:
      assert(false && "Illegal VGT Event Id");
  }

  MEC_EVENT_WRITE_event_index_enum index;
  index = event_index__mec_event_write__other;
  cp_event_initiator.bitfields2.event_index = index;
  cp_event_initiator.bitfields2.event_type = eventType;

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, cp_event_initiator);
}

void Gfx9CmdWriter::BuildWriteUnshadowRegPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value) {
  WriteRegPacket packet;
  packet.item[0] =
      PM4_TYPE3_HDR(IT_SET_UCONFIG_REG, (1 + sizeof(PM4MEC_SET_CONFIG_REG) / sizeof(uint32_t)));
  packet.item[1] = (addr - UCONFIG_SPACE_START);
  packet.item[2] = value;

  APPEND_COMMAND_WRAPPER(cmdbuf, packet);
}

void Gfx9CmdWriter::BuildWriteUConfigRegPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value) {
  WriteRegPacket packet;
  packet.item[0] =
      PM4_TYPE3_HDR(IT_SET_UCONFIG_REG, (1 + sizeof(PM4MEC_SET_CONFIG_REG) / sizeof(uint32_t)));
  packet.item[1] = (addr - UCONFIG_SPACE_START);
  packet.item[2] = value;

  APPEND_COMMAND_WRAPPER(cmdbuf, packet);
}

void Gfx9CmdWriter::BuildWriteShRegPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value) {
  WriteRegPacket packet;
  packet.item[0] =
      PM4_TYPE3_HDR(IT_SET_SH_REG, (1 + sizeof(PM4MEC_SET_CONFIG_REG) / sizeof(uint32_t)));
  packet.item[1] = (addr - PERSISTENT_SPACE_START);
  packet.item[2] = value;

  APPEND_COMMAND_WRAPPER(cmdbuf, packet);
}

void Gfx9CmdWriter::BuildCopyDataPacket(CmdBuf* cmdbuf, uint32_t src_sel, uint32_t src_addr_lo,
                                        uint32_t src_addr_hi, uint32_t* dst_addr, uint32_t size,
                                        bool wait) {
  PM4MEC_COPY_DATA cmd_data;
  memset(&cmd_data, 0, sizeof(PM4MEC_COPY_DATA));
  cmd_data.ordinal1 = PM4_TYPE3_HDR(IT_COPY_DATA, (sizeof(PM4MEC_COPY_DATA) / sizeof(uint32_t)));

  MEC_COPY_DATA_src_sel_enum data_src = src_sel__mec_copy_data__memory;
  switch (src_sel) {
    case 0:
      data_src = src_sel__mec_copy_data__mem_mapped_register;
      break;
    case 4:
      data_src = src_sel__mec_copy_data__perfcounters;
      break;
    default:
      assert(false && "CopyData Illegal value for source of data");
      break;
  }
  cmd_data.bitfields2.src_sel = data_src;
  cmd_data.bitfields2.src_cache_policy = src_cache_policy__mec_copy_data__stream;

  cmd_data.bitfields2.dst_sel = dst_sel__mec_copy_data__memory;
  cmd_data.bitfields2.dst_cache_policy = dst_cache_policy__mec_copy_data__stream;

  cmd_data.bitfields2.wr_confirm = (MEC_COPY_DATA_wr_confirm_enum)wait;
  cmd_data.bitfields2.count_sel = (size == 0) ? count_sel__mec_copy_data__32_bits_of_data
                                              : count_sel__mec_copy_data__64_bits_of_data;

  // Specify the source register offset
  cmd_data.bitfields3a.src_reg_offset = src_addr_lo;

  // Specify the destination memory address
  cmd_data.dst_addr_hi = PtrHigh32(dst_addr);
  if (size == 0) {
    cmd_data.bitfields5b.dst_32b_addr_lo = (PtrLow32(dst_addr) >> 2);
  } else {
    cmd_data.bitfields5c.dst_64b_addr_lo = (PtrLow32(dst_addr) >> 3);
  }

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, cmd_data);
}

void Gfx9CmdWriter::BuildCacheFlushPacket(CmdBuf* cmdbuf) {
  // Initialize the command header
  PM4MEC_ACQUIRE_MEM cache_flush = invalidate_cache_template_.acquire_mem;

  // Program Coherence Control Register. Initialize L2 Cache flush
  // for Non-Coherent memory blocks
  uint32_t coher_cntl = 0;

  coher_cntl |= CP_COHER_CNTL__TC_ACTION_ENA_MASK;
  coher_cntl |= CP_COHER_CNTL__TCL1_ACTION_ENA_MASK;
  coher_cntl |= CP_COHER_CNTL__TC_WB_ACTION_ENA_MASK;
  coher_cntl |= CP_COHER_CNTL__SH_ICACHE_ACTION_ENA_MASK;
  coher_cntl |= CP_COHER_CNTL__SH_KCACHE_ACTION_ENA_MASK;
  cache_flush.bitfields2.coher_cntl = coher_cntl;

  // Copy AcquireMem command buffer stream
  APPEND_COMMAND_WRAPPER(cmdbuf, cache_flush);
}

void Gfx9CmdWriter::BuildWriteWaitIdlePacket(CmdBuf* cmdbuf) {
  BuildBarrierCommand(cmdbuf);
  BuildCacheFlushPacket(cmdbuf);
}

// Will issue a VGT event including a cache flush later on
void Gfx9CmdWriter::BuildVgtEventPacket(CmdBuf* cmdbuf, uint32_t vgtEvent) {
  PM4MEC_EVENT_WRITE cp_event_initiator;
  memset(&cp_event_initiator, 0, sizeof(PM4MEC_EVENT_WRITE));
  cp_event_initiator.ordinal1 =
      PM4_TYPE3_HDR(IT_EVENT_WRITE, (sizeof(PM4MEC_EVENT_WRITE) / sizeof(uint32_t)));
  cp_event_initiator.ordinal2 = 0;

  VGT_EVENT_TYPE eventType = Reserved_0x00;
  switch (vgtEvent) {
    case kPerfCntrsStart:
      eventType = PERFCOUNTER_START;
      break;
    case kPerfCntrsStop:
      eventType = PERFCOUNTER_STOP;
      break;
    case kPerfCntrsSample:
      eventType = PERFCOUNTER_SAMPLE;
      break;
    case kThrdTraceStart:
      eventType = THREAD_TRACE_START;
      break;
    case kThrdTraceStop:
      eventType = THREAD_TRACE_STOP;
      break;
    case kThrdTraceFlush:
      eventType = THREAD_TRACE_FLUSH;
      break;
    case kThrdTraceFinish:
      eventType = THREAD_TRACE_FINISH;
      break;
    default:
      assert(false && "Illegal VGT Event Id");
  }

  MEC_EVENT_WRITE_event_index_enum index;
  index = event_index__mec_event_write__other;
  cp_event_initiator.bitfields2.event_index = index;
  cp_event_initiator.bitfields2.event_type = eventType;

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, cp_event_initiator);

  // Check If I should be issuing a cache flush operation as well
  // test and remove it
  BuildCacheFlushPacket(cmdbuf);
}

void Gfx9CmdWriter::BuildWriteRegisterPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value) {
  /*
  WriteRegPacket packet;
  packet.item[0] = (PM4_TYPE3_HDR(
      IT_SET_CONFIG_REG, 1 + PM4_CMD_SET_CONFIG_REG_DWORDS, ShaderGraphics, 0));
  packet.item[1] = addr - CONFIG_SPACE_START;
  packet.item[2] = value;

  APPEND_COMMAND_WRAPPER(cmdbuf, packet);

  return;
  */
}

void Gfx9CmdWriter::BuildWriteEventQueryPacket(CmdBuf* cmdbuf, uint32_t event, uint32_t* addr) {
  PM4MEC_EVENT_WRITE_QUERY cp_event_initiator;
  memset(&cp_event_initiator, 0, sizeof(PM4MEC_EVENT_WRITE_QUERY));
  cp_event_initiator.ordinal1 =
      PM4_TYPE3_HDR(IT_EVENT_WRITE, (sizeof(PM4MEC_EVENT_WRITE_QUERY) / sizeof(uint32_t)));
  cp_event_initiator.ordinal2 = 0;

  // Update switch statements you want to support
  VGT_EVENT_TYPE eventType = Reserved_0x00;
  switch (event) {
    default:
      assert(false && "Illegal VGT Event Id");
  }

  MEC_EVENT_WRITE_event_index_enum index;
  cp_event_initiator.bitfields2.event_type = eventType;
  index = (MEC_EVENT_WRITE_event_index_enum)EventTypeToIndexTable[eventType];
  cp_event_initiator.bitfields2.event_index = index;

  // set the address
  uint32_t addrLo = PtrLow32(addr);
  uint32_t addrHi = PtrHigh32(addr);
  ((addrLo & 0x7) != 0) ? assert(false) : assert(true);

  cp_event_initiator.address_hi = addrHi;
  cp_event_initiator.bitfields3.address_lo = (addrLo >> 3);

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, cp_event_initiator);
}

size_t Gfx9CmdWriter::SizeOfAtomicPacket() const {
  return sizeof(AtomicTemplate) / sizeof(uint32_t);
}

void Gfx9CmdWriter::BuildFlushCacheCmd(CmdBuf* cmdbuf, FlushCacheOptions* options,
                                       uint32_t* writeAddr, uint32_t writeVal) {
  PM4MEC_ACQUIRE_MEM cache_flush = invalidate_cache_template_.acquire_mem;

  // Verify write back address is valid. Note that this address is NOT
  // used on CI. But to have a same interface as that on SI, we keep
  // the address argument in this function. Thus, this check always pass
  // no matter the address is NULL or not.
  (writeAddr == NULL) ? assert(true) : assert(true);

  // Program Coherence Control Register. Initialize L2 Cache flush
  // for Non-Coherent memory blocks
  uint32_t coher_cntl = 0;
  coher_cntl |= (options->l1) ? CP_COHER_CNTL__TCL1_ACTION_ENA_MASK : 0;
  coher_cntl |= (options->l2)
      ? (CP_COHER_CNTL__TC_ACTION_ENA_MASK | CP_COHER_CNTL__TC_WB_ACTION_ENA_MASK)
      : 0;
  coher_cntl |= (options->icache) ? CP_COHER_CNTL__SH_ICACHE_ACTION_ENA_MASK : 0;
  coher_cntl |= (options->kcache) ? CP_COHER_CNTL__SH_KCACHE_ACTION_ENA_MASK : 0;
  cache_flush.bitfields2.coher_cntl = coher_cntl;

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, cache_flush);
  return;
}

void Gfx9CmdWriter::BuildDmaDataPacket(CmdBuf* cmdbuf, uint32_t* srcAddr, uint32_t* dstAddr,
                                       uint32_t copySize, bool waitForConfirm) {
  PM4MEC_DMA_DATA cmdDmaData;
  memset(&cmdDmaData, 0, sizeof(PM4MEC_DMA_DATA));
  cmdDmaData.header.u32All =
      PM4_TYPE3_HDR(IT_DMA_DATA, (sizeof(PM4MEC_DMA_DATA) / sizeof(uint32_t)));

  // Specify attributes of source buffer such as its
  // location and Cache policy
  cmdDmaData.bitfields2.src_sel = src_sel__mec_dma_data__src_addr_using_sas;
  cmdDmaData.bitfields2.src_cache_policy = src_cache_policy__mec_dma_data__stream;

  // Specify attributes of destination buffer such as its
  // location and Cache policy
  cmdDmaData.bitfields2.dst_sel = dst_sel__mec_dma_data__dst_addr_using_das;
  cmdDmaData.bitfields2.dst_cache_policy = dst_cache_policy__mec_dma_data__stream;

  // Specify the source and destination addr
  cmdDmaData.src_addr_lo_or_data = PtrLow32(srcAddr);
  cmdDmaData.src_addr_hi = PtrHigh32(srcAddr);
  cmdDmaData.dst_addr_lo = PtrLow32(dstAddr);
  cmdDmaData.dst_addr_hi = PtrHigh32(dstAddr);

  // Number of bytes to copy. The command restricts
  // the size to be (64 MB - 1) - 26 Bits
  assert(copySize < 0x1FFFFF);
  cmdDmaData.bitfields7.byte_count = copySize;

  // Indicate that DMA Cmd should wait if its source
  // is the destination of a previous DMA Cmd
  cmdDmaData.bitfields7.raw_wait = waitForConfirm;

  APPEND_COMMAND_WRAPPER(cmdbuf, cmdDmaData);
  return;
}


}  // gfx9 namespace

}  // pm4_profile
