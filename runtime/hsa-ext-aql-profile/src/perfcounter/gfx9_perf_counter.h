#ifndef _AI_PMU_H_
#define _AI_PMU_H_

#include <stdint.h>

#include "perf_counter.h"

namespace pm4_profile {
class CommandWriter;

// This class implement the AI PMU.  It is responsible for setting up
// CounterGroups to represent each AI hardware block which exposes performance
// counters.
class Gfx9PerfCounter : public pm4_profile::Pmu {
 public:
  Gfx9PerfCounter();

  // Returns number of shader engines per block
  // for the blocks featured shader engines instancing
  uint32_t getNumSe() { return num_se_; }

  int getLastError();

  std::string getErrorString(int error);

  void begin(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter, const CountersMap& countersMap);

  uint32_t end(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter, const CountersMap& countersMap,
               void* dataBuff);

 private:
  void Init();

  // Program SQ block related counters
  uint32_t ProgramSQCntrs(uint32_t sqRegIdx, uint32_t* regAddr, uint32_t* regVal, uint32_t blkId,
                          uint32_t blkCntrIdx);

  // Program TA block related counters
  uint32_t ProgramTaCntrs(uint32_t taRegIdx, uint32_t* regAddr, uint32_t* regVal, uint32_t blkId,
                          uint32_t blkCntrIdx);

  // Program TCA block related counters
  uint32_t ProgramTcaCntrs(uint32_t tcaRegIdx, uint32_t* regAddr, uint32_t* regVal, uint32_t blkId,
                           uint32_t blkCntrIdx);

  // Program TCC block related counters
  uint32_t ProgramTccCntrs(uint32_t tccRegIdx, uint32_t* regAddr, uint32_t* regVal, uint32_t blkId,
                           uint32_t blkCntrIdx);

  // Program TCP block related counters
  uint32_t ProgramTcpCntrs(uint32_t tcpRegIdx, uint32_t* regAddr, uint32_t* regVal, uint32_t blkId,
                           uint32_t blkCntrIdx);

  // Program TD block related counters
  uint32_t ProgramTdCntrs(uint32_t tdRegIdx, uint32_t* regAddr, uint32_t* regVal, uint32_t blkId,
                          uint32_t blkCntrIdx);

  // Build counter selection register, return how many registers are built
  uint32_t BuildCounterSelRegister(uint32_t cntrIdx, uint32_t* regAddr, uint32_t* regVal,
                                   uint32_t blkId, uint32_t blkCntrIdx);

  // Build counter selection register, return how many registers are built
  uint32_t BuildCounterReadRegisters(uint32_t reg_index, uint32_t block_id, uint32_t* reg_addr,
                                     uint32_t* reg_val);

 private:
  int error_code_;

  // Indicates the number of Shader Engines Present
  uint32_t num_se_;

  // Used to reset GRBM to its default state
  uint32_t reset_grbm_;
};
}

#endif  // _AI_PMU_H_
