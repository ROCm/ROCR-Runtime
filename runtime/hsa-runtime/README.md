# Package Contents

This directory contains the ROC Runtime source code based on the HSA Runtime
but modified to support AMD/ATI discrete GPUs.

## Source & Include Directories

`core` - Contains the source code for AMD's implementation of the core HSA
         Runtime API's.

`cmake_modules` - CMake support modules and files.

`inc` - Contains the public and AMD specific header files exposing the HSA
        Runtime`s interfaces.

`libamdhsacode` - Code object definitions and interface.

`loader` - Used to load code objects.

`utils` - Utilities required to build the core runtime.

## Build Environment

CMake build framework is used to build the ROC runtime. The minimum version is
3.7.

Obtain cmake infrastructure: <http://www.cmake.org/download/>

Export cmake bin into your PATH

## Package Dependencies

The following support packages are required to successfully build the runtime:

* `libelf-dev`
* `g++`

## Building the Runtime

To build the runtime a compatible version of the `libhsakmt` library and the
`hsakmt.h` header file must be available. The latest version of these files
can be obtained from the ROCT-Thunk-Interface repository, available here:

<https://github.com/RadeonOpenCompute/ROCT-Thunk-Interface>

As of ROCm release 3.7 `libhsakmt` development packages now include a CMake
package config file. The runtime will now locate `libhsakmt` via `find_package`
if `libhsakmt` is installed to a standard location. For installations that do
not use ROCm standard paths set CMake variables `CMAKE_PREFIX_PATH` or
`hsakmt_DIR` to override `find_package` search paths.

As of ROCm release 3.7 the runtime includes an optional image support module
(previously `hsa-ext-rocr-dev`). By default this module is included in builds of
the runtime. The image module may be excluded the runtime by setting
CMake variable `IMAGE_SUPPORT` to `OFF`.

When building the optional image module additional build dependencies are
required. An AMDGCN compatible clang and device library must be installed
to build the image module. The latest version of these requirements can be
obtained from the ROCm package repository
(see:
<https://rocmdocs.amd.com/en/latest/Installation_Guide/Installation-Guide.html>)
The latest source for these projects may be found here:

<https://github.com/RadeonOpenCompute/llvm-project>

<https://github.com/RadeonOpenCompute/ROCm-Device-Libs>

Additionally `xxd` must be installed.

The runtime optionally supports use of the cmake user package registry. By
default the registry is not modified. Set CMake variable
`EXPORT_TO_USER_PACKAGE_REGISTRY` to `ON` to enable updating the package
registry.

For example, to build, install, and produce packages on a system with standard
ROCm packages installed, execute the following from `src/`:

```bash
mkdir build
cd build
cmake -DCMAKE_INSTALL_PATH=/opt/rocm ..
make
make install
make package
```

Example with a custom installation path, build dependency path, and options:

```bash
cmake -DIMAGE_SUPPORT=OFF \
      -DEXPORT_TO_USER_PACKAGE_REGISTRY=ON \
      -DCMAKE_VERBOSE_MAKEFILE=1 \
      -DCMAKE_PREFIX_PATH=<alternate path(s) to build dependencies> \
      -DCMAKE_INSTALL_PATH=<custom install path for this build> \
      ..
```

Alternately `ccmake` and `cmake-gui` are supported:

```bash
mkdir build
cd build
ccmake ..
press c to configure
populate variables as desired
press c again
press g to generate and exit
make
```

## Building Against the Runtime

The runtime provides a CMake package config file, installed by default to
`/opt/rocm/lib/cmake/hsa-runtime64`. The runtime exports CMake target
`hsa-runtime64` in namespace `hsa-runtime64`. A CMake project (`Foo`) using the
runtime may locate, include, and link the runtime with the following template:

```cmake
# Add /opt/rocm to CMAKE_PREFIX_PATH.

find_package(hsa-runtime64 1.0 REQUIRED)
...
add_library(Foo ...)
...
target_link_libraries(Foo PRIVATE hsa-runtime64::hsa-runtime64)
```

## Specs

<http://www.hsafoundation.com/standards/>

HSA Runtime Specification 1.1

HSA Programmer Reference Manual Specification 1.1

HSA Platform System Architecture Specification 1.1

## Runtime Design Overview

The AMD ROC runtime consists of three primary layers:

* C interface adaptors
* C++ interfaces classes and common functions
* AMD device specific implementations

Additionally the runtime is dependent on a small utility library which provides
simple common functions, limited operating system and compiler abstraction, as
well as atomic operation interfaces.

## C Interface Adaptors

Files:

* `hsa.h`(cpp)
* `hsa_ext_interface.h`(cpp)

The C interface layer provides C99 APIs as defined in the HSA Runtime
Specification 1.1. The interfaces and default definitions for the standard
extensions are also provided. The interface functions simply forward to a
function pointer table defined here. The table is initialized to point to
default definitions, which simply return an appropriate error code. If
available the extension library is loaded as part of runtime initialization and
the table is updated to point into the extension library.

## C++ Interfaces Classes & Common Functions

Files:

* `runtime.h`(cpp)
* `agent.h`
* `queue.h`
* `signal.h`
* `memory_region.h`(cpp)
* `checked.h`
* `memory_database.h`(cpp)
* `default_signal.h`(cpp)

The C++ interface layer provides abstract interface classes encapsulating
commands to HSA Signals, Agents, and Queues. This layer also contains the
implementation of device independent commands, such as `hsa_init` and
`hsa_system_get_info`, and a default signal and queue implementation.

## Device Specific Implementations

Files:

* `amd_cpu_agent.h`(cpp)
* `amd_gpu_agent.h`(cpp)
* `amd_hw_aql_command_processor.h`(cpp)
* `amd_memory_region.h`(cpp)
* `amd_memory_registration.h`(cpp)
* `amd_topology.h`(cpp)
* `host_queue.h`(cpp)
* `interrupt_signal.h`(cpp)
* `hsa_ext_private_amd.h`(cpp)

The device specific layer contains implementations of the C++ interface classes
which implement HSA functionality for ROCm supported devices.

## Implemented Functionality

* The following queries are not implemented:

  * `hsa_code_symbol_get_info`:
    * `HSA_CODE_SYMBOL_INFO_INDIRECT_FUNCTION_CALL_CONVENTION`
  * `hsa_executable_symbol_get_info`:
    * `HSA_EXECUTABLE_SYMBOL_INFO_INDIRECT_FUNCTION_OBJECT`
    * `HSA_EXECUTABLE_SYMBOL_INFO_INDIRECT_FUNCTION_CALL_CONVENTION`

## Known Issues

* `hsa_agent_get_exception_policies` is not implemented.
* `hsa_system_get_extension_table` is not implemented for
  `HSA_EXTENSION_AMD_PROFILER`.

## Disclaimer

The information contained herein is for informational purposes only, and is
subject to change without notice. While every precaution has been taken in the
preparation of this document, it may contain technical inaccuracies, omissions
and typographical errors, and AMD is under no obligation to update or otherwise
correct this information. Advanced Micro Devices, Inc. makes no representations
or warranties with respect to the accuracy or completeness of the contents of
this document, and assumes no liability of any kind, including the implied
warranties of noninfringement, merchantability or fitness for particular
purposes, with respect to the operation or use of AMD hardware, software or
other products described herein. No license, including implied or arising by
estoppel, to any intellectual property rights is granted by this document.
Terms and limitations applicable to the purchase or use of AMD's products are
as set forth in a signed agreement between the parties or in AMD's Standard
Terms and Conditions of Sale.

AMD, the AMD Arrow logo, and combinations thereof are trademarks of Advanced
Micro Devices, Inc. Other product names used in this publication are for
identification purposes only and may be trademarks of their respective
companies.

Copyright © 2014-2021 Advanced Micro Devices, Inc. All rights reserved.
