#ifndef _GFX8_CMDWRITER_H_
#define _GFX8_CMDWRITER_H_

#include "cmdwriter.h"
#include "gfx8_cmds.h"

namespace pm4_profile {

namespace gfx8 {

/// @brief class Gfx8CmdWriter implements the virtual class CommandWriter
/// for Sea Islands (CI) and VI chipset
class Gfx8CmdWriter : public CommandWriter {
 public:
  Gfx8CmdWriter(bool atc_support, bool pcie_atomic_support);

  /// @brief Dword specifying NOOP command for SI/CI/VI chipsets. The macro
  /// populates the NOOP command which is 32-bits wide. The second parameter,
  /// the COUNT field of NOOP command, specifies the number of Dwords to skip.
  /// To skip ZERO Dwords the value should be set to 0x3FFF. Since the macro
  /// decrements the second parameter by TWO, an artifact of its definition,
  /// the value is incremented by TWO to 0x4001 (0x3FFF + 2).
  ///
  inline uint32_t GetNoOpCmd() {
    static const uint32_t nopCmd = PM4_TYPE_3_HDR(IT_NOP, 0x4001, ShaderCompute, 0);
    return nopCmd;
  }

  void BuildBarrierCommand(CmdBuf* cmdBuf);

  void BuildIndirectBufferCmd(CmdBuf* cmdbuf, const void* cmd_addr, std::size_t cmd_size);

  void BuildBOPNotifyCmd(CmdBuf* cmdbuf, const void* write_addr, uint32_t write_val,
                         bool interrupt);

  void BuildBarrierFenceCommands(CmdBuf* cmdbuf);

  void BuildWriteEventPacket(CmdBuf* cmdbuf, uint32_t event);

  void BuildWaitRegMemCommand(CmdBuf* cmdbuf, bool mem_space, uint64_t wait_addr, bool func_eq,
                              uint32_t mask_val, uint32_t wait_val);

  void BuildWriteUnshadowRegPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value);

  /// @brief Build CP command to program a Gpu register
  ///
  /// @param cmdbuf Pointer to command buffer to be appended
  /// @param addr Register to be programmed
  /// @param value Value to write into register
  ///
  /// @return void
  void BuildWriteUConfigRegPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value);

  void BuildWriteShRegPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value);

  void BuildCopyDataPacket(CmdBuf* cmdbuf, uint32_t src_sel, uint32_t src_addr_lo,
                           uint32_t src_addr_hi, uint32_t* dst_addr, uint32_t size, bool wait);

  void BuildWriteWaitIdlePacket(CmdBuf* cmdbuf);

  // Will issue a VGT event including a cache flush later on
  void BuildVgtEventPacket(CmdBuf* cmdbuf, uint32_t vgtEvent);

  void BuildWriteRegisterPacket(CmdBuf* cmdbuf, uint32_t addr, uint32_t value);

  void BuildWriteEventQueryPacket(CmdBuf* cmdbuf, uint32_t event, uint32_t* addr);

  void BuildAtomicPacket(CmdBuf* cmdbuf, AtomicType atomic_op, volatile uint32_t* addr,
                         uint32_t value, uint32_t compare);

  void BuildAtomicPacket64(CmdBuf* cmdbuf, AtomicType atomic_op, volatile uint64_t* addr,
                           uint64_t value = 0, uint64_t compare = 0);

  size_t SizeOfAtomicPacket() const;

  void BuildConditionalExecute(CmdBuf* cmdbuf, uint32_t* signal, uint16_t count);

  void BuildWriteDataCommand(CmdBuf* cmdbuf, uint32_t* write_addr, uint32_t write_value);

  void BuildWriteData64Command(CmdBuf* cmdbuf, uint64_t* write_addr, uint64_t write_value);

  void BuildCacheFlushPacket(CmdBuf* cmdbuf);

  /// Writes into input buffer Gpu commands to flush its cache. It is
  /// necessary that the buffer provided for flush commands is large
  /// enough to accommodate the full set of commands. It should be at
  /// least 512 bytes.
  ///
  /// @param tsCmdBuf Buffer to write commands to.
  /// @param writeAddr Registered address into which GPU should write
  /// a user provided value upon executing the flush commands.
  /// @param writeVal User provided value written by GPU at user provided
  /// address, upon executing the flush commands.
  ///
  /// @return void
  void BuildFlushCacheCmd(CmdBuf* cmdBuf, FlushCacheOptions* options, uint32_t* writeAddr,
                          uint32_t writeVal);

  /// Builds Gpu command to copy data from source to destination buffer
  /// using DMA engine.
  ///
  /// @param cmdbuf Buffer updated with Gpu copy command
  /// @param srcAddr Address of source buffer address
  /// @param dstAddr Address of destination buffer address
  /// @param copySize Size of data to copy in bytes
  /// @param waitForCompletion if command should wait for copying to complete
  void BuildDmaDataPacket(CmdBuf* cmdBuf, uint32_t* srcAddr, uint32_t* dstAddr, uint32_t copySize,
                          bool waitForCompletion);

 protected:
  /// @brief Copies data from source buffer to destination buffer
  ///
  /// @param dst_addr Address of destination buffer data
  ///
  /// @count Size of data to copy in 32-bit words
  ///
  /// @param src_addr Address of buffer containing source data
  ///
  /// @return void
  virtual void WriteUserData(uint32_t* dst_addr, uint32_t count, const void* src_addr);

  /// @brief Append an instance of Gpu command into input command buffer stream.
  ///
  /// @param cmdbuf CommandWriter object appended with anohter Gpu command
  ///
  /// @param cmd Gpu command to be appended into command buffer
  ///
  /// @return void
  template <class T> void AppendCommand(CmdBuf* cmdbuf, const T& cmd);

 private:
  /// @brief Initializes a Gpu command which can be used to
  /// reference a Gpu command stream indirectly
  void InitializeLaunchTemplate();

  /// @brief Initializes a Gpu command to perform atomic operations
  ////
  void InitializeAtomicTemplate();

  /// @brief Initializes a Gpu command to allow conditional execution
  /// of a Gpu command stream
  void InitializeConditionalTemplate();

  /// @brief Initializes a Gpu command to let command processor
  /// wait for some update before letting other commands to be
  /// processed
  void InitializeWaitRegMemTemplate();

  /// @brief Initializes the template for Barrier command.
  /// Applications can use Barrier command to ensure their
  /// command is executed only after all other commands have
  /// completed their execution.
  void InitializeBarrierTemplate();

  void BuildUpdateHostAddress(CmdBuf* cmdbuf, uint64_t* addr, int64_t value);

  /// @brief Initializes Acquire Memory command template. Users
  /// can submit this command to invalidate Gpu caches - L1 and
  /// or L2.
  void InitializeAcquireMemTemplate();

  /// @brief Initializes an instance of Write Data command
  /// for use by an application
  void InitializeWriteDataTemplate();
  void InitializeWriteData64Template();

  /// @brief Instance of Gpu command to reference dispatch commands
  LaunchTemplate launch_template_;

  /// @brief Instance of Gpu command to use in performing atomic operations
  AtomicTemplate atomic_template_;

  /// @brief Instance of Gpu command to use in conditional execution
  /// of a command stream
  ConditionalExecuteTemplate conditional_template_;

  /// @brief Instance of Pm4 command WRITE_DATA
  WriteDataTemplate write_data_template_;
  WriteData64Template write_data64_template_;

  /// @brief Instance of Pm4 command EVENT_WRITE
  BarrierTemplate pending_dispatch_template_;

  /// @brief Instance of Pm4 command ACQUIRE_MEM
  AcquireMemTemplate invalidate_cache_template_;

  /// @brief Instance of Pm4 command WAIT_REG_MEM
  WaitRegMemTemplate wait_reg_mem_template_;

  /// @brief ATC support.
  bool atc_support_;

  /// @brief PCIe atomic support.
  bool pcie_atomic_support_;
};

}  // gfx8

}  // pm4_profile

#endif  //  _GFX8_CMDWRITER_H_
