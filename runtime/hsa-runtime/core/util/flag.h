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

#include <algorithm>
#include <cctype>
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

    var = os::GetEnvVar("HSA_DEBUG_FAULT");
    std::transform(var.begin(), var.end(), var.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (var == "analyze") {
      debug_fault_ = DEBUG_FAULT_ANALYZE;
    } else {
      debug_fault_ = DEBUG_FAULT_OFF;
    }

    var = os::GetEnvVar("HSA_ENABLE_QUEUE_FAULT_MESSAGE");
    enable_queue_fault_message_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_ENABLE_INTERRUPT");
    enable_interrupt_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_ENABLE_THREAD_TRACE");
    enable_thread_trace_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_THREAD_TRACE_MEM_SIZE");
    thread_trace_buff_size_ = atoi(var.c_str());

    var = os::GetEnvVar("HSA_ENABLE_SDMA");
    enable_sdma_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_EMULATE_AQL");
    emulate_aql_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_RUNNING_UNDER_VALGRIND");
    running_valgrind_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_SDMA_WAIT_IDLE");
    sdma_wait_idle_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_MAX_QUEUES");
    max_queues_ = static_cast<uint32_t>(atoi(var.c_str()));

    var = os::GetEnvVar("HSA_SCRATCH_MEM");
    scratch_mem_size_ = atoi(var.c_str());

    tools_lib_names_ = os::GetEnvVar("HSA_TOOLS_LIB");
  }

  enum DebugFaultEnum {
    DEBUG_FAULT_OFF,
    DEBUG_FAULT_ANALYZE,
  };

  bool check_flat_scratch() const { return check_flat_scratch_; }

  DebugFaultEnum debug_fault() const { return debug_fault_; }

  bool enable_queue_fault_message() const { return enable_queue_fault_message_; }

  bool enable_interrupt() const { return enable_interrupt_; }

  bool enable_thread_trace() const { return enable_thread_trace_; }

  bool thread_trace_buff_size() const { return thread_trace_buff_size_; }

  bool enable_sdma() const { return enable_sdma_; }

  bool emulate_aql() const { return emulate_aql_; }

  bool running_valgrind() const { return running_valgrind_; }

  bool sdma_wait_idle() const { return sdma_wait_idle_; }

  uint32_t max_queues() const { return max_queues_; }

  size_t scratch_mem_size() const { return scratch_mem_size_; }

  std::string tools_lib_names() const { return tools_lib_names_; }

 private:
  bool check_flat_scratch_;
  DebugFaultEnum debug_fault_;
  bool enable_interrupt_;
  bool enable_sdma_;
  bool emulate_aql_;
  bool running_valgrind_;
  bool sdma_wait_idle_;
  bool enable_queue_fault_message_;

  bool enable_thread_trace_;
  size_t thread_trace_buff_size_;

  uint32_t max_queues_;

  size_t scratch_mem_size_;

  std::string tools_lib_names_;

  DISALLOW_COPY_AND_ASSIGN(Flag);
};

#endif  // header guard
