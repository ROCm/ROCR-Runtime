#ifndef _GFX8_CMDS_H_
#define _GFX8_CMDS_H_

#include "gfxip/gfx8/si_ci_vi_merged_enum.h"
#include "gfxip/gfx8/si_ci_vi_merged_mask.h"
#include "gfxip/gfx8/si_ci_vi_merged_offset.h"
#include "gfxip/gfx8/si_ci_vi_merged_registers.h"
#include "gfxip/gfx8/si_ci_vi_merged_typedef.h"
#include "gfxip/gfx8/si_ci_vi_merged_pm4_it_opcodes.h"
#include "gfxip/gfx8/si_pm4defs.h"

namespace pm4_profile {

namespace gfx8 {

// Desc: Defines the Gpu command to dispatch a kernel. It embeds
// various Gpu hardware specific data structures for initialization
// and configuration before a dispatch begins to run
struct DispatchTemplate {
  // Desc: Structure used to initialize the group dimensions
  // of a kernel dispatch and if performance counters are enabled
  struct DispatchDimensionRegs {
    PM4CMDSETDATA cmd_set_data;
    regCOMPUTE_START_X compute_start_x;
    regCOMPUTE_START_Y compute_start_y;
    regCOMPUTE_START_Z compute_start_z;
    regCOMPUTE_NUM_THREAD_X compute_num_thread_x;
    regCOMPUTE_NUM_THREAD_Y compute_num_thread_y;
    regCOMPUTE_NUM_THREAD_Z compute_num_thread_z;
    regCOMPUTE_PIPELINESTAT_ENABLE__CI__VI compute_pipelinestat_enable;
  } dimension_regs;

  // Desc: Structure used to initialize kernel Isa, trap
  // handler, trap handler buffer, number of SGPR and VGPR
  // registers needed, amount of Group memory and LDS needed,
  // Rounding mode for Floating point numbers, etc.
  struct DispatchProgramRegs {
    PM4CMDSETDATA cmd_set_data;
    regCOMPUTE_PGM_LO compute_pgm_lo;
    regCOMPUTE_PGM_HI compute_pgm_hi;
    regCOMPUTE_TBA_LO compute_tba_lo;
    regCOMPUTE_TBA_HI compute_tba_hi;
    regCOMPUTE_TMA_LO compute_tma_lo;
    regCOMPUTE_TMA_HI compute_tma_hi;
    regCOMPUTE_PGM_RSRC1 compute_pgm_rsrc1;
    regCOMPUTE_PGM_RSRC2 compute_pgm_rsrc2;
  } program_regs;

  // Desc: Structure used to initialize parameters related to
  // thread management i.e. number of waves to issue and number
  // of Compute Units to use
  struct DispatchResourceRegs {
    PM4CMDSETDATA cmd_set_data;
    regCOMPUTE_RESOURCE_LIMITS compute_resource_limits;
    regCOMPUTE_STATIC_THREAD_MGMT_SE0 compute_static_thread_mgmt_se0;
    regCOMPUTE_STATIC_THREAD_MGMT_SE1 compute_static_thread_mgmt_se1;
    regCOMPUTE_TMPRING_SIZE compute_tmpring_size;
    regCOMPUTE_STATIC_THREAD_MGMT_SE2__CI__VI compute_static_thread_mgmt_se2;
    regCOMPUTE_STATIC_THREAD_MGMT_SE3__CI__VI compute_static_thread_mgmt_se3;
    regCOMPUTE_RESTART_X__CI__VI compute_restart_x;
    regCOMPUTE_RESTART_Y__CI__VI compute_restart_y;
    regCOMPUTE_RESTART_Z__CI__VI compute_restart_z;
    regCOMPUTE_THREAD_TRACE_ENABLE__CI__VI compute_thread_trace_enable;
  } resource_regs;

  // Desc: Structure used to pass handles of the Aql dispatch
  // packet, Aql queue, Kernel argument address block, Scratch
  // buffer
  struct DispatchComputeUserDataRegs {
    PM4CMDSETDATA cmd_set_data;
    uint32_t compute_user_data[16];
  } compute_user_data_regs;

  // Desc: Structure used to configure Cache flush policy
  // and dimensions of total work size
  PM4CMDDISPATCHDIRECT dispatch_direct;
};

// Desc: Structure used to issue a Gpu Barrier command
struct BarrierTemplate {
  PM4CMDEVENTWRITE event_write;
};

// Desc: Structure used to configure the flushing
// of various caches - instruction, constants, L1
// and L2
struct AcquireMemTemplate {
  PM4CMDACQUIREMEM acquire_mem;
};

// Desc: Structure used to reference another Gpu command
// indirectly. Generally used to reference a list of Gpu
// commands (dispatch cmds) indirectly
struct LaunchTemplate {
  PM4CMDINDIRECTBUFFER indirect_buffer;
};

// Desc: Structure used to determine the end of
// a kernel including cache flushes and writing to
// a user configurable memory location
struct EndofKernelNotifyTemplate {
  PM4CMDRELEASEMEM release_mem;
};

// Desc: Strucuture used to perform various atomic
// operations - add, subtract, increment, etc
struct AtomicTemplate {
  PM4CMDATOMIC atomic;
};

// Desc: Structure used to conditionalize the execution
// of a Gpu command stream
struct ConditionalExecuteTemplate {
  PM4CMDCONDEXEC_CI conditional;
};

// Desc: PM4 command to write a 32-bit value into a memory
// location accessible to Gpu
struct WriteDataTemplate {
  PM4CMDWRITEDATA write_data;
  uint32_t write_data_value;
};

// Desc: PM4 command to write a 64-bit value into a memory
// location accessible to Gpu
struct WriteData64Template {
  PM4CMDWRITEDATA write_data;
  uint64_t write_data_value;
};

// Desc: PM4 command to wait for a certain event before proceeding
// to process another command on the queue
struct WaitRegMemTemplate {
  PM4CMDWAITREGMEM wait_reg_mem;
};

// Desc: Initializer for commands that set shader registers
template <class T> void GenerateSetShRegHeader(T* pm4, uint32_t reg_addr) {
  pm4->cmd_set_data.header.u32All =
      PM4_TYPE_3_HDR(IT_SET_SH_REG, sizeof(T) / sizeof(uint32_t), ShaderCompute, 0);
  pm4->cmd_set_data.regOffset = reg_addr - PERSISTENT_SPACE_START;
}

// Desc: Initializer for various Gpu command headers
template <class T> void GenerateCmdHeader(T* pm4, IT_OpCodeType op_code) {
  pm4->header.u32All = PM4_TYPE_3_HDR(op_code, sizeof(T) / sizeof(uint32_t), ShaderCompute, 0);
}

// Desc: Initializer for commands that set configuration registers
template <class T> void GenerateSetConfigRegHeader(T* pm4, uint32_t reg_addr) {
  pm4->cmd_set_data.header.u32All =
      PM4_TYPE_3_HDR(IT_SET_CONFIG_REG, sizeof(T) / sizeof(uint32_t), ShaderCompute, 0);
  pm4->cmd_set_data.regOffset = reg_addr - CONFIG_SPACE_START;
}


}  // gfx8

}  // pm4_profile

#endif  //  _GFX8_CMDS_H_
