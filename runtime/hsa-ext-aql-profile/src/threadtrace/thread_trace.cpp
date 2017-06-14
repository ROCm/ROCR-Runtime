#include <assert.h>

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

uint8_t ThreadTrace::GetCuId() {
  uint32_t cuId = ttConfig_.threadTraceTargetCu;
  assert((cuId <= 15) && "Cu Id must be between 0 and 15");
  return cuId;
}

uint8_t ThreadTrace::GetVmId() {
  uint32_t vmId = ttConfig_.threadTraceVmIdMask;
  assert((vmId <= 2) && "VmId must be between 0 and 2");
  return vmId;
}

uint32_t ThreadTrace::GetMask() {
  uint32_t ttMask = ttConfig_.threadTraceMask;
  const uint32_t validMask = 0x00C0D0;
  assert(((ttMask & validMask) == 0) && "Mask should have bits [4,6,7] set to Zero");
  return ttMask;
}

uint32_t ThreadTrace::GetTokenMask() {
  uint32_t tokenMask = ttConfig_.threadTraceTokenMask;
  const uint32_t validMask = 0xFF000000;
  assert(((tokenMask & validMask) == 0) && "TokenMask should have bits [31:25] set to Zero");
  return tokenMask;
}

uint32_t ThreadTrace::GetTokenMask2() {
  uint32_t tokenMask2 = ttConfig_.threadTraceTokenMask2;
  const uint32_t validMask = 0xFFFF0000;
  assert(((tokenMask2 & validMask) == 0) && "TokenMask2 should have bits [31:16] set to Zero");
  return tokenMask2;
}

}  // pm4_profile
