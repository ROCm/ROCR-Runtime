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

#include "image_bandwidth.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/hsatimer.h"
#include "gtest/gtest.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

ImageBandwidth::ImageBandwidth(size_t num) :
  BaseRocR(), import_bandwidth_ {0.0}, export_bandwidth_ {0.0},
                                                        copy_bandwidth_ {0.0} {
  format_.channel_order = HSA_EXT_IMAGE_CHANNEL_ORDER_RGBA;
  format_.channel_type = HSA_EXT_IMAGE_CHANNEL_TYPE_UNSIGNED_INT8;
  geometry_ = HSA_EXT_IMAGE_GEOMETRY_2D;

  set_requires_profile (HSA_PROFILE_FULL);
}

ImageBandwidth::~ImageBandwidth() {
}

const size_t ImageBandwidth::Size[10] = {32, 64, 128, 256, 512, 1024, 2048,
                                         4096, 8192, 16384
                                        };
const char* const ImageBandwidth::Str[10] = {"4K", "16K", "64K", "256K", "1M",
                                             "4M", "16M", "64M", "256M", "1G"
                                            };

void ImageBandwidth::SetUp() {
  hsa_status_t err;

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();

  // Find the global region
  err = hsa_amd_agent_iterate_memory_pools(*gpu_dev, rocrtst::FindGlobalPool,
                                                                  &cpu_pool());
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

void ImageBandwidth::Run() {
  hsa_status_t err;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();

  for (int i = 0; i < 10; i++) {
    // Create timer for import, export and copy tests
    rocrtst::PerfTimer import_timer;
    rocrtst::PerfTimer export_timer;
    rocrtst::PerfTimer copy_timer;
    std::vector<double> import_image;
    std::vector<double> export_image;
    std::vector<double> copy_image;
    // Allocate image buffer in host memory
    uint32_t* image_buffer = NULL;
    err = hsa_amd_memory_pool_allocate(cpu_pool(),
                                       Size[i] * Size[i] * sizeof(uint32_t),
                                                    0, (void**) &image_buffer);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    // rocrtst::CommonCleanUp the image buffer
    for (uint32_t j = 0; j < Size[i] * Size[i]; j++) {
      image_buffer[j] = 0x10101010;
    }

    // Prepare for 2D image creation
    hsa_ext_image_t image_handle;

    hsa_ext_image_descriptor_t image_descriptor;
    image_descriptor.geometry = geometry_;
    image_descriptor.width = Size[i];
    image_descriptor.height = Size[i];
    image_descriptor.depth = 1;
    image_descriptor.array_size = 0;
    image_descriptor.format = format_;

    // Check if device_ supports at least read and write operation on
    // image format
    uint32_t capability_mask;
    err = hsa_ext_image_get_capability(*gpu_dev, HSA_EXT_IMAGE_GEOMETRY_2D,
                                       &format_, &capability_mask);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    if (!(capability_mask & HSA_EXT_IMAGE_CAPABILITY_READ_WRITE)) {
      std::cout <<
       "Device does not support read and write operation on this kind of image!"
                << std::endl;
      ASSERT_NE(capability_mask & HSA_EXT_IMAGE_CAPABILITY_READ_WRITE, 0);
    }

    // Get image info
    hsa_ext_image_data_info_t image_info;
    err = hsa_ext_image_data_get_info(*gpu_dev, &image_descriptor,
                                      HSA_ACCESS_PERMISSION_RW, &image_info);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    // Allocate memory for image
    uintptr_t ptr_temp = 0;
    err = hsa_amd_memory_pool_allocate(cpu_pool(),
              image_info.size + image_info.alignment, 0, (void**) &ptr_temp);

    // Align the image address
    uintptr_t mul = ptr_temp / image_info.alignment;
    void* ptr_image = (void*) ((mul + 1) * image_info.alignment);

    // rocrtst::CommonCleanUp the image to 0
    hsa_amd_memory_fill(ptr_image, 0, image_info.size);

    // Create image handle
    err = hsa_ext_image_create(*gpu_dev, &image_descriptor, ptr_image,
                               HSA_ACCESS_PERMISSION_RW, &image_handle);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    // Set import image region
    hsa_dim3_t range = {(uint32_t) Size[i], (uint32_t) Size[i], 1};

    hsa_ext_image_region_t image_region;
    hsa_dim3_t image_offset = {0, 0, 0};
    image_region.offset = image_offset;
    image_region.range = range;

    size_t iterations = RealIterationNum();

    for (uint32_t it = 0; it < iterations; it++) {
      // Create a timer
      int index = import_timer.CreateTimer();

      // Stamp at the beginning
      import_timer.StartTimer(index);

      // Import image from host
      err = hsa_ext_image_import(*gpu_dev, image_buffer, 0, 0, image_handle,
                                 &image_region);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      // Stamp in the end
      import_timer.StopTimer(index);
      import_image.push_back(import_timer.ReadTimer(index));
    }

    // Reset image_buffer
    hsa_amd_memory_fill(image_buffer, 0, Size[i] * Size[i] * sizeof(uint32_t));

    for (uint32_t it = 0; it < iterations; it++) {
      // Export image
      // Stamp at the beginning
      int index = export_timer.CreateTimer();
      export_timer.StartTimer(index);

      err = hsa_ext_image_export(*gpu_dev, image_handle, image_buffer, 0, 0,
                                 &image_region);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      export_timer.StopTimer(index);
      export_image.push_back(export_timer.ReadTimer(index));

      // Check if the value is correct
      for (uint32_t j = 0; j < Size[i] * Size[i]; j++) {
        ASSERT_EQ(image_buffer[j], 0x10101010);
      }
    }

    // Create another image for copy
    // Allocate memory for image
    uintptr_t ptr_temp2 = 0;
    err = hsa_amd_memory_pool_allocate(cpu_pool(),
              image_info.size + image_info.alignment, 0, (void**) &ptr_temp2);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    // Align the image address
    mul = ptr_temp2 / image_info.alignment;
    void* ptr_image2 = (void*) ((mul + 1) * image_info.alignment);

    // rocrtst::CommonCleanUp the image to 0
    hsa_amd_memory_fill(ptr_image2, 0, image_info.size);

    // Create image handle
    hsa_ext_image_t image_handle_copy;
    err = hsa_ext_image_create(*gpu_dev, &image_descriptor, ptr_image2,
                               HSA_ACCESS_PERMISSION_RW, &image_handle_copy);
    ASSERT_EQ(err, HSA_STATUS_SUCCESS);

    for (uint32_t it = 0; it < iterations; it++) {
      // Stamp at the beginning
      int index = copy_timer.CreateTimer();
      copy_timer.StartTimer(index);

      err = hsa_ext_image_copy(*gpu_dev, image_handle, &image_offset,
                               image_handle_copy, &image_offset, &range);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      // Stamp in the end
      copy_timer.StopTimer(index);
      copy_image.push_back(copy_timer.ReadTimer(index));

      // Check if image data is correct
      hsa_amd_memory_fill(image_buffer, 0,
                                      Size[i] * Size[i] * sizeof(uint32_t));

      // Export image
      err = hsa_ext_image_export(*gpu_dev, image_handle_copy, image_buffer,
                                 0, 0, &image_region);
      ASSERT_EQ(err, HSA_STATUS_SUCCESS);

      // Check if the value is correct
      for (uint32_t j = 0; j < Size[i] * Size[i]; j++) {
        ASSERT_EQ(image_buffer[j], 0x10101010);
      }

    }

    // Calculate Bandwidth
    import_bandwidth_[i] = CalculateBandwidth(import_image, Size[i]);
    export_bandwidth_[i] = CalculateBandwidth(export_image, Size[i]);
    copy_bandwidth_[i] = CalculateBandwidth(copy_image, Size[i]);
  }
}

double ImageBandwidth::CalculateBandwidth(std::vector<double>& vec,
    size_t size) {
  double mean = 0.0;

  // Delete the first timer result, which is warm up test
  vec.erase(vec.begin());

  // Sort the results
  std::sort(vec.begin(), vec.end());

  // Delete the last 20% of the results

  vec.erase(vec.begin() + num_iteration(), vec.end());

  int num = vec.size();

  for (int index = 0; index < num; index++) {
    mean += vec[index];
  }

  mean /= num;

  return (double) size * size * 4 / mean / 1024 / 1024 / 1024;
}

void ImageBandwidth::DisplayResults() const {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  fprintf(stdout, "==================================================="
                                                "=========================\n");

  fprintf(stdout,
          "  Size        Import                Export                 Copy\n");

  for (int i = 0; i < 10; i++) {
    fprintf(stdout,
            "  %s         %f(GB/s)          %f(GB/s)             %f(GB/s)\n",
            Str[i], import_bandwidth_[i], export_bandwidth_[i],
                                                           copy_bandwidth_[i]);
    fprintf(stdout, "================================================="
                                              "===========================\n");
  }
}

void ImageBandwidth::Close() {
  hsa_status_t err;
  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
}

size_t ImageBandwidth::RealIterationNum() {
  return num_iteration() * 1.2 + 1;
}
