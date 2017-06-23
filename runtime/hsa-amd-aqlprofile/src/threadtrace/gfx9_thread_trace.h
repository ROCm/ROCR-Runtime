#ifndef _GFX9_THREAD_TRACE_H_
#define _GFX9_THREAD_TRACE_H_

#include "gfxip/gfx9/gfx9_registers.h"
#include "gfxip/gfx9/gfx9_typedef.h"
#include "gfxip/gfx9/gfx9_enum.h"
#include "gfxip/gfx9/gfx9_offset.h"
#include "gfxip/gfx9/gfx9_pm4defs.h"
#include "thread_trace.h"

#include <string>

using namespace pm4_profile::gfx9;

namespace pm4_profile {

typedef struct Gfx9ThreadTraceCfgRegs {
  // Size of thread trace buffer
  regSQ_THREAD_TRACE_SIZE ttRegSize;
  // Thread trace mode
  regSQ_THREAD_TRACE_MODE ttRegMode;
  // Thread trace wave mask
  regSQ_THREAD_TRACE_MASK ttRegMask;
  // Thread trace token mask
  regSQ_THREAD_TRACE_TOKEN_MASK ttRegTokenMask;
  // Thread trace token mask2
  regSQ_THREAD_TRACE_TOKEN_MASK2 ttRegTokenMask2;
  // Thread trace perf mask
  regSQ_THREAD_TRACE_PERF_MASK ttRegPerfMask;
} Gfx9ThreadTraceCfgRegs;

// Encapsulates the various Api and structures used to enable a thread
// trace session and collect its data
class Gfx9ThreadTrace : public ThreadTrace {
 public:
  Gfx9ThreadTrace();

  ~Gfx9ThreadTrace();

  // Initializes various data structures and handles that
  // are needed to support a thread trace session
  bool Init(const ThreadTraceConfig* config);

  // Builds Pm4 command stream to program hardware registers that
  // enable a thread trace session, including the issue of an event
  // to begin thread session
  void BeginSession(pm4_profile::DefaultCmdBuf* cmdBuff, pm4_profile::CommandWriter* cmdWriter);

  // Builds Pm4 command stream to program hardware registers that
  // disable a thread trace session, including the issue of an event
  // to stop currently ongoing thread session
  void StopSession(pm4_profile::DefaultCmdBuf* cmdBuff, pm4_profile::CommandWriter* cmdWriter);

  // Validates that thread trace session ran correctly i.e. did not
  // encounter any errors.
  bool Validate();

  // Initializes the handle of buffer used to collect SQTT data
  void setSqttDataBuff(uint8_t* sqttBuffer, uint32_t sqttBuffSz);

  // Initializes the handle of buffer used to read control data of SQTT
  void setSqttCtrlBuff(uint32_t* ctrlBuff) { ttStatus_ = ctrlBuff; }

  // Return status info size
  uint32_t StatusSizeInfo() const { return TT_STATUS_IDX_MAX * sizeof(uint32_t) * numSE_; }

  // Return number of Shader Engines
  uint32_t getNumSe() { return numSE_; }

 private:
  // Holds number of Shader Engines present on device
  uint32_t numSE_;

  // Thread traces status register indices to determine
  // status of thread trace run
  typedef enum {
    TT_STATUS_IDX_STATUS = 0,
    TT_STATUS_IDX_CNTR = 1,
    TT_STATUS_IDX_WPTR = 2,
    TT_STATUS_IDX_MAX = 3
  } TTStatusReg;

  // A list of tuples of TT_STATUS_IDX_MAX size,
  // giving status of thread trace
  uint32_t* ttStatus_;

  // Size of thread trace buffer per shader engine
  uint32_t ttBuffSize_;

  // Handles of Device memory used for thread trace
  std::vector<uint64_t> devMemList_;

  // Registers that need to be programmed for Thread Trace
  Gfx9ThreadTraceCfgRegs ttCfgRegs_;

  // Initializes thread trace registers with default parameters.
  // These are potentially updated based on updates to thread trace
  // configuration object by user
  void InitThreadTraceCfgRegs();
};

}  // pm4_profile

#endif  // _GFX9_THREAD_TRACE_H_
