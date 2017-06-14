/******************************************************************************

Copyright ©2013 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#ifndef _TEST_HSA_H_
#define _TEST_HSA_H_

#include "test_aql.h"
#include "test_kernel.h"
#include "hsa_rsrc_factory.h"

// Class implements HSA test
class TestHSA : public TestAql {
 public:
  // Constructor
  TestHSA(TestKernel* test) : test_(test), name_(test->Name()) {
    total_time_taken_ = 0;
    setup_time_taken_ = 0;
    dispatch_time_taken_ = 0;
  }

  // Get methods for Agent Info, HAS queue, HSA Resourcse Manager
  AgentInfo* getAgentInfo() { return agent_info_; }
  hsa_queue_t* getQueue() { return hsa_queue_; }
  HsaRsrcFactory* getRsrcFactory() { return hsa_rsrc_; }

  // Initialize application environment including setting
  // up of various configuration parameters based on
  // command line arguments
  // @return bool true on success and false on failure
  bool initialize(int argc, char** argv);

  // Setup application parameters for exectuion
  // @return bool true on success and false on failure
  bool setup();

  // Run the BinarySearch kernel
  // @return bool true on success and false on failure
  bool run();

  // Verify against reference implementation
  // @return bool true on success and false on failure
  bool verify_results();

  // Print to console the time taken to execute kernel
  void print_time();

  // Release resources e.g. memory allocations
  // @return bool true on success and false on failure
  bool cleanup();

 private:
  typedef TestKernel::mem_descr_t mem_descr_t;
  typedef TestKernel::mem_map_t mem_map_t;
  typedef TestKernel::mem_it_t mem_it_t;

  // Test object
  TestKernel* test_;

  // Path of Brig file
  std::string brig_path_obj_;

  // Used to track time taken to run the sample
  double total_time_taken_;
  double setup_time_taken_;
  double dispatch_time_taken_;

  // Handle to an Hsa Gpu Agent
  AgentInfo* agent_info_;

  // Handle to an Hsa Queue
  hsa_queue_t* hsa_queue_;

  // Handle of signal
  hsa_signal_t hsa_signal_;

  // Handle of Kernel Code Descriptor
  hsa_executable_symbol_t kernel_code_desc_;

  // Instance of timer object
  uint32_t setup_timer_idx_;
  uint32_t dispatch_timer_idx_;
  PerfTimer hsa_timer_;

  // Instance of Hsa Resources Factory
  HsaRsrcFactory* hsa_rsrc_;

  // Test kernel name
  std::string name_;
};

#endif  // _TEST_HSA_H_
