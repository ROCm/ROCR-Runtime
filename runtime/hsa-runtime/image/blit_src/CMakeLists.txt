################################################################################
##
## The University of Illinois/NCSA
## Open Source License (NCSA)
##
## Copyright (c) 2014-2020, Advanced Micro Devices, Inc. All rights reserved.
##
## Developed by:
##
##                 AMD Research and AMD HSA Software Development
##
##                 Advanced Micro Devices, Inc.
##
##                 www.amd.com
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to
## deal with the Software without restriction, including without limitation
## the rights to use, copy, modify, merge, publish, distribute, sublicense,
## and#or sell copies of the Software, and to permit persons to whom the
## Software is furnished to do so, subject to the following conditions:
##
##  - Redistributions of source code must retain the above copyright notice,
##    this list of conditions and the following disclaimers.
##  - Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimers in
##    the documentation and#or other materials provided with the distribution.
##  - Neither the names of Advanced Micro Devices, Inc,
##    nor the names of its contributors may be used to endorse or promote
##    products derived from this Software without specific prior written
##    permission.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
## THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
## OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
## ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
## DEALINGS WITH THE SOFTWARE.
##
################################################################################

cmake_minimum_required ( VERSION 3.5.0 )

# Flag to abort before executing after default initialization of cache variables
set (QUIT 0)

# Import target 'clang'
find_package(Clang REQUIRED HINTS ${CMAKE_INSTALL_PREFIX}/llvm ${CMAKE_PREFIX_PATH}/llvm PATHS /opt/rocm/llvm )

# Device libs doesn't support find_package yet.
# FIXME: HINTS should not use CMAKE_INSTALL_PREFIX
get_include_path(BITCODE_DIR "Bitcode library path" RESULT FOUND NAMES "opencl.bc" "opencl.amdgcn.bc"
  HINTS
    "${CMAKE_INSTALL_PREFIX}/amdgcn/bitcode"
    "${CMAKE_INSTALL_PREFIX}/lib/bitcode"
    "${CMAKE_INSTALL_PREFIX}/lib/x86_64/bitcode"
    "${CMAKE_PREFIX_PATH}/amdgcn/bitcode"
    "${CMAKE_PREFIX_PATH}/lib/bitcode"
    "${CMAKE_PREFIX_PATH}/lib/x86_64/bitcode"
  PATHS
    "/opt/rocm/amdgcn/bitcode"
    "/opt/rocm/lib/bitcode"
    "/opt/rocm/lib"
    "/opt/rocm/lib/x86_64/bitcode")
if (NOT ${FOUND})
  set (QUIT 1)
endif()

# Define the target devices with xnack enable
if (NOT DEFINED XNACK_DEVS)
  set (XNACK_DEVS "gfx801;gfx902")
endif()
set( XNACK_DEVS ${XNACK_DEVS} CACHE STRING "XNACK targets" FORCE )

# Determine the target devices if not specified
if (NOT DEFINED TARGET_DEVICES)
  set (TARGET_DEVICES "gfx700;gfx701;gfx702;gfx801;gfx802;gfx803;gfx900;gfx902;gfx904;gfx906;gfx908;gfx1010;gfx1011;gfx1012;gfx1030;gfx1031")
endif()
set( TARGET_DEVICES ${TARGET_DEVICES} CACHE STRING "Build targets" FORCE )

# End of default configuration and path checking.
# Quit if configuration is incomplete.
if (QUIT)
  message(FATAL_ERROR "Configuration halted.")
  return()
endif()

if(${CMAKE_VERBOSE_MAKEFILE})
  get_property(clang_path TARGET clang PROPERTY LOCATION)
  message("Using clang from: ${clang_path}")
  message("Build Setting:")
  message("   Target Devices: ${TARGET_DEVICES}")
  message("    XNACK Devices: ${XNACK_DEVS}")
  message("       Clang path: ${clang_path}")
  message("      Bitcode Dir: ${BITCODE_DIR}")
endif()

##==========================================
##  Add custom command to generate a kernel code object file
##==========================================
function(gen_kernel_bc TARGET_DEV XNACK_OPT INPUT_FILE OUTPUT_FILE)

  string (REPLACE "gfx" "" GFXIP "${TARGET_DEV}")
  # Determine if device-libs is following old or new layout
  if(EXISTS "${BITCODE_DIR}/opencl.amdgcn.bc")
    set(BITCODE_ARGS "-nogpulib
      -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/opencl.amdgcn.bc
      -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/ockl.amdgcn.bc
      -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/ocml.amdgcn.bc
      -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/oclc_daz_opt_on.amdgcn.bc
      -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/oclc_isa_version_${GFXIP}.amdgcn.bc
      -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/oclc_unsafe_math_off.amdgcn.bc
      -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/oclc_finite_only_off.amdgcn.bc")
  else()
    set(BITCODE_ARGS "--hip-device-lib-path=${BITCODE_DIR}")
  endif()

  separate_arguments(CLANG_ARG_LIST UNIX_COMMAND
    "-O2 -x cl -cl-denorms-are-zero -cl-std=CL2.0 -target amdgcn-amd-amdhsa
    -Xclang -finclude-default-header -mcpu=${TARGET_DEV} -m${XNACK_OPT}
    ${BITCODE_ARGS} -o ${OUTPUT_FILE} ${INPUT_FILE}")

  ## Add custom command to produce a code object file.
  ## This depends on the kernel source file & compiler.
  ## It does not pickup devicelib changes.  It is not clear
  ## how to do that after conversion to --rocm-path is done.
  add_custom_command(OUTPUT ${OUTPUT_FILE} COMMAND clang ${CLANG_ARG_LIST}
    DEPENDS ${INPUT_FILE} clang
    COMMENT "BUILDING bitcode for ${OUTPUT_FILE}..."
    VERBATIM)

if(${CMAKE_VERBOSE_MAKEFILE})
  message("      Kernel Source: " ${INPUT_FILE})
  message("     Kernel Bitcode: " ${OUTPUT_FILE})
endif()

endfunction(gen_kernel_bc)

##==========================================
## Find device code object name and forward to custom command
##==========================================
function(build_kernel BLIT_NAME TARG_DEV)

  list (FIND XNACK_DEVS ${TARG_DEV} XNACK_IDX)
  if (${XNACK_IDX} GREATER -1)
    set (XNACK_OPT "xnack")
  else()
    set (XNACK_OPT "no-xnack")
  endif()

  ##  generate kernel bitcodes
  set (CODE_OBJECT_FILE "${BLIT_NAME}_${TARG_DEV}")
  set (CL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/imageblit_kernels.cl)
  gen_kernel_bc(${TARG_DEV} ${XNACK_OPT} ${CL_FILE} ${CODE_OBJECT_FILE})

  ## Build a list of code object file names
  ## These will be target dependencies.
  set (HSACO_TARG_LIST ${HSACO_TARG_LIST} "${CODE_OBJECT_FILE}" PARENT_SCOPE)

endfunction(build_kernel)

##==========================================
## Build the kernel for a list of devices
##==========================================
function(build_kernel_for_devices BLIT_NAME)

  set(HSACO_TARG_LIST "")

  foreach(dev ${TARGET_DEVICES})
    if(${CMAKE_VERBOSE_MAKEFILE})
      message("\n  Generating: ${dev} ...")
    endif()
    build_kernel(${BLIT_NAME} ${dev})
  endforeach(dev)

  set(HSACO_TARG_LIST ${HSACO_TARG_LIST} PARENT_SCOPE)

endfunction(build_kernel_for_devices)

##==========================================
## Create BLIT Code Object blobs file
##==========================================
function(generate_blit_file BFILE)

  ## Add a custom command that generates opencl_blit_objects.cpp
  ## This depends on all the generated code object files and the C++ generator script.
  add_custom_command(OUTPUT ${BFILE}.cpp
                     COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/create_hsaco_ascii_file.sh ${CMAKE_CURRENT_BINARY_DIR}/${BFILE}.cpp
                     DEPENDS ${HSACO_TARG_LIST} create_hsaco_ascii_file.sh )

  ## Export a target that builds (and depends on) opencl_blit_objects.cpp
  add_custom_target( ${BFILE} DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${BFILE}.cpp )

endfunction(generate_blit_file)

build_kernel_for_devices("ocl_blit_object")
generate_blit_file("opencl_blit_objects")
