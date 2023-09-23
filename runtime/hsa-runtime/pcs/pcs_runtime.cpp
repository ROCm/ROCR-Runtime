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

  instance_.store(NULL, std::memory_order_release);
  delete instance;
}

void ReleasePcSamplingRsrcs() { PcsRuntime::DestroySingleton(); }
hsa_status_t PcsRuntime::PcSamplingIterateConfig(
    core::Agent* agent, hsa_ven_amd_pcs_iterate_configuration_callback_t configuration_callback,
    void* callback_data) {
  AMD::GpuAgentInt* gpu_agent = static_cast<AMD::GpuAgentInt*>(agent);
  return gpu_agent->PcSamplingIterateConfig(configuration_callback, callback_data);
}


}  // namespace pcs
}  // namespace rocr
