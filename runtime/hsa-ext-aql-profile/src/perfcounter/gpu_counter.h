#ifndef _GPU_COUNTER_H_
#define _GPU_COUNTER_H_

#include "hsa_perf.h"
#include "parameter_set.h"

#include <stdlib.h>
#include <stdint.h>
#include <list>

namespace pm4_profile {
// @brief This class represent each CI performance counter
class GpuCounter : public pm4_profile::Counter {
 public:
  GpuCounter();

  virtual ~GpuCounter();

  virtual int getLastError();

  virtual std::string getErrorString(int error);

  virtual bool getResult(uint64_t* p_result);

  virtual pm4_profile::CounterBlock* getCounterBlock();

  virtual bool setEnable(bool b);

  virtual bool isEnabled() { return counter_enabled_; }

  virtual bool isResultReady() { return is_result_ready_; }

  virtual bool getParameter(uint32_t param, uint32_t& ret_size, void** pp_data);

  virtual bool setParameter(uint32_t param, uint32_t param_size, const void* p_data);

  bool setCounterBlock(pm4_profile::CounterBlock* p_cntr_group);

  void setResult(uint64_t result);

 private:
  bool counter_enabled_;
  bool is_result_ready_;
  uint64_t result_;
  pm4_profile::ParameterSet* parameter_set_;
  pm4_profile::CounterBlock* counter_block_;
  uint32_t error_code_;
};

typedef std::list<GpuCounter*> GpuCounterList;
}
#endif  // _GPU_COUNTER_H_
