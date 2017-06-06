#include "gpu_countergroup.h"
#include "gpu_counter.h"
#include "gpu_enum.h"

using namespace pm4_profile;

namespace pm4_profile {

static char error_string[][64] = {
    {"No error"}, {"Counter block error"}, {"Max counter reached"}, {"Unkown counter"}};

GpuCounterBlock::GpuCounterBlock() : CounterBlock() {
  cntr_list_.clear();
  parameter_set_ = new ParameterSet();
  info_set_ = new InfoSet();

  // Initialize pointer to NULL
  pp_cntrs_ = NULL;

  _initCounterBlockType();
}

GpuCounterBlock::~GpuCounterBlock() {
  GpuCounterList::iterator it = cntr_list_.begin();
  GpuCounterList::iterator it_end = cntr_list_.end();

  for (; it != it_end; it++) {
    if (*it) {
      delete (*it);
    }
  }
  cntr_list_.clear();

  delete parameter_set_;
  delete info_set_;

  if (pp_cntrs_) {
    free(pp_cntrs_);
    pp_cntrs_ = NULL;
  }
}

void GpuCounterBlock::_initCounterBlockType() {
  block_type_ = HSA_EXT_TOOLS_COUNTER_BLOCK_TYPE_ASYNC;
}

Counter* GpuCounterBlock::createCounter() {
  if (!_checkMaxNumOfCounters()) {
    return NULL;
  }

  GpuCounter* p_cntr = new GpuCounter();
  if (!p_cntr) {
    return NULL;
  }

  cntr_list_.push_back(p_cntr);

  return (Counter*)p_cntr;
}

bool GpuCounterBlock::destroyCounter(Counter* p_cntr) {
  bool ret = false;

  if (!p_cntr) {
    return ret;
  }

  GpuCounterList::iterator it = cntr_list_.begin();
  GpuCounterList::iterator it_end = cntr_list_.end();
  for (; it != it_end; it++) {
    if (*it == p_cntr) {
      delete (*it);
      cntr_list_.erase(it);
      ret = true;
      break;
    }
  }

  return ret;
}

bool GpuCounterBlock::destroyAllCounters() {
  GpuCounterList::iterator it = cntr_list_.begin();
  GpuCounterList::iterator it_end = cntr_list_.end();

  for (; it != it_end; it++) {
    if (*it) {
      delete (*it);
    }
  }

  cntr_list_.clear();

  return true;
}

Counter** GpuCounterBlock::getEnabledCounters(uint32_t& num) {
  if (pp_cntrs_) {
    free(pp_cntrs_);
    pp_cntrs_ = NULL;
  }

  pp_cntrs_ = (Counter**)malloc(sizeof(GpuCounter*) * cntr_list_.size());

  if (!pp_cntrs_) {
    return NULL;
  }

  int cnt = 0;
  GpuCounterList::iterator it = cntr_list_.begin();
  GpuCounterList::iterator it_end = cntr_list_.end();
  for (; it != it_end; it++) {
    GpuCounter* p_cntr = (*it);
    bool is_enabled;
    is_enabled = p_cntr->isEnabled();
    if (is_enabled) {
      *(pp_cntrs_ + cnt) = (Counter*)*it;
      cnt++;
    }
  }

  num = cnt;
  if (0 == num) {
    return NULL;
  }

  return pp_cntrs_;
}

Counter** GpuCounterBlock::getAllCounters(uint32_t& num) {
  if (pp_cntrs_) {
    free(pp_cntrs_);
    pp_cntrs_ = NULL;
  }

  pp_cntrs_ = (Counter**)malloc(sizeof(GpuCounter*) * cntr_list_.size());

  if (!pp_cntrs_) {
    return NULL;
  }

  int cnt = 0;
  GpuCounterList::iterator it = cntr_list_.begin();
  GpuCounterList::iterator it_end = cntr_list_.end();
  for (; it != it_end; it++, cnt++) {
    *(pp_cntrs_ + cnt) = (Counter*)*it;
  }

  num = cnt;
  if (0 == num) {
    return NULL;
  }

  return pp_cntrs_;
}

bool GpuCounterBlock::setInfo(GPU_BLK_INFOS blk_info, uint32_t size, void* data) {
  return info_set_->setInfo(blk_info, size, data);
}

bool GpuCounterBlock::_checkMaxNumOfCounters() {
  uint32_t num_enabled = _getNumOfEnabledCounters();

  uint32_t* p_num_max = NULL;
  uint32_t size = 0;

  if (!getInfo(GPU_BLK_INFO_MAX_SIMULTANEOUS_COUNTERS, size, (void**)&p_num_max)) {
    return false;
  }

  if (num_enabled >= *p_num_max) {
    return false;
  }

  return true;
}

uint32_t GpuCounterBlock::_getNumOfEnabledCounters() {
  uint32_t cnt = 0;
  GpuCounterList::iterator it = cntr_list_.begin();
  GpuCounterList::iterator it_end = cntr_list_.end();

  for (; it != it_end; it++) {
    GpuCounter* p_cntr = (*it);
    bool is_enabled;
    is_enabled = p_cntr->isEnabled();
    if (is_enabled) {
      cnt++;
    }
  }

  return cnt;
}

std::string GpuCounterBlock::getErrorString(int error) {
  if ((error >= 0) && (error < kHsaCounterBlockErrorCodeMaxError)) {
    std::string err_string(error_string[error]);
    return err_string;
  }
  return "incorrect error code";
}

bool GpuCounterBlock::getParameter(uint32_t param, uint32_t& ret_size, void** pp_data) {
  return parameter_set_->getParameter(param, ret_size, pp_data);
}

bool GpuCounterBlock::setParameter(uint32_t param, uint32_t param_size, const void* pData) {
  return parameter_set_->setParameter(param, param_size, pData);
}

bool GpuCounterBlock::getInfo(uint32_t info, uint32_t& ret_size, void** pp_data) {
  return info_set_->getInfo(info, ret_size, pp_data);
}
}
