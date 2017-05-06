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

#include "vector_copy.h"
#include "common/base_rocr_utils.h"
#include "gtest/gtest.h"

// Copy vector buffer size.
static const size_t BUFFER_SIZE = 1024 * 1024 * 4;
static char* gCPUOutput = nullptr;
static uint64_t gQueueIndex = 0;

//Constructor
VectorCopy::VectorCopy() :
  BaseRocR() {
  set_kernel_name("&__vector_copy_kernel");
  kernarg_address = NULL;
}

//Destructor
VectorCopy::~VectorCopy() {
}

// Find coarse grained system memory.
static hsa_status_t get_sys_coarse_grained_memory_pool(
  hsa_amd_memory_pool_t pool, void* data) {
  hsa_amd_segment_t segment;
  hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_SEGMENT,
                               &segment);

  if (HSA_AMD_SEGMENT_GLOBAL != segment) {
    return HSA_STATUS_SUCCESS;
  }

  hsa_amd_memory_pool_global_flag_t flags;
  hsa_status_t err = hsa_amd_memory_pool_get_info(pool,
                     HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &flags);

  if (HSA_STATUS_SUCCESS == err
      && (flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED)) {
    hsa_amd_memory_pool_t* ret = (hsa_amd_memory_pool_t*) data;
    *ret = pool;
    return HSA_STATUS_INFO_BREAK;
  }

  return err;
}

// Find out dGPU's local memory pool.
static hsa_status_t get_local_memory_pool(hsa_amd_memory_pool_t pool,
    void* data) {
  // With memory pool API, each agent will only report it is own memory pools.
  // So, a coarse grained memory pool in global segment is what we want.
  hsa_amd_segment_t segment;

  hsa_status_t err = hsa_amd_memory_pool_get_info(pool,
                     HSA_AMD_MEMORY_POOL_INFO_SEGMENT, &segment);

  if (HSA_STATUS_SUCCESS != err) {
    return err;
  }

  if (HSA_AMD_SEGMENT_GLOBAL != segment) {
    return HSA_STATUS_SUCCESS;
  }

  hsa_amd_memory_pool_global_flag_t flags;
  err = hsa_amd_memory_pool_get_info(pool,
                          HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &flags);

  if (HSA_STATUS_SUCCESS == err
      && (flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED)) {
    hsa_amd_memory_pool_t* ret = (hsa_amd_memory_pool_t*) data;
    *ret = pool;
    return HSA_STATUS_INFO_BREAK;
  }

  return err;
}

void VectorCopy::SetUp() {
  hsa_status_t err;
  hsa_agent_t* gpu_dev = gpu_device1();

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  //Create a queue with max number size
  hsa_queue_t* q;
  rocrtst::CreateQueue(*gpu_dev, &q);
  set_main_queue(q);

  rocrtst::LoadKernelFromObjFile(this);

  // Obtain the current queue write index.
  gQueueIndex = hsa_queue_load_write_index_scacquire(main_queue());

  rocrtst::InitializeAQLPacket(this, &aql());
  uint16_t header = 0;
  header |= HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;

  aql().grid_size_x = (uint32_t)(1024 * 1024);
  aql().kernarg_address = (void*) kernarg_address;

  // Find system memory pool for kernarg allocation.
  // hsa_amd_memory_pool_t sys_coarse_grained_pool;
  err = hsa_amd_agent_iterate_memory_pools(cpus[0],
        get_sys_coarse_grained_memory_pool, &sys_coarse_grained_pool_);
  ASSERT_EQ(err, HSA_STATUS_INFO_BREAK);

  // Get local memory pool of the first GPU.
  // hsa_amd_memory_pool_t gpu_pool_;
  err = hsa_amd_agent_iterate_memory_pools(gpus[0], get_local_memory_pool,
        &gpu_pool_);
  ASSERT_EQ(err, HSA_STATUS_INFO_BREAK);

  return;
}

void VectorCopy::Run() {
  hsa_status_t err;
  void* in;
  void* out;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  // Allocate vector on the first GPU local memory as input.
  err = hsa_amd_memory_pool_allocate(gpu_pool_, BUFFER_SIZE, 0, &in);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  std::cout << "Allocating " << BUFFER_SIZE <<
            " Bytes of local memory on the first GPU, address = " <<
                                                              in << std::endl;

  // rocrtst::CommonCleanUp input buffer on the first GPU to 1 for each byte.
  err = hsa_amd_memory_fill(in, 0x01010101, BUFFER_SIZE / 4);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Allocate vector on the first GPU local memory as output
  err = hsa_amd_memory_pool_allocate(gpu_pool_, BUFFER_SIZE, 0, &out);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  std::cout << "Allocating " << BUFFER_SIZE <<
            " Bytes of local memory on the second GPU, address = " <<
                                                             out << std::endl;

  // rocrtst::CommonCleanUp output buffer on the first GPU to 0.
  err = hsa_amd_memory_fill(out, 0x00000000, BUFFER_SIZE / 4);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  typedef struct args_t {
    void* in;
    void* out;
  } args;

  args* kargs;

  kargs->in = in;
  kargs->out = out;

  // Allocate the kernel argument buffer from the system memory pool.
  err = hsa_amd_memory_pool_allocate(sys_coarse_grained_pool_, kernarg_size(),
                                     0, &kernarg_address);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  memcpy(kernarg_address, &kargs, sizeof(args));

  // Map kernarg space to the first GPU
  err = hsa_amd_agents_allow_access(1, &gpus[0], NULL, kernarg_address);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  /*
   * Increment the write index and ring the doorbell to dispatch the kernel.
   */
  hsa_queue_store_write_index_screlease(main_queue(), gQueueIndex + 1);
  hsa_signal_store_relaxed(main_queue()->doorbell_signal, gQueueIndex);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Wait on the dispatch completion signal until the kernel is finished.
  while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_EQ, 0,
                                   UINT64_MAX, HSA_WAIT_STATE_BLOCKED))
    ;

  // Reset signal value for future usage to copy output.
  hsa_signal_store_screlease(signal(), 1);

  // Allocate vector on the system memory pool.
  err = hsa_amd_memory_pool_allocate(sys_coarse_grained_pool_, BUFFER_SIZE, 0,
                                     (void**) &gCPUOutput);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Allow the first GPU to access the output
  err = hsa_amd_agents_allow_access(1, &gpus[0], NULL, gCPUOutput);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  //Copy the output from GPU to the CPU buffer for validation
  err = hsa_amd_memory_async_copy(gCPUOutput, cpus[0], out, gpus[0],
                                  BUFFER_SIZE, 0, NULL, signal());
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  // Wait on the completion signal until the async copy is finished.
  while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_EQ, 0,
                                   UINT64_MAX, HSA_WAIT_STATE_BLOCKED))
    ;

  for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
    ASSERT_EQ(gCPUOutput[i], 1);
  }

  return;
}

void VectorCopy::Close() {
  hsa_status_t err;
  // Cleanup all allocated resources.
  err = hsa_amd_memory_pool_free(kernarg_address);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_signal_destroy(signal());
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_queue_destroy(main_queue());
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_free(gCPUOutput);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  return;
}

void VectorCopy::DisplayResults() const {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }
}
