#ifndef _GFX9_CMDS_H_
#define _GFX9_CMDS_H_

#include "gfxip/gfx9/gfx9_utils.h"
#include "gfxip/gfx9/gfx9_enum.h"
#include "gfxip/gfx9/gfx9_mask.h"
#include "gfxip/gfx9/gfx9_offset.h"
#include "gfxip/gfx9/gfx9_typedef.h"
#include "gfxip/gfx9/gfx9_registers.h"
#include "gfxip/gfx9/gfx9_pm4_it_opcodes.h"
#include "gfxip/gfx9/f32_mec_pm4_packets_vg10.h"
#include "gfxip/gfx9/f32_pfp_pm4_packets_vg10.h"

namespace pm4_profile {

namespace gfx9 {

/// @brief Initializer for commands that set shader registers
template <class T> void GenerateSetShRegHeader(T* pm4, uint32_t reg_addr) {
  pm4->cmd_set_data.header.u32All = PM4_TYPE3_HDR(IT_SET_SH_REG, sizeof(T) / sizeof(uint32_t));
  pm4->cmd_set_data.bitfields2.reg_offset = reg_addr - PERSISTENT_SPACE_START;
}

// @brief Initializer for various Gpu command headers
template <class T> void GenerateCmdHeader(T* pm4, IT_OpCodeType op_code) {
  pm4->header.u32All = PM4_TYPE3_HDR(op_code, sizeof(T) / sizeof(uint32_t));
}

// @brief Initializer for commands that set configuration registers
template <class T> void GenerateSetConfigRegHeader(T* pm4, uint32_t reg_addr) {
  pm4->cmd_set_data.header.u32All = PM4_TYPE3_HDR(IT_SET_CONFIG_REG, sizeof(T) / sizeof(uint32_t));
  pm4->cmd_set_data.bitfields2.reg_offset = reg_addr - CONFIG_SPACE_START;
}

/// @brief Structure used to issue a Gpu Barrier command
struct BarrierTemplate {
  PM4MEC_EVENT_WRITE event_write;
};

/// @brief Structure used to configure the flushing of
/// various caches - instruction, constants, L1 and L2
struct AcquireMemTemplate {
  PM4MEC_ACQUIRE_MEM acquire_mem;
};

/// @brief Structure used to reference another Gpu command
/// indirectly. Generally used to reference a list of Gpu
/// commands (dispatch cmds) indirectly
struct LaunchTemplate {
  PM4MEC_INDIRECT_BUFFER indirect_buffer;
};

/// @brief Structure used to determine the end of
/// a kernel including cache flushes and writing to
/// a user configurable memory location
struct EndofKernelNotifyTemplate {
  PM4MEC_RELEASE_MEM release_mem;
};

// Desc: Strucuture used to perform various atomic
// operations - add, subtract, increment, etc
struct AtomicTemplate {
  PM4MEC_ATOMIC_MEM atomic;
};

/// @brief PM4 command to write a 32-bit value into a memory
/// location accessible to Gpu
struct WriteDataTemplate {
  PM4MEC_WRITE_DATA write_data;
  uint32_t write_data_value;
};

/// @brief PM4 command to write a 64-bit value into a memory
/// location accessible to Gpu
struct WriteData64Template {
  PM4MEC_WRITE_DATA write_data;
  uint64_t write_data_value;
};

/// @brief PM4 command to wait for a certain event before proceeding
/// to process another command on the queue
struct WaitRegMemTemplate {
  PM4MEC_WAIT_REG_MEM wait_reg_mem;
};

}  // gfx9

}  // pm4_profile

#endif  //  _GFX9_CMDS_H_
