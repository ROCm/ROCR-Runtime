HSA extension AMD AQL profile library.
Provides AQL packets helper methods for
perfcounters (PMC) and SQ threadtraces (SQTT).

Current library implementation supports only GFX9.
The library source tree:
 - doc  - Documantation, the API specification and the presentation
 - inc  - Public API
   - hsa_ext_amd_aql_profile.h - AMD AQL profile library public API
 - src  - AMD AQL profile library sources
   - aqlprofile - AMD AQL profile library
   - commandwriter - PM4 command writer originated from 'hsa-runtime/tools'
   - perfcounter - PM4 perfcounter manager originated from 'hsa-runtime/tools'
   - threadtrace - PM4 threadtrace manager originated from 'hsa-runtime/tools'
 - test - the library test suite
   - ctrl - Test controll
   - util - Test utils
   - SimpleConvolution - Simple convolution test

To build the library:

$ cd .../hsa-ext-aql-profile
$ mkdir build
$ cd build
$ cmake ..
$ make

To run the test:

$ cd .../hsa-ext-aql-profile/build
$ test/ctrl

To enable PMC profiling:

$ export ROCR_ENABLE_PMC=1

To enable SQTT profiling:

$ export ROCR_ENABLE_SQTT=1
