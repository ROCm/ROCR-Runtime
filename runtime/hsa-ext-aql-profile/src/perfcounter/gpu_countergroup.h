#ifndef _GPU_COUNTER_GROUP_H_
#define _GPU_COUNTER_GROUP_H_

// This file contains declaration of Sea Island (CI) CounterBlock class.
#include "hsa_perf.h"
#include "gpu_counter.h"
#include "parameter_set.h"
#include "info_set.h"
#include "gpu_enum.h"

#include <stdlib.h>
#include <stdint.h>

namespace pm4_profile {
// This class represents one CI hardware block. Each block contains
// multiple performance counters.
class GpuCounterBlock : public pm4_profile::CounterBlock {
 public:
  GpuCounterBlock();
  ~GpuCounterBlock();

  // NOTE [Suravee] : We specify CiPmu as a friend
  // because the CiPmu needs to be able to setup info of
  // the counter block.
  friend class CiPmu;
  friend class ViPmu;
  friend class AiPmu;

  std::string getErrorString(int error);

  pm4_profile::Counter* createCounter();

  virtual bool destroyCounter(pm4_profile::Counter* p_cntr);

  virtual bool destroyAllCounters();

  virtual pm4_profile::Counter** getEnabledCounters(uint32_t& num);

  virtual pm4_profile::Counter** getAllCounters(uint32_t& num);

  virtual bool getParameter(uint32_t param, uint32_t& ret_size, void** pp_data);

  virtual bool setParameter(uint32_t param, uint32_t param_size, const void* p_data);

  virtual bool getInfo(uint32_t info, uint32_t& ret_size, void** pp_data);

 protected:
  void _initCounterBlockType();

  bool setInfo(GPU_BLK_INFOS blk_info, uint32_t size, void* data);

  hsa_ext_tools_counter_block_type_t block_type_;

 private:
  bool _checkMaxNumOfCounters();

  uint32_t _getNumOfEnabledCounters();

  pm4_profile::ParameterSet* parameter_set_;
  pm4_profile::InfoSet* info_set_;
  GpuCounterList cntr_list_;
  uint32_t error_code_;

  // Pointer of buffer to store counter list
  pm4_profile::Counter** pp_cntrs_;
};

}  // pm4_profile

#endif  // _GPU_COUNTER_GROUP_H_
