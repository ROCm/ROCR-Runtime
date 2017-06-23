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

#include "test_assert.h"
#include "simple_convolution.h"
#include "test_hsa.h"
#include "test_pgen_pmc.h"
#include "test_pgen_sqtt.h"

int main(int argc, char* argv[]) {
#if defined(NDEBUG)
  clog.rdbuf(NULL);
#endif

  bool ret_val = true;

  // Create SimpleConvolution test object
  TestKernel* test_kernel = new SimpleConvolution();
  TestAql* test_aql = new TestHSA(test_kernel);

  const bool pmc_enable = (getenv("ROCR_ENABLE_PMC") != NULL);
  const bool sqtt_enable = (getenv("ROCR_ENABLE_SQTT") != NULL);
  if (pmc_enable)
    test_aql = new TestPGenPMC(test_aql);
  else if (sqtt_enable)
    test_aql = new TestPGenSQTT(test_aql);
  test_assert(test_aql != NULL);
  if (test_aql == NULL) return 1;

  // Initialization of Hsa Runtime
  ret_val = test_aql->initialize(argc, argv);
  if (ret_val == false) {
    std::cout << "Error in the test initialization" << std::endl;
    test_assert(ret_val);
    return 1;
  }

  // Setup Hsa resources needed for execution
  ret_val = test_aql->setup();
  if (ret_val == false) {
    std::cout << "Error in creating hsa resources" << std::endl;
    test_assert(ret_val);
    return 1;
  }

  // Run SimpleConvolution kernel
  ret_val = test_aql->run();
  if (ret_val == false) {
    std::cout << "Error in running the test kernel" << std::endl;
    test_assert(ret_val);
    return 1;
  }

  // Verify the results of the execution
  ret_val = test_aql->verify_results();
  if (ret_val) {
    std::cout << "Test : Passed" << std::endl;
  } else {
    std::cout << "Test : Failed" << std::endl;
  }

  // Print time taken by sample
  test_aql->print_time();
  test_aql->cleanup();

  return (ret_val) ? 0 : 1;
}
