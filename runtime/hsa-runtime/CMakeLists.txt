################################################################################
##
## The University of Illinois/NCSA
## Open Source License (NCSA)
##
## Copyright (c) 2014-2021, Advanced Micro Devices, Inc. All rights reserved.
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
## and/or sell copies of the Software, and to permit persons to whom the
## Software is furnished to do so, subject to the following conditions:
##
##  - Redistributions of source code must retain the above copyright notice,
##    this list of conditions and the following disclaimers.
##  - Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimers in
##    the documentation and/or other materials provided with the distribution.
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

cmake_minimum_required ( VERSION 3.7 )

## Clear target dependency data.
## Needed to allow UI transitions between static and dynamic builds.
## Need an update to CMake 3.12 to remove this hack. See CMake policy change CMP0073.
unset ( hsa-runtime64_LIB_DEPENDS CACHE )

## Set core runtime module name and project name.
set ( CORE_RUNTIME_NAME "hsa-runtime64" )
set ( CORE_RUNTIME_TARGET "${CORE_RUNTIME_NAME}" )
set ( CORE_RUNTIME_LIBRARY "lib${CORE_RUNTIME_TARGET}" )

## Set project name
project( ${CORE_RUNTIME_TARGET} )

## Utilty functions
list ( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules" )
include ( utils )
include ( hsa_common )
include ( GNUInstallDirs )

## Expose static library option
if ( NOT DEFINED BUILD_SHARED_LIBS )
  set ( BUILD_SHARED_LIBS ON )
endif()
set ( BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS} CACHE BOOL "Build shared library (.so) or not.")

## Adjust target name for static builds
## Original name will be an interface target that adds --whole-archive linker options around the target.
if( NOT ${BUILD_SHARED_LIBS} )
  set ( CORE_RUNTIME_TARGET "${CORE_RUNTIME_TARGET}_static" )
endif()

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

## Get version strings
get_version ( "1.11.0" )
if ( ${ROCM_PATCH_VERSION} )
  set ( VERSION_PATCH ${ROCM_PATCH_VERSION})
endif()
set ( SO_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" )
set ( PACKAGE_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_COMMIT_COUNT}" )

## Find external dependencies.
find_package(PkgConfig)
find_package(LibElf REQUIRED)
find_package(hsakmt 1.0 REQUIRED HINTS ${CMAKE_INSTALL_PREFIX} PATHS /opt/rocm)
pkg_check_modules(drm REQUIRED IMPORTED_TARGET libdrm)

## Create the rocr target.
add_library( ${CORE_RUNTIME_TARGET} "" )

## Enforce uniform output file naming.
set_property(TARGET  ${CORE_RUNTIME_TARGET} PROPERTY OUTPUT_NAME ${CORE_RUNTIME_NAME} )

## Compiler preproc definitions.
target_compile_definitions(${CORE_RUNTIME_TARGET} PRIVATE "${HSA_COMMON_DEFS}" __linux__ HSA_EXPORT=1 HSA_EXPORT_FINALIZER=1 HSA_EXPORT_IMAGES=1 HSA_DEPRECATED=
ROCR_BUILD_ID="${PACKAGE_VERSION_STRING}-${VERSION_JOB}-${VERSION_HASH}" )

## Check for memfd_create syscall
include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS ( "__NR_memfd_create" "sys/syscall.h" HAVE_MEMFD_CREATE )
if ( HAVE_MEMFD_CREATE )
  target_compile_definitions(${CORE_RUNTIME_TARGET} PRIVATE HAVE_MEMFD_CREATE )
endif()

## Set include directories for ROCr runtime
target_include_directories( ${CORE_RUNTIME_TARGET}
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/libamdhsacode
  ${CMAKE_CURRENT_BINARY_DIR}/core/runtime/trap_handler)


## ------------------------- Linux Compiler and Linker options -------------------------
set ( HSA_CXX_FLAGS ${HSA_COMMON_CXX_FLAGS} -fexceptions -fno-rtti -fvisibility=hidden -Wno-error=missing-braces -Wno-error=sign-compare -Wno-sign-compare -Wno-write-strings -Wno-conversion-null -fno-math-errno -fno-threadsafe-statics -fmerge-all-constants -fms-extensions -Wno-error=comment -Wno-comment -Wno-error=pointer-arith -Wno-pointer-arith -Wno-error=unused-variable -Wno-error=unused-function )

## Extra x86 specific settings
if ( CMAKE_SYSTEM_PROCESSOR MATCHES "i?86|x86_64|amd64|AMD64" )
  set ( HSA_CXX_FLAGS ${HSA_CXX_FLAGS} -mmwaitx )
endif()

## Extra image settings - audit!
set ( HSA_CXX_FLAGS ${HSA_CXX_FLAGS} -Wno-deprecated-declarations )

if ( CMAKE_COMPILER_IS_GNUCXX )
    set ( HSA_CXX_FLAGS ${HSA_CXX_FLAGS} -Wno-error=maybe-uninitialized -Wno-error=unused-but-set-variable)
endif ()
if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set ( HSA_CXX_FLAGS ${HSA_CXX_FLAGS} -Wno-error=self-assign)
  if( ${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL 13)
    set ( HSA_CXX_FLAGS ${HSA_CXX_FLAGS} -Wno-error=unused-but-set-variable)
  endif()
endif()

set ( DRVDEF "${CMAKE_CURRENT_SOURCE_DIR}/hsacore.so.def" )
set ( LNKSCR "hsacore.so.link" )
set ( HSA_SHARED_LINK_FLAGS "-Wl,-Bdynamic -Wl,-z,noexecstack -Wl,${CMAKE_CURRENT_SOURCE_DIR}/${LNKSCR} -Wl,--version-script=${DRVDEF} -Wl,--enable-new-dtags" )

target_compile_options(${CORE_RUNTIME_TARGET} PRIVATE ${HSA_CXX_FLAGS})
#target_link_options not available prior to CMake 3.13
set_property(TARGET ${CORE_RUNTIME_TARGET} PROPERTY LINK_FLAGS ${HSA_SHARED_LINK_FLAGS})
##  -------------------------  End Compiler and Linker options ----------------------------

## Source files.
set ( SRCS core/util/lnx/os_linux.cpp
           core/util/small_heap.cpp
           core/util/timer.cpp
           core/util/flag.cpp
           core/runtime/amd_blit_kernel.cpp
           core/runtime/amd_blit_sdma.cpp
           core/runtime/amd_cpu_agent.cpp
           core/runtime/amd_gpu_agent.cpp
           core/runtime/amd_hsa_loader.cpp
           core/runtime/amd_aql_queue.cpp
           core/runtime/amd_loader_context.cpp
           core/runtime/hsa_ven_amd_loader.cpp
           core/runtime/amd_memory_region.cpp
           core/runtime/amd_filter_device.cpp
           core/runtime/amd_topology.cpp
           core/runtime/default_signal.cpp
           core/runtime/host_queue.cpp
           core/runtime/hsa.cpp
           core/runtime/hsa_api_trace.cpp
           core/runtime/hsa_ext_amd.cpp
           core/runtime/hsa_ext_interface.cpp
           core/runtime/interrupt_signal.cpp
           core/runtime/intercept_queue.cpp
           core/runtime/ipc_signal.cpp
           core/runtime/isa.cpp
           core/runtime/runtime.cpp
           core/runtime/signal.cpp
           core/runtime/queue.cpp
           core/runtime/cache.cpp
           core/runtime/svm_profiler.cpp
           core/common/shared.cpp
           core/common/hsa_table_interface.cpp
           loader/executable.cpp
           libamdhsacode/amd_elf_image.cpp
           libamdhsacode/amd_hsa_code_util.cpp
           libamdhsacode/amd_hsa_locks.cpp
           libamdhsacode/amd_options.cpp
           libamdhsacode/amd_hsa_code.cpp )

target_sources( ${CORE_RUNTIME_TARGET} PRIVATE ${SRCS} )

## Depend on trap handler target.
add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/core/runtime/trap_handler )
add_dependencies( ${CORE_RUNTIME_TARGET} amd_trap_handler_v2 )

if ( NOT DEFINED IMAGE_SUPPORT AND CMAKE_SYSTEM_PROCESSOR MATCHES "i?86|x86_64|amd64|AMD64" )
  set ( IMAGE_SUPPORT ON )
endif()
set ( IMAGE_SUPPORT ${IMAGE_SUPPORT} CACHE BOOL "Build with image support (default: ON for x86, OFF elsewise)." )

## Optional image module defintions.
if(${IMAGE_SUPPORT})
  ## Image definitons - audit!
  target_compile_definitions(${CORE_RUNTIME_TARGET} PRIVATE
    HSA_IMAGE_SUPPORT
    UNIX_OS
    LINUX
    AMD_INTERNAL_BUILD
    BRAHMA_BUILD=1 )

  set ( IMAGE_SRCS image/addrlib/src/addrinterface.cpp
                   image/addrlib/src/core/coord.cpp
                   image/addrlib/src/core/addrlib.cpp
                   image/addrlib/src/core/addrlib1.cpp
                   image/addrlib/src/core/addrlib2.cpp
                   image/addrlib/src/core/addrobject.cpp
                   image/addrlib/src/core/addrelemlib.cpp
                   image/addrlib/src/r800/ciaddrlib.cpp
                   image/addrlib/src/r800/egbaddrlib.cpp
                   image/addrlib/src/r800/siaddrlib.cpp
                   image/addrlib/src/gfx9/gfx9addrlib.cpp
                   image/addrlib/src/gfx10/gfx10addrlib.cpp
                   image/addrlib/src/gfx11/gfx11addrlib.cpp
                   image/device_info.cpp
                   image/hsa_ext_image.cpp
                   image/image_runtime.cpp
                   image/image_manager.cpp
                   image/image_manager_kv.cpp
                   image/image_manager_ai.cpp
                   image/image_manager_nv.cpp
                   image/image_manager_gfx11.cpp
                   image/image_lut_kv.cpp
                   image/image_lut_gfx11.cpp
                   image/blit_object_gfx7xx.cpp
                   image/blit_object_gfx8xx.cpp
                   image/blit_object_gfx9xx.cpp
                   image/blit_kernel.cpp
                   ${CMAKE_CURRENT_BINARY_DIR}/image/blit_src/opencl_blit_objects.cpp )

  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/image/blit_src/opencl_blit_objects.cpp PROPERTIES GENERATED TRUE)

  target_include_directories( ${CORE_RUNTIME_TARGET}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/image
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src/r800
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src/gfx9
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src/gfx10
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src/gfx11
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src/chip/r800
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src/chip/gfx9
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src/chip/gfx10
    ${CMAKE_CURRENT_SOURCE_DIR}/image/addrlib/src/chip/gfx11 )

  target_sources( ${CORE_RUNTIME_TARGET} PRIVATE ${IMAGE_SRCS} )

  ## Depend on blit kernel target.
  add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/image/blit_src )
  add_dependencies( ${CORE_RUNTIME_TARGET} opencl_blit_objects )

endif()

## Link dependencies.
target_link_libraries ( ${CORE_RUNTIME_TARGET} PRIVATE hsakmt::hsakmt PkgConfig::drm)
target_link_libraries ( ${CORE_RUNTIME_TARGET} PRIVATE elf::elf dl pthread rt )

## Set the VERSION and SOVERSION values
set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY VERSION "${SO_VERSION_STRING}" )
set_property ( TARGET ${CORE_RUNTIME_TARGET} PROPERTY SOVERSION "${VERSION_MAJOR}" )

## Add the public interface export target if doing a static build.
## Bind ROCr dependencies to the interface target rather than to the source build
## target so that -Wl,--whole-archive is tightly applied.  Requires binding
## indirectly to the source build taret.
if( NOT ${BUILD_SHARED_LIBS} )
  add_library(${CORE_RUNTIME_NAME} INTERFACE)

  ## Bind to source build target interface but not its link requirements.
  target_include_directories( ${CORE_RUNTIME_NAME} INTERFACE $<TARGET_PROPERTY:${CORE_RUNTIME_NAME}::${CORE_RUNTIME_TARGET},INTERFACE_INCLUDE_DIRECTORIES> )
  target_link_libraries ( ${CORE_RUNTIME_NAME} INTERFACE -Wl,$<INSTALL_PREFIX>/${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_RUNTIME_NAME}/${LNKSCR}
    -Wl,--whole-archive $<TARGET_FILE:${CORE_RUNTIME_NAME}::${CORE_RUNTIME_TARGET}> -Wl,--no-whole-archive)
  add_dependencies( ${CORE_RUNTIME_NAME} ${CORE_RUNTIME_TARGET} )

  ## Add external link requirements.
  target_link_libraries ( ${CORE_RUNTIME_NAME} INTERFACE hsakmt::hsakmt )
  target_link_libraries ( ${CORE_RUNTIME_NAME} INTERFACE elf::elf dl pthread rt )

  install ( TARGETS ${CORE_RUNTIME_NAME} EXPORT ${CORE_RUNTIME_NAME}Targets )
endif()

## Set install information
# Installs binaries and exports the library usage data to ${HSAKMT_TARGET}Targets
install ( TARGETS ${CORE_RUNTIME_TARGET} EXPORT ${CORE_RUNTIME_NAME}Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT binary
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT binary )
install ( TARGETS ${CORE_RUNTIME_TARGET}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT asan )

# Install license
if(ENABLE_ASAN_PACKAGING)
   install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md DESTINATION ${CMAKE_INSTALL_DOCDIR}-asan COMPONENT asan )
endif()
install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT binary )

# Install public headers
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/inc/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hsa COMPONENT dev )
# Disable header path migration helpers for ASAN package
if(NOT ENABLE_ASAN_PACKAGING)
# Option to build header path migration helpers.
  option(INCLUDE_PATH_COMPATIBILITY "Generate backward compatible headers and include paths.  Use of these headers will warn when included." ON)
endif()
if(INCLUDE_PATH_COMPATIBILITY)
# To enable/disable #error in wrapper header files
  if(NOT DEFINED ROCM_HEADER_WRAPPER_WERROR)
      if(DEFINED ENV{ROCM_HEADER_WRAPPER_WERROR})
          set(ROCM_HEADER_WRAPPER_WERROR "$ENV{ROCM_HEADER_WRAPPER_WERROR}"
                  CACHE STRING "Header wrapper warnings as errors.")
      else()
          set(ROCM_HEADER_WRAPPER_WERROR "OFF" CACHE STRING "Header wrapper warnings as errors.")
      endif()
  endif()
  if(ROCM_HEADER_WRAPPER_WERROR)
      set(deprecated_error 1)
  else()
      set(deprecated_error 0)
  endif()

  include(hsa-backward-compat.cmake)
  target_include_directories( ${CORE_RUNTIME_TARGET}
    PUBLIC
    $<INSTALL_INTERFACE:hsa/include/hsa> )
endif()

## Configure and install package config file
# Record our usage data for clients find_package calls.
install ( EXPORT ${CORE_RUNTIME_NAME}Targets
  FILE ${CORE_RUNTIME_NAME}Targets.cmake
  NAMESPACE ${CORE_RUNTIME_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_RUNTIME_NAME}
  COMPONENT dev)

# Adds the target alias hsa-runtime64::hsa-runtime64 to the local cmake cache.
# This isn't necessary today.  It's harmless preparation for some
# hypothetical future in which the we might be included by add_subdirectory()
# in some other project's cmake file.  It allows uniform use of find_package
# and target_link_library() without regard to whether a target is external or
# a subdirectory of the current build.
add_library( ${CORE_RUNTIME_NAME}::${CORE_RUNTIME_NAME} ALIAS ${CORE_RUNTIME_NAME} )

# Create cmake configuration files
include(CMakePackageConfigHelpers)

configure_package_config_file(${CORE_RUNTIME_NAME}-config.cmake.in
  ${CORE_RUNTIME_NAME}-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_RUNTIME_NAME} )

write_basic_package_version_file(${CORE_RUNTIME_NAME}-config-version.cmake
  VERSION ${SO_VERSION_STRING} COMPATIBILITY AnyNewerVersion )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CORE_RUNTIME_NAME}-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/${CORE_RUNTIME_NAME}-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_RUNTIME_NAME}
  COMPONENT dev)

# Install build files needed only when using a static build.
if( NOT ${BUILD_SHARED_LIBS} )
  # libelf find package module
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/FindLibElf.cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/COPYING-CMAKE-SCRIPTS
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_RUNTIME_NAME}
    COMPONENT dev)
  # Linker script (defines function aliases)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${LNKSCR}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CORE_RUNTIME_NAME}
    COMPONENT dev)
endif()

# Optionally record the package's find module in the user's package cache.
if ( NOT DEFINED EXPORT_TO_USER_PACKAGE_REGISTRY )
  set ( EXPORT_TO_USER_PACKAGE_REGISTRY "off" )
endif()
set ( EXPORT_TO_USER_PACKAGE_REGISTRY ${EXPORT_TO_USER_PACKAGE_REGISTRY} CACHE BOOL "Add cmake package config location to the user's cmake package registry.")
if(${EXPORT_TO_USER_PACKAGE_REGISTRY})
  # Enable writing to the registry
  set(CMAKE_EXPORT_PACKAGE_REGISTRY ON)
  # Generate a target file for the build
  export(TARGETS ${CORE_RUNTIME_NAME} NAMESPACE ${CORE_RUNTIME_NAME}:: FILE ${CORE_RUNTIME_NAME}Targets.cmake)
  # Record the package in the user's cache.
  export(PACKAGE ${CORE_RUNTIME_NAME})
endif()

## Packaging directives
set ( CPACK_GENERATOR "DEB;RPM" CACHE STRING "Package types to build")
set ( ENABLE_LDCONFIG ON CACHE BOOL "Set library links and caches using ldconfig.")

## Only pack the "binary" and "dev" components, post install script will add the directory link.
set ( CPACK_COMPONENTS_ALL binary dev )
# ASAN Package will have libraries and license file
if(ENABLE_ASAN_PACKAGING)
   set(CPACK_COMPONENTS_ALL asan)
endif()
set ( CPACK_DEB_COMPONENT_INSTALL ON)
set ( CPACK_RPM_COMPONENT_INSTALL ON)
set ( CPACK_PACKAGE_VENDOR "Advanced Micro Devices, Inc." )
set ( CPACK_PACKAGE_VERSION ${PACKAGE_VERSION_STRING} )
set ( CPACK_PACKAGE_CONTACT "AMD HSA Support <dl.HSA-Runtime-Support@amd.com>")
set ( CPACK_COMPONENT_BINARY_DESCRIPTION "AMD Heterogeneous System Architecture HSA - Linux HSA Runtime for Boltzmann (ROCm) platforms" )
set ( CPACK_COMPONENT_DEV_DESCRIPTION "AMD Heterogeneous System Architecture HSA development package.\n This package contains the headers and cmake files for the hsa-rocr package." )
set ( CPACK_COMPONENT_ASAN_DESCRIPTION "AMD Heterogeneous System Architecture HSA - Linux HSA instrumented libraries for Boltzmann (ROCm) platforms" )
set ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md" )

if ( DEFINED ENV{ROCM_LIBPATCH_VERSION} )
  set ( CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}.$ENV{ROCM_LIBPATCH_VERSION}" )
  message ( "Using CPACK_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION}" )
endif()

# Debian package specific variables
set ( CPACK_DEBIAN_BINARY_PACKAGE_NAME "hsa-rocr")
set ( CPACK_DEBIAN_DEV_PACKAGE_NAME "hsa-rocr-dev")
set ( CPACK_DEBIAN_ASAN_PACKAGE_NAME "hsa-rocr-asan")
if ( DEFINED ENV{CPACK_DEBIAN_PACKAGE_RELEASE} )
  set ( CPACK_DEBIAN_PACKAGE_RELEASE $ENV{CPACK_DEBIAN_PACKAGE_RELEASE} )
else()
  set ( CPACK_DEBIAN_PACKAGE_RELEASE "local" )
endif()
message ( "Using CPACK_DEBIAN_PACKAGE_RELEASE ${CPACK_DEBIAN_PACKAGE_RELEASE}" )
set ( CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT" )
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCR-Runtime" )

## Process the Debian install/remove scripts to update the CPACK variables
configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/Binary/postinst.in DEBIAN/Binary/postinst @ONLY )
configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/Binary/prerm.in DEBIAN/Binary/prerm @ONLY )
set ( CPACK_DEBIAN_BINARY_PACKAGE_CONTROL_EXTRA "DEBIAN/Binary/postinst;DEBIAN/Binary/prerm" )

# package dependencies
set( CPACK_DEBIAN_BINARY_PACKAGE_DEPENDS "libdrm-amdgpu-amdgpu1 | libdrm-amdgpu1, libnuma1, libelf1")
set( CPACK_DEBIAN_ASAN_PACKAGE_DEPENDS "libdrm-amdgpu-amdgpu1 | libdrm-amdgpu1, libnuma1, libelf1")
set( CPACK_DEBIAN_BINARY_PACKAGE_RECOMMENDS "libdrm-amdgpu-amdgpu1")
set ( CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "hsa-rocr, hsakmt-roct-dev" )
if ( ROCM_DEP_ROCMCORE )
  string ( APPEND CPACK_DEBIAN_BINARY_PACKAGE_DEPENDS ", rocm-core" )
  string ( APPEND CPACK_DEBIAN_ASAN_PACKAGE_DEPENDS ", rocm-core-asan" )
endif()
# Declare package relationships (hsa-ext-rocr-dev is a legacy package that we subsume)
set ( CPACK_DEBIAN_PACKAGE_BREAKS "hsa-ext-rocr-dev" )
set ( CPACK_DEBIAN_PACKAGE_REPLACES "hsa-ext-rocr-dev" )

# RPM package specific variables
set ( EL7_DISTRO "FALSE" )
checksetel7(EL7_DISTRO)
set ( CPACK_RPM_BINARY_PACKAGE_NAME "hsa-rocr" )
set ( CPACK_RPM_DEV_PACKAGE_NAME "hsa-rocr-devel" )
set ( CPACK_RPM_ASAN_PACKAGE_NAME "hsa-rocr-asan" )
if ( DEFINED ENV{CPACK_RPM_PACKAGE_RELEASE} )
  set ( CPACK_RPM_PACKAGE_RELEASE $ENV{CPACK_RPM_PACKAGE_RELEASE} )
else()
  set ( CPACK_RPM_PACKAGE_RELEASE "local" )
endif()

string ( APPEND CPACK_RPM_PACKAGE_RELEASE "%{?dist}" )
set ( CPACK_RPM_FILE_NAME "RPM-DEFAULT" )
message("CPACK_RPM_PACKAGE_RELEASE: ${CPACK_RPM_PACKAGE_RELEASE}")
set( CPACK_RPM_PACKAGE_LICENSE "NCSA" )

## Process the Rpm install/remove scripts to update the CPACK variables
configure_file ( "${CMAKE_CURRENT_SOURCE_DIR}/RPM/Binary/post.in" RPM/Binary/post @ONLY )
configure_file ( "${CMAKE_CURRENT_SOURCE_DIR}/RPM/Binary/postun.in" RPM/Binary/postun @ONLY )
set ( CPACK_RPM_BINARY_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/RPM/Binary/post" )
set ( CPACK_RPM_BINARY_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/RPM/Binary/postun" )

# package dependencies
if (${EL7_DISTRO} STREQUAL "TRUE")
  set ( CPACK_RPM_BINARY_PACKAGE_REQUIRES "libdrm-amdgpu, numactl-libs" )
  set ( CPACK_RPM_ASAN_PACKAGE_REQUIRES "libdrm-amdgpu, numactl-libs" )
else()
  set ( CPACK_RPM_BINARY_PACKAGE_REQUIRES "(libdrm-amdgpu or libdrm or libdrm_amdgpu1), (libnuma1 or numactl-libs)" )
  set ( CPACK_RPM_ASAN_PACKAGE_REQUIRES "(libdrm-amdgpu or libdrm or libdrm_amdgpu1), (libnuma1 or numactl-libs)" )
  set ( CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/hsa-rocr.spec.in" )
  set ( CPACK_RPM_PACKAGE_RECOMMENDS "libdrm-amdgpu" )
endif()
set ( CPACK_RPM_DEV_PACKAGE_REQUIRES "hsa-rocr , hsakmt-roct-devel" )
if ( ROCM_DEP_ROCMCORE )
  string ( APPEND CPACK_RPM_BINARY_PACKAGE_REQUIRES " rocm-core" )
  string ( APPEND CPACK_RPM_ASAN_PACKAGE_REQUIRES " rocm-core-asan" )
endif()
# Declare package relationships (hsa-ext-rocr-dev is a legacy package that we subsume)
set ( CPACK_RPM_PACKAGE_PROVIDES "hsa-ext-rocr-dev hsa-rocr-dev" )
set ( CPACK_RPM_PACKAGE_OBSOLETES "hsa-ext-rocr-dev" )

## Include packaging
include ( CPack )
