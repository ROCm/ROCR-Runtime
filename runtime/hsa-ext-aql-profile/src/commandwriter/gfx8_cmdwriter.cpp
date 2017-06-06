#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "gfx8_cmdwriter.h"
#include "gfxip/gfx8/gfx8_utils.h"

// RELEASE MEM DST SEL Definitions
#define RELEASE_MEM_DST_SEL_MEMORY_CONTROLLER 0
#define RELEASE_MEM_DST_SEL_TC_L2 1

// RELEASE MEM CACHE POLICY Definitions
#define RELEASE_MEM_CACHE_POLICY_LRU 0
#define RELEASE_MEM_CACHE_POLICY_STREAM 1
#define RELEASE_MEM_CACHE_POLICY_BYPASS 2

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
namespace gfx8 {

template <class T> void Gfx8CmdWriter::AppendCommand(CmdBuf* cmdbuf, const T& command) {
  cmdbuf->AppendCommand(&command, sizeof(command));
}

void Gfx8CmdWriter::InitializeAtomicTemplate() {
  memset(&atomic_template_.atomic, 0, sizeof(atomic_template_));
  GenerateCmdHeader(&atomic_template_.atomic, IT_ATOMIC_MEM__CI);

  if (atc_support_) {
    const uint32_t kAtcShift = 24;
    atomic_template_.atomic.ordinal2 |= 1 << kAtcShift;
  }
}

void Gfx8CmdWriter::InitializeConditionalTemplate() {
  memset(&conditional_template_.conditional, 0, sizeof(conditional_template_));
  gfx8::GenerateCmdHeader(&conditional_template_.conditional, IT_COND_EXEC);

  if (atc_support_) {
    const uint32_t kAtcShift = 24;
    conditional_template_.conditional.ordinal4 |= 1 << kAtcShift;
  }
}

void Gfx8CmdWriter::InitializeLaunchTemplate() {
  memset(&launch_template_, 0, sizeof(launch_template_));

  GenerateCmdHeader(&launch_template_.indirect_buffer, IT_INDIRECT_BUFFER);
  launch_template_.indirect_buffer.CI.valid = true;
}

void Gfx8CmdWriter::InitializeWriteDataTemplate() {
  // Set the header of write data command
  memset(&write_data_template_, 0, sizeof(write_data_template_));

  // Initialize the header of command packet
  PM4CMDWRITEDATA* command = &(write_data_template_.write_data);
  uint32_t cmd_size = sizeof(write_data_template_) / sizeof(uint32_t);
  command->ordinal1 = PM4_TYPE_3_HDR(IT_WRITE_DATA, cmd_size, ShaderCompute, 0);

  // Set the ATC bit of command template - specifies if the address
  // belongs to system memory
  write_data_template_.write_data.atc__CI = (atc_support_) ? 1 : 0;

  // Set the bit to confirm the write operation and cache policy
  write_data_template_.write_data.wrConfirm = 1;
  write_data_template_.write_data.cachePolicy__CI = WRITE_DATA_CACHE_POLICY_BYPASS;

  // Specify the module that will execute the write data command
  write_data_template_.write_data.engineSel = WRITE_DATA_ENGINE_ME;

  // Specify the class to which the write destination belongs
  write_data_template_.write_data.dstSel = WRITE_DATA_DST_SEL_MEMORY_ASYNC;
}

void Gfx8CmdWriter::InitializeWriteData64Template() {
  // Set the header of write data command
  memset(&write_data64_template_, 0, sizeof(write_data64_template_));

  // Initialize the header of command packet
  PM4CMDWRITEDATA* command = &(write_data64_template_.write_data);
  uint32_t cmd_size = sizeof(write_data64_template_) / sizeof(uint32_t);
  command->ordinal1 = PM4_TYPE_3_HDR(IT_WRITE_DATA, cmd_size, ShaderCompute, 0);

  // Set the ATC bit of command template - specifies if the address
  // belongs to system memory
  write_data64_template_.write_data.atc__CI = (atc_support_) ? 1 : 0;

  // Set the bit to confirm the write operation and cache policy
  write_data64_template_.write_data.wrConfirm = 1;
  write_data64_template_.write_data.cachePolicy__CI = WRITE_DATA_CACHE_POLICY_BYPASS;

  // Specify the module that will execute the write data command
  write_data64_template_.write_data.engineSel = WRITE_DATA_ENGINE_ME;

  // Specify the class to which the write destination belongs
  // write_data64_template_.write_data.dstSel = WRITE_DATA_DST_SEL_TCL2;
  // TODO: For Hawaii bring up only.
  write_data64_template_.write_data.dstSel = WRITE_DATA_DST_SEL_MEMORY_ASYNC;
}

void Gfx8CmdWriter::InitializeBarrierTemplate() {
  memset(&pending_dispatch_template_, 0, sizeof(pending_dispatch_template_));

  gfx8::GenerateCmdHeader(&pending_dispatch_template_.event_write, IT_EVENT_WRITE);
  pending_dispatch_template_.event_write.eventType = CS_PARTIAL_FLUSH;
  pending_dispatch_template_.event_write.eventIndex = EventTypeToIndexTable[CS_PARTIAL_FLUSH];
}

void Gfx8CmdWriter::InitializeAcquireMemTemplate() {
  memset(&invalidate_cache_template_, 0, sizeof(invalidate_cache_template_));

  gfx8::GenerateCmdHeader(&invalidate_cache_template_.acquire_mem, IT_ACQUIRE_MEM__CI__VI);
  invalidate_cache_template_.acquire_mem.cpCoherBase.u32All = 0x00;
  invalidate_cache_template_.acquire_mem.cpCoherBaseHi.u32All = 0x00;
  invalidate_cache_template_.acquire_mem.cpCoherSize.u32All = 0xFFFFFFFF;
  invalidate_cache_template_.acquire_mem.cpCoherSizeHi.u32All = 0xFF;
  invalidate_cache_template_.acquire_mem.pollInterval = 0;
}

void Gfx8CmdWriter::InitializeWaitRegMemTemplate() {
  memset(&wait_reg_mem_template_, 0, sizeof(wait_reg_mem_template_));

  gfx8::GenerateCmdHeader(&wait_reg_mem_template_.wait_reg_mem, IT_WAIT_REG_MEM);
  wait_reg_mem_template_.wait_reg_mem.atc__CI = (atc_support_) ? 1 : 0;
  wait_reg_mem_template_.wait_reg_mem.cachePolicy__CI = 2;  // bypass
  wait_reg_mem_template_.wait_reg_mem.pollInterval = 0;
  wait_reg_mem_template_.wait_reg_mem.engine = WAIT_REG_MEM_ENGINE_ME;
}

Gfx8CmdWriter::Gfx8CmdWriter(bool atc_support, bool pcie_atomic_support) {
  // Initialize various state variables related to
  // atomic operations and atc support
  pcie_atomic_support_ = pcie_atomic_support;
  atc_support_ = atc_support;

  InitializeLaunchTemplate();
  InitializeAtomicTemplate();
  InitializeConditionalTemplate();
  InitializeWriteDataTemplate();
  InitializeWriteData64Template();
  InitializeBarrierTemplate();
  InitializeAcquireMemTemplate();
  InitializeWaitRegMemTemplate();
}

void Gfx8CmdWriter::BuildWaitRegMemCommand(CmdBuf* cmdbuf, bool mem_space, uint64_t wait_addr,
                                              bool func_eq, uint32_t mask_val, uint32_t wait_val) {
  gfx8::WaitRegMemTemplate wait_cmd = wait_reg_mem_template_;

  // Apply the space to which addr belongs
  if (mem_space) {
    wait_cmd.wait_reg_mem.memSpace = WAIT_REG_MEM_SPACE_MEMORY;
  } else {
    wait_cmd.wait_reg_mem.memSpace = WAIT_REG_MEM_SPACE_REGISTER;
  }

  // Apply the function - equal / not equal desired by user
  if (func_eq) {
    wait_cmd.wait_reg_mem.function = WAIT_REG_MEM_FUNC_EQUAL;
  } else {
    wait_cmd.wait_reg_mem.function = WAIT_REG_MEM_FUNC_NOT_EQUAL;
  }

  // Apply the mask on value at address/register
  wait_cmd.wait_reg_mem.mask = mask_val;

  // Value to use in applying equal / not equal function
  wait_cmd.wait_reg_mem.reference = wait_val;

  // Update upper 32 bit address if addr is not a register
  if (mem_space) {
    assert(!(wait_addr & 0x3) && "WaitRegMem address must be 4 byte aligned");
  }
  wait_cmd.wait_reg_mem.pollAddressLo = Low32(wait_addr);
  if (mem_space) {
    wait_cmd.wait_reg_mem.pollAddressHi = High32(wait_addr);
  }

  APPEND_COMMAND_WRAPPER(cmdbuf, wait_cmd);
}

void Gfx8CmdWriter::BuildUpdateHostAddress(CmdBuf* cmdbuf, uint64_t* addr, int64_t value) {
  // If Atomics are supported, use it
  if (pcie_atomic_support_) {
    BuildAtomicPacket64(cmdbuf, CommandWriter::AtomicType::kAtomicSwap, (volatile uint64_t*)addr,
                        value);
    return;
  }

  BuildWriteData64Command(cmdbuf, addr, value);
  return;
}

void Gfx8CmdWriter::BuildIndirectBufferCmd(CmdBuf* cmdbuf, const void* cmd_addr,
                                              std::size_t cmd_size) {
  gfx8::LaunchTemplate launch = launch_template_;

  launch.indirect_buffer.ibBaseLo = PtrLow32(cmd_addr);
  launch.indirect_buffer.ibBaseHi = PtrHigh32(cmd_addr);
  launch.indirect_buffer.CI.ibSize = cmd_size / sizeof(uint32_t);

  APPEND_COMMAND_WRAPPER(cmdbuf, launch);
}

void Gfx8CmdWriter::BuildBOPNotifyCmd(CmdBuf* cmdbuf, const void* write_addr, uint32_t write_val,
                                         bool interrupt) {
  // Initialize the command including its header
  gfx8::EndofKernelNotifyTemplate eopCmd;
  memset(&eopCmd, 0, sizeof(eopCmd));
  gfx8::GenerateCmdHeader(&eopCmd.release_mem, IT_RELEASE_MEM__CI__VI);

  // Program CP to wait until following event is notified by SPI
  eopCmd.release_mem.eventType = BOTTOM_OF_PIPE_TS;
  eopCmd.release_mem.eventIndex = EventTypeToIndexTable[BOTTOM_OF_PIPE_TS];

  // Program CP to perform various cache operations
  // which complete before Write operation commences
  eopCmd.release_mem.atc = atc_support_;
  eopCmd.release_mem.l2Invlidate = true;
  eopCmd.release_mem.l2WriteBack = true;

  // Set destination as Memory with Write bypassing Cache
  eopCmd.release_mem.cachePolicy = RELEASE_MEM_CACHE_POLICY_BYPASS;
  eopCmd.release_mem.dstSel = RELEASE_MEM_DST_SEL_MEMORY_CONTROLLER;

  // Program CP to write user specified value to user specified address
  eopCmd.release_mem.ordinal4 = Low32(uint64_t(write_addr));
  eopCmd.release_mem.addrHi = High32(uint64_t(write_addr));
  eopCmd.release_mem.dataLo = Low32(write_val);
  eopCmd.release_mem.dataHi = High32(write_val);
  eopCmd.release_mem.dataSel = EVENTWRITEEOP_DATA_SEL_SEND_DATA32;

  // Determine if host will poll or wait for interrupt
  eopCmd.release_mem.intSel =
      (interrupt == false) ? EVENTWRITEEOP_INT_SEL_NONE : EVENTWRITEEOP_INT_SEL_SEND_INT_ON_CONFIRM;

  APPEND_COMMAND_WRAPPER(cmdbuf, eopCmd);
}


void Gfx8CmdWriter::BuildBarrierFenceCommands(CmdBuf* cmdbuf) {
  gfx8::AcquireMemTemplate invalidate_src_caches = invalidate_cache_template_;

  // wbINVL2 by default writes-back and invalidates both L1 and L2
  invalidate_src_caches.acquire_mem.coherCntl =
      CP_COHER_CNTL__TC_ACTION_ENA_MASK | CP_COHER_CNTL__TC_WB_ACTION_ENA_MASK__CI__VI;

  APPEND_COMMAND_WRAPPER(cmdbuf, invalidate_src_caches);
}

// PM4 packet for profilers
#define PM4_PACKET3 (0xC0000000)
#define PM4_PACKET3_CMD_SHIFT 8
#define PM4_PACKET3_COUNT_SHIFT 16

#define PACKET3(cmd, count) \
  (PM4_PACKET3 | (((count)-1) << PM4_PACKET3_COUNT_SHIFT) | ((cmd) << PM4_PACKET3_CMD_SHIFT))

// Structure to store the event PM4 packet
typedef struct WriteRegPacket_ { uint32_t item[3]; } WriteRegPacket;

typedef struct WriteEventPacket_ { uint32_t item[7]; } WriteEventPacket;

void Gfx8CmdWriter::BuildWriteEventPacket(CmdBuf* cmdbuf, uint32_t event) {

  PM4CMDEVENTWRITE cp_event_initiator;
  cp_event_initiator.ordinal1 = PACKET3(IT_EVENT_WRITE, 1);
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

  cp_event_initiator.eventType = eventType;
  cp_event_initiator.eventIndex = EventTypeToIndexTable[eventType];

  APPEND_COMMAND_WRAPPER(cmdbuf, cp_event_initiator);

  return;
}

void Gfx8CmdWriter::BuildWriteUnshadowRegPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value) {
  WriteRegPacket packet;
  packet.item[0] = (PM4_TYPE_3_HDR(IT_SET_UCONFIG_REG__CI__VI, 1 + PM4_CMD_SET_CONFIG_REG_DWORDS,
                                   ShaderGraphics, 0));
  packet.item[1] = (addr - UCONFIG_SPACE_START__CI__VI);
  packet.item[2] = value;

  APPEND_COMMAND_WRAPPER(cmdbuf, packet);

  return;
}

void Gfx8CmdWriter::BuildWriteUConfigRegPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value) {
  WriteRegPacket packet;
  packet.item[0] = (PM4_TYPE_3_HDR(IT_SET_UCONFIG_REG__CI__VI, 1 + PM4_CMD_SET_CONFIG_REG_DWORDS,
                                   ShaderCompute, 0));
  packet.item[1] = (addr - UCONFIG_SPACE_START__CI__VI);
  packet.item[2] = value;

  APPEND_COMMAND_WRAPPER(cmdbuf, packet);

  return;
}

void Gfx8CmdWriter::BuildWriteShRegPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value) {
  WriteRegPacket packet;
  packet.item[0] = (PM4_TYPE_3_HDR(IT_SET_SH_REG, 1 + PM4_CMD_SET_SH_REG_DWORDS, ShaderCompute, 0));
  packet.item[1] = (addr - PERSISTENT_SPACE_START);
  packet.item[2] = value;

  APPEND_COMMAND_WRAPPER(cmdbuf, packet);

  return;
}

void Gfx8CmdWriter::BuildCopyDataPacket(CmdBuf* cmdbuf, uint32_t src_sel, uint32_t src_addr_lo,
                                           uint32_t src_addr_hi, uint32_t* dst_addr, uint32_t size,
                                           bool wait) {
  PM4CMDCOPYDATA cmd_data;
  memset(&cmd_data, 0, sizeof(PM4CMDCOPYDATA));

  cmd_data.header.u32All = PACKET3(IT_COPY_DATA, 5);

  cmd_data.srcAtc__CI = atc_support_;
  cmd_data.srcCachePolicy__CI = COPY_DATA_SRC_CACHE_POLICY_BYPASS;
  cmd_data.srcSel = src_sel;

  cmd_data.dstAtc__CI = atc_support_;
  cmd_data.dstSel = COPY_DATA_SEL_DST_ASYNC_MEMORY;
  cmd_data.dstCachePolicy__CI = COPY_DATA_DST_CACHE_POLICY_BYPASS;

  uint32_t dst_addr_lo, dst_addr_hi;

  dst_addr_lo = PtrLow32(dst_addr);
  dst_addr_hi = PtrHigh32(dst_addr);

  cmd_data.srcAddressLo = src_addr_lo;
  cmd_data.srcAddressHi = src_addr_hi;
  cmd_data.dstAddressLo = dst_addr_lo;
  cmd_data.dstAddressHi = dst_addr_hi;

  cmd_data.countSel = size;
  cmd_data.wrConfirm = wait;
  cmd_data.engineSel = COPY_DATA_ENGINE_ME;

  APPEND_COMMAND_WRAPPER(cmdbuf, cmd_data);

  return;
}

void Gfx8CmdWriter::BuildCacheFlushPacket(CmdBuf* cmdbuf) {
  WriteEventPacket packet;
  packet.item[0] = PACKET3(IT_ACQUIRE_MEM__CI__VI, 6);
  packet.item[1] = 0x28C00000;
  packet.item[2] = 0xFFFFFFFF;
  packet.item[3] = 0;
  packet.item[4] = 0;
  packet.item[5] = 0;
  packet.item[6] = 0x00000004;

  APPEND_COMMAND_WRAPPER(cmdbuf, packet);
}

void Gfx8CmdWriter::BuildWriteWaitIdlePacket(CmdBuf* cmdbuf) {
  BuildBarrierCommand(cmdbuf);
  BuildCacheFlushPacket(cmdbuf);
  return;
}

// Will issue a VGT event including a cache flush later on
void Gfx8CmdWriter::BuildVgtEventPacket(CmdBuf* cmdbuf, uint32_t vgtEvent) {
  PM4CMDEVENTWRITE cp_event_initiator;

  cp_event_initiator.ordinal1 = PACKET3(IT_EVENT_WRITE, 1);
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

  cp_event_initiator.eventType = eventType;
  cp_event_initiator.eventIndex = EventTypeToIndexTable[eventType];

  APPEND_COMMAND_WRAPPER(cmdbuf, cp_event_initiator);

  // Check If I should be issuing a cache flush operation as well
  // test and remove it
  BuildCacheFlushPacket(cmdbuf);
  return;
}

void Gfx8CmdWriter::BuildWriteRegisterPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value) {
  WriteRegPacket packet;
  packet.item[0] =
      (PM4_TYPE_3_HDR(IT_SET_CONFIG_REG, 1 + PM4_CMD_SET_CONFIG_REG_DWORDS, ShaderGraphics, 0));
  packet.item[1] = addr - CONFIG_SPACE_START;
  packet.item[2] = value;

  APPEND_COMMAND_WRAPPER(cmdbuf, packet);

  return;
}

void Gfx8CmdWriter::BuildWriteEventQueryPacket(CmdBuf* cmdbuf, uint32_t event, uint32_t* addr) {
  PM4CMDEVENTWRITEQUERY cp_event_initiator;
  cp_event_initiator.ordinal1 = PACKET3(IT_EVENT_WRITE, 3);
  cp_event_initiator.ordinal2 = 0;

  // Update switch statements you want to support
  VGT_EVENT_TYPE eventType = Reserved_0x00;
  switch (event) {
    default:
      assert(false && "Illegal VGT Event Id");
  }

  cp_event_initiator.eventType = eventType;
  cp_event_initiator.eventIndex = EventTypeToIndexTable[eventType];

  // set the address
  uint32_t addrLo = PtrLow32(addr);
  uint32_t addrHi = PtrHigh32(addr);
  ((addrLo & 0x7) != 0) ? assert(false) : assert(true);

  cp_event_initiator.ordinal3 = 0;
  cp_event_initiator.ordinal4 = 0;
  cp_event_initiator.addressLo = addrLo;
  cp_event_initiator.addressHi = addrHi;

  APPEND_COMMAND_WRAPPER(cmdbuf, cp_event_initiator);

  return;
}

void Gfx8CmdWriter::BuildBarrierCommand(CmdBuf* cmdBuf) {
  APPEND_COMMAND_WRAPPER(cmdBuf, pending_dispatch_template_);
}

void Gfx8CmdWriter::WriteUserData(uint32_t* dst_addr, uint32_t count, const void* src_addr) {
  memcpy(dst_addr, src_addr, count * sizeof(uint32_t));
}


void Gfx8CmdWriter::BuildAtomicPacket(CmdBuf* cmdbuf, AtomicType atomic_op,
                                         volatile uint32_t* addr, uint32_t value,
                                         uint32_t compare) {
  gfx8::AtomicTemplate atomic = atomic_template_;

  // make sure the destination adddress is aligned
  uint32_t address_low = PtrLow32((void*)addr);
  uint32_t address_high = PtrHigh32((void*)addr);
  assert(!(address_low & 0x7) && "destination address must be 8 byte aligned");

  atomic.atomic.addressLo = address_low;
  atomic.atomic.addressHi = address_high;

  switch (atomic_op) {
    case CommandWriter::kAtomicTypeIncrement: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_ADD_RTN_32;
      atomic.atomic.srcDataLo = 1;
      break;
    }
    case CommandWriter::kAtomicTypeDecrement: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_SUB_RTN_32;
      atomic.atomic.srcDataLo = 1;
      break;
    }
    case CommandWriter::kAtomicTypeCompareAndSwap: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_CMPSWAP_RTN_32;
      atomic.atomic.srcDataLo = value;
      atomic.atomic.cmpDataLo = compare;
      break;
    }
    case CommandWriter::kAtomicTypeBlockingCompareAndSwap: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_CMPSWAP_RTN_32;
      atomic.atomic.srcDataLo = value;
      atomic.atomic.cmpDataLo = compare;
      atomic.atomic.command = 1;
      atomic.atomic.loopInterval = 128;
      break;
    }
    case CommandWriter::kAtomicAdd: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_ADD_RTN_32;
      atomic.atomic.srcDataLo = value;
      break;
    }
    case CommandWriter::kAtomicSubtract: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_SUB_RTN_32;
      atomic.atomic.srcDataLo = value;
      break;
    }
    case CommandWriter::kAtomicSwap: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_SWAP_RTN_32;
      atomic.atomic.srcDataLo = value;
      break;
    }
  }

  APPEND_COMMAND_WRAPPER(cmdbuf, atomic);
}

void Gfx8CmdWriter::BuildAtomicPacket64(CmdBuf* cmdbuf, AtomicType atomic_op,
                                           volatile uint64_t* addr, uint64_t value,
                                           uint64_t compare) {
  AtomicTemplate atomic = atomic_template_;

  // make sure the destination adddress is aligned
  uint32_t address_low = PtrLow32((void*)addr);
  uint32_t address_high = PtrHigh32((void*)addr);
  assert(!(address_low & 0x7) && "destination address must be 8 byte aligned");

  atomic.atomic.addressLo = address_low;
  atomic.atomic.addressHi = address_high;

  atomic.atomic.atc = (atc_support_) ? 1 : 0;
  atomic.atomic.cachePolicy = 2;

  switch (atomic_op) {
    case CommandWriter::kAtomicTypeIncrement: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_ADD_RTN_64;
      atomic.atomic.srcDataLo = 1;
      break;
    }
    case CommandWriter::kAtomicTypeDecrement: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_SUB_RTN_64;
      atomic.atomic.srcDataLo = 1;
      break;
    }
    case CommandWriter::kAtomicTypeCompareAndSwap: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_CMPSWAP_RTN_64;
      atomic.atomic.srcDataLo = Low32(value);
      atomic.atomic.srcDataHi = High32(value);
      atomic.atomic.cmpDataLo = Low32(compare);
      atomic.atomic.cmpDataHi = High32(compare);
      break;
    }
    case CommandWriter::kAtomicTypeBlockingCompareAndSwap: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_CMPSWAP_RTN_64;
      atomic.atomic.srcDataLo = Low32(value);
      atomic.atomic.srcDataHi = High32(value);
      atomic.atomic.cmpDataLo = Low32(compare);
      atomic.atomic.cmpDataHi = High32(compare);
      atomic.atomic.command = 1;
      atomic.atomic.loopInterval = 128;
      break;
    }
    case CommandWriter::kAtomicAdd: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_ADD_RTN_64;
      atomic.atomic.srcDataLo = Low32(value);
      atomic.atomic.srcDataHi = High32(value);
      break;
    }
    case CommandWriter::kAtomicSubtract: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_SUB_RTN_64;
      atomic.atomic.srcDataLo = Low32(value);
      atomic.atomic.srcDataHi = High32(value);
      break;
    }
    case CommandWriter::kAtomicSwap: {
      atomic.atomic.atomOp = TC_OP_ATOMIC_SWAP_RTN_64;
      atomic.atomic.srcDataLo = Low32(value);
      atomic.atomic.srcDataHi = High32(value);
      break;
    }
  }

  APPEND_COMMAND_WRAPPER(cmdbuf, atomic);
}

size_t Gfx8CmdWriter::SizeOfAtomicPacket() const {
  return sizeof(AtomicTemplate) / sizeof(uint32_t);
}

void Gfx8CmdWriter::BuildConditionalExecute(CmdBuf* cmdbuf, uint32_t* signal, uint16_t count) {
  ConditionalExecuteTemplate conditional = conditional_template_;

  uint32_t address_low = PtrLow32(signal);
  uint32_t address_high = PtrHigh32(signal);
  assert(!(address_low & 0x7) && "destination address must be 8 byte aligned");

  conditional.conditional.boolAddrLo = address_low;
  conditional.conditional.boolAddrHi = address_high;
  conditional.conditional.execCount = count;

  APPEND_COMMAND_WRAPPER(cmdbuf, conditional);
}

void Gfx8CmdWriter::BuildWriteDataCommand(CmdBuf* cmdbuf, uint32_t* write_addr,
                                             uint32_t write_value) {
  // Copy the initialize command packet
  gfx8::WriteDataTemplate command = write_data_template_;

  // Encode the user specified value to write
  command.write_data_value = write_value;

  // Encode the user specified address to write to
  command.write_data.dstAddrLo = PtrLow32(write_addr);
  command.write_data.dstAddrHi = PtrHigh32(write_addr);

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, command);
}

void Gfx8CmdWriter::BuildWriteData64Command(CmdBuf* cmdbuf, uint64_t* write_addr,
                                               uint64_t write_value) {
  // Copy the initialize command packet
  gfx8::WriteData64Template command = write_data64_template_;

  // Encode the user specified value to write
  command.write_data_value = write_value;

  // Encode the user specified address to write to
  command.write_data.dstAddrLo = PtrLow32(write_addr);
  command.write_data.dstAddrHi = PtrHigh32(write_addr);

  // Append the built command into output Command Buffer
  APPEND_COMMAND_WRAPPER(cmdbuf, command);
}

void Gfx8CmdWriter::BuildFlushCacheCmd(CmdBuf* cmdbuf, FlushCacheOptions* options,
                                          uint32_t* writeAddr, uint32_t writeVal) {
  PM4CMDACQUIREMEM flushCmd;
  memset(&flushCmd, 0, sizeof(flushCmd));

  // Verify write back address is valid. Note that this address is NOT
  // used on CI. But to have a same interface as that on SI, we keep
  // the address argument in this function. Thus, this check always pass
  // no matter the address is NULL or not.
  (writeAddr == NULL) ? assert(true) : assert(true);

  // Initialize the command header
  gfx8::GenerateCmdHeader(&flushCmd, IT_ACQUIRE_MEM__CI__VI);

  // Specify the base address of memory being synchronized.
  // The starting address is indicated as follows: bits [0-48].
  flushCmd.cpCoherBase.u32All = 0;
  flushCmd.cpCoherBaseHi.u32All = 0;

  // Specify the size of memory being synchronized. It is indicated
  // as follows:
  //    COHER_SIZE_256B_MASK = 0xffffffffL
  //    COHER_SIZE_HI_256B_MASK__CI__VI = 0x000000ffL
  flushCmd.cpCoherSize.u32All = CP_COHER_SIZE__COHER_SIZE_256B_MASK;
  flushCmd.cpCoherSizeHi.u32All = CP_COHER_SIZE_HI__COHER_SIZE_HI_256B_MASK__CI__VI;

  // Periodicity of polling - interval to wait from the time
  // of unsuccessful polling result is returned and a new
  // poll is issued
  flushCmd.pollInterval = 0x04;

  // Program Coherence Control Register. Initialize L2 Cache flush
  // for Non-Coherent memory blocks
  uint32_t coher_cntl = 0;

  coher_cntl |= (options->l1) ? CP_COHER_CNTL__TCL1_ACTION_ENA_MASK : 0;
  coher_cntl |= (options->l2)
      ? (CP_COHER_CNTL__TC_ACTION_ENA_MASK | CP_COHER_CNTL__TC_WB_ACTION_ENA_MASK__CI__VI)
      : 0;
  coher_cntl |= (options->icache) ? CP_COHER_CNTL__SH_ICACHE_ACTION_ENA_MASK : 0;
  coher_cntl |= (options->kcache) ? CP_COHER_CNTL__SH_KCACHE_ACTION_ENA_MASK : 0;
  flushCmd.coherCntl = coher_cntl;

  // Copy AcquireMem command buffer stream
  APPEND_COMMAND_WRAPPER(cmdbuf, flushCmd);
  return;
}

void Gfx8CmdWriter::BuildDmaDataPacket(CmdBuf* cmdbuf, uint32_t* srcAddr, uint32_t* dstAddr,
                                          uint32_t copySize, bool waitForConfirm) {
  PM4CMDDMADATA cmdDmaData;
  memset(&cmdDmaData, 0, sizeof(PM4CMDDMADATA));
  cmdDmaData.header.u32All =
      (PM4_TYPE_3_HDR(IT_DMA_DATA__CI__VI, PM4_CMD_DMA_DATA_DWORDS, ShaderCompute, 0));

  // Id of Micro Engine
  cmdDmaData.engine = 0;

  // Specify attributes of source buffer such as its
  // location, ATC property, Cache policy and Volatile
  // A value of 1 for cache policy means to Stream
  cmdDmaData.srcSel = 0;
  cmdDmaData.srcATC = atc_support_;
  cmdDmaData.srcCachePolicy = 1;
  cmdDmaData.srcVolatile = 0;

  // Specify attributes of destination buffer such as
  // its location, ATC property, Cache policy and Volatile
  // A value of 1 for cache policy means to Stream
  cmdDmaData.dstSel = 0;
  cmdDmaData.dstATC = atc_support_;
  cmdDmaData.dstCachePolicy = 1;
  cmdDmaData.dstVolatile = 0;

  // Specify the source and destination addr
  cmdDmaData.srcAddrHi = PtrHigh32(srcAddr);
  cmdDmaData.srcAddrLoOrData = PtrLow32(srcAddr);
  cmdDmaData.dstAddrLo = PtrLow32(dstAddr);
  cmdDmaData.dstAddrHi = PtrHigh32(dstAddr);

  // Number of bytes to copy. The command restricts
  // the size to be (2 MB - 1) - 21 Bits
  assert(copySize < 0x1FFFFF);
  cmdDmaData.command.byteCount = copySize;

  // Indicate that DMA Cmd should wait if its source
  // is the destination of a previous DMA Cmd
  cmdDmaData.command.rawWait = waitForConfirm;

  APPEND_COMMAND_WRAPPER(cmdbuf, cmdDmaData);
  return;
}

}  // gfx8
}  // pm4_profile
