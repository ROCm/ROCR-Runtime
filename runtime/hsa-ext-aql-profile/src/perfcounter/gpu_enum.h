#ifndef _GPU_ENUM_H_
#define _GPU_ENUM_H_

namespace pm4_profile {

// Enumeration containing GPU hardware block information
enum GPU_BLK_INFOS {
  GPU_BLK_INFO_BLOCK_NAME,
  GPU_BLK_INFO_ID,
  GPU_BLK_INFO_MAX_SHADER_ENGINE_COUNT,
  GPU_BLK_INFO_MAX_SHADER_ARRAY_COUNT,
  GPU_BLK_INFO_MAX_INSTANCE_COUNT,
  GPU_BLK_INFO_CONTROL_METHOD,
  GPU_BLK_INFO_MAX_EVENT_ID,
  GPU_BLK_INFO_MAX_SIMULTANEOUS_COUNTERS,
  GPU_BLK_INFO_MAX_STREAMING_COUNTERS,
  GPU_BLK_INFO_SHARED_HW_COUNTERS,
  GPU_BLK_INFO_HAS_FILTERS,

  // Trace-specific stuff
  GPU_TRC_BLK_INFO_BUFFER_SIZE,
  GPU_TRC_BLK_INFO_BUFFER_WRITE_POINTER_OFFSET,
  GPU_TRC_BLK_INFO_BUFFER_WRAPPED,
  GPU_TRC_BLK_INFO_DATA_SIZE_ESTIMATE,
  GPU_TRC_BLK_INFO_DATA_POINTER,
};


/**
 * Trace buffer parameters
 */
enum GPU_BLK_PARAMS {
  // Allows user to specify the size of the trace buffers.
  GPU_BLK_PARAM_TRACE_BUFFER_SIZE,

  // If we decide to implement this functionality, this will allow the user
  // to specify the number of trace buffers to create.
  GPU_BLK_PARAM_TRACE_BUFFER_ARRAY,

  // Specifies whether a new trace buffer should be used for each cmd buffer.
  // This allows for better correlation of data back to the host application
  // If this is enabled, and the user does not explicitly specify a
  // TRACE_BUFFER_ARRAY, then the driver should automatically allocate
  // additional buffers as needed so that as much of the application
  // can be traced as possible, until the PerfExperiment is ended.
  // If a TRACE_BUFFER_ARRAY is specified, then only as many buffers
  // as specified should be created. If more cmd buffers get submitted
  // than there are trace buffers, then the later cmd buffers should
  // not be traced.
  GPU_BLK_PARAM_TRACE_NEW_BUFFER_ON_SUBMIT,
};


// Enumeration containing GPU counter parameters
enum GPU_CNTR_PARAMS {
  GPU_CNTR_PARAM_SHADERENGINE_ID,
  GPU_CNTR_PARAM_SHADERARRAY_ID,
  GPU_CNTR_PARAM_INSTANCE_ID,
  GPU_CNTR_PARAM_EVENT_SELECT_ID,
  GPU_CNTR_PARAM_SIMD_MASK,
  GPU_CNTR_PARAM_PERF_MODE,
  GPU_CNTR_PARAM_TRACE_TYPE,
};
}
#endif
