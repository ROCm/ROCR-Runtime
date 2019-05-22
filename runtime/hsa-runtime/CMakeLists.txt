################################################################################
##
## The University of Illinois/NCSA
## Open Source License (NCSA)
##
## Copyright (c) 2014-2017, Advanced Micro Devices, Inc. All rights reserved.
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

## Verbose output.
set ( CMAKE_VERBOSE_MAKEFILE on )

## Set core runtime module name and project name.
set ( CORE_RUNTIME_NAME "hsa-runtime" )
set ( CORE_RUNTIME_TARGET "${CORE_RUNTIME_NAME}64" )
set ( CORE_RUNTIME_LIBRARY "lib${CORE_RUNTIME_TARGET}" )
project( ${CORE_RUNTIME_TARGET} )

list ( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules" )
include ( utils )
include ( hsa_common )

## Find LibElf
find_package(LibElf REQUIRED)

## Check for memfd_create syscall
include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS ( "__NR_memfd_create" "sys/syscall.h" HAVE_MEMFD_CREATE )

## Compiler preproc definitions.
add_definitions ( -D__linux__  )
add_definitions ( -DHSA_EXPORT=1 )
add_definitions ( -DHSA_EXPORT_FINALIZER=1 )
add_definitions ( -DHSA_EXPORT_IMAGES=1 )
add_definitions ( -D HSA_DEPRECATED= )
if ( HAVE_MEMFD_CREATE )
    add_definitions ( -DHAVE_MEMFD_CREATE )
endif()

## Get the package version.
get_version ( "1.1.9" )

set ( BUILD_VERSION_MAJOR ${VERSION_MAJOR} )
set ( BUILD_VERSION_MINOR ${VERSION_MINOR} )
set ( BUILD_VERSION_PATCH ${VERSION_PATCH} )
set ( LIB_VERSION_STRING "${BUILD_VERSION_MAJOR}.${BUILD_VERSION_MINOR}.${BUILD_VERSION_PATCH}" )

if ( VERSION_BUILD )
    set ( BUILD_VERSION_PATCH "${BUILD_VERSION_PATCH}-${VERSION_BUILD}" )
endif ()
set ( BUILD_VERSION_STRING "${BUILD_VERSION_MAJOR}.${BUILD_VERSION_MINOR}.${BUILD_VERSION_PATCH}" )

## Find the hsakmt library and include files, use directory hint from cache
find_file ( HSAKMT_INC NAMES "hsakmt.h" "libhsakmt/hsakmt.h" PATHS ${HSAKMT_INC_PATH} )
find_library ( HSAKMT_LIB "libhsakmt.so" ${HSAKMT_LIB_PATH} )
get_filename_component ( HSAKMT_INC_PATH ${HSAKMT_INC} DIRECTORY CACHE )
get_filename_component ( HSAKMT_LIB_PATH ${HSAKMT_LIB} DIRECTORY CACHE )
unset( HSAKMT_INC CACHE )
unset( HSAKMT_LIB CACHE )

## Set include directories for ROCr runtime
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/inc )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/core/inc )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/libamdhsacode )

## Set include and link directories for libhsakmt
include_directories ( ${HSAKMT_INC_PATH} )
link_directories ( ${HSAKMT_LIB_PATH} )

## ROCr build internal versioning
if ( VERSION_BUILD )
    add_definitions ( -DROCR_BUILD_ID=${BUILD_VERSION_STRING} )
else ()
    add_definitions ( -DROCR_BUILD_ID="${BUILD_VERSION_STRING}-unknown" )
endif ()

## ------------------------- Linux Compiler and Linker options -------------------------
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fexceptions -fno-rtti -fvisibility=hidden -Wno-error=sign-compare -Wno-sign-compare -Wno-write-strings -Wno-conversion-null -fno-math-errno -fno-threadsafe-statics -fmerge-all-constants -fms-extensions -Wno-error=comment -Wno-comment -Wno-error=pointer-arith -Wno-pointer-arith -Wno-error=unused-variable -Wno-error=unused-function" )
if ( CMAKE_COMPILER_IS_GNUCXX )
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-but-set-variable")
endif ()

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
            "core/runtime/intercept_queue.cpp"
            "core/runtime/ipc_signal.cpp"
            "core/runtime/isa.cpp"
            "core/runtime/runtime.cpp"
            "core/runtime/signal.cpp"
            "core/runtime/queue.cpp"
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

## Set the VERSION and SOVERSION values
set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY VERSION "${LIB_VERSION_STRING}" )
set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY SOVERSION "${BUILD_VERSION_MAJOR}" )

target_link_libraries ( ${CORE_RUNTIME_TARGET}
  PRIVATE hsakmt
  elf dl pthread rt
)

## Strip should be optional or not at all
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
#    add_custom_command ( TARGET ${CORE_RUNTIME_TARGET} POST_BUILD COMMAND ${CMAKE_STRIP} *.so )
endif ()

## Create symlinks for packaging and install
add_custom_target ( hsa-link ALL WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/include/hsa hsa-link )
add_custom_target ( ${CORE_RUNTIME_TARGET}.so-link ALL WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/lib/${CORE_RUNTIME_LIBRARY}.so ${CORE_RUNTIME_LIBRARY}.so-link )

## Set install information
install ( TARGETS ${CORE_RUNTIME_TARGET} LIBRARY DESTINATION hsa/lib )
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/inc/ DESTINATION hsa/include/hsa )
install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/hsa-link DESTINATION include PERMISSIONS OWNER_WRITE OWNER_READ RENAME hsa )
install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/${CORE_RUNTIME_LIBRARY}.so-link DESTINATION lib PERMISSIONS OWNER_WRITE OWNER_READ RENAME ${CORE_RUNTIME_LIBRARY}.so )

## Packaging directives
set ( CPACK_PACKAGE_NAME "hsa-rocr-dev" )
set ( CPACK_PACKAGE_VENDOR "AMD" )
set ( CPACK_PACKAGE_VERSION_MAJOR ${BUILD_VERSION_MAJOR} )
set ( CPACK_PACKAGE_VERSION_MINOR ${BUILD_VERSION_MINOR} )
set ( CPACK_PACKAGE_VERSION_PATCH ${BUILD_VERSION_PATCH} )
set ( CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc." )
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "AMD Heterogeneous System Architecture HSA - Linux HSA Runtime for Boltzmann (ROCm) platforms" )
set ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md" )

# Debian package specific variables
set ( CPACK_DEBIAN_PACKAGE_DEPENDS "hsakmt-roct-dev" )
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCR-Runtime" )
set ( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/postinst;${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/prerm" )

## RPM package specific variables
set ( CPACK_RPM_PACKAGE_DEPENDS "hsakmt-roct-dev" )
set ( CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_post" )
set ( CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_postun" )

## Include packaging
include ( CPack )
