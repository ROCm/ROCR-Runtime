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

cmake_minimum_required ( VERSION 3.5.0 )

## Cosmetic Cmake version warnings.
if(NOT EXISTS VERSION_WARNED)
  set (VERSION_WARNED FALSE CACHE BOOL "")
endif()
if(${CMAKE_VERSION} VERSION_LESS "3.6.0" AND NOT ${VERSION_WARNED})
  message("Your CMake version is too old for full functionality.
Generated package file names may be incorrect.
Please update to CMake 3.6 or newer to generate correct package file names")
  set( VERSION_WARNED TRUE CACHE BOOL "Suppress cosmetic build errors due to CMake version after first warning." FORCE )
  mark_as_advanced( FORCE VERSION_WARNED )
endif()

set ( HSAKMT "hsakmt" )
set ( HSAKMT_PACKAGE "hsakmt-roct" )
set ( HSAKMT_COMPONENT "lib${HSAKMT}" )
set ( HSAKMT_TARGET "${HSAKMT}" )

project ( ${HSAKMT_TARGET} VERSION 1.9.0)

if ( NOT DEFINED BUILD_SHARED_LIBS )
  set ( BUILD_SHARED_LIBS "on" )
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
set (HSAKMT_LINK_FLAGS "-Wl,--enable-new-dtags -Wl,--version-script=${HSAKMT_LINKER_SCRIPT} -Wl,-soname=${HSAKMT_COMPONENT}.so.${LIB_VERSION_MAJOR} -Wl,-z,nodelete -Wl,-no-undefined" )

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
                 "src/version.c")

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

target_compile_options(${HSAKMT_TARGET} PRIVATE ${HSAKMT_C_FLAGS})
set_property(TARGET ${HSAKMT_TARGET} PROPERTY LINK_FLAGS ${HSAKMT_LINK_FLAGS})

## Set the VERSION and SOVERSION values
set_property ( TARGET ${HSAKMT_TARGET} PROPERTY VERSION "${LIB_VERSION_STRING}" )
set_property ( TARGET ${HSAKMT_TARGET} PROPERTY SOVERSION "${LIB_VERSION_MAJOR}" )

#find_package(PkgConfig)
target_link_libraries ( ${HSAKMT_TARGET}
  PRIVATE pthread rt numa
)

## Define default paths and packages.
if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
  set ( CMAKE_INSTALL_PREFIX "/opt/rocm" )
endif()
set ( CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "Default installation directory." FORCE )
set ( CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"  CACHE STRING "Default packaging prefix." )
set ( CPACK_GENERATOR "DEB;RPM"  CACHE STRING "Default packaging generators." )

# Installs binaries and exports the library usage data to ${HSAKMT_TARGET}Targets
install ( TARGETS ${HSAKMT_TARGET} EXPORT ${HSAKMT_TARGET}Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT binary
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT binary )

install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT binary )

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

# Use component packaging
set ( CPACK_COMPONENTS_GROUPING IGNORE )
set ( CPACK_COMPONENTS_ALL binary devel )
set ( CPACK_DEB_COMPONENT_INSTALL ON )
set ( CPACK_RPM_COMPONENT_INSTALL ON )

## Add the packaging directives for the runtime library.
set ( CPACK_PACKAGE_VENDOR "AMD" )
set ( CPACK_PACKAGE_VERSION_MAJOR ${BUILD_VERSION_MAJOR} )
set ( CPACK_PACKAGE_VERSION_MINOR ${BUILD_VERSION_MINOR} )
set ( CPACK_PACKAGE_VERSION_PATCH ${BUILD_VERSION_PATCH} )
set ( CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc." )
set ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md" )

## Component packaging, BINARY
set ( CPACK_COMPONENT_BINARY_DESCRIPTION "HSAKMT library for AMD KFD support" )
set ( CPACK_DEBIAN_BINARY_PACKAGE_NAME ${HSAKMT_PACKAGE} )
set ( CPACK_DEBIAN_BINARY_FILE_NAME "${CPACK_DEBIAN_BINARY_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.x86_64.deb" )
set ( CPACK_RPM_BINARY_PACKAGE_NAME ${HSAKMT_PACKAGE} )
set ( CPACK_RPM_BINARY_FILE_NAME "${CPACK_RPM_BINARY_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.x86_64.rpm" )

#checking for OS-version
if( EXISTS "/etc/os-release" )
    file( STRINGS "/etc/os-release" DISTRO_ID REGEX "^ID=" )
    file( STRINGS "/etc/os-release" DISTRO_RELEASE REGEX "^VERSION_ID=" )
    string( REPLACE "ID=" "" DISTRO_ID "${DISTRO_ID}" )
    string( REPLACE "VERSION_ID=" "" DISTRO_RELEASE "${DISTRO_RELEASE}" )
    message( STATUS "Detected distribution: ${DISTRO_ID}:${DISTRO_RELEASE}" )
elseif( EXISTS "/etc/centos-release" )
    # Example: CentOS release 6.10 (Final)
    file( STRINGS "/etc/centos-release" DISTRO_FULL_STR REGEX "release" )
    string( REGEX MATCH "^[a-zA-Z]+" DISTRO_ID "${DISTRO_FULL_STR}" )
    string( TOLOWER "${DISTRO_ID}" DISTRO_ID )
    string( REGEX MATCH "[0-9]+" DISTRO_RELEASE "${DISTRO_FULL_STR}" )
    message( STATUS "Detected distribution: ${DISTRO_ID}:${DISTRO_RELEASE}" )
else()
     message( STATUS "Not able to detect OS" )
endif()

# Debian package specific variables
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCT-Thunk-Interface" )
set ( CPACK_DEBIAN_BINARY_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/postinst;${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/prerm" )
set ( CPACK_DEBIAN_BINARY_PACKAGE_DEPENDS "libc6, libnuma1, zlib1g, libudev1" )

# RPM package specific variables
if( DISTRO_ID MATCHES "sles" )
    set ( CPACK_RPM_BINARY_PACKAGE_REQUIRES "glibc, libnuma-devel, libgcc_s1")
elseif( DISTRO_ID MATCHES "centos" AND DISTRO_RELEASE MATCHES "6" )
    set ( CPACK_RPM_BINARY_PACKAGE_REQUIRES "glibc, numactl, libgcc" )
else()
    set ( CPACK_RPM_BINARY_PACKAGE_REQUIRES "glibc, numactl-libs, libgcc" )
endif()
set ( CPACK_RPM_BINARY_PRE_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_post" )
set ( CPACK_RPM_BINARY_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_postun" )

## Component packaging, DEVEL
set ( CPACK_COMPONENT_DEVEL_DESCRIPTION "HSAKMT development package." )

# Debian package specific variables
set ( CPACK_DEBIAN_DEVEL_PACKAGE_NAME "hsakmt-roct-dev" )
set ( CPACK_DEBIAN_DEVEL_FILE_NAME "${CPACK_DEBIAN_DEVEL_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.x86_64.deb" )
set ( CPACK_DEBIAN_DEVEL_PACKAGE_DEPENDS "${HSAKMT_PACKAGE} (=${BUILD_VERSION_MAJOR}.${BUILD_VERSION_MINOR}.${BUILD_VERSION_PATCH})" )

# RPM package specific variables
set ( CPACK_RPM_DEVEL_PACKAGE_NAME "hsakmt-roct-devel" )
set ( CPACK_RPM_DEVEL_FILE_NAME "${CPACK_RPM_DEVEL_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.x86_64.rpm" )
set ( CPACK_RPM_DEVEL_PACKAGE_DEPENDS "${HSAKMT_PACKAGE} = ${BUILD_VERSION_MAJOR}.${BUILD_VERSION_MINOR}.${BUILD_VERSION_PATCH}" )
# Since we changed the package name to match RPM specs, take care of older builds that had -dev installed
set ( CPACK_RPM_DEVEL_PACKAGE_OBSOLETES "hsakmt-roct-dev" )

include ( CPack )
