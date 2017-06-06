#include "parameter_set.h"

using namespace std;

namespace pm4_profile {
ParameterSet::ParameterSet() {
  releaseParameters();
  param_table_.clear();
  p_data_ = NULL;
}

ParameterSet::~ParameterSet() {
  releaseParameters();
  param_table_.clear();
  free(p_data_);
  p_data_ = NULL;
}

bool ParameterSet::setParameter(uint32_t param, uint32_t param_size, const void* p_data) {
  if (param_table_.end() != param_table_.find(param)) {
    return false;
  }

  VarData data;
  if (!data.set(param_size, p_data)) {
    return false;
  }

  param_table_.insert(VarDataMap::value_type(param, data));
  return true;
}

bool ParameterSet::getParameter(uint32_t param, uint32_t& ret_size, void** pp_data) {
  if (!pp_data || (0 == param_table_.size())) {
    return false;
  }

  VarDataMap::iterator it = param_table_.find(param);
  if (it == param_table_.end()) {
    return false;
  }

  int size = it->second.getSize();
  if (size == 0) {
    return false;
  }

  // for NULL pointer, free does nothing
  free(p_data_);
  p_data_ = malloc(size);
  if (!p_data_) {
    return false;
  }

  // store the pointer to be freed
  *pp_data = p_data_;

  ret_size = param_table_[param].get(size, *pp_data);

  return true;
}

bool ParameterSet::releaseParameters() {
  VarDataMap::iterator it = param_table_.begin();
  VarDataMap::iterator table_end = param_table_.end();

  for (; it != table_end; it++) {
    it->second.clear();
  }

  return true;
}

}  // pm4_profile
