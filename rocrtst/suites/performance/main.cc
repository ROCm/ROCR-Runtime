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

#include "cp_process_time.h"
#include "cu_masking.h"
#include "device_load_bandwidth.h"
#include "device_store_bandwidth.h"
#include "dispatch_time.h"
#include "flush_latency.h"
#include "gtest/gtest.h"
#include "hsa_info.h"
#include "image_bandwidth.h"
#include "image_load_bandwidth.h"
#include "image_store_bandwidth.h"
#include "matrix_transpose.h"
#include "memory_copy.h"
#include "memory_allocation.h"
#include "memory_async_copy.h"
#include "queue_concurrency.h"
#include "queue_create_destroy_latency.h"
#include "system_load_bandwidth.h"
#include "system_store_bandwidth.h"
#include "vector_copy.h"

/**
 * Try to order tests from fastest running to slowest running.
 */

// DisplayResultsResults HSA system information first.
TEST(rocrtst, Feature_Hsa_Info) {
  HsaInfo hi;
  hi.SetUp();
  hi.Run();
  hi.Close();
}

// Requires HSA_PFOFILE_FULL
TEST(rocrtst, Perf_Image_Store_Bandwidth) {
  ImageStoreBandwidth isb;
  isb.SetUp();
  isb.Run();
  isb.DisplayResults();
  isb.Close();
}

// Requires HSA_PFOFILE_FULL
TEST(rocrtst, Perf_Image_Load_Bandwidth) {
  ImageLoadBandwidth ilb;
  ilb.SetUp();
  ilb.Run();
  ilb.DisplayResults();
  ilb.Close();
}

// Requires HSA_PFOFILE_FULL
TEST(rocrtst, Perf_Image_Bandwidth) {
  ImageBandwidth ib;
  ib.SetUp();
  ib.Run();
  ib.DisplayResults();
  ib.Close();
}

// Requires HSA_PFOFILE_FULL
TEST(rocrtst, Perf_Queue_Concurrency) {
  QueueConcurrency mc;
  mc.SetUp();
  mc.Run();
  mc.DisplayResults();
  mc.Close();
}

TEST(rocrtst, Feature_Cu_Masking) {
  CuMasking cm;
  cm.SetUp();
  cm.Run();
  cm.Close();
}

TEST(rocrtst, Perf_Flush_Latency) {
  FlushLatency fl;
  fl.SetUp();
  fl.Run();
  fl.DisplayResults();
  fl.Close();
}

// This test apparently has some sort of memory bounds overwrite
// issue with the out_data_ buffer. Commenting out the free of
// out_data_ avoids the problem. Left uncommented, a crash will
// occur immediately or some time after.
TEST(rocrtst, DISABLED_Perf_Device_Memory_Store_Bandwidth) {
  DeviceStoreBandwidth slb;
  slb.SetUp();
  slb.Run();
  slb.DisplayResults();
  slb.Close();
}

// This test apparently has some sort of memory bounds overwrite
// issue with the out_data_ buffer. Commenting out the free of
// out_data_ avoids the problem. Left uncommented, a crash will
// occur immediately or some time after.
TEST(rocrtst, DISABLED_Perf_Device_Memory_Load_Bandwidth) {
  DeviceLoadBandwidth slb;
  slb.SetUp();
  slb.Run();
  slb.DisplayResults();
  slb.Close();
}
TEST(rocrtst, Perf_Dispatch_Time_Single_SpinWait) {
  DispatchTime dt;
  dt.set_num_iteration(100);
  dt.UseDefaultSignal(true);
  dt.LaunchSingleKernel(true);
  dt.SetUp();
  dt.Run();
  dt.DisplayResults();
  dt.Close();
}

TEST(rocrtst, Perf_Dispatch_Time_Single_Interrupt) {
  DispatchTime dt;
  dt.UseDefaultSignal(false);
  dt.LaunchSingleKernel(true);
  dt.SetUp();
  dt.Run();
  dt.DisplayResults();
  dt.Close();
}

TEST(rocrtst, Perf_Dispatch_Time_Multi_SpinWait) {
  DispatchTime dt;
  dt.UseDefaultSignal(true);
  dt.LaunchSingleKernel(false);
  dt.SetUp();
  dt.Run();
  dt.DisplayResults();
  dt.Close();
}

TEST(rocrtst, Perf_Dispatch_Time_Multi_Interrupt) {
  DispatchTime dt;
  dt.UseDefaultSignal(false);
  dt.LaunchSingleKernel(false);
  dt.SetUp();
  dt.Run();
  dt.DisplayResults();
  dt.Close();
}
TEST(rocrtst, DISABLED_Perf_CpProcessTime) {
  CpProcessTime cpt;
  cpt.set_num_iteration(10);
  cpt.SetUp();
  cpt.Run();
  cpt.DisplayResults();
  cpt.Close();
}

TEST(rocrtst, Perf_Memory_Allocation) {
  MemoryAllocation ma(10);
  ma.SetUp();
  ma.Run();
  ma.DisplayResults();
  ma.Close();
}

#if MEM_POOL_FILL_BUG
TEST(rocrtst, Perf_Queue_Latency) {
  QueueLatency ql;
  ql.set_num_iteration(10);
  ql.SetUp();
  ql.Run();
  ql.DisplayResults();
  ql.Close();
}

TEST(rocrtst, Perf_System_Memory_Load_Bandwidth) {
  SystemLoadBandwidth slb;
  slb.SetUp();
  slb.Run();
  slb.DisplayResults();
  slb.Close();
}

TEST(rocrtst, Perf_System_Memory_Store_Bandwidth) {
  SystemStoreBandwidth ssb;
  ssb.SetUp();
  ssb.Run();
  ssb.DisplayResults();
  ssb.Close();
}

TEST(rocrtst, Perf_Memory_Copy) {
  MemoryCopy mc;
  mc.set_num_iteration(10);
  mc.SetUp();
  mc.Run();
  mc.DisplayResults();
  mc.Close();
}

#endif

#if 0
// These tests were not complete. Needs research/work.
TEST(rocrtst, Feature_Vector_Copy) {
  VectorCopy vc;
  vc.SetUp();
  vc.Run();
  vc.Close();
}

TEST(rocrtst, Perf_Matrix_Transpose) {
  MatrixTranspose mt;
  mt.SetUp();
  mt.Run();
  mt.DisplayResults();
  mt.Close();
}

#endif

//#if NEED_TO_MAKE_BATCH
TEST(rocrtst, Perf_Memory_Async_Copy) {
  MemoryAsyncCopy mac;
  mac.set_num_iteration(10);
  mac.SetUp();
  mac.Run();
  mac.DisplayResults();
  mac.Close();
}
//#endif

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
