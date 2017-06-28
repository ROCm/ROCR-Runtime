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

#include <assert.h>
#include <stdint.h>
#include <iostream>
#include <getopt.h>

#include "suites/test_common/test_common.h"

RocrtstOptions::RocrtstOptions(uint32_t *verb, uint32_t *iter) {
  assert(verb != nullptr);
  assert(iter != nullptr);

  verbosity_ = verb;
  iterations_ = iter;
}

RocrtstOptions::~RocrtstOptions() {
}

static const struct option long_options[] = {
  {"iterations", required_argument, nullptr, 'i'},
  {"verbose", no_argument, nullptr, 'v'},

  {nullptr, 0, nullptr, 0}
};
static const char* short_options = "i:v:r";

static void PrintHelp(void) {
  std::cout <<
//            "Required Arguments:\n"
//           "--kernel, -k <path to kernel obj. file>\n"
     "Optional RocRTst Arguments:\n"
     "--iterations, -i <number of iterations to execute>; override default, "
         "which varies for each test\n"
     "--rocrtst_help, -r print this help message\n"
     "--verbosity, -v <verbosity level>\n"
     "  Verbosity levels:\n"
     "   0    -- minimal; just summary information\n"
     "   1    -- intermediate; show intermediate values such as intermediate "
                  "perf. data\n"
     "   2    -- progress; show progress displays\n"
     "   >= 3 -- more debug output\n";
}

uint32_t ProcessCmdline(RocrtstOptions* test, int arg_cnt, char** arg_list) {
  int a;
  int ind = -1;

  assert(test != nullptr);

  while (true) {
    a = getopt_long(arg_cnt, arg_list, short_options, long_options, &ind);

    if (a == -1) {
      break;
    }

    switch (a) {
      case 'i':
        *test->iterations_ = std::stoi(optarg);
        break;

      case 'v':
        *test->verbosity_ = std::stoi(optarg);
        break;

      case 'r':
        PrintHelp();
        return 1;

      default:
        PrintHelp();
        return 1;
    }
  }
  return 0;
}
