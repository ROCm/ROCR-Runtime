HSA extension AMD AQL profile library.
Provides AQL packets helper methods for
perfcounters (PMC) and SQ threadtraces (SQTT).

Current library implementation supports only GFX9.
The library source tree:
 - doc  - Documantation, the API specification and the presentation
 - inc  - Public API
   - hsa_ven_amd_aqlprofile.h - AMD AQL profile library public API
 - src  - AMD AQL profile library sources
   - core - the library sources
   - commandwriter - PM4 command writer originated from 'hsa-runtime/tools'
   - perfcounter - PM4 perfcounter manager originated from 'hsa-runtime/tools'
   - threadtrace - PM4 threadtrace manager originated from 'hsa-runtime/tools'
 - test - the library test suite
   - ctrl - Test controll
   - util - Test utils
   - SimpleConvolution - Simple convolution test

To build the library:

$ cd .../hsa-amd-aqlprofile
$ mkdir build
$ cd build
$ cmake ..
$ make

To run the test:

$ cd .../hsa-amd-aqlprofile/build
$ export LD_LIBRARY_PATH=$PWD
$ ./test/ctrl

To enable PMC profiling:

$ export ROCR_ENABLE_PMC=1

To enable SQTT profiling:

$ export ROCR_ENABLE_SQTT=1

Or to use the script:

$ ./run.sh
