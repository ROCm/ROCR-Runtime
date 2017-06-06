#ifndef _INFO_SET_H_
#define _INFO_SET_H_

// This file contains declaration of IInfoSet class.
#include "hsa_perf.h"
#include "var_data.h"

#include <stdlib.h>
#include <stdint.h>

namespace pm4_profile {
// An abstract class defining a container to hold a information data set
// (e.g. PMU info, CounterGroup info, etc.).  Unlike \ref IParameterSet,
// This class allows only the children of the class to set the information.
class InfoSet {
 public:
  // IInfoSet constructor
  InfoSet();

  // IInfoSet destructor
  virtual ~InfoSet();

  // Query value of the information specified by info
  // @param[in] info The enumeration of information to be queried
  // @param[out] ret_size The returned size of data
  // @param[out] pp_data The pointer to the returned data
  // /return true or false
  bool getInfo(uint32_t info, uint32_t& ret_size, void** pp_data);

  // Set value for the information specified by info
  // @param[in] info The enumeration of information to be queried
  // @param[out] info_size The size of data
  // @param[out] p_data The pointer to the data to be set
  // /return true or false
  bool setInfo(uint32_t info, uint32_t info_size, void* p_data);

 private:
  // Remove all data in the parameter table
  void releaseParameters();

  // InfoSet property: The info table
  VarDataMap info_table_;

  // Pointer to the buffer used in getInfo
  void* p_data_;
};
}
#endif
