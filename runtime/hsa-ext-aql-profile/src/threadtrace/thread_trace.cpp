#include <iostream>

#include "core/util/os.h"
#include "thread_trace.h"

namespace pm4_profile {

bool ThreadTrace::Init(const ThreadTraceConfig* config) {
  if (config) {
    ttConfig_ = *config;
  } else {
    InitThreadTraceConfig(&ttConfig_);
  }
  return true;
}

void ThreadTrace::InitThreadTraceConfig(ThreadTraceConfig* config) const {
  memset(config, 0, sizeof(ThreadTraceConfig));

  config->threadTraceTargetCu = 0;
  config->threadTraceVmIdMask = 0;
  config->threadTraceMask = 0;
  config->threadTraceTokenMask = 0;
  config->threadTraceTokenMask2 = 0;
}

uint8_t ThreadTrace::SetCuId() {
  uint32_t cuId = ttConfig_.threadTraceTargetCu;

  // Allow users to specify the CU to choose for Target tokens
  std::string var = os::GetEnvVar("HSA_THREAD_TRACE_SELECT_CU");
  if (var.length() > 0) {
    cuId = std::stol(var, nullptr, 16);
    std::cout << "Using " << cuId << " as CUID for Thread Trace" << std::endl;
  }

  assert((cuId <= 15) && "Cu Id must be between 0 and 15");

  return cuId;
}

uint8_t ThreadTrace::SetVmId() {
  uint32_t vmId = ttConfig_.threadTraceVmIdMask;

  // Allow users to specify the VMID to choose for Target tokens
  std::string var = os::GetEnvVar("HSA_THREAD_TRACE_SELECT_VMID");
  if (var.length() > 0) {
    vmId = std::stol(var, nullptr, 16);
    std::cout << "Using " << vmId << " as VMID for Thread Trace" << std::endl;
  }

  assert((vmId <= 2) && "VmId must be between 0 and 2");

  return vmId;
}

uint32_t ThreadTrace::SetMask() {
  uint32_t ttMask = ttConfig_.threadTraceMask;
  const uint32_t validMask = 0x00C0D0;

  // Allow users to specify the Mask to choose for configuration parameters
  std::string var = os::GetEnvVar("HSA_THREAD_TRACE_SELECT_MASK");
  if (var.length() > 0) {
    ttMask = std::stol(var, nullptr, 16);
    std::cout << "Using " << ttMask << " as Mask for Thread Trace" << std::endl;
  }

  assert(((ttMask & validMask) == 0) && "Mask should have bits [4,6,7] set to Zero");

  return ttMask;
}

uint32_t ThreadTrace::SetTokenMask() {
  uint32_t tokenMask = ttConfig_.threadTraceTokenMask;
  const uint32_t validMask = 0xFF000000;

  // Allow users to specify the TokenMask to choose for Target tokens
  std::string var = os::GetEnvVar("HSA_THREAD_TRACE_SELECT_TOKEN_MASK1");
  if (var.length() > 0) {
    tokenMask = std::stol(var, nullptr, 16);
    std::cout << "Using " << tokenMask << " as TokenMask for Thread Trace" << std::endl;
  }

  assert(((tokenMask & validMask) == 0) && "TokenMask should have bits [31:25] set to Zero");

  return tokenMask;
}

uint32_t ThreadTrace::SetTokenMask2() {
  uint32_t tokenMask2 = ttConfig_.threadTraceTokenMask2;
  const uint32_t validMask = 0xFFFF0000;

  // Allow users to specify the TokenMask2 to choose for Target tokens
  std::string var = os::GetEnvVar("HSA_THREAD_TRACE_SELECT_TOKEN_MASK2");
  if (var.length() > 0) {
    tokenMask2 = std::stol(var, nullptr, 16);
    std::cout << "Using " << tokenMask2 << " as TokenMask2 for Thread Trace" << std::endl;
  }

  assert(((tokenMask2 & validMask) == 0) && "TokenMask2 should have bits [31:16] set to Zero");

  return tokenMask2;
}

}  // pm4_profile
