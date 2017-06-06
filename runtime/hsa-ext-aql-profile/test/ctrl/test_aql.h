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

#ifndef _TESTAQL_H_
#define _TESTAQL_H_

#include "hsa.h"
#include "hsa_rsrc_factory.hpp"
#include "hsa_ext_amd_aql_profile.h"

#define test_assert(cond)                                                                          \
  {                                                                                                \
    if (cond) {                                                                                    \
      std::cout << "ASSERT FAILED: " << #cond << " : " << __FILE__ << "(" << __LINE__ << ")"       \
                << std::endl;                                                                      \
      abort();                                                                                     \
    }                                                                                              \
  }

// Test AQL interface
class TestAql {
  TestAql* const test_aql;

 public:
  TestAql(TestAql* t = 0) : test_aql(t) {}
  virtual ~TestAql() {}

  TestAql* testAql() { return test_aql; }
  virtual AgentInfo* getAgentInfo() { return (test_aql) ? test_aql->getAgentInfo() : 0; }
  virtual hsa_queue_t* getQueue() { return (test_aql) ? test_aql->getQueue() : 0; }
  virtual HsaRsrcFactory* getRsrcFactory() { return (test_aql) ? test_aql->getRsrcFactory() : 0; }

  // Initialize application environment including setting
  // up of various configuration parameters based on
  // command line arguments
  // @return bool true on success and false on failure
  virtual bool initialize(int argc, char** argv) {
    return (test_aql) ? test_aql->initialize(argc, argv) : true;
  }

  // Setup application parameters for exectuion
  // @return bool true on success and false on failure
  virtual bool setup() { return (test_aql) ? test_aql->setup() : true; }

  // Run the kernel
  // @return bool true on success and false on failure
  virtual bool run() { return (test_aql) ? test_aql->run() : true; }

  // Verify results
  // @return bool true on success and false on failure
  virtual bool verify_results() { return (test_aql) ? test_aql->verify_results() : true; }

  // Print to console the time taken to execute kernel
  virtual void print_time() {
    if (test_aql) test_aql->print_time();
  }

  // Release resources e.g. memory allocations
  // @return bool true on success and false on failure
  virtual bool cleanup() { return (test_aql) ? test_aql->cleanup() : true; }
};

#endif  // _TESTAQL_H_
