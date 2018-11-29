////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2014-2015, Advanced Micro Devices, Inc. All rights reserved.
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

#include <string>

#include "core/util/os.h"
#include "core/util/utils.h"

class Flag {
 public:
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

    enable_sdma_ = os::GetEnvVar("HSA_ENABLE_SDMA");

    visible_gpus_ = os::GetEnvVar("ROCR_VISIBLE_DEVICES");

    var = os::GetEnvVar("HSA_RUNNING_UNDER_VALGRIND");
    running_valgrind_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_SDMA_WAIT_IDLE");
    sdma_wait_idle_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_MAX_QUEUES");
    max_queues_ = static_cast<uint32_t>(atoi(var.c_str()));

    var = os::GetEnvVar("HSA_SCRATCH_MEM");
    scratch_mem_size_ = atoi(var.c_str());

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

  std::string enable_sdma() const { return enable_sdma_; }

  std::string visible_gpus() const { return visible_gpus_; }

  uint32_t max_queues() const { return max_queues_; }

  size_t scratch_mem_size() const { return scratch_mem_size_; }

  std::string tools_lib_names() const { return tools_lib_names_; }

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

  std::string enable_sdma_;

  std::string visible_gpus_;

  uint32_t max_queues_;

  size_t scratch_mem_size_;

  std::string tools_lib_names_;

  DISALLOW_COPY_AND_ASSIGN(Flag);
};

#endif  // header guard
