################################################################################
##
## Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
##
## MIT LICENSE:
## Permission is hereby granted, free of charge, to any person obtaining a copy of
## this software and associated documentation files (the "Software"), to deal in
## the Software without restriction, including without limitation the rights to
## use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
## of the Software, and to permit persons to whom the Software is furnished to do
## so, subject to the following conditions:
##
## The above copyright notice and this permission notice shall be included in all
## copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
## OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
## SOFTWARE.
##
################################################################################

cmake_minimum_required ( VERSION 3.6.3 )

set ( HSAKMT "hsakmt" )
set ( HSAKMT_PACKAGE "hsakmt-roct" )
set ( HSAKMT_COMPONENT "lib${HSAKMT}" )
set ( HSAKMT_TARGET "${HSAKMT}" )

project ( ${HSAKMT_TARGET} VERSION 1.9.0)

# Build thunk as static by default
if ( NOT DEFINED BUILD_SHARED_LIBS )
  set ( BUILD_SHARED_LIBS "off" )
endif()
set ( BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS} CACHE BOOL "Build shared library (.so) or not.")

# Optionally, build HSAKMT with ccache.
set(ROCM_CCACHE_BUILD OFF CACHE BOOL "Set to ON for a ccache enabled build")
if (ROCM_CCACHE_BUILD)
  find_program(CCACHE_PROGRAM ccache)
  if (CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PROGRAM})
  else()
    message(WARNING "Unable to find ccache. Falling back to real compiler")
  endif() # if (CCACHE_PROGRAM)
endif() # if (ROCM_CCACHE_BUILD)

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules" )

## Include common cmake modules
include ( utils )
include ( GNUInstallDirs )

## Setup the package version.
get_version ( "1.0.0" )

set ( BUILD_VERSION_MAJOR ${VERSION_MAJOR} )
set ( BUILD_VERSION_MINOR ${VERSION_MINOR} )
set ( BUILD_VERSION_PATCH ${VERSION_PATCH} )

set ( LIB_VERSION_MAJOR 1)
set ( LIB_VERSION_MINOR 0)
if (${ROCM_PATCH_VERSION})
    set ( LIB_VERSION_PATCH ${ROCM_PATCH_VERSION} )
else ()
    set ( LIB_VERSION_PATCH 6)
endif ()
set ( LIB_VERSION_STRING "${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH}" )

if ( DEFINED VERSION_BUILD AND NOT ${VERSION_BUILD} STREQUAL "" )
    message ( "VERSION BUILD DEFINED ${VERSION_BUILD}" )
    set ( BUILD_VERSION_PATCH "${BUILD_VERSION_PATCH}-${VERSION_BUILD}" )
endif ()
set ( BUILD_VERSION_STRING "${BUILD_VERSION_MAJOR}.${BUILD_VERSION_MINOR}.${BUILD_VERSION_PATCH}" )

## Compiler flags
set (HSAKMT_C_FLAGS -fPIC -W -Wall -Wextra -Wno-unused-parameter -Wformat-security -Wswitch-default -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wunreachable-code -std=gnu99 -fvisibility=hidden)
if ( CMAKE_COMPILER_IS_GNUCC )
    set ( HSAKMT_C_FLAGS "${HSAKMT_C_FLAGS}" -Wlogical-op)
endif ()
if ( ${HSAKMT_WERROR} )
    set ( HSAKMT_C_FLAGS "${HSAKMT_C_FLAGS}" -Werror )
endif ()
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    set ( HSAKMT_C_FLAGS "${HSAKMT_C_FLAGS}" -O2 )
else ()
    set ( HSAKMT_C_FLAGS "${HSAKMT_C_FLAGS}" -g )
endif ()

set ( HSAKMT_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/libhsakmt.ver" )

## Linker Flags
## Add --enable-new-dtags to generate DT_RUNPATH
set (HSAKMT_LINK_FLAGS "${HSAKMT_LINK_FLAGS} -Wl,--enable-new-dtags -Wl,--version-script=${HSAKMT_LINKER_SCRIPT} -Wl,-soname=${HSAKMT_COMPONENT}.so.${LIB_VERSION_MAJOR} -Wl,-z,nodelete")

## Address Sanitize Flag
if ( ${ADDRESS_SANITIZER} )
    set ( HSAKMT_C_FLAGS "${HSAKMT_C_FLAGS}" -fsanitize=address )
    set ( HSAKMT_LINK_FLAGS "${HSAKMT_LINK_FLAGS} -fsanitize=address" )
    if ( BUILD_SHARED_LIBS )
        set ( HSAKMT_LINK_FLAGS "${HSAKMT_LINK_FLAGS} -shared-libsan" )
    else ()
        set ( HSAKMT_LINK_FLAGS "${HSAKMT_LINK_FLAGS} -static-libsan" )
    endif ()
else ()
    if ( CMAKE_COMPILER_IS_GNUCC )
        set ( HSAKMT_LINK_FLAGS "${HSAKMT_LINK_FLAGS} -Wl,-no-undefined" )
    else ()
        set ( HSAKMT_LINK_FLAGS "${HSAKMT_LINK_FLAGS} -Wl,-undefined,error" )
    endif ()
endif ()

## Source files
set ( HSAKMT_SRC "src/debug.c"
                 "src/events.c"
                 "src/fmm.c"
                 "src/globals.c"
                 "src/libhsakmt.c"
                 "src/memory.c"
                 "src/openclose.c"
                 "src/pci_ids.c"
                 "src/perfctr.c"
                 "src/pmc_table.c"
                 "src/queues.c"
                 "src/time.c"
                 "src/topology.c"
                 "src/rbtree.c"
                 "src/spm.c"
                 "src/version.c"
                 "src/svm.c")

## Declare the library target name
add_library ( ${HSAKMT_TARGET} "")

## Add sources
target_sources ( ${HSAKMT_TARGET} PRIVATE ${HSAKMT_SRC} )

## Add headers.  The public headers need to point at their location in both build and install
## directory layouts.  This declaration allows publishing library use data to downstream clients.
target_include_directories( ${HSAKMT_TARGET}
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src )

set_property(TARGET ${HSAKMT_TARGET} PROPERTY LINK_FLAGS ${HSAKMT_LINK_FLAGS})

## Set the VERSION and SOVERSION values
set_property ( TARGET ${HSAKMT_TARGET} PROPERTY VERSION "${LIB_VERSION_STRING}" )
set_property ( TARGET ${HSAKMT_TARGET} PROPERTY SOVERSION "${LIB_VERSION_MAJOR}" )

find_package(PkgConfig)
pkg_check_modules(LIBDRM REQUIRED libdrm)
# get OS-info for OS-specific build dependencies
get_os_info()

find_package(PkgConfig)
# Check for libraries required for building
find_library(LIBC NAMES libc.so.6 REQUIRED)
find_library(NUMA NAMES libnuma.so REQUIRED)
message(STATUS "LIBC:" ${LIBC})
message(STATUS "NUMA:" ${NUMA})

target_link_libraries ( ${HSAKMT_TARGET}
  PRIVATE pthread rt ${LIBC} ${NUMA}
)

target_compile_options(${HSAKMT_TARGET} PRIVATE ${LIBDRM_CFLAGS} ${HSAKMT_C_FLAGS})
if(NOT DISTRO_ID MATCHES "ubuntu")
  find_library(LIBGCC NAMES libgcc_s.so.1 REQUIRED)
  message(STATUS "LIBGCC:" ${LIBGCC})
  target_link_libraries( ${HSAKMT_TARGET} PRIVATE ${LIBGCC} )
else()
  find_library(UDEV NAMES libudev.so libudev.a REQUIRED)
  message(STATUS "UDEV:" ${UDEV})
  find_package(ZLIB REQUIRED)
  target_link_libraries( ${HSAKMT_TARGET} PRIVATE ${ZLIB} ${UDEV} )
endif()

## Define default paths and packages.
if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
  set ( CMAKE_INSTALL_PREFIX "/opt/rocm" )
endif()
set ( CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "Default installation directory." FORCE )
set ( CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"  CACHE STRING "Default packaging prefix." )
set ( CPACK_GENERATOR "DEB;RPM"  CACHE STRING "Default packaging generators." )

# Installs binaries and exports the library usage data to ${HSAKMT_TARGET}Targets
install ( TARGETS ${HSAKMT_TARGET} EXPORT ${HSAKMT_TARGET}Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT devel
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT devel )

install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT devel )

# Install public headers
install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT devel PATTERN "linux" EXCLUDE )

# Record our usage data for clients find_package calls.
install ( EXPORT ${HSAKMT_TARGET}Targets
  FILE ${HSAKMT_TARGET}Targets.cmake
  NAMESPACE ${HSAKMT_TARGET}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${HSAKMT_TARGET}
  COMPONENT devel)

# Adds the target alias hsakmt::hsakmt to the local cmake cache.
# This isn't necessary today.  It's harmless preparation for some
# hypothetical future in which the we might be included by add_subdirectory()
# in some other project's cmake file.  It allows uniform use of find_package
# and target_link_library() without regard to whether a target is external or
# a subdirectory of the current build.
add_library( ${HSAKMT_TARGET}::${HSAKMT_TARGET} ALIAS ${HSAKMT_TARGET} )

# Create cmake configuration files
include(CMakePackageConfigHelpers)

configure_package_config_file(${HSAKMT_TARGET}-config.cmake.in
                              ${HSAKMT_TARGET}-config.cmake
                              INSTALL_DESTINATION
                                  ${CMAKE_INSTALL_LIBDIR}/cmake/${HSAKMT_TARGET} )

write_basic_package_version_file(${HSAKMT_TARGET}-config-version.cmake
                 VERSION ${BUILD_VERSION_STRING}
                                 COMPATIBILITY
                                     AnyNewerVersion)

install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/${HSAKMT_TARGET}-config.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/${HSAKMT_TARGET}-config-version.cmake
        DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/${HSAKMT_TARGET}
        COMPONENT devel)

# Optionally record the package's find module in the user's package cache.
if ( NOT DEFINED EXPORT_TO_USER_PACKAGE_REGISTRY )
  set ( EXPORT_TO_USER_PACKAGE_REGISTRY "off" )
endif()
set ( EXPORT_TO_USER_PACKAGE_REGISTRY ${EXPORT_TO_USER_PACKAGE_REGISTRY} CACHE BOOL "Add cmake package config location to the user's cmake package registry.")
if(${EXPORT_TO_USER_PACKAGE_REGISTRY})
  # Enable writing to the registry
  set(CMAKE_EXPORT_PACKAGE_REGISTRY ON)
  # Generate a target file for the build
  export(TARGETS ${HSAKMT_TARGET} NAMESPACE ${HSAKMT_TARGET}:: FILE ${HSAKMT_TARGET}Targets.cmake)
  # Record the package in the user's cache.
  export(PACKAGE ${HSAKMT_TARGET})
endif()

# CPACK_PACKAGING_INSTALL_PREFIX is needed in libhsakmt.pc.in
# TODO: Add support for relocatable packages.
configure_file ( libhsakmt.pc.in libhsakmt.pc @ONLY )

install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/libhsakmt.pc DESTINATION ${CMAKE_INSTALL_DATADIR}/pkgconfig COMPONENT devel)

###########################
# Packaging directives
###########################
# Use component packaging
set(CPACK_COMPONENTS_GROUPING IGNORE)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_PACKAGE_VENDOR "Advanced Micro Devices, Inc.")
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT "AMD GFX mailing list <amd-gfx@lists.freedesktop.org>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
set(CPACK_COMPONENT_DESCRIPTION "HSAKMT development package.\n This package includes the user-mode API interfaces\nused to interact with the ROCk driver.\n This package contains the headers, pkgonfig and\n cmake files for the ROCT package.")
set ( ENABLE_LDCONFIG ON CACHE BOOL "Set library links and caches using ldconfig.")

# Make proper version for appending
# Default Value is 99999, setting it first
set(ROCM_VERSION_FOR_PACKAGE "99999")
if(DEFINED ENV{ROCM_LIBPATCH_VERSION})
  set(ROCM_VERSION_FOR_PACKAGE $ENV{ROCM_LIBPATCH_VERSION})
endif()

# Debian package specific variables
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCT-Thunk-Interface")
set(CPACK_DEBIAN_PACKAGE_NAME "hsakmt-roct-dev")
if (DEFINED ENV{CPACK_DEBIAN_PACKAGE_RELEASE})
   set(CPACK_DEBIAN_PACKAGE_RELEASE $ENV{CPACK_DEBIAN_PACKAGE_RELEASE})
else()
   set(CPACK_DEBIAN_PACKAGE_RELEASE "local")
endif()
## Process the Debian install/remove scripts to update the CPACK variables
configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/postinst.in DEBIAN/postinst @ONLY )
configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/prerm.in DEBIAN/prerm @ONLY )
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "DEBIAN/postinst;DEBIAN/prerm")
# Needed since some packages still say they need hsakmt-roct
set(CPACK_DEBIAN_PACKAGE_REPLACES "hsakmt-roct")
set(CPACK_DEBIAN_PACKAGE_PROVIDES "hsakmt-roct")

## Process the Rpm install/remove scripts to update the CPACK variables
configure_file ( "${CMAKE_CURRENT_SOURCE_DIR}/RPM/post.in" RPM/post @ONLY )
configure_file ( "${CMAKE_CURRENT_SOURCE_DIR}/RPM/postun.in" RPM/postun @ONLY )
## RPM package specific variables
set ( CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/RPM/post" )
set ( CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/RPM/postun" )

if(DEFINED CPACK_PACKAGING_INSTALL_PREFIX)
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "${CPACK_PACKAGING_INSTALL_PREFIX} ${CPACK_PACKAGING_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
endif()
set(CPACK_RPM_PACKAGE_NAME "hsakmt-roct-devel")
# Since we changed the package name to match RPM specs, take care of older builds that had -dev installed
# Also cover the fact that this now replaces the old binary package hsakmt-roct
set(CPACK_RPM_PACKAGE_OBSOLETES "hsakmt-roct-dev hsakmt-roct")
if(DEFINED ENV{CPACK_RPM_PACKAGE_RELEASE})
  set(CPACK_RPM_PACKAGE_RELEASE $ENV{CPACK_RPM_PACKAGE_RELEASE})
else()
  set(CPACK_RPM_PACKAGE_RELEASE "local")
endif()

# 'dist' breaks manual builds on debian systems due to empty Provides
execute_process( COMMAND rpm --eval %{?dist}
                 RESULT_VARIABLE PROC_RESULT
                 OUTPUT_VARIABLE EVAL_RESULT
                 OUTPUT_STRIP_TRAILING_WHITESPACE )
message("RESULT_VARIABLE ${PROC_RESULT} OUTPUT_VARIABLE: ${EVAL_RESULT}")

if ( PROC_RESULT EQUAL "0" AND NOT EVAL_RESULT STREQUAL "" )
  string ( APPEND CPACK_RPM_PACKAGE_RELEASE "%{?dist}" )
endif()

# Prepare final version for the CPACK use
set(PACKAGE_VERSION_STR "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.${ROCM_VERSION_FOR_PACKAGE}")
set(CPACK_PACKAGE_VERSION "${PACKAGE_VERSION_STR}")

# Setting devel package dependendent version
set(CPACK_DEBIAN_PACKAGE_DEPENDS "rocm-core")
set(CPACK_DEBIAN_PACKAGE_REPLACES "hsakmt-roct")

set(CPACK_RPM_PACKAGE_REQUIRES "rocm-core")

# Set the names now using CPACK utility
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")

# Remove dependency on rocm-core if -DROCM_DEP_ROCMCORE=ON not given to cmake
if(NOT ROCM_DEP_ROCMCORE)
    string(REGEX REPLACE ",? ?rocm-core" "" CPACK_RPM_PACKAGE_REQUIRES ${CPACK_RPM_PACKAGE_REQUIRES})
    string(REGEX REPLACE ",? ?rocm-core" "" CPACK_DEBIAN_PACKAGE_DEPENDS ${CPACK_DEBIAN_PACKAGE_DEPENDS})
endif()

include(CPack)
