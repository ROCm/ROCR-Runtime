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

#include "image_store_bandwidth.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "gtest/gtest.h"
#include "hsa/hsa_ext_image.h"
#include <stdio.h>
#include <vector>

// Constructor of the class
ImageStoreBandwidth::ImageStoreBandwidth() :
  BaseRocR() {
  store_bandwidth_ = 0.0;
  store_bandwidth_ = 0.0;
  image_size_ = 0;

  set_requires_profile (HSA_PROFILE_FULL);
}

// Destructor of the class
ImageStoreBandwidth::~ImageStoreBandwidth() {

}

// Set up the environment
void ImageStoreBandwidth::SetUp() {

  set_kernel_file_name("store_2d_image.o");
  set_kernel_name("&__OpenCL_store_2d_image_kernel");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();

  //Create a queue with max number size
  hsa_queue_t* q = nullptr;
  rocrtst::CreateQueue(*gpu_dev, &q);
  set_main_queue(q);

  rocrtst::LoadKernelFromObjFile(this);

  //Fill up part of aql
  rocrtst::InitializeAQLPacket(this, &aql());
  aql().setup = 0;
  aql().setup |= 2 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;

  return;
}

// Run the test
void ImageStoreBandwidth::Run() {
  hsa_status_t err;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();
  hsa_agent_t* cpu_dev = cpu_device();

  hsa_ext_image_descriptor_t image_descriptor;
  image_descriptor.geometry = HSA_EXT_IMAGE_GEOMETRY_2D;
  image_descriptor.width = 256;
  image_descriptor.height = 256;
  image_descriptor.depth = 1;
  image_descriptor.array_size = 0;
  image_descriptor.format.channel_type =
    HSA_EXT_IMAGE_CHANNEL_TYPE_UNSIGNED_INT8;
  image_descriptor.format.channel_order = HSA_EXT_IMAGE_CHANNEL_ORDER_RGBA;

  hsa_ext_image_format_t image_format;
  image_format.channel_type = HSA_EXT_IMAGE_CHANNEL_TYPE_UNSIGNED_INT8;
  image_format.channel_order = HSA_EXT_IMAGE_CHANNEL_ORDER_RGBA;

  // Check if device_ supports at least read only operation on image format
  uint32_t capability_mask;
  err = hsa_ext_image_get_capability(*gpu_dev, HSA_EXT_IMAGE_GEOMETRY_2D,
                                     &image_format, &capability_mask);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  if (!(capability_mask & HSA_EXT_IMAGE_CAPABILITY_READ_ONLY)) {
    std::cout << 
     "Device does not support read and write operation on this kind of image!"
        << std::endl;
    ASSERT_NE(capability_mask & HSA_EXT_IMAGE_CAPABILITY_READ_ONLY, 0);
  }

  // Get image info
  hsa_ext_image_data_info_t image_info;
  err = hsa_ext_image_data_get_info(*gpu_dev, &image_descriptor,
                                    HSA_ACCESS_PERMISSION_RW, &image_info);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  image_size_ = image_info.size;

  std::vector<double> time;

  for (uint32_t i = 0; i < num_iteration(); i++) {
#ifdef DEBUG
    std::cout << ".";
    fflush(stdout);
#endif
    // Allocate memory space for image
    err = hsa_amd_agent_iterate_memory_pools(*cpu_dev, rocrtst::FindGlobalPool,
                                                                   &cpu_pool());
    ASSERT_EQ(err, HSA_STATUS_INFO_BREAK);

    uintptr_t ptr_temp = 0;
    err = hsa_amd_memory_pool_allocate(cpu_pool(),
                                       image_info.size + image_info.alignment,
                                                         0, (void**) &ptr_temp);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    // Align the image address
    uintptr_t mul = ptr_temp / image_info.alignment;
    void* ptr_image = (void*) ((mul + 1) * image_info.alignment);

    // rocrtst::CommonCleanUp the image memory to 0
    err = hsa_amd_memory_fill(ptr_image, 0, image_info.size);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    // Create image handle
    hsa_ext_image_t image_handle;
    err = hsa_ext_image_create(*gpu_dev, &image_descriptor, ptr_image,
                               HSA_ACCESS_PERMISSION_RO, &image_handle);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    // Allocate and initialize the kernel argument
    typedef struct args_t {
      uint64_t arg0;
      int istart;
      int iend;
      int istep;
    } args;

    //int local_out = 5;
    int istart = 0;
    int iend = 64;
    int istep = 1;

    args* kern_ptr = NULL;
    err = hsa_amd_memory_pool_allocate(cpu_pool(), sizeof(args), 0,
                                       (void**) &kern_ptr);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    kern_ptr->arg0 = image_handle.handle;
    kern_ptr->istart = istart;
    kern_ptr->iend = iend;
    kern_ptr->istep = istep;

    aql().kernarg_address = kern_ptr;

    // Obtain the current queue write index
    uint64_t index = hsa_queue_add_write_index_relaxed(main_queue(), 1);

    void *q_base_addr = main_queue()->base_address;
    // Write the aql packet at the calculated queue index address.
    const uint32_t queue_mask = main_queue()->size - 1;
    ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask] = aql();

    rocrtst::PerfTimer p_timer;
    int id = p_timer.CreateTimer();
    p_timer.StartTimer(id);

    ((hsa_kernel_dispatch_packet_t*)q_base_addr)[index & queue_mask].header |=
                      HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;
    hsa_signal_store_release(main_queue()->doorbell_signal, index);

    // Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;

    p_timer.StopTimer(id);

    time.push_back(p_timer.ReadTimer(id));

    hsa_signal_store_release(signal(), 1);

    err = hsa_ext_image_destroy(*gpu_dev, image_handle);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    err = hsa_memory_deregister(ptr_image, image_info.size);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    hsa_amd_memory_pool_free(reinterpret_cast<void*>(ptr_temp));
  }

  // Calculte the mean load time
  time.erase(time.begin());
#ifdef DEBUG

  for (size_t i = 0; i < time.size(); i++) {
    std::cout << time[i] << std::endl;
  }

#endif
  double mean_time = rocrtst::CalcMean(time);
  std::cout << "mean time: " << mean_time << std::endl;

  store_bandwidth_ = image_size_ / mean_time / 1024 / 1024 / 1024;
}

void ImageStoreBandwidth::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void ImageStoreBandwidth::DisplayResults() const {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  std::cout << "============================================="
                                "===============================" << std::endl;

  std::cout << " Image Size(bytes):              StoreBandwidth(GB/S):    "
            << std::cout;
  std::cout << " " << image_size_ << "                                "
            << store_bandwidth_ << std::endl;
}

