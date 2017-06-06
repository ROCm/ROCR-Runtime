#include <string.h>
#include "var_data.h"

namespace pm4_profile {
VarData::VarData() {
  size_ = 0;
  p_data_ = NULL;
}

VarData::~VarData() {}

void VarData::clear() {
  size_ = 0;
  if (p_data_) {
    free(p_data_);
    p_data_ = NULL;
  }
}

bool VarData::set(uint32_t size, const void* p_data) {
  if (!p_data || (size == 0)) {
    return false;
  }

  clear();

  if (NULL == (p_data_ = malloc(size))) {
    return false;
  }

  memcpy(p_data_, p_data, size);
  size_ = size;

  return true;
}

uint32_t VarData::get(uint32_t size, void* p_data) {
  if (!p_data || !size || !p_data_ || !size_) {
    return 0;
  }

  uint32_t ret_size = size < size_ ? size : size_;

  memcpy(p_data, p_data_, ret_size);

  return ret_size;
}
}  // pm4_profile
