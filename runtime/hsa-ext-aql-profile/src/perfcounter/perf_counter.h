#ifndef _HSA_PERF_H_
#define _HSA_PERF_H_

#include <vector>
#include <map>
#include <string>
#include <stdint.h>

namespace pm4_profile {
class DefaultCmdBuf;
class CommandWriter;

typedef std::vector<uint32_t> CountersVec;
typedef std::map<uint32_t, CountersVec> CountersMap;

class Pmu {
 public:
  // Enumeration of Pmu error codes
  typedef enum ErrorCode {
    // Generic PMU error
    kErrorCodeNoError = 0x0,

    // Unknown CounterBlock ID
    kErrorCodeUnknownCounterBlockId,

    // No CounterBlock exists
    kErrorCodeNoCounterBlock,

    // The previously operation is not valid. This could be due to
    // invalid transition from the current state.
    kErrorCodeInvalidOperation,

    // PMU is not currently available (e.g. PMU is currently
    // in-used by others)
    kErrorCodeNotAvailable,

    // PMU is not currently available (e.g. PMU is currently
    // in-used by others)
    kErrorCodeErrorState,

    // PMU result is timeout
    kErrorCodeTimeOut,

    // Max error count
    kErrorCodeMax
  } ErrorCode;

  // Destructor of PMU.
  // note This stops the performance counters if running and releases
  // any resources used by the PMU.
  virtual ~Pmu() {}

  // Retrieve the last error code generated.  This should be checked when
  // values returned are NULL or void.
  // Return an integer corresponding to the last error reported.
  virtual int getLastError() = 0;

  // Given and error number reported from getLastError or returned from a
  // function call, retreive the corresponding stl string.
  // @param[in] error The error corresponding to a call to getLastError
  // or a return code from a function call.
  // Return An stl string representing a text corresponding to the error
  //   number. If invalid error code is given, the returned string is empty.
  virtual std::string getErrorString(int error) = 0;

  // Start profiling on the PMU.
  // @param[in] reset_counter indicates whether reset counter before
  // recording. Default is reset counters.
  // note This function must be implemented by children classes.
  // Return true or false
  //   Possible error codes are:
  //     kErrorCodeInvalidOperation
  //     kErrorCodeNotAvailable
  virtual void begin(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter,
                     const CountersMap& countersMap) = 0;

  // Stop profiling on the PMU.
  // note This function must be called after \ref begin().
  // note This function must be implemented by children classes.
  // Return true or false
  //   Possible error codes are:
  //     kErrorCodeInvalidOperation
  virtual uint32_t end(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter,
                       const CountersMap& countersMap, void* dataBuff) = 0;

  // Returns number of shader engines per block
  // for the blocks featured shader engines instancing
  virtual uint32_t getNumSe() = 0;

};  // class Pmu
}  // pm4_profile
#endif  // _HSA_PERF_H_
