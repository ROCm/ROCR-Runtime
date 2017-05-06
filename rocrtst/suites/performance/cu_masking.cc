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

#include "cu_masking.h"
#include "common/base_rocr_utils.h"
#include "gtest/gtest.h"

CuMasking::CuMasking() :
  BaseRocR() {
  memset(&aql(), 0, sizeof(hsa_kernel_dispatch_packet_t));
  mean_ = 0.0;
  group_region_.handle = 0;
  cu_ = NULL;
}

CuMasking::~CuMasking() {
}

void CuMasking::SetUp() {
  hsa_status_t err;

  hsa_agent_t* gpu_dev = gpu_device1();
  hsa_agent_t* cpu_dev = cpu_device();

  set_kernel_file_name("cu_masking.o");
  set_kernel_name("&main");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  // Create a queue
  hsa_queue_t* q = nullptr;
  rocrtst::CreateQueue(*gpu_dev, &q);
  set_main_queue(q);

  rocrtst::LoadKernelFromObjFile(this);

  // Fill up the kernel packet except header
  // aql().completion_signal=signal();
  // TODO: Will delete manual_input later
  uint32_t cu_count = 0;
  err = hsa_agent_get_info(*gpu_dev,
          (hsa_agent_info_t) HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT, &cu_count);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  std::cout << "CU# is: " << cu_count << std::endl;

  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  rocrtst::InitializeAQLPacket(this, &aql());
  aql().workgroup_size_x = 1024;

  //manual_input * group_input;  // workgroup_max_size;
  aql().grid_size_x = (long long) 1024 * 640 * 640;

  // TODO:Manully set the max cu number to 8, the api return 10
  std::cout << "Grid size is: " << aql().grid_size_x << std::endl;

  err = hsa_amd_agent_iterate_memory_pools(*cpu_dev,
                                        rocrtst::FindGlobalPool, &cpu_pool());
  ASSERT_EQ(err, HSA_STATUS_INFO_BREAK);
}

size_t CuMasking::RealIterationNum() {
  return num_iteration() * 1.2 + 1;
}

void CuMasking::Run() {
  hsa_status_t err;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  std::vector<double> timer;

  typedef struct args_t {
    uint32_t* iteration;
    uint32_t* result;
  } local_args;

  uint32_t* iter = NULL;
  uint32_t* result = NULL;
  err = hsa_amd_memory_pool_allocate(cpu_pool(), sizeof(uint32_t), 0,
                                     (void**) &iter);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(cpu_pool(), sizeof(uint32_t), 0,
                                     (void**) &result);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  *iter = 0xff;
  *result = 0;

  err = hsa_amd_agents_allow_access(1, gpu_device1(), NULL, iter);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  err = hsa_amd_agents_allow_access(1, gpu_device1(), NULL, result);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  local_args* kernarg = NULL;
  err = hsa_amd_memory_pool_allocate(cpu_pool(), kernarg_size(), 0,
                                     (void**) &kernarg);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_agents_allow_access(1, gpu_device1(), NULL, kernarg);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  kernarg->iteration = iter;
  kernarg->result = result;

  aql().kernarg_address = kernarg;

  // Obtain the current queue write inex.
  uint64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

  // Write the aql packet at the calculate queue index address.
  const uint32_t queue_mask = main_queue()->size - 1;

  // Set CU mask
  uint32_t cu_mask = 0;
#if 0
  std::cout << "Enter cu mask value:" << std::endl;
  ASSERT_NE(scanf("%d", &cu_mask), EOF);
#else
  cu_mask = 0xAAAAAAAA;
#endif

  std::cout << "Value of bit array is: 0x" << std::hex << cu_mask << std::endl;
  err = hsa_amd_queue_cu_set_mask(main_queue(), 32, &cu_mask);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  void *q_base_addr = main_queue()->base_address;
  // Write the aql packet at the calculate queue index address.
  aql().completion_signal = signal();
  ((hsa_kernel_dispatch_packet_t*)(q_base_addr))[index & queue_mask] = aql();

  // Get timing stamp an ring the doorbell to dispatch the kernel.
  rocrtst::PerfTimer p_timer;
  int id = p_timer.CreateTimer();
  p_timer.StartTimer(id);
  ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask].header |=
                     HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;
  hsa_signal_store_screlease(main_queue()->doorbell_signal, index);

  // Wait on the dispatch signal until the kernel is finished.
  while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                   (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
    ;

  p_timer.StopTimer(id);

  hsa_signal_store_screlease(signal(), 1);

  double t1 = p_timer.ReadTimer(id) * 1e6;
  std::cout << "Execution time after setting cu masking: " << t1 << std::endl;

  return;
}

void CuMasking::DisplayResults() const {

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  std::cout << "===================================================="
            << std::endl;

  std::cout << "====================================================="
            << std::endl;
  return;
}

void CuMasking::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}
