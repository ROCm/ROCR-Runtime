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

## Set core runtime module name and project name.
set ( CORE_RUNTIME_NAME "hsa-runtime" )
set ( CORE_RUNTIME_TARGET "${CORE_RUNTIME_NAME}64" )
set ( CORE_RUNTIME_LIBRARY "lib${CORE_RUNTIME_TARGET}" )
project( ${CORE_RUNTIME_TARGET} )

if ( NOT DEFINED BUILD_SHARED_LIBS )
  set ( BUILD_SHARED_LIBS "on" )
endif()
set ( BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS} CACHE BOOL "Build shared library (.so) or not.")

# Optionally, build HSA Runtime with ccache.
set(ROCM_CCACHE_BUILD OFF CACHE BOOL "Set to ON for a ccache enabled build")
if (ROCM_CCACHE_BUILD)
  find_program(CCACHE_PROGRAM ccache)
  if (CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PROGRAM})
  else()
    message(WARNING "Unable to find ccache. Falling back to real compiler")
  endif() # if (CCACHE_PROGRAM)
endif() # if (ROCM_CCACHE_BUILD)

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
set (SO_MAJOR 1)
set (SO_MINOR 1)
if ( ${ROCM_PATCH_VERSION} )
    set ( SO_PATCH ${ROCM_PATCH_VERSION})
    set ( VERSION_PATCH ${ROCM_PATCH_VERSION})
else ()
    set(SO_PATCH 9)
endif ()

set ( SO_VERSION_STRING "${SO_MAJOR}.${SO_MINOR}.${SO_PATCH}" )
set ( PACKAGE_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_COMMIT_COUNT}-${VERSION_JOB}-${VERSION_HASH}" )

## Find the hsakmt library and include files, use directory hint from cache
## Search relative to build directory, relative to source directory, and finally the rocm install default (/opt/rocm)
get_include_path( HSAKMT_INC_PATH "libhsakmt include path" NAMES "hsakmt.h" "libhsakmt/hsakmt.h" HINTS "${CMAKE_BINARY_DIR}/../../include" "${CMAKE_CURRENT_SOURCE_DIR}/../../../../libhsakmt/include" PATHS "/opt/rocm/include")
get_library_path( HSAKMT_LIB_PATH "libhsakmt library path" NAMES "libhsakmt.so" HINTS "${CMAKE_BINARY_DIR}/../../lib" "${CMAKE_BINARY_DIR}/../roct" PATHS "/opt/rocm/lib")
include_directories ( ${HSAKMT_INC_PATH} )
link_directories ( ${HSAKMT_LIB_PATH} )

## Set include directories for ROCr runtime
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/libamdhsacode )

## ROCr build internal versioning
add_definitions ( -DROCR_BUILD_ID=${PACKAGE_VERSION_STRING} )

## Set RUNPATH - ../../lib covers use of the legacy symlink in /hsa/lib/
set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/../../lib;$ORIGIN/../../lib64;$ORIGIN/../lib64")

## ------------------------- Linux Compiler and Linker options -------------------------
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fexceptions -fno-rtti -fvisibility=hidden -Wno-error=sign-compare -Wno-sign-compare -Wno-write-strings -Wno-conversion-null -fno-math-errno -fno-threadsafe-statics -fmerge-all-constants -fms-extensions -Wno-error=comment -Wno-comment -Wno-error=pointer-arith -Wno-pointer-arith -Wno-error=unused-variable -Wno-error=unused-function" )
if ( CMAKE_COMPILER_IS_GNUCXX )
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-but-set-variable")
endif ()

set ( DRVDEF "${CMAKE_CURRENT_SOURCE_DIR}/hsacore.so.def" )

set ( LNKSCR "${CMAKE_CURRENT_SOURCE_DIR}/hsacore.so.link" )

set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bdynamic -Wl,-z,noexecstack -Wl,${LNKSCR} -Wl,--version-script=${DRVDEF} -Wl,--enable-new-dtags" )

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
            "core/runtime/amd_filter_device.cpp"
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

add_library( ${CORE_RUNTIME_TARGET} ${SRCS} )

target_link_libraries ( ${CORE_RUNTIME_TARGET}
  PRIVATE hsakmt
  elf dl pthread rt
)

## Strip should be optional or not at all
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
#    add_custom_command ( TARGET ${CORE_RUNTIME_TARGET} POST_BUILD COMMAND ${CMAKE_STRIP} $<TARGET_FILE_NAME:${CORE_RUNTIME_TARGET}> )
endif ()

## Set the VERSION and SOVERSION values
set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY VERSION "${SO_VERSION_STRING}" )
set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY SOVERSION "${SO_MAJOR}" )

## Create symlinks for legacy packaging and install
add_custom_target ( hsa_include_link ALL WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/include/hsa hsa_include_link )
if ( ${BUILD_SHARED_LIBS} )
    add_custom_target ( hsa_lib_link ALL WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/lib/${CORE_RUNTIME_LIBRARY}.so ${CORE_RUNTIME_LIBRARY}-link.so )
    add_custom_target ( hsa_lib_link2 ALL WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/lib/${CORE_RUNTIME_LIBRARY}.so.${SO_MAJOR} ${CORE_RUNTIME_LIBRARY}-link.so.${SO_MAJOR} )
endif()

## Set install information
install ( TARGETS ${CORE_RUNTIME_TARGET} DESTINATION hsa/lib COMPONENT binary)
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/inc/ DESTINATION hsa/include/hsa COMPONENT binary)
install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/hsa_include_link DESTINATION include PERMISSIONS OWNER_WRITE OWNER_READ RENAME hsa COMPONENT dirlink)

if ( ${BUILD_SHARED_LIBS} )
    install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/${CORE_RUNTIME_LIBRARY}-link.so DESTINATION lib PERMISSIONS OWNER_WRITE OWNER_READ RENAME ${CORE_RUNTIME_LIBRARY}.so COMPONENT binary)
    install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/${CORE_RUNTIME_LIBRARY}-link.so.${SO_MAJOR} DESTINATION lib PERMISSIONS OWNER_WRITE OWNER_READ RENAME ${CORE_RUNTIME_LIBRARY}.so.${SO_MAJOR} COMPONENT binary)
endif ()

## Packaging directives
set ( CPACK_GENERATOR "DEB;RPM" CACHE STRING "Package types to build")

## Only pack the "binary" components, post install script will add the directory link.
set (CPACK_DEB_COMPONENT_INSTALL ON)
set (CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
set (CPACK_COMPONENTS_ALL binary)

set ( CPACK_PACKAGE_NAME "hsa-rocr-dev" )
set ( CPACK_PACKAGE_VENDOR "AMD" )
set ( CPACK_PACKAGE_VERSION ${PACKAGE_VERSION_STRING} )
set ( CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc." )
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "AMD Heterogeneous System Architecture HSA - Linux HSA Runtime for Boltzmann (ROCm) platforms" )
set ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md" )

## Process the install scripts to update the CPACK variables
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/post_install ${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/postinst @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/pre_remove ${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/prerm @ONLY)

# Debian package specific variables
set ( CPACK_DEBIAN_PACKAGE_DEPENDS "hsakmt-roct" )
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCR-Runtime" )
set ( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/postinst;${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/prerm" )

## RPM package specific variables
set ( CPACK_RPM_PACKAGE_DEPENDS "hsakmt-roct" )
set ( CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_post" )
set ( CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_postun" )

## Include packaging
include ( CPack )
