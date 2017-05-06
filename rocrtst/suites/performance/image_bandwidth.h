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

#ifndef __ROCRTST_SRC_IMAGE_BANDWIDTH_H__
#define __ROCRTST_SRC_IMAGE_BANDWIDTH_H__

#include "perf_common/perf_base.h"
#include "common/base_rocr.h"
#include "hsa/hsa.h"
#include "hsa/hsa_ext_image.h"
#include <vector>

class ImageBandwidth: public rocrtst::BaseRocR, public PerfBase {
 public:
  //@Brief: Constructor for test case of ImageBandwidth
  ImageBandwidth(size_t num = 100);

  //@Brief: Destructor
  virtual ~ImageBandwidth();

  //@Brief: Setup the environment for measurement
  virtual void SetUp();

  //@Brief: Core measurement execution
  virtual void Run();

  //@Brief: Clean up and retrive the resource
  virtual void Close();

  //@Brief: Display  results
  virtual void DisplayResults() const;

 private:
  //@Brief: Define image size and corresponding string
  static const size_t Size[10];
  static const char* const Str[10];

  //@Brief: Get actual iteration number
  size_t RealIterationNum();

  //@Brief: Calculate Bandwidth
  double CalculateBandwidth(std::vector<double>& vec, size_t size);

 protected:
  //@Brief: bandwidth data
  double import_bandwidth_[10];
  double export_bandwidth_[10];
  double copy_bandwidth_[10];

  //@Brief: Image format
  hsa_ext_image_format_t format_;

  //@Brief: Image geometry
  hsa_ext_image_geometry_t geometry_;
};

#endif
