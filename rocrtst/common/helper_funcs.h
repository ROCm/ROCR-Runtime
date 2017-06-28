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

#ifndef ROCRTST_COMMON_HELPER_FUNCS_H_
#define ROCRTST_COMMON_HELPER_FUNCS_H_

/// \file
/// General-purpose helper functions

#include <string>
#include <vector>
namespace rocrtst {


bool Compare(const float* refData, const float* data,
             const int length, const float epsilon = 1e-6f);
bool Compare(const double* refData, const double* data,
             const int length, const double epsilon = 1e-6);

/// Calculate the mean number of the vector
double CalcMean(const std::vector<double> &scores);

/// Calculate the mean time of difference of the two vectors
double CalcMean(const std::vector<double>& v1, const std::vector<double>& v2);

/// Return the median value of a vector of doubles
/// \param[in] scores Vector of doubles
/// \returns double Median value of provided vector
double CalcMedian(const std::vector<double> &scores);

/// Calculate the standard deviation of the vector
double CalcStdDeviation(std::vector<double> scores, int score_mean);

/// Display an array to std::out
template<typename T>
void PrintArray(
  const std::string header,
  const T* data,
  const int width,
  const int height);

/// Fill an array with random values
template<typename T>
int FillRandom(
  T* arrayPtr,
  const int width,
  const int height,
  const T rangeMin,
  const T rangeMax,
  unsigned int seed = 123);

intptr_t AlignDown(intptr_t value, size_t alignment);
void* AlignDown(void* value, size_t alignment);
void* AlignUp(void* value, size_t alignment);

/// Rounds to a power of 2
uint64_t RoundToPowerOf2(uint64_t val);

///  Checks if a value is a power of 2
bool IsPowerOf2(uint64_t val);

}  // namespace rocrtst
#endif  //  ROCRTST_COMMON_HELPER_FUNCS_H_
