////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2021, Advanced Micro Devices, Inc. All rights reserved.
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
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIESd OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef HSA_RUNTIME_CORE_INC_FLAG_H_
#define HSA_RUNTIME_CORE_INC_FLAG_H_

#include <stdint.h>

#include <vector>
#include <map>
#include <string>

#include "core/util/os.h"
#include "core/util/utils.h"

namespace rocr {

class Flag {
 public:
  enum SDMA_OVERRIDE { SDMA_DISABLE, SDMA_ENABLE, SDMA_DEFAULT };
  enum SRAMECC_ENABLE { SRAMECC_DISABLED, SRAMECC_ENABLED, SRAMECC_DEFAULT };

  // The values are meaningful and chosen to satisfy the thunk API.
  enum XNACK_REQUEST { XNACK_DISABLE = 0, XNACK_ENABLE = 1, XNACK_UNCHANGED = 2 };
  static_assert(XNACK_DISABLE == 0, "XNACK_REQUEST enum values improperly changed.");
  static_assert(XNACK_ENABLE == 1, "XNACK_REQUEST enum values improperly changed.");

  // Lift limit for 2.10 release RCCL workaround.
  const size_t DEFAULT_SCRATCH_SINGLE_LIMIT = 146800640;  // small_limit >> 2;

  explicit Flag() { Refresh(); }

  virtual ~Flag() {}

  void Refresh() {
    std::string var = os::GetEnvVar("HSA_CHECK_FLAT_SCRATCH");
    check_flat_scratch_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_ENABLE_VM_FAULT_MESSAGE");
    enable_vm_fault_message_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_ENABLE_QUEUE_FAULT_MESSAGE");
    enable_queue_fault_message_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_ENABLE_INTERRUPT");
    enable_interrupt_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_ENABLE_SDMA");
    enable_sdma_ = (var == "0") ? SDMA_DISABLE : ((var == "1") ? SDMA_ENABLE : SDMA_DEFAULT);

    var = os::GetEnvVar("HSA_ENABLE_PEER_SDMA");
    enable_peer_sdma_ = (var == "0") ? SDMA_DISABLE : ((var == "1") ? SDMA_ENABLE : SDMA_DEFAULT);

    visible_gpus_ = os::GetEnvVar("ROCR_VISIBLE_DEVICES");
    filter_visible_gpus_ = os::IsEnvVarSet("ROCR_VISIBLE_DEVICES");

    var = os::GetEnvVar("HSA_RUNNING_UNDER_VALGRIND");
    running_valgrind_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_SDMA_WAIT_IDLE");
    sdma_wait_idle_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_MAX_QUEUES");
    max_queues_ = static_cast<uint32_t>(atoi(var.c_str()));

    // Maximum amount of scratch mem that can be used per process per gpu
    var = os::GetEnvVar("HSA_SCRATCH_MEM");
    scratch_mem_size_ = atoi(var.c_str());

    // Scratch memory sizes > HSA_SCRATCH_SINGLE_LIMIT will trigger a use-once scheme
    // We also reserve HSA_SCRATCH_SINGLE_LIMIT per process per gpu to guarrantee we
    // have sufficient memory to for scratch in case user tried to allocate all device
    // memory
    if (os::IsEnvVarSet("HSA_SCRATCH_SINGLE_LIMIT")) {
      var = os::GetEnvVar("HSA_SCRATCH_SINGLE_LIMIT");
      scratch_single_limit_ = atoi(var.c_str());
    } else {
      scratch_single_limit_ = DEFAULT_SCRATCH_SINGLE_LIMIT;
    }

    tools_lib_names_ = os::GetEnvVar("HSA_TOOLS_LIB");

    var = os::GetEnvVar("HSA_TOOLS_REPORT_LOAD_FAILURE");

    ifdebug {
      report_tool_load_failures_ = (var == "1") ? true : false;
    } else {
      report_tool_load_failures_ = (var == "0") ? false : true;
    }

    var = os::GetEnvVar("HSA_DISABLE_FRAGMENT_ALLOCATOR");
    disable_fragment_alloc_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_ENABLE_SDMA_HDP_FLUSH");
    enable_sdma_hdp_flush_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_REV_COPY_DIR");
    rev_copy_dir_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_FORCE_FINE_GRAIN_PCIE");
    fine_grain_pcie_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_NO_SCRATCH_RECLAIM");
    no_scratch_reclaim_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_NO_SCRATCH_THREAD_LIMITER");
    no_scratch_thread_limit_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_DISABLE_IMAGE");
    disable_image_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_DISABLE_PC_SAMPLING");
    disable_pc_sampling_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_LOADER_ENABLE_MMAP_URI");
    loader_enable_mmap_uri_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_FORCE_SDMA_SIZE");
    force_sdma_size_ = var.empty() ? 1024 * 1024 : atoi(var.c_str());

    var = os::GetEnvVar("HSA_IGNORE_SRAMECC_MISREPORT");
    check_sramecc_validity_ = (var == "1") ? false : true;
    
    // Legal values are zero "0" or one "1". Any other value will
    // be interpreted as not defining the env variable.
    var = os::GetEnvVar("HSA_XNACK");
    xnack_ = (var == "0") ? XNACK_DISABLE : ((var == "1") ? XNACK_ENABLE : XNACK_UNCHANGED);

    var = os::GetEnvVar("HSA_ENABLE_DEBUG");
    debug_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_CU_MASK_SKIP_INIT");
    cu_mask_skip_init_ = (var == "1") ? true : false;

    // Temporary opt-in for corrected HSA_AMD_AGENT_INFO_COOPERATIVE_COMPUTE_UNIT_COUNT behavior.
    // Will become opt-out and possibly removed in future releases.
    var = os::GetEnvVar("HSA_COOP_CU_COUNT");
    coop_cu_count_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_DISCOVER_COPY_AGENTS");
    discover_copy_agents_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_SVM_PROFILE");
    svm_profile_ = var;

    var = os::GetEnvVar("HSA_ENABLE_SRAMECC");
    sramecc_enable_ =
        (var == "0") ? SRAMECC_DISABLED : ((var == "1") ? SRAMECC_ENABLED : SRAMECC_DEFAULT);

    var = os::GetEnvVar("HSA_IMAGE_PRINT_SRD");
    image_print_srd_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_ENABLE_MWAITX");
    enable_mwaitx_ = (var == "1") ? true : false;

    // Temporary environment variable to disable CPU affinity override
    // Will either rename to HSA_OVERRIDE_CPU_AFFINITY later or remove completely.
    var = os::GetEnvVar("HSA_OVERRIDE_CPU_AFFINITY_DEBUG");
    override_cpu_affinity_ = (var == "0") ? false : true;
  }

  void parse_masks(uint32_t maxGpu, uint32_t maxCU) {
    std::string var = os::GetEnvVar("HSA_CU_MASK");
    parse_masks(var, maxGpu, maxCU);
  }

  bool check_flat_scratch() const { return check_flat_scratch_; }

  bool enable_vm_fault_message() const { return enable_vm_fault_message_; }

  bool enable_queue_fault_message() const { return enable_queue_fault_message_; }

  bool enable_interrupt() const { return enable_interrupt_; }

  bool enable_sdma_hdp_flush() const { return enable_sdma_hdp_flush_; }

  bool running_valgrind() const { return running_valgrind_; }

  bool sdma_wait_idle() const { return sdma_wait_idle_; }

  bool report_tool_load_failures() const { return report_tool_load_failures_; }

  bool disable_fragment_alloc() const { return disable_fragment_alloc_; }

  bool rev_copy_dir() const { return rev_copy_dir_; }

  bool fine_grain_pcie() const { return fine_grain_pcie_; }

  bool no_scratch_reclaim() const { return no_scratch_reclaim_; }

  bool no_scratch_thread_limiter() const { return no_scratch_thread_limit_; }

  SDMA_OVERRIDE enable_sdma() const { return enable_sdma_; }

  SDMA_OVERRIDE enable_peer_sdma() const { return enable_peer_sdma_; }

  std::string visible_gpus() const { return visible_gpus_; }

  bool filter_visible_gpus() const { return filter_visible_gpus_; }

  uint32_t max_queues() const { return max_queues_; }

  size_t scratch_mem_size() const { return scratch_mem_size_; }

  size_t scratch_single_limit() const { return scratch_single_limit_; }

  std::string tools_lib_names() const { return tools_lib_names_; }

  bool disable_image() const { return disable_image_; }

  bool disable_pc_sampling() const { return disable_pc_sampling_; }

  bool loader_enable_mmap_uri() const { return loader_enable_mmap_uri_; }

  size_t force_sdma_size() const { return force_sdma_size_; }

  bool check_sramecc_validity() const { return check_sramecc_validity_; }

  bool override_cpu_affinity() const { return override_cpu_affinity_; }

  bool image_print_srd() const { return image_print_srd_; }

  bool check_mwaitx(bool mwaitx_supported) {
    if (enable_mwaitx_ && !mwaitx_supported) enable_mwaitx_ = false;

    return enable_mwaitx_;
  }

  XNACK_REQUEST xnack() const { return xnack_; }

  bool debug() const { return debug_; }

  const std::vector<uint32_t>& cu_mask(uint32_t gpu_index) const {
    static const std::vector<uint32_t> empty;
    auto it = cu_mask_.find(gpu_index);
    if (it == cu_mask_.end()) return empty;
    return it->second;
  }

  bool cu_mask_skip_init() const { return cu_mask_skip_init_; }

  bool coop_cu_count() const { return coop_cu_count_; }

  bool discover_copy_agents() const { return discover_copy_agents_; }

  const std::string& svm_profile() const { return svm_profile_; }

  SRAMECC_ENABLE sramecc_enable() const { return sramecc_enable_; }

 private:
  bool check_flat_scratch_;
  bool enable_vm_fault_message_;
  bool enable_interrupt_;
  bool enable_sdma_hdp_flush_;
  bool running_valgrind_;
  bool sdma_wait_idle_;
  bool enable_queue_fault_message_;
  bool report_tool_load_failures_;
  bool disable_fragment_alloc_;
  bool rev_copy_dir_;
  bool fine_grain_pcie_;
  bool no_scratch_reclaim_;
  bool no_scratch_thread_limit_;
  bool disable_image_;
  bool disable_pc_sampling_;
  bool loader_enable_mmap_uri_;
  bool check_sramecc_validity_;
  bool debug_;
  bool cu_mask_skip_init_;
  bool coop_cu_count_;
  bool discover_copy_agents_;
  bool override_cpu_affinity_;
  bool image_print_srd_;
  bool enable_mwaitx_;

  SDMA_OVERRIDE enable_sdma_;
  SDMA_OVERRIDE enable_peer_sdma_;

  bool filter_visible_gpus_;
  std::string visible_gpus_;

  uint32_t max_queues_;

  size_t scratch_mem_size_;
  size_t scratch_single_limit_;

  std::string tools_lib_names_;
  std::string svm_profile_;

  size_t force_sdma_size_;

  // Indicates user preference for Xnack state.
  XNACK_REQUEST xnack_;

  SRAMECC_ENABLE sramecc_enable_;

  // Map GPU index post RVD to its default cu mask.
  std::map<uint32_t, std::vector<uint32_t>> cu_mask_;

  void parse_masks(std::string& args, uint32_t maxGpu, uint32_t maxCU);

  DISALLOW_COPY_AND_ASSIGN(Flag);
};

}  // namespace rocr

#endif  // header guard
