////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2023, Advanced Micro Devices, Inc. All rights reserved.
//
// Developed by:
//
//                 AMD Research and AMD HSA Software Development
//
//                 Advanced Micro Devices, Inc.
//
//                 www.amd.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#include "pcs_runtime.h"

#include <assert.h>
#include <mutex>

#include "core/inc/runtime.h"

#include "core/inc/amd_gpu_agent.h"

namespace rocr {
namespace pcs {

std::atomic<PcsRuntime*> PcsRuntime::instance_(NULL);
std::mutex PcsRuntime::instance_mutex_;

PcsRuntime* PcsRuntime::instance() {
  PcsRuntime* instance = instance_.load(std::memory_order_acquire);
  if (instance == NULL) {
    // Protect the initialization from multi threaded access.
    std::lock_guard<std::mutex> lock(instance_mutex_);

    // Make sure we are not initializing it twice.
    instance = instance_.load(std::memory_order_relaxed);
    if (instance != NULL) {
      return instance;
    }

    instance = CreateSingleton();
    if (instance == NULL) {
      return NULL;
    }
  }

  return instance;
}

PcsRuntime* PcsRuntime::CreateSingleton() {
  PcsRuntime* instance = new PcsRuntime();

  instance_.store(instance, std::memory_order_release);
  return instance;
}

void PcsRuntime::DestroySingleton() {
  PcsRuntime* instance = instance_.load(std::memory_order_acquire);
  if (instance == NULL) {
    return;
  }

  instance->Cleanup();

  instance_.store(NULL, std::memory_order_release);
  delete instance;
}

void PcsRuntime::Cleanup() {
  //TODO: Implement me
}

void ReleasePcSamplingRsrcs() { PcsRuntime::DestroySingleton(); }

PcsRuntime::PcSamplingSession::PcSamplingSession(
    core::Agent* _agent, hsa_ven_amd_pcs_method_kind_t method,
    hsa_ven_amd_pcs_units_t units, size_t interval, size_t latency, size_t buffer_size,
    hsa_ven_amd_pcs_data_ready_callback_t data_ready_callback, void* client_callback_data)
    : agent(_agent), thunkId_(0), valid_(true), sample_size_(0) {
  switch (method) {
    case HSA_VEN_AMD_PCS_METHOD_HOSTTRAP_V1:
      sample_size_ = sizeof(perf_sample_hosttrap_v1_t);
      break;
    case HSA_VEN_AMD_PCS_METHOD_STOCHASTIC_V1:
      sample_size_ = sizeof(perf_sample_snapshot_v1_t);
      break;
    default:
      valid_ = false;
      return;
  }

  if (!interval || !buffer_size || !data_ready_callback || (buffer_size % sample_size_)) {
    valid_ = false;
    return;
  }

  csd.method = method;
  csd.units = units;
  csd.interval = interval;
  csd.latency = latency;
  csd.buffer_size = buffer_size;
  csd.data_ready_callback = data_ready_callback;
  csd.client_callback_data = client_callback_data;
}

void PcsRuntime::PcSamplingSession::GetHsaKmtSamplingInfo(HsaPcSamplingInfo* sampleInfo) {
  sampleInfo->value_min = 0;
  sampleInfo->value_max = 0;
  sampleInfo->flags = 0;
  sampleInfo->value = csd.interval;

  switch (csd.method) {
    case HSA_VEN_AMD_PCS_METHOD_HOSTTRAP_V1:
      sampleInfo->method = HSA_PC_SAMPLING_METHOD_KIND_HOSTTRAP_V1;
      break;
   case HSA_VEN_AMD_PCS_METHOD_STOCHASTIC_V1:
      sampleInfo->method = HSA_PC_SAMPLING_METHOD_KIND_STOCHASTIC_V1;
      break;
  }

  switch (csd.units) {
    case HSA_VEN_AMD_PCS_INTERVAL_UNITS_MICRO_SECONDS:
      sampleInfo->units = HSA_PC_SAMPLING_UNIT_INTERVAL_MICROSECONDS;
      break;
    case HSA_VEN_AMD_PCS_INTERVAL_UNITS_CLOCK_CYCLES:
      sampleInfo->units = HSA_PC_SAMPLING_UNIT_INTERVAL_CYCLES;
      break;
    case HSA_VEN_AMD_PCS_INTERVAL_UNITS_INSTRUCTIONS:
      sampleInfo->units = HSA_PC_SAMPLING_UNIT_INTERVAL_INSTRUCTIONS;
      break;
  }
}

hsa_status_t PcsRuntime::PcSamplingIterateConfig(core::Agent* agent, hsa_ven_amd_pcs_iterate_configuration_callback_t configuration_callback,
    void* callback_data) {

  AMD::GpuAgentInt* gpu_agent = static_cast<AMD::GpuAgentInt*>(agent);
  return gpu_agent->PcSamplingIterateConfig(configuration_callback, callback_data);
}

hsa_status_t PcsRuntime::PcSamplingCreate(core::Agent* agent, hsa_ven_amd_pcs_method_kind_t method,
                                       hsa_ven_amd_pcs_units_t units, size_t interval,
                                       size_t latency, size_t buffer_size,
                                       hsa_ven_amd_pcs_data_ready_callback_t data_ready_cb,
                                       void* client_cb_data, hsa_ven_amd_pcs_t* handle) {

  AMD::GpuAgentInt* gpu_agent = static_cast<AMD::GpuAgentInt*>(agent);

  ScopedAcquire<KernelMutex> lock(&pc_sampling_lock_);

  handle->handle = ++pc_sampling_id_;
  pc_sampling_[handle->handle] =
      PcSamplingSession(agent, method, units, interval, latency, buffer_size, data_ready_cb, client_cb_data);

  hsa_status_t ret = gpu_agent->PcSamplingCreate(pc_sampling_[handle->handle]);
  if (ret != HSA_STATUS_SUCCESS) {
      pc_sampling_.erase(handle->handle);
      return ret;
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t PcsRuntime::PcSamplingDestroy(hsa_ven_amd_pcs_t handle) {
  ScopedAcquire<KernelMutex> lock(&pc_sampling_lock_);
  auto pcSamplingSessionIt = pc_sampling_.find(reinterpret_cast<uint64_t>(handle.handle));
  if (pcSamplingSessionIt == pc_sampling_.end()) {
      debug_warning(false && "Cannot find PcSampling session");
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  AMD::GpuAgentInt* gpu_agent = static_cast<AMD::GpuAgentInt*>(pcSamplingSessionIt->second.agent);

  hsa_status_t ret = gpu_agent->PcSamplingDestroy(pcSamplingSessionIt->second);
  pc_sampling_.erase(pcSamplingSessionIt);
  return ret;
}

hsa_status_t PcsRuntime::PcSamplingStart(hsa_ven_amd_pcs_t handle) {
  ScopedAcquire<KernelMutex> lock(&pc_sampling_lock_);
  auto pcSamplingSessionIt = pc_sampling_.find(reinterpret_cast<uint64_t>(handle.handle));
  if (pcSamplingSessionIt == pc_sampling_.end()) {
      debug_warning(false && "Cannot find PcSampling session");
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  AMD::GpuAgentInt* gpu_agent = static_cast<AMD::GpuAgentInt*>(pcSamplingSessionIt->second.agent);

  return gpu_agent->PcSamplingStart(pcSamplingSessionIt->second);
}

hsa_status_t PcsRuntime::PcSamplingStop(hsa_ven_amd_pcs_t handle) {
  ScopedAcquire<KernelMutex> lock(&pc_sampling_lock_);
  auto pcSamplingSessionIt = pc_sampling_.find(reinterpret_cast<uint64_t>(handle.handle));
  if (pcSamplingSessionIt == pc_sampling_.end()) {
      debug_warning(false && "Cannot find PcSampling session");
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  AMD::GpuAgentInt* gpu_agent = static_cast<AMD::GpuAgentInt*>(pcSamplingSessionIt->second.agent);

  return gpu_agent->PcSamplingStop(pcSamplingSessionIt->second);
}

hsa_status_t PcsRuntime::PcSamplingFlush(hsa_ven_amd_pcs_t handle) {
  ScopedAcquire<KernelMutex> lock(&pc_sampling_lock_);
  auto pcSamplingSessionIt = pc_sampling_.find(reinterpret_cast<uint64_t>(handle.handle));
  if (pcSamplingSessionIt == pc_sampling_.end()) {
      debug_warning(false && "Cannot find PcSampling session");
      return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  AMD::GpuAgentInt* gpu_agent = static_cast<AMD::GpuAgentInt*>(pcSamplingSessionIt->second.agent);

  return gpu_agent->PcSamplingFlush(pcSamplingSessionIt->second);
}

}  // namespace pcs
}  // namespace rocr
