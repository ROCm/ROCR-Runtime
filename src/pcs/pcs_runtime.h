////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef HSA_RUNTIME_PCS_RUNTIME_H
#define HSA_RUNTIME_PCS_RUNTIME_H

#include <atomic>
#include <map>
#include <mutex>

#include "hsakmt/hsakmt.h"

#include "hsa_ven_amd_pc_sampling.h"
#include "core/inc/agent.h"
#include "core/inc/exceptions.h"


namespace rocr {
namespace pcs {

class PcsRuntime {
 public:
 PcsRuntime() : pc_sampling_id_(0) {}
 ~PcsRuntime() {}

  /// @brief Getter for the PcsRuntime singleton object.
  static PcsRuntime* instance();

  /// @brief Destroy singleton object.
  static void DestroySingleton();

  class PcSamplingSession {
    public:
      PcSamplingSession() : agent(NULL), thunkId_(0) {};
      PcSamplingSession(core::Agent* agent, hsa_ven_amd_pcs_method_kind_t method,
                       hsa_ven_amd_pcs_units_t units, size_t interval, size_t latency,
                       size_t buffer_size,
                       hsa_ven_amd_pcs_data_ready_callback_t data_ready_callback,
                       void* client_callback_data);
    ~PcSamplingSession() {};

    const bool isValid() { return valid_; }
    const size_t buffer_size() { return csd.buffer_size; }
    const hsa_ven_amd_pcs_method_kind_t method() { return csd.method; }
    const size_t latency() { return csd.latency; }
     const size_t sample_size() { return sample_size_; }

    void GetHsaKmtSamplingInfo(HsaPcSamplingInfo* sampleInfo);

    core::Agent* agent;
    void SetThunkId(HsaPcSamplingTraceId thunkId) {
     thunkId_ = thunkId;
    }
    HsaPcSamplingTraceId ThunkId() { return thunkId_; }
    bool isActive() { return active_; }
    void start() { active_ = true; }
    void stop() { active_ = false; }
    private:
    HsaPcSamplingTraceId thunkId_;

    bool active_;  // Set to true when the session is started
    bool valid_;   // Whether configuration parameters are valid
    size_t sample_size_;

    struct client_session_data_t {
      hsa_ven_amd_pcs_method_kind_t method;
      hsa_ven_amd_pcs_units_t units;
      size_t interval;
      size_t latency;
      size_t buffer_size;
      hsa_ven_amd_pcs_data_ready_callback_t data_ready_callback;
      void* client_callback_data;
    };
    struct client_session_data_t csd;
  }; // class PcSamplingSession

  hsa_status_t PcSamplingIterateConfig(core::Agent* agent, hsa_ven_amd_pcs_iterate_configuration_callback_t configuration_callback, void* callback_data);

  hsa_status_t PcSamplingCreate(core::Agent* agent, hsa_ven_amd_pcs_method_kind_t method,
                                       hsa_ven_amd_pcs_units_t units, size_t interval,
                                       size_t latency, size_t buffer_size,
                                       hsa_ven_amd_pcs_data_ready_callback_t data_ready_cb,
                                       void* client_cb_data, hsa_ven_amd_pcs_t* handle);

  hsa_status_t PcSamplingDestroy(hsa_ven_amd_pcs_t handle);
  hsa_status_t PcSamplingStart(hsa_ven_amd_pcs_t handle);
  hsa_status_t PcSamplingStop(hsa_ven_amd_pcs_t handle);
  hsa_status_t PcSamplingFlush(hsa_ven_amd_pcs_t handle);

 private:
  /// @brief Initialize singleton object, must be called once.
  static PcsRuntime* CreateSingleton();

  void Cleanup();

  /// Pointer to singleton object.
  static std::atomic<PcsRuntime*> instance_;
  static std::mutex instance_mutex_;

  // Map of pc sampling sessions indexed by hsa_ven_amd_pcs_t handle
  std::map<uint64_t, PcSamplingSession> pc_sampling_;
  KernelMutex pc_sampling_lock_;
  uint64_t pc_sampling_id_;

  DISALLOW_COPY_AND_ASSIGN(PcsRuntime);
};

}  // namespace pcs
}  // namespace rocr
#endif  // HSA_RUNTIME_PCS_RUNTIME_H
