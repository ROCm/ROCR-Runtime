#ifndef _VAR_DATA_H_
#define _VAR_DATA_H_

/*!
   \note This file contains declaration of IVarData class.
 */

#include "hsa_perf.h"

#include <map>
#include <stdlib.h>
#include <stdint.h>

namespace pm4_profile {
/*!
   This abstract class implements variable-size storage for information and
      parameter
   sets.
 */
class VarData {
 public:
  /*! Constructor for IVarData */
  VarData();

  /*! Destructor for IVarData */
  ~VarData();

  /*! Deallocate the memory and clean up */
  void clear();

  /*!
     Set the data to be stored.
     @param[in] size Size of data to be stored.
     @param[in] p_data Pointer to data to be stored.
     \return true or false
   */
  bool set(uint32_t size, const void* p_data);

  /*!
     Query the data that was stored.
     @param[in] size Size (in bytes) of the memory pointed to by p_data.
       This determines maximum size of the returned data.
     @param[in,out] p_data Pointer to the result buffer.
     \return Size (in bytes) of the returned result which is coppied into
       the buffer pointed to by p_data.
   */
  uint32_t get(uint32_t size, void* p_data);

  /*!
     Get size of the current data stored
     \return Size (in bytes) of the data stored.
   */
  uint32_t getSize() { return size_; }

 private:
  /*! Size of data being stored */
  uint32_t size_;

  /*! Pointer to the stored data */
  void* p_data_;
};

typedef std::map<uint32_t, VarData> VarDataMap;
}
#endif
