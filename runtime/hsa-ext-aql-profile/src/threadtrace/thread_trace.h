#ifndef _THREAD_TRACE_H_
#define _THREAD_TRACE_H_

#include <stdint.h>

#include "cmdwriter.h"

// Move them as static variables later on
#define TT_WRITE_PTR_MASK (0x3FFFFFFF)
// Size of block in bytesper increment in WPTR
#define TT_WRITE_PTR_BLK (32)
// Factor by which to shift buffer address
#define TT_BUFF_ALIGN_SHIFT (12)

namespace pm4_profile {

// ThreadTrace config
typedef struct ThreadTraceConfig {
  uint32_t threadTraceTargetCu;
  uint32_t threadTraceVmIdMask;
  uint32_t threadTraceMask;
  uint32_t threadTraceTokenMask;
  uint32_t threadTraceTokenMask2;
} ThreadTraceConfig;

// Encapsulates the various Api and structures that are used to enable
// a thread trace session and collect its data. Implementations of this
// interface program device specific registers to realize the functionality
class ThreadTrace {
  // Holds Thread Trace configuration information
  // @note: Currently not used i.e. is not exposed to users
  ThreadTraceConfig ttConfig_;

 public:
  // Destructor of the thread trace service handle
  virtual ~ThreadTrace(){};

  // Obtain the CU id to use for thread tracing
  uint8_t GetCuId();

  // Obtain the VM id to use for thread tracing
  uint8_t GetVmId();

  // Obtain the Mask to use for thread tracing
  uint32_t GetMask();

  // Obtain the Token Mask 1 to use for thread tracing
  uint32_t GetTokenMask();

  // Obtain the Token Mask 2 to use for thread tracing
  uint32_t GetTokenMask2();

  // Initializes various data structures and handles that
  // are needed to support a thread trace session
  virtual bool Init(const ThreadTraceConfig* config);

  // Initializes thread trace configuration object with default
  // parameters, that could potentially be overriden by user
  // @note: Currently not used i.e. is not exposed to users
  virtual void InitThreadTraceConfig(ThreadTraceConfig* config) const;

  // Allows user to configure various parameters of a thread trace session
  // @note: Currently not used i.e. is not exposed to users
  bool Config(uint32_t key, uint32_t value) { return true; };

  // Builds Pm4 command stream to program hardware registers that
  // enable a thread trace session, including the issue of an event
  // to begin thread session
  virtual void BeginSession(pm4_profile::DefaultCmdBuf* cmdBuff,
                            pm4_profile::CommandWriter* cmdWriter) = 0;

  // Builds Pm4 command stream to program hardware registers that
  // disable a thread trace session, including the issue of an event
  // to stop currently ongoing thread session
  virtual void StopSession(pm4_profile::DefaultCmdBuf* cmdBuff,
                           pm4_profile::CommandWriter* cmdWriter) = 0;

  // Validates that thread trace session ran correctly i.e. did not
  // encounter any errors.
  virtual bool Validate() = 0;

  // Initializes the handle of buffer used to collect SQTT data
  virtual void setSqttDataBuff(uint8_t* sqttBuffer, uint32_t sqttBuffSz) = 0;

  // Initializes the handle of buffer used to read control data of SQTT
  virtual void setSqttCtrlBuff(uint32_t* ctrlBuff) = 0;

  // Return number of Shader Engines
  virtual uint32_t getNumSe() = 0;

  // Return status info size
  virtual uint32_t StatusSizeInfo() const = 0;
};

}  // pm4_profile

#endif  // _THREAD_TRACE_H_
