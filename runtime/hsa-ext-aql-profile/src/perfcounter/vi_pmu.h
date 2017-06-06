#ifndef _VI_PMU_H_
#define _VI_PMU_H_

#include "hsa.h"
#include "cmdwriter.h"
#include "hsa_perf.h"
#include "info_set.h"
#include "parameter_set.h"
#include "vi_blockinfo.h"
#include "rocr_profiler.h"

#include <stdlib.h>
#include <stdint.h>
#include <map>

namespace pm4_profile {
typedef std::map<HsaViCounterBlockId, pm4_profile::CounterBlock*> ViCounterBlockMap;

// This class implement the VI PMU.  It is responsible for setting up
// CounterGroups to represent each VI hardware block which exposes performance
// counters.
class ViPmu : public pm4_profile::Pmu {
 public:
  ViPmu();
  ~ViPmu();

  // Returns number of shader engines per block
  // for the blocks featured shader engines instancing
  uint32_t getNumSe() { return num_se_; }

  // Initializes the handle of buffer used to collect PMC data
  bool setPmcDataBuff(uint8_t* pmcBuffer, uint32_t pmcBuffSz);

  int getLastError();

  std::string getErrorString(int error);

  virtual bool begin(pm4_profile::DefaultCmdBuf* cmdBuff, pm4_profile::CommandWriter* cmdWriter,
                     bool reset = true);

  virtual bool end(pm4_profile::DefaultCmdBuf* cmdBuff, pm4_profile::CommandWriter* cmdWriter);

  // IPMU inherits the IParameterSet and IInfoSetso we implement it
  // through composition and function forwarding
  bool getParameter(uint32_t param, uint32_t& ret_size, void** pp_data);

  bool setParameter(uint32_t param, uint32_t param_size, const void* p_data);

  bool getInfo(uint32_t info, uint32_t& ret_size, void** pp_data);

  pm4_profile::CounterBlock* getCounterBlockById(uint32_t id);

  rocr_pmu_state_t getCurrentState() { return profiler_state_; }

  pm4_profile::CounterBlock** getAllCounterBlocks(uint32_t& num_groups);

 private:
  // Addr of Counter Data Buffer
  uint32_t* pmcData_;

  // Size of Counter Data Buffer
  uint32_t pmcDataSz_;

  void Init();

  bool initCounterBlock();

  bool isResultReady();

  // Clear CounterBlockMap
  void clearCounterBlockMap();

  // Reset SQ and CB counters
  void ResetCounterBlocks(pm4_profile::DefaultCmdBuf* cmdBuff,
                          pm4_profile::CommandWriter* cmdWriter);

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
                                   uint32_t blkId, pm4_profile::Counter* blkCntr);

  // Build counter selection register, return how many registers are built
  uint32_t BuildCounterReadRegisters(uint32_t reg_index, uint32_t block_id, uint32_t* reg_addr,
                                     uint32_t* reg_val);

 private:
  // Delete counter blocks in the PMU
  hsa_status_t RemoveCounterBlocks();

 private:
  // This contains the available counter groups.
  ViCounterBlockMap blk_map_;

  // This stores the current profiling state.
  rocr_pmu_state_t profiler_state_;

  pm4_profile::ParameterSet* parameter_set_;

  pm4_profile::InfoSet* info_set_;

  int error_code_;

// A flag to indicate the current packet is for copy register value
#define COPY_DATA_FLAG 0xFFFFFFFF
#define MAX_REG_NUM 100

  // Pointer used to store counter block list internally
  uint32_t blk_list_size_;
  pm4_profile::CounterBlock** blk_list_;

  // Indicates the number of Shader Engines Present
  uint32_t num_se_;

  // Used to reset GRBM to its default state
  uint32_t reset_grbm_;
};
}
#endif
