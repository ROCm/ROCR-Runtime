#ifndef _PARAMETER_SET_H_
#define _PARAMETER_SET_H_

/*!
   \note This file contains declaration of IParameterSet class.
 */
#include "hsa_perf.h"
#include "var_data.h"

#include <stdlib.h>
#include <stdint.h>

namespace pm4_profile {
/*!
   A class defining a container to hold parameter data set
   (e.g. PMU parameter, CounterGroup parameter, etc.).
 */
class ParameterSet {
 public:
  /*!
     Enumeration containing types of parameters
   */
  enum parameter {
    PARAM_MAX,
  };

  /*! IParameterSet constructor */
  ParameterSet();

  /*! IParameterSet destructor */
  virtual ~ParameterSet();

  /*!
     Query value of the parameter specified by param
     @param[in] param The enumeration of parameter to be queried
     @param[out] ret_size The returned size of data
     @param[out] pp_data The pointer to the returned data
     /return true or false
   */
  bool getParameter(
      /*in*/ uint32_t param,
      /*out*/ uint32_t& ret_size,
      /*out*/ void** pp_data);

  /*!
     Set value for the parameter specified by param
     @param[in] param The enumeration of parameter to be queried
     @param[out] param_size The size of data
     @param[out] p_data The pointer to the data to be set
     /return true or false
   */
  bool setParameter(
      /*in*/ uint32_t param,
      /*in*/ uint32_t param_size,
      /*in*/ const void* p_data);

 private:
  /*!
     Remove all data in the parameter table
  */
  bool releaseParameters();

  /*!
     IParameterSet property: The parameter table
   */
  VarDataMap param_table_;

  /*!
    Pointer to the buffer used in getParameter
   */
  void* p_data_;
};
}

#endif  // _PARAMETER_SET_H_
