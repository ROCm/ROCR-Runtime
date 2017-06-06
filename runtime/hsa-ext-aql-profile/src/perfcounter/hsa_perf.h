#ifndef _HSA_PERF_H_
#define _HSA_PERF_H_

#include "rocr_profiler.h"

#if !defined(AMD_AMP_HSA_INCLUDES)
#include <map>
#include <string>
#include <stdlib.h>
#include <stdint.h>
#endif

namespace pm4_profile {
class Pmu;
class Counter;
class CounterBlock;
class TraceGroup;
class CommandWriter;
class DefaultCmdBuf;


// @brief This is an abstract class for defining a CounterBlock. Each
// CounterBlock contains a set of Counters that often belong to the
// same functional unit
//
// For AMD GPU, this can represent blocks of Counters in each HW block
// (e.g. SQ, SQI, CP, etc.).
// For AMD CPU, this can represent blocks of core PMCs, NB PMCs, L2I PMCs
// on each CPU device
//
// Generally, CounterBlocks are created and initialized by the \ref Pmu class.
// Users can query them by calling \ref Pmu::getAllCounterBlocks() or
// \ref Pmu::getCounterBlockById(). A CounterBlock is enabled if it contains
// enabled Counters in the block.
//
// Users can manage Counters in each GounterBlock (e.g. create, destroy,
// enable and disable).  To specify a Counter, users simply call \ref
// createCounter. Then it can be enabled or disabled using \ref
// Counter::setEnable.  When a Counter is enabled, it is checked against the
// CounterBlock checks to make sure that the enabled-counter is valid and is
// not conflicting with the current Counters in the block.
class CounterBlock {
 public:
  typedef enum HsaCounterBlockErrorCode {
    //  Generic CounterBlock error
    kHsaCounterBlockErrorCodeNoError = 0x0,

    // Generic CounterBlock error
    kHsaCounterBlockErrorCodeGenericError,

    // The maximum number of Counters in the block is reached.
    kHsaCounterBlockErrorCodeMaxNumCounterReached,

    // The counter does not belong to this block.
    kHsaCounterBlockErrorCodeUnknownCounter,

    // The counter does not belong to this block.
    kHsaCounterBlockErrorCodeMaxError
  } HsaCounterBlockErrorCode;

  // Destructor of CounterBlock.
  virtual ~CounterBlock() {}

  // Given and error number reported from getLastError or returned from a
  // function call, retreive the corresponding stl string.
  // @param[in] error The error corresponding to a call to getLastError
  // or a return code from a function call.
  // Return An stl string representing a text corresponding to the error
  // number.
  // If invalid error code is given, the returned string is empty.
  virtual std::string getErrorString(int error) = 0;

  // Create an Counter object return a pointer to caller.
  // Return On success, this function returns a pointer to Counter
  //        On failure, this function returns NULL
  // Possible error codes are:
  //        kHSAPerfErrorCodesUnmodifiableState
  //        kHsaCounterBlockErrorCodeMaxNumCounterReached
  virtual Counter* createCounter() = 0;

  // Destroy the Counter. The CounterBlock which owns the Counter must be in
  // disabled state.
  // Return true or false
  // Possible error codes are:
  //   kHSAPerfErrorCodesInvalidAargs
  //   kHSAPerfErrorCodesUnmodifiableState
  //   kHsaCounterBlockErrorCodeUnknownCounter
  virtual bool destroyCounter(Counter* p_counter) = 0;

  // Destroy all counters in the block. The CounterBlock must be in disable
  // state.
  // Return true or false.
  // Possible error codes are:
  //   kHSAPerfErrorCodesUnmodifiableState
  virtual bool destroyAllCounters() = 0;

  // Get a list of pointers to the enabled Counters in this CounterBlock.
  // note The Counter must be created by the same CounterBlock object using
  // createCounter().
  // @param[in] num The number of Counter pointers returned.
  // Return
  //   return a list of pointers to the enabled Counters.
  //   return NULL if no counter is enabled.
  virtual Counter** getEnabledCounters(uint32_t& num) = 0;

  // Get a list of pointers to the all Counters in this CounterBlock.
  // note The Counter must be created by the same CounterBlock object using
  // createCounter().
  // @param[in] num The number of Counter pointers returned.
  // Return
  //   return a list of pointers in the CounterBlock.
  //   return NULL if no counter is enabled.
  virtual Counter** getAllCounters(uint32_t& num) = 0;

  // Query value of the parameter specified by param
  // @param[in] param The enumeration of parameter to be queried
  // @param[out] return_size The returned size of data
  // @param[out] pp_data The pointer to the returned data. The API is
  // responsible for managing the memory to store the information as specified
  // by return_size.
  //
  // Return true or false
  // Possible error codes are:
  //   kHSAPerfErrorCodesInvalidParam
  //   kHSAPerfErrorCodesInvalidParamSize
  //   kHSAPerfErrorCodesInvalidParamData
  virtual bool getParameter(uint32_t param, uint32_t& return_size, void** pp_data) = 0;

  // Set value for the parameter specified by param
  // @param[in] param The enumeration of parameter to be queried
  // @param[out] param_size The size of data
  // @param[out] p_data The pointer to the data to be set. Users are responsible
  // for deallocating the memory of p_data after calling the API.
  // Return true or false
  // Possible error codes are:
  //   kHSAPerfErrorCodesUnmodifiableState
  //   kHSAPerfErrorCodesInvalidParam
  //   kHSAPerfErrorCodesInvalidParamSize
  //   kHSAPerfErrorCodesInvalidParamData
  virtual bool setParameter(uint32_t param, uint32_t param_size, const void* p_data) = 0;

  // Query value of the information specified by info
  // @param[in] info The enumeration of information to be queried
  // @param[out] Return_size The returned size of data
  // @param[out] pp_data The pointer to the returned data
  // Return true or false
  // Possible error codes are:
  //   kHSAPerfErrorCodesInvalidInfo
  //   kHSAPerfErrorCodesInvalidInfoSize
  //   kHSAPerfErrorCodesInvalidInfoData
  virtual bool getInfo(uint32_t info, uint32_t& return_size, void** pp_data) = 0;
};  // class CounterBlock


// This is an abstract class for defining a TraceGroup. TraceGroup inherits
// CounterBlock and add interfaces for managing trace buffer. It also supports
// user-data insertion into trace.  This allows users to insert arbitary data
// (e.g. markers) into trace which and can be used to correlating a specific
// events to the collected trace data.
class TraceGroup : public CounterBlock {
 public:
  typedef enum HsaTraceGroupErrorCode {
    // Generic TraceGroup error
    HsaTraceGroupErrorCodeGenericError = 0x100,
  } HsaTraceGroupErrorCode;

  // Destructor of TraceGroup.
  virtual ~TraceGroup() {}

  // Obtains the number of buffers which were collected as part of
  // the trace.
  // Return The number of collected buffers.
  virtual uint32_t getCollectedBufferCount() = 0;

  // Locks a trace buffer for host access.
  // @param[in] buffer_id The index of the buffer to be locked.
  // Return true or false
  virtual bool lock(uint32_t buffer_id) = 0;

  // Unlock a trace buffer that was previously locked.
  // @param[in] buffer_id The index of the buffer to be unlocked.
  // Return true or false
  virtual bool unlock(uint32_t buffer_id) = 0;

  // Inserts data (e.g. trace marker) into the trace.
  // @param[in] type The type of data to be inserted.
  // @param[in] p_data The data to be inserted.
  // @param[in] data_size The size of data to be inserted.
  // Return true or false
  virtual bool insertUserData(uint32_t type, void* p_data, uint32_t data_size) = 0;
};  // class TraceGroup


// This is an abstract class for defining a performance Counter.
// Users can obtain a Counter from \ref CounterBlock::createCounter().
// Once obtained, users can set up Counter parameters, and enable it using
// \ref Counter::setEnable().
//
// There are several types of Counter as defined in \ref
//    HsaCounterBlockTypeMask.
// Only the supported Counter type can be added to the CounterBlock.
//
// Each Counter can store Counter-specific parameters.  The Counter is used to
// specify types of event to be counted.
class Counter {
 public:
  typedef enum HsaCounterErrorCode {
    // Generic Counter error
    kHsaCounterErrorCodeNoError = 0x0,

    // Generic Counter error
    kHsaCounterErrorCodeGenericError = 0x1,

    // Counter already error
    kHsaCounterErrorCodeAlreadySet = 0x2,

    // Counter result is not ready.
    kHsaCounterErrorCodeResultNotReady = 0x3,

    // Max counter error num
    kHsaCounterErrorCodeMax,
  } HsaCounterErrorCode;

  // Destructor of Counter
  virtual ~Counter() {}

  // Retrieve the last error code generated.  This should be checked when
  // values returned are NULL or void.
  // Return an integer corresponding to the last error reported.
  virtual int getLastError() = 0;

  // Given and error number reported from getLastError or returned from a
  // function call, retreive the corresponding stl string.
  // @param[in] error The error corresponding to a call to getLastError
  // or a return code from a function call.
  // Return An stl string representing a text corresponding to the error
  // number. If invalid error code is given, the returned string is empty.
  virtual std::string getErrorString(int error) = 0;

  // Get the \ref CounterBlock which owns this counter.
  // Return
  //   On success, it returns a pointer to the CounterBlock.
  //   On Failure, it returns NULL.
  virtual CounterBlock* getCounterBlock() = 0;

  // Enable or disable the Counter.
  // @param[in] b Set to true to enable the CounterBlock.
  // Return
  //   return true when successfully set the state.
  //   return false otherwise.
  //   In case of the current state already is set to the specified value,
  //   the API returns true.
  //   Possible error codes are:
  //     kHSAPerfErrorCodesUnmodifiableState
  virtual bool setEnable(bool b) = 0;

  // Return the current state of the Counter.
  // Return true or false
  virtual bool isEnabled() = 0;

  // Return the status of this Counter whether the result is available.
  // Return true or false
  virtual bool isResultReady() = 0;

  // Query Counter result
  // note Must be implemented by derived classes
  // @param[out] p_result The pointer containing the returned result.
  // Return true or false
  // Possible error codes are:
  //   kHSAPerfErrorCodesInvalidAargs
  //   kHsaCounterErrorCodeResultNotReady
  virtual bool getResult(uint64_t* p_result) = 0;

  // Query value of the parameter specified by param
  // @param[in] param The enumeration of parameter to be queried
  // @param[out] Return_size The returned size of data
  // @param[out] pp_data The pointer to the returned data. The API is
  // responsible for managing the memory to store the information as
  // specified by return_size.
  // Return true or false
  //   Possible error codes are:
  //     kHSAPerfErrorCodesInvalidParam
  //     kHSAPerfErrorCodesInvalidParamSize
  //     kHSAPerfErrorCodesInvalidParamData
  virtual bool getParameter(uint32_t param, uint32_t& return_size, void** pp_data) = 0;

  // Set value for the parameter specified by param
  // @param[in] param The enumeration of parameter to be queried
  // @param[out] param_size The size of data
  // @param[out] p_data The pointer to the data to be set. Users are responsible
  // for deallocating the memory of p_data after calling the API.
  // Return true or false
  //   Possible error codes are:
  //     kHSAPerfErrorCodesUnmodifiableState
  //     kHSAPerfErrorCodesInvalidParam
  //     kHSAPerfErrorCodesInvalidParamSize
  //     kHSAPerfErrorCodesInvalidParamData
  virtual bool setParameter(uint32_t param, uint32_t param_size, const void* p_data) = 0;
};  // class Counter

class Pmu {
 public:
  // Enumeration of Pmu error codes
  typedef enum HsaPmuErrorCode {
    // Generic PMU error
    kHsaPmuErrorCodeNoError = 0x0,

    // Unknown CounterBlock ID
    kHsaPmuErrorCodeUnknownCounterBlockId,

    // No CounterBlock exists
    kHsaPmuErrorCodeNoCounterBlock,

    // The previously operation is not valid. This could be due to
    // invalid transition from the current state.
    kHsaPmuErrorCodeInvalidOperation,

    // PMU is not currently available (e.g. PMU is currently
    // in-used by others)
    kHsaPmuErrorCodeNotAvailable,

    // PMU is not currently available (e.g. PMU is currently
    // in-used by others)
    kHsaPmuErrorCodeErrorState,

    // PMU result is timeout
    kHsaPmuErrorCodeTimeOut,

    // Max error count
    kHsaPmuErrorCodeMax
  } HsaPmuErrorCode;

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

  // Get CounterBlock from Id
  // @param[in] id ID of the target CounterBlock
  // Return
  //   On success, it returns a pointer to specified CounterBlock.
  //   On Failure, it returns NULL.
  //   Possible error codes are:
  //     kHsaPmuErrorCodeUnknownCounterBlockId.
  virtual CounterBlock* getCounterBlockById(uint32_t id) = 0;

  // Get all available CounterBlock
  // @param[out] num_block The returned number of CounterBlocks
  // Return On success, it returns an array of CounterBlock pointers.
  //   On Failure, it returns NULL.
  virtual CounterBlock** getAllCounterBlocks(uint32_t& num_block) = 0;

  // Get current PMU profiling state.
  // Return The PMU profiling state as defined in \ref PMU_PROFILE_STATES
  virtual rocr_pmu_state_t getCurrentState() = 0;

  // Start profiling on the PMU.
  // @param[in] reset_counter indicates whether reset counter before
  // recording. Default is reset counters.
  // note This function must be implemented by children classes.
  // Return true or false
  //   Possible error codes are:
  //     kHsaPmuErrorCodeInvalidOperation
  //     kHsaPmuErrorCodeNotAvailable
  virtual bool begin(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter, bool reset = true) = 0;

  // Stop profiling on the PMU.
  // note This function must be called after \ref begin().
  // note This function must be implemented by children classes.
  // Return true or false
  //   Possible error codes are:
  //     kHsaPmuErrorCodeInvalidOperation
  virtual bool end(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter) = 0;

  // Initializes the handle of buffer used to collect PMC data
  // @param pmcBuffer The buffer pointer
  // @param cmdBufSz Size in terms of bytes
  virtual bool setPmcDataBuff(uint8_t* pmcBuffer, uint32_t pmcBuffSz) = 0;

  // Query value of the parameter specified by param
  // @param[in] param The enumeration of parameter to be queried
  // @param[out] Return_size The returned size of data
  // @param[out] pp_data The pointer to the returned data. The API is
  // responsible for managing the memory to store the information as
  // specified by return_size.
  // Return true or false
  //   Possible error codes are:
  //     kHSAPerfErrorCodesInvalidParam
  //     kHSAPerfErrorCodesInvalidParamSize
  //     kHSAPerfErrorCodesInvalidParamData
  virtual bool getParameter(uint32_t param, uint32_t& return_size, void** pp_data) = 0;

  // Set value for the parameter specified by param
  // @param[in] param The enumeration of parameter to be queried
  // @param[out] param_size The size of data
  // @param[out] p_data The pointer to the data to be set. Users are responsible
  // for deallocating the memory of p_data after calling the API.
  // Return true or false
  //   Possible error codes are:
  //     kHSAPerfErrorCodesUnmodifiableState
  //     kHSAPerfErrorCodesInvalidParam
  //     kHSAPerfErrorCodesInvalidParamSize
  //     kHSAPerfErrorCodesInvalidParamData
  virtual bool setParameter(uint32_t param, uint32_t param_size, const void* p_data) = 0;

  // Query value of the information specified by info
  // @param[in] info The enumeration of information to be queried
  // @param[out] Return_size The returned size of data
  // @param[out] pp_data The pointer to the returned data
  // Return true or false
  //   Possible error codes are:
  //     kHSAPerfErrorCodesInvalidInfo
  //     kHSAPerfErrorCodesInvalidInfoSize
  //     kHSAPerfErrorCodesInvalidInfoData
  virtual bool getInfo(uint32_t info, uint32_t& return_size, void** pp_data) = 0;

  // Returns number of shader engines per block
  // for the blocks featured shader engines instancing
  virtual uint32_t getNumSe() = 0;

};  // class Pmu
}  // pm4_profile
#endif  // _HSA_PERF_H_
