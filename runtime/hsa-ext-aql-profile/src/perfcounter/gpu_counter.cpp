#include "gpu_counter.h"

using namespace pm4_profile;

namespace pm4_profile {
static char error_string[][64] = {
    {"No error"}, {"Counter generic error"}, {"Counter is already set"}, {"Counter not ready"},
};

GpuCounter::GpuCounter() : Counter() {
  counter_enabled_ = false;
  parameter_set_ = new ParameterSet();
}

GpuCounter::~GpuCounter() { delete parameter_set_; }

bool GpuCounter::getResult(uint64_t* p_result) {
  if (!p_result) {
    return false;
  }

  *p_result = result_;

  return true;
}

bool GpuCounter::setCounterBlock(pm4_profile::CounterBlock* p_cntr_group) {
  if (!p_cntr_group) {
    return false;
  }

  counter_block_ = p_cntr_group;

  return true;
}

pm4_profile::CounterBlock* GpuCounter::getCounterBlock() { return counter_block_; }

bool GpuCounter::setEnable(bool b) {
  // TODO: Validate counter
  counter_enabled_ = b;
  return true;
}

void GpuCounter::setResult(uint64_t result) { result_ = result; }

int GpuCounter::getLastError() { return error_code_; }

std::string GpuCounter::getErrorString(int error) {
  if ((error >= 0) && (error < kHsaCounterErrorCodeMax)) {
    std::string err_string(error_string[error]);
    return err_string;
  }
  return "Incorrect error index";
}

bool GpuCounter::getParameter(uint32_t param, uint32_t& ret_size, void** pp_data) {
  return parameter_set_->getParameter(param, ret_size, pp_data);
}

bool GpuCounter::setParameter(uint32_t param, uint32_t param_size, const void* p_data) {
  bool ret_code;

  error_code_ = kHsaCounterErrorCodeNoError;

  ret_code = parameter_set_->setParameter(param, param_size, p_data);
  if (ret_code == false) {
    error_code_ = kHsaCounterErrorCodeAlreadySet;
  }

  return ret_code;
}
}
