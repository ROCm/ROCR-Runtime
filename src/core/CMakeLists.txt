################################################################################
##
## The University of Illinois/NCSA
## Open Source License (NCSA)
##
## Copyright (c) 2014-2015, Advanced Micro Devices, Inc. All rights reserved.
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

cmake_minimum_required ( VERSION 2.8.0 )
## GCC 4.8 or higher compiler required.

if ( WIN32 )
    MESSAGE ( FATAL_ERROR "Windows build is not supported." )
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules")

## Process environment variables.
if ( DEFINED HSATHK_BUILD_TARGET_BITS AND "${HSATHK_BUILD_TARGET_BITS}" STREQUAL 32 )
    set ( ONLY64STR "" )
    set ( IS64BIT 0 )
else ()
    set ( ONLY64STR "64" )
    set ( IS64BIT 1 )
endif ()

if ( NOT EXISTS ${HSATHK_BUILD_INC_PATH}/hsakmt.h )
    MESSAGE ( FATAL_ERROR "Environment variable HSATHK_BUILD_INC_PATH is not set to point to the location where KFD Thunk header file hsakmt.h (and rest of the thunk headers) could be found." )
endif ()

if ( NOT EXISTS ${HSATHK_BUILD_LIB_PATH}/libhsakmt.so.1 )
    MESSAGE ( FATAL_ERROR "Environment variable HSATHK_BUILD_LIB_PATH is not set to point to the location where KFD Thunk library libhsakmt.so.1 could be found." )
endif ()

if ( EXISTS ${LIBSP3_BUILD_INC_PATH}/sp3.h )
  set ( LIBSP3_BUILD_INC_PATH ${LIBSP3_BUILD_INC_PATH} )
else ()
  set ( LIBSP3_BUILD_INC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../utils/sp3 )
endif ()

if ( EXISTS ${LIBSP3_BUILD_LIB_PATH}/libsp3.a )
  set ( LIBSP3_BUILD_LIB_PATH ${LIBSP3_BUILD_LIB_PATH} )
else ()
  set ( LIBSP3_BUILD_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../utils/sp3 )
endif ()

MESSAGE ( ------IS64BIT: ${IS64BIT} )
MESSAGE ( ------Compiler: ${CMAKE_CXX_COMPILER} )
MESSAGE ( ------Version: ${CMAKE_CXX_COMPILER_VERSION} )

## Set core runtime module name and project name.
set ( CORE_RUNTIME_NAME "hsa-runtime" )
set ( CORE_RUNTIME_COMPONENT "lib${CORE_RUNTIME_NAME}" )
set ( CORE_RUNTIME_TARGET "${CORE_RUNTIME_NAME}${ONLY64STR}" )
project ( ${CORE_RUNTIME_TARGET} )

## Verbose output.
set ( CMAKE_VERBOSE_MAKEFILE on )

## Compiler preproc definitions.
add_definitions ( -D__linux__ )
add_definitions ( -DHSA_EXPORT=1 )
add_definitions ( -DHSA_EXPORT_FINALIZER=1 )
add_definitions ( -DHSA_EXPORT_IMAGES=1 )

if ( DEFINED BUILDID )
    add_definitions ( -DROCR_BUILD_ID=${BUILDID})
else ( NOT DEFINED BUILDID)
    ## get date information based on UTC - full date
    execute_process(COMMAND date --utc +%F WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE ROCR_BUILD_TIME OUTPUT_STRIP_TRAILING_WHITESPACE)
    ## get commit information
    execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE ROCR_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
    ## check dirty tree status
    execute_process(COMMAND git diff --shortstat COMMAND wc -l WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE ROCR_DIRTY_TREE  OUTPUT_STRIP_TRAILING_WHITESPACE)
    set  ( BUILD_ID "${ROCR_BUILD_COMMIT}.${ROCR_BUILD_TIME}.${ROCR_DIRTY_TREE}")
    add_definitions ( -DROCR_BUILD_ID=${BUILD_ID})
endif ()


## ------------------------- Linux Compiler and Linker options -------------------------
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fexceptions -fno-rtti -fvisibility=hidden -Wno-error=sign-compare -Wno-sign-compare -Wno-write-strings -Wno-deprecated-declarations -Wno-conversion-null -fno-math-errno -fno-threadsafe-statics -fmerge-all-constants -fms-extensions -Wno-error=comment -Wno-comment -Wno-error=pointer-arith -Wno-pointer-arith -Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-function" )

set ( DRVDEF "${CMAKE_CURRENT_SOURCE_DIR}/hsacore.so.def" )

set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bdynamic -Wl,-z,noexecstack -Wl,--version-script=${DRVDEF}" )

set ( CMAKE_SKIP_BUILD_RPATH TRUE)

##  -------------------------  End Compiler and Linker options ----------------------------

## Source files.
set ( CORE_SRCS util/lnx/os_linux.cpp )
set ( CORE_SRCS ${CORE_SRCS} util/small_heap.cpp )
set ( CORE_SRCS ${CORE_SRCS} util/timer.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/amd_blit_kernel.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/amd_blit_sdma.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/amd_cpu_agent.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/amd_gpu_agent.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/amd_aql_queue.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/amd_loader_context.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/hsa_ven_amd_loaded_code_object.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/amd_memory_region.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/amd_topology.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/default_signal.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/host_queue.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/hsa.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/hsa_api_trace.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/hsa_ext_amd.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/hsa_ext_interface.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/interrupt_signal.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/isa.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/runtime.cpp )
set ( CORE_SRCS ${CORE_SRCS} runtime/signal.cpp )
set ( CORE_SRCS ${CORE_SRCS} common/shared.cpp )
set ( CORE_SRCS ${CORE_SRCS} common/hsa_table_interface.cpp )

## Include path(s).
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/.. )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/../inc )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/inc )
include_directories ( ${HSATHK_BUILD_INC_PATH} )
include_directories ( ${LIBSP3_BUILD_INC_PATH} )

## Library path(s).
link_directories ( ${HSATHK_BUILD_LIB_PATH} )
link_directories ( ${LIBSP3_BUILD_LIB_PATH} )

add_library ( ${CORE_RUNTIME_TARGET} SHARED ${CORE_SRCS} )

## Set the VERSION and SOVERSION values
if ( DEFINED VERSION_STRING )
    set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY VERSION "${VERSION_STRING}" )
endif ()

set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY SOVERSION "${VERSION_MAJOR}" )

target_link_libraries ( ${CORE_RUNTIME_TARGET}
  PRIVATE amdhsaloader
  PRIVATE amdhsacode
  PRIVATE hsakmt
  PRIVATE sp3
  dl pthread rt
)

## If the build is Release, strip the target library
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    add_custom_command ( TARGET ${CORE_RUNTIME_TARGET} POST_BUILD COMMAND ${CMAKE_STRIP} *.so )
endif ()

## Set install information
install ( TARGETS ${CORE_RUNTIME_TARGET} LIBRARY DESTINATION lib COMPONENT ${CORE_RUNTIME_COMPONENT})
