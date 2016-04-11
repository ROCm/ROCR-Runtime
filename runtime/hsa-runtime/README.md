### Package Contents

This directory contains the HSA Runtime source code for the Boltzmann release. It has been modified to support
AMD/ATI discrete GPUs.

#### Source & Include directories

core - Contains the source code for AMD's implementation of the core HSA Runtime API's.

cmake_modules - CMake support modules and files.

inc - Contains the public and AMD specific header files exposing the HSA Runtimes interfaces.

libamdhsacode - HSAIL/Finalizer runtime interface. 

loader - Used to load code objects.

utils - Utilities required to build the core runtime.

#### Build environment

CMake build framework is used to build the HSA runtime. The minimum version is 2.8.

Obtain cmake infrastructure: http://www.cmake.org/download/

Export cmake bin into your PATH
HSA Runtime CMake build file CMakeLists.txt is located in runtime/core folder.

#### Package Dependencies

The following support packages are requried to succesfully build the runtime:

* libelf-dev
* g++
* libc6-dev-i386 (for libhsakmt 32bit)

#### Building the runtime

To build the runtime a compatible version of the libhsakmt library and the 
hsakmt.h header file must be available. The latest version of these files
can be obtained from the ROCT-Thunk-Interface repository, available here:

https://github.com/RadeonOpenCompute/ROCT-Thunk-Interface
 
Specify the directory containing libhsakmt.so.1 and hsakmt.h using the following 
cmake variables:

HSATHK_BUILD_INC_PATH - Set to the dirctory containing hsakmt.h.

HSATHK_BUILD_LIB_PATH - Set to the directory containing libhsakmt.so.1

For example, from the top level ROCR repository execute:

    mkdir build
    cd build
    cmake -D HSATHK_BUILD_INC_PATH=<location of hsakmt.h> \
          -D HSATHK_BUILD_LIB_PATH=<location of libhsakmt.so> \
          ../src
    make

The name of the core hsa runtime is libhsa-runtime64.so.1.

#### External requirements

The core runtime requires the sp3.a library to be able to compiler
on x86_64 architechtures. The binaries for the sp3.a librariy can
be found on the amd-codexl-analyzer GitHub repository:

https://github.com/GPUOpen-Tools/amd-codexl-analyzer

The x86_64 library and associated header files have been added to
this code base for convenience, but are still subject to the 
AMD copyright license.

#### Specs

http://www.hsafoundation.com/standards/

HSA Runtime Specification 1.0

HSA Programmer Reference Manual Specification 1.0

HSA Platform System Architecture Specification 1.0

#### Runtime Design overview

The AMD HSA runtime consists of three primary layers:

C interface adaptors
C++ interfaces classes and common functions
AMD device specific implementations
Additionally the runtime is dependent on a small utility library which provides simple common functions, limited operating system and compiler abstraction, as well as atomic operation interfaces.

#### C interface adaptors

Files :

hsa.h(cpp)

hsa_ext_interface.h(cpp)

The C interface layer provides C99 APIs as defined in the HSA Runtime Specification 1.0. The interfaces and default definitions for the standard extensions are also provided. The interface functions simply forward to a function pointer table defined here. The table is initialized to point to default definitions, which simply return an appropriate error code. If available the extension library is loaded as part of runtime initialization and the table is updated to point into the extension library.  In this release the standard extensions (image support and finalizer) are implemented in a separate libraries (not open sourced), and can be obtained from the HSA-Runtime-AMD git repository.

#### C++ interfaces classes and common functions

Files :

runtime.h(cpp)

agent.h

queue.h

signal.h

memory_region.h(cpp)

checked.h

memory_database.h(cpp)

default_signal.h(cpp)

The C++ interface layer provides abstract interface classes encapsulating commands to HSA Signals, Agents, and Queues. This layer also contains the implementation of device independent commands, such as hsa_init and hsa_system_get_info, and a default signal and queue implementation.

#### Device Specific Implementations

Files:

amd_cpu_agent.h(cpp)

amd_gpu_agent.h(cpp)

amd_hw_aql_command_processor.h(cpp)

amd_memory_region.h(cpp)

amd_memory_registration.h(cpp)

amd_topology.h(cpp)

host_queue.h(cpp)

interrupt_signal.h(cpp)

hsa_ext_private_amd.h(cpp)

The device specific layer contains implementations of the C++ interface classes which implement HSA functionality for AMD Kaveri & Carrizo APUs.

#### Implemented functionality

* The following queries are not implemented:
  ** hsa_code_symbol_get_info: HSA_CODE_SYMBOL_INFO_INDIRECT_FUNCTION_CALL_CONVENTION
  ** hsa_executable_symbol_get_info: HSA_EXECUTABLE_SYMBOL_INFO_INDIRECT_FUNCTION_OBJECT, HSA_EXECUTABLE_SYMBOL_INFO_INDIRECT_FUNCTION_CALL_CONVENTION

#### Known Issues

* Max total coarse grain region limit is 8GB.
* hsa_agent_get_exception_policies is not implemented.
* Image import/export/copy/fill only support image created with memory from host accessible region.
* hsa_system_get_extension_table is not implemented for HSA_EXTENSION_AMD_PROFILER.

#### Disclaimer

The information contained herein is for informational purposes only, and is subject to change without notice. While every precaution has been taken in the preparation of this document, it may contain technical inaccuracies, omissions and typographical errors, and AMD is under no obligation to update or otherwise correct this information. Advanced Micro Devices, Inc. makes no representations or warranties with respect to the accuracy or completeness of the contents of this document, and assumes no liability of any kind, including the implied warranties of noninfringement, merchantability or fitness for particular purposes, with respect to the operation or use of AMD hardware, software or other products described herein. No license, including implied or arising by estoppel, to any intellectual property rights is granted by this document. Terms and limitations applicable to the purchase or use of AMD's products are as set forth in a signed agreement between the parties or in AMD's Standard Terms and Conditions of Sale.

AMD, the AMD Arrow logo, and combinations thereof are trademarks of Advanced Micro Devices, Inc. Other product names used in this publication are for identification purposes only and may be trademarks of their respective companies.

Copyright (c) 2014-2015 Advanced Micro Devices, Inc. All rights reserved.
