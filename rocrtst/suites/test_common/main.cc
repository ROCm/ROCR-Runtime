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

#include <string>
#include <vector>
#include <memory>

#include "gtest/gtest.h"
#include "suites/functional/memory_basic.h"
#include "suites/functional/memory_access.h"
#include "suites/functional/ipc.h"
#include "suites/performance/dispatch_time.h"
#include "suites/performance/memory_async_copy.h"
#include "suites/performance/memory_async_copy_numa.h"
#include "suites/performance/enqueueLatency.h"
#include "suites/test_common/test_case_template.h"
#include "suites/test_common/main.h"
#include "suites/test_common/test_common.h"

#if ENABLE_SMI
#include "rocm_smi/rocm_smi.h"
#endif

static RocrTstGlobals *sRocrtstGlvalues = nullptr;
#if ENABLE_SMI
static bool GetMonitorDevices(const std::shared_ptr<amd::smi::Device> &d,
                                                                    void *p) {
  std::string val_str;

  assert(p != nullptr);

  std::vector<std::shared_ptr<amd::smi::Device>> *device_list =
    reinterpret_cast<std::vector<std::shared_ptr<amd::smi::Device>> *>(p);

  if (d->monitor() != nullptr) {
    device_list->push_back(d);
  }
  return false;
}
#endif

static void SetFlags(TestBase *test) {
  assert(sRocrtstGlvalues != nullptr);

  test->set_num_iteration(sRocrtstGlvalues->num_iterations);
  test->set_verbosity(sRocrtstGlvalues->verbosity);
  test->set_monitor_verbosity(sRocrtstGlvalues->monitor_verbosity);
  test->set_monitor_devices(&sRocrtstGlvalues->monitor_devices);
}


static void RunCustomTestProlog(TestBase *test) {
  SetFlags(test);

  test->DisplayTestInfo();
  test->SetUp();
  test->Run();
  return;
}
static void RunCustomTestEpilog(TestBase *test) {
  test->DisplayResults();
  test->Close();
  return;
}

// If the test case one big test, you should use RunGenericTest()
// to run the test case. OTOH, if the test case consists of multiple
// functions to be run as separate tests, follow this pattern:
//   * RunCustomTestProlog(test)  // Run() should contain minimal code
//   * <insert call to actual test function within test case>
//   * RunCustomTestEpilog(test)
static void RunGenericTest(TestBase *test) {
  RunCustomTestProlog(test);
  RunCustomTestEpilog(test);
  return;
}

// TEST ENTRY TEMPLATE:
// TEST(rocrtst, Perf_<test name>) {
//  <Test Implementation class> <test_obj>;
//
//  // Copy and modify implementation of RunGenericTest() if you need to deviate
//  // from the standard pattern implemented there.
//  RunGenericTest(&<test_obj>);
// }

TEST(rocrtst, Test_Example) {
  TestExample tst;

  RunGenericTest(&tst);
}

// Temporarily disable this test until hsa_init()/hsa_shut_down() works
// simultaneously in 2 different processes (SWDEV-134085); The test can be run
// by itself to test IPC and avoid the negative consequnces of the defect
// mentioned. To do this, use the --gtest_also_run_disabled_tests flag.
TEST(rocrtstFunc, IPC) {
  IPCTest ipc;
  RunGenericTest(&ipc);
}

TEST(rocrtstFunc, MemoryAccessTests) {
  MemoryAccessTest mt;
  RunCustomTestProlog(&mt);
  mt.CPUAccessToGPUMemoryTest();
  mt.GPUAccessToCPUMemoryTest();
  RunCustomTestEpilog(&mt);
}
// Temporarily disable this test until hsa_shut_down() is (probably not the
// same as with the IPC test above) is addressed. To override the disable,
// run with --gtest-also_run_disabled_tests flag.
TEST(rocrtstFunc, DISABLED_Memory_Max_Mem) {
  MemoryTest mt;

  RunCustomTestProlog(&mt);
  mt.MaxSingleAllocationTest();
  RunCustomTestEpilog(&mt);
}

TEST(rocrtstPerf, ENQUEUE_LATENCY) {
  EnqueueLatency singlePacketequeue(true);
  EnqueueLatency multiPacketequeue(false);
  RunGenericTest(&singlePacketequeue);
  RunGenericTest(&multiPacketequeue);
}

TEST(rocrtstPerf, Memory_Async_Copy) {
  MemoryAsyncCopy mac;
  // To do full test, uncomment this:
  //  mac.set_full_test(true);
  // To test only 1 path, add lines like this:
  //  mac.set_src_pool(<src pool id>);
  //  mac.set_dst_pool(<dst pool id>);
  // The default is to and from the cpu to 1 gpu, and to/from a gpu to
  // another gpu
  RunGenericTest(&mac);
}

TEST(rocrtstPerf, Memory_Async_Copy_NUMA) {
  MemoryAsyncCopyNUMA numa;
  RunGenericTest(&numa);
}

TEST(rocrtstPerf, AQL_Dispatch_Time_Single_SpinWait) {
  DispatchTime dt(true, true);
  RunGenericTest(&dt);
}

TEST(rocrtstPerf, AQL_Dispatch_Time_Single_Interrupt) {
  DispatchTime dt(false, true);
  RunGenericTest(&dt);
}

TEST(rocrtstPerf, AQL_Dispatch_Time_Multi_SpinWait) {
  DispatchTime dt(true, false);
  RunGenericTest(&dt);
}

TEST(rocrtstPerf, AQL_Dispatch_Time_Multi_Interrupt) {
  DispatchTime dt(false, false);
  RunGenericTest(&dt);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  RocrTstGlobals settings;

  // Set some default values
  settings.verbosity = 1;
  settings.monitor_verbosity = 1;
  settings.num_iterations = 5;


  if (ProcessCmdline(&settings, argc, argv)) {
    return 1;
  }
  sRocrtstGlvalues = &settings;
#if ENABLE_SMI
  amd::smi::RocmSMI hw;
  hw.DiscoverDevices();
  hw.IterateSMIDevices(
       GetMonitorDevices, reinterpret_cast<void *>(&settings.monitor_devices));

  sRocrtstGlvalues = &settings;

  // Use this dummy test to get one output of monitors at the beginning
  {
    TestExample dummy;
    dummy.set_monitor_devices(&sRocrtstGlvalues->monitor_devices);

    std::cout << "*** Initial Hardware Monitor Values:" << std::endl;
    DumpMonitorInfo(&dummy);
  }
#endif
  return RUN_ALL_TESTS();
}
