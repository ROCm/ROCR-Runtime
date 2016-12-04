################################################################################
##
## The University of Illinois/NCSA
## Open Source License (NCSA)
##
## Copyright (c) 2014-2016, Advanced Micro Devices, Inc. All rights reserved.
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

## Verbose output.
set ( CMAKE_VERBOSE_MAKEFILE on )
## Set core runtime module name and project name.
set ( CORE_RUNTIME_NAME "hsa-runtime" )
set ( CORE_RUNTIME_COMPONENT "lib${CORE_RUNTIME_NAME}" )
set ( CORE_RUNTIME_TARGET "${CORE_RUNTIME_NAME}64" )
project( ${CORE_RUNTIME_TARGET} )

list ( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules" )

## Find LibElf
find_package(LibElf REQUIRED)

## Compiler preproc definitions.
add_definitions ( -D__linux__  )
add_definitions ( -DHSA_EXPORT=1 )
add_definitions ( -DHSA_EXPORT_FINALIZER=1 )
add_definitions ( -DHSA_EXPORT_IMAGES=1 )
add_definitions ( -D HSA_DEPRECATED= )

include ( utils )
include ( hsa_common )
include ( EnableCompilerWarnings )
include ( ClangTidy )

## Setup clang tidy
enable_clang_tidy(
    CHECKS 
        *
        -cert-dcl03-c
        -cert-err60-cpp
        -clang-analyzer-alpha.core.CastToStruct
        # Disable this because it doesn't recognize clean up in the destructor
        -clang-analyzer-alpha.unix.Stream
        -clang-analyzer-optin.performance.Padding
        -clang-diagnostic-extern-c-compat
        -cppcoreguidelines-pro-bounds-array-to-pointer-decay
        -cppcoreguidelines-pro-bounds-constant-array-index
        -cppcoreguidelines-pro-bounds-pointer-arithmetic
        -cppcoreguidelines-pro-type-reinterpret-cast
        -cppcoreguidelines-pro-type-static-cast-downcast # Need rtti
        -cppcoreguidelines-pro-type-union-access # Need C++17 or boost
        -cppcoreguidelines-pro-type-vararg
        -google-explicit-constructor
        -google-readability-braces-around-statements
        -google-readability-namespace-comments
        -google-readability-todo
        -google-runtime-int
        -google-runtime-references
        -llvm-include-order
        -llvm-namespace-comment
        -misc-macro-parentheses
        -misc-misplaced-const
        -modernize-loop-convert
        -modernize-make-unique # Need C++14
        -modernize-pass-by-value
        -modernize-use-auto
        -performance-unnecessary-value-param
        -readability-braces-around-statements
        -readability-else-after-return
        -readability-implicit-bool-cast
        -readability-named-parameter
    ERRORS
        *
        -readability-inconsistent-declaration-parameter-name
    HEADER_FILTER
        "hpp$"
    ANALYZE_TEMPORARY_DTORS ON

)

## Parse the package version
if( NOT DEFINED VERSION_STRING )
    set ( VERSION_STRING "1.1.0" )
endif()

## Parse the package version
parse_version ( ${VERSION_STRING} )

set ( PACKAGE_VERSION_MAJOR ${VERSION_MAJOR} )
set ( PACKAGE_VERSION_MINOR ${VERSION_MINOR} )
set ( PACKAGE_VERSION_PATCH ${VERSION_PATCH} )

## Parse the library version
if( NOT DEFINED RUNTIME_VERSION_STRING )
    set ( RUNTIME_VERSION_STRING "1" )
endif()

parse_version ( ${RUNTIME_VERSION_STRING} )
set ( RUNTIME_VERSION_MAJOR ${VERSION_MAJOR} )
set ( RUNTIME_VERSION_MINOR ${VERSION_MINOR} )
set ( RUNTIME_VERSION_PATCH ${VERSION_PATCH} )

include_directories ( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/inc )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/core/inc )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/libamdhsacode )
include_directories ( ${HSAKMT_BUILD_INC_PATH})

link_directories ( ${HSAKMT_BUILD_LIB_PATH} )

if ( DEFINED BUILDID )
    add_definitions ( -DROCR_BUILD_ID=${BUILDID} )
else ( NOT DEFINED BUILDID )
    ## get date information based on UTC - full date
    execute_process(COMMAND date --utc +%F WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE ROCR_BUILD_TIME OUTPUT_STRIP_TRAILING_WHITESPACE )
    ## get commit information
    execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE ROCR_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE )
    ## check dirty tree status
    execute_process(COMMAND git diff --shortstat COMMAND wc -l WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE ROCR_DIRTY_TREE  OUTPUT_STRIP_TRAILING_WHITESPACE )
    set  ( BUILD_ID "${ROCR_BUILD_COMMIT}.${ROCR_BUILD_TIME}.${ROCR_DIRTY_TREE}" )
    add_definitions ( -DROCR_BUILD_ID=${BUILD_ID} )
endif ()

## ------------------------- Linux Compiler and Linker options -------------------------
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -fno-rtti -fvisibility=hidden -Wno-error=sign-compare -fno-math-errno -fno-threadsafe-statics -fmerge-all-constants -fms-extensions" )

set ( DRVDEF "${CMAKE_CURRENT_SOURCE_DIR}/hsacore.so.def" )

set ( LNKSCR "${CMAKE_CURRENT_SOURCE_DIR}/hsacore.so.link" )

set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bdynamic -Wl,-z,noexecstack -Wl,${LNKSCR} -Wl,--version-script=${DRVDEF}" )

set ( CMAKE_SKIP_BUILD_RPATH TRUE )

##  -------------------------  End Compiler and Linker options ----------------------------

## Source files.
set ( SRCS "core/util/lnx/os_linux.cpp"
            "core/util/small_heap.cpp"
            "core/util/timer.cpp"
            "core/runtime/amd_blit_kernel.cpp"
            "core/runtime/amd_blit_sdma.cpp"
            "core/runtime/amd_cpu_agent.cpp"
            "core/runtime/amd_gpu_agent.cpp"
            "core/runtime/amd_aql_queue.cpp"
            "core/runtime/amd_loader_context.cpp"
            "core/runtime/hsa_ven_amd_loader.cpp"
            "core/runtime/amd_memory_region.cpp"
            "core/runtime/amd_topology.cpp"
            "core/runtime/default_signal.cpp"
            "core/runtime/host_queue.cpp"
            "core/runtime/hsa.cpp"
            "core/runtime/hsa_api_trace.cpp"
            "core/runtime/hsa_ext_amd.cpp"
            "core/runtime/hsa_ext_interface.cpp"
            "core/runtime/interrupt_signal.cpp"
            "core/runtime/isa.cpp"
            "core/runtime/runtime.cpp"
            "core/runtime/signal.cpp"
            "core/runtime/cache.cpp"
            "core/common/shared.cpp"
            "core/common/hsa_table_interface.cpp"
            "loader/executable.cpp"
            "loader/loaders.cpp"
            "libamdhsacode/amd_elf_image.cpp"
            "libamdhsacode/amd_hsa_code_util.cpp"
            "libamdhsacode/amd_hsa_locks.cpp"
            "libamdhsacode/amd_options.cpp"
            "libamdhsacode/amd_hsa_code.cpp" 
)

add_library( ${CORE_RUNTIME_TARGET} SHARED ${SRCS} )
clang_tidy_check(${CORE_RUNTIME_TARGET})
link_directories ( ${HSAKMT_BUILD_LIB_PATH} )

## Set the VERSION and SOVERSION values
if ( DEFINED VERSION_STRING )
    set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY VERSION "${RUNTIME_VERSION_STRING}" )
endif ()

set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY SOVERSION "${RUNTIME_VERSION_MAJOR}" )

target_link_libraries ( ${CORE_RUNTIME_TARGET}
  PRIVATE hsakmt
  elf dl pthread rt
)

## If the build is Release, strip the target library
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    add_custom_command ( TARGET ${CORE_RUNTIME_TARGET} POST_BUILD COMMAND ${CMAKE_STRIP} *.so )
endif ()

## Set install information
install ( TARGETS ${CORE_RUNTIME_TARGET} LIBRARY DESTINATION lib COMPONENT ${CORE_RUNTIME_COMPONENT} )
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/inc/ DESTINATION include/hsa )

## Packaging directives
set ( CPACK_PACKAGE_NAME "hsa-rocr-dev" )
set ( CPACK_PACKAGE_VENDOR "AMD" )
set ( CPACK_PACKAGE_VERSION_MAJOR ${PACKAGE_VERSION_MAJOR} )
set ( CPACK_PACKAGE_VERSION_MINOR ${PACKAGE_VERSION_MINOR} )
set ( CPACK_PACKAGE_VERSION_PATCH ${PACKAGE_VERSION_PATCH} )
set ( CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc." )
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "AMD Heterogeneous System Architecture HSA - Linux HSA Runtime for Boltzmann (ROCm) platforms" )
set ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md" )

## Set dependency strings
set ( HSAKMT_ROCT_DEV_DEPS "" )
if ( DEFINED HSAKMT_VERSION_STRING )
    set ( HSAKMT_ROCT_DEV_DEPS " (=${HSAKMT_VERSION_STRING})" )
endif ()

# Debian package specific variables
set ( CPACK_DEBIAN_PACKAGE_DEPENDS "hsakmt-roct-dev${HSAKMT_ROCT_DEV_DEPS}" )
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCR-Runtime" )
set ( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/postinst;${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/prerm" )

## RPM package specific variables
set ( CPACK_RPM_PACKAGE_DEPENDS "hsakmt-roct-dev${HSAKMT_ROCT_DEV_DEPS}" )
set ( CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_post" )
set ( CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_postun" )

## Include packaging
include ( CPack )
