#include "info_set.h"
#include "var_data.h"
using namespace std;

namespace pm4_profile {
InfoSet::InfoSet() {
  releaseParameters();
  info_table_.clear();
  p_data_ = NULL;
}

InfoSet::~InfoSet() {
  releaseParameters();
  info_table_.clear();
  free(p_data_);
  p_data_ = NULL;
}

bool InfoSet::setInfo(uint32_t info, uint32_t info_size, void* p_data) {
  if (info_table_.end() != info_table_.find(info)) {
    return false;
  }

  VarData data;
  if (!data.set(info_size, p_data)) {
    return false;
  }

  info_table_.insert(VarDataMap::value_type(info, data));
  return true;
}

bool InfoSet::getInfo(uint32_t info, uint32_t& ret_size, void** pp_data) {
  if (!pp_data || (0 == info_table_.size())) {
    return false;
  }

  VarDataMap::iterator it = info_table_.find(info);
  if (it == info_table_.end()) {
    return false;
  }

  int size = it->second.getSize();
  if (size == 0) {
    return false;
  }

  free(p_data_);
  p_data_ = NULL;

  p_data_ = malloc(size);
  if (!p_data_) {
    return false;
  }

  *pp_data = p_data_;

  ret_size = info_table_[info].get(size, *pp_data);

  return true;
}

void InfoSet::releaseParameters() {
  VarDataMap::iterator it = info_table_.begin();
  VarDataMap::iterator table_end = info_table_.end();

  for (; it != table_end; it++) {
    it->second.clear();
  }

  return;
}

}  // pm4_profile
