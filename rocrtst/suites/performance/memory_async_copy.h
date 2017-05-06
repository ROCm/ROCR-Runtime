/*
 * =============================================================================
 *   ROC Runtime Conformance Release License
 * =============================================================================
 * The University of Illinois/NCSA
 * Open Source License (NCSA)
 *
 * Copyright (c) 2017, Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Developed by:
 *
 *                 AMD Research and AMD ROC Software Development
 *
 *                 Advanced Micro Devices, Inc.
 *
 *                 www.amd.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimers in
 *    the documentation and/or other materials provided with the distribution.
 *  - Neither the names of <Name of Development Group, Name of Institution>,
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this Software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#ifndef __ROCRTST_SRC_MEMORY_ASYNC_COPY_H__
#define __ROCRTST_SRC_MEMORY_ASYNC_COPY_H__

#include "perf_common/perf_base.h"
#include "common/base_rocr.h"
#include "common/common.h"
#include "common/hsatimer.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"
#include <unistd.h>
#include <algorithm>
#include <vector>
#include <cctype>

extern int mac_argc;
extern char** mac_argv;

typedef struct transaction {
  int src;
  int dst;
  hsa_signal_t signal;
  size_t size;
  size_t num_dep_signal;
  hsa_signal_t* dep_signal;
} transaction;

typedef struct agent_info {
  agent_info(hsa_agent_t agent, int index, hsa_device_type_t device_type) {
    agent_ = agent;
    index_ = index;
    device_type_ = device_type;
  }
  agent_info() {
  }
  hsa_agent_t agent_;
  int index_;
  hsa_device_type_t device_type_;
} agent_info;

typedef struct region_info {
  region_info(hsa_amd_memory_pool_t region, int index,
              hsa_amd_segment_t segment, bool is_fine_graind, size_t size,
              hsa_agent_t agent) {
    region_ = region;
    index_ = index;
    segment_ = segment;
    is_fine_grained_ = is_fine_graind;
    allocable_size_ = size;
    owner_agent_ = agent;
  }
  region_info() {
  }
  hsa_amd_memory_pool_t region_;
  int index_;
  hsa_amd_segment_t segment_;
  bool is_fine_grained_;
  size_t allocable_size_;
  hsa_agent_t owner_agent_;
} region_info;

// Used to print out topology info
typedef struct node_info {
  node_info() {
  }
  agent_info agent;
  std::vector<region_info> region;
} node_info;

hsa_status_t AgentInfo(hsa_agent_t agent, void* data);
hsa_status_t RegionInfo(hsa_amd_memory_pool_t region, void* data);

class MemoryAsyncCopy: public rocrtst::BaseRocR, public PerfBase {
 public:
  MemoryAsyncCopy();

  //@Brief: Destructor for test case of MemoryAsyncCopy
  virtual ~MemoryAsyncCopy();

  //@Brief: Setup the environment for measurement
  virtual void SetUp();

  //@Brief: Core measurement execution
  virtual void Run();

  //@Brief: Clean up and retrive the resource
  virtual void Close();

  //@Brief: Display  results
  virtual void DisplayResults() const;

 private:
  //@Brief: Get real iteration number
  virtual size_t RealIterationNum();

  //@Brief: Get the mean copy time
  virtual double GetMeanTime(std::vector<double>& vec);

  //@Brief: Get the min copy time
  virtual double GetMinTime(std::vector<double>& vec);

  //@Brief: Find and print out the needed topology info
  void FindTopology();

  //@Brief: Parse the argument and interact with the user
  // to fill the vectors.
  void ParseArgument();

  //@Brief: Run for Benchmark mode
  void RunBenchmark();

  //@Brief: Run for Benchmark mode with verification
  void RunBenchmarkWithVerification();

  //@Brief: Dispaly Benchmark result
  void DisplayBenchmark();

  //@Brief: Run user defined
  void RunNormal();

  //@Brief: Print topology info
  void PrintTopology();

  //@Brief: Find system region
  void FindSystemRegion();

  //@Brief: Check if agent and access memory pool, if so, set
  //access to the agent, if not, exit
  void AcquireAccess(hsa_agent_t agent, hsa_amd_memory_pool_t pool, void* ptr);

  friend hsa_status_t AgentInfo(hsa_agent_t agent, void* data);
  friend hsa_status_t RegionInfo(hsa_amd_memory_pool_t region, void* data);

 protected:
  // More variables declared for testing
  std::vector<transaction> tran_;

  // Variable used to store agent info, indexed by agent_index_
  std::vector<agent_info> agent_info_;

  // Variable used to store region info, indexed by region_index_
  std::vector<region_info> region_info_;

  // Variable to store argument number
  int argc_;

  // Pointer to store address of argument text
  char** argv_;

  // Variable to help count agent index
  int agent_index_;

  // Variable to help count region index
  int region_index_;

  // BenchMark mode by default
  bool bench_mark_mode_;

  // BenchMark copy time
  std::vector<double> benchmark_copy_time_;

  // Min time
  std::vector<double> min_time_;

  // User define copy time
  double user_copy_time_;

  // Verification result
  bool verified_;

  // If it needs verification
  bool verification_;

  // To store node info
  std::vector<node_info> node_info_;

  // System region
  hsa_amd_memory_pool_t sys_region_;

  // CPU agent used for verification
  hsa_agent_t cpu_agent_;

  constexpr const static char* help_info = 
     MULTILINE(. / memory_async_copy - f source_region - t dst_region - s data_size_in_KB - r[y | n] - i iteration_number - b\n\
      \n\
      -h Help info \n\
      -f Memory Pool where data copy from \n\
      -t Memory Pool where data copy to \n\

    -s Size of copy data, 256MB by default \n\
        -r If wants to add more copy \n\
        -i Iteration number for each copy \n\
        -b Enable benchmark mode \n\
        Note : -f - t must be specified\n);
};

#endif
