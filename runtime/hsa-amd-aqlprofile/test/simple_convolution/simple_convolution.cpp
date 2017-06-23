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

#include <iostream>
#include <string.h>

#include "helper_funcs.h"
#include "simple_convolution.h"

SimpleConvolution::SimpleConvolution() {
  width_ = 64;
  height_ = 64;
  mask_width_ = 3;
  mask_height_ = mask_width_;

  if (!isPowerOf2(width_)) {
    width_ = roundToPowerOf2(width_);
  }

  if (!isPowerOf2(height_)) {
    height_ = roundToPowerOf2(height_);
  }

  if (!(mask_width_ % 2)) {
    mask_width_++;
  }

  if (!(mask_height_ % 2)) {
    mask_height_++;
  }

  if (width_ * height_ < 256) {
    width_ = 64;
    height_ = 64;
  }

  const uint32_t input_size_bytes = width_ * height_ * sizeof(uint32_t);
  const uint32_t mask_size_bytes = mask_width_ * mask_height_ * sizeof(float);

  set_sys_descr(KERNARG_DES_ID, sizeof(kernel_args_t));
  set_sys_descr(INPUT_DES_ID, input_size_bytes);
  set_sys_descr(OUTPUT_DES_ID, input_size_bytes);
  set_local_descr(LOCAL_DES_ID, input_size_bytes);
  set_sys_descr(MASK_DES_ID, mask_size_bytes);
  set_sys_descr(REFOUT_DES_ID, input_size_bytes);
}

void SimpleConvolution::init() {
  std::cout << "SimpleConvolution::init :" << std::endl;

  mem_descr_t input_des = get_descr(INPUT_DES_ID);
  mem_descr_t local_des = get_descr(LOCAL_DES_ID);
  mem_descr_t mask_des = get_descr(MASK_DES_ID);
  mem_descr_t refout_des = get_descr(REFOUT_DES_ID);
  mem_descr_t kernarg_des = get_descr(KERNARG_DES_ID);

  uint32_t* input = (uint32_t*)input_des.ptr;
  uint32_t* output_local = (uint32_t*)local_des.ptr;
  float* mask = (float*)mask_des.ptr;
  kernel_args_t* kernel_args = (kernel_args_t*)kernarg_des.ptr;

  // random initialisation of input
  fillRandom<uint32_t>(input, width_, height_, 0, 255);

  // Fill a blurr filter or some other filter of your choice
  const float val = 1.0f / (mask_width_ * 2.0f - 1.0f);
  for (uint32_t i = 0; i < (mask_width_ * mask_height_); i++) {
    mask[i] = 0;
  }
  for (uint32_t i = 0; i < mask_width_; i++) {
    uint32_t y = mask_height_ / 2;
    mask[y * mask_width_ + i] = val;
  }
  for (uint32_t i = 0; i < mask_height_; i++) {
    uint32_t x = mask_width_ / 2;
    mask[i * mask_width_ + x] = val;
  }

  // Print the INPUT array.
  printArray<uint32_t>("> Input[0]", input, width_, 1);
  printArray<float>("> Mask", mask, mask_width_, mask_height_);

  // Fill the kernel args
  kernel_args->arg1 = output_local;
  kernel_args->arg2 = input;
  kernel_args->arg3 = mask;
  kernel_args->arg4 = width_;
  kernel_args->arg41 = height_;
  kernel_args->arg5 = mask_width_;
  kernel_args->arg51 = mask_height_;

  // Calculate the reference output
  memset(refout_des.ptr, 0, refout_des.size);
  reference_impl((uint32_t*)refout_des.ptr, input, mask, width_, height_, mask_width_,
                 mask_height_);
}

void SimpleConvolution::print_output() const {
  printArray<uint32_t>("> Output[0]", (uint32_t*)get_output_ptr(), width_, 1);
}

bool SimpleConvolution::reference_impl(uint32_t* output, const uint32_t* input, const float* mask,
                                       const uint32_t width, const uint32_t height,
                                       const uint32_t mask_width, const uint32_t mask_height) {
  const uint32_t vstep = (mask_width - 1) / 2;
  const uint32_t hstep = (mask_height - 1) / 2;

  // for each pixel in the input
  for (uint32_t x = 0; x < width; x++) {
    for (uint32_t y = 0; y < height; y++) {
      // find the left, right, top and bottom indices such that
      // the indices do not go beyond image boundaires
      const uint32_t left = (x < vstep) ? 0 : (x - vstep);
      const uint32_t right = ((x + vstep) >= width) ? width - 1 : (x + vstep);
      const uint32_t top = (y < hstep) ? 0 : (y - hstep);
      const uint32_t bottom = ((y + hstep) >= height) ? height - 1 : (y + hstep);

      // initializing wighted sum value
      float sum_fx = 0;
      for (uint32_t i = left; i <= right; ++i) {
        for (uint32_t j = top; j <= bottom; ++j) {
          // performing wighted sum within the mask boundaries
          uint32_t mask_idx = (j - (y - hstep)) * mask_width + (i - (x - vstep));
          uint32_t index = j * width + i;

          // to round to the nearest integer
          sum_fx += ((float)input[index] * mask[mask_idx]);
        }
      }
      sum_fx += 0.5f;
      output[y * width + x] = uint32_t(sum_fx);
    }
  }

  return true;
}
