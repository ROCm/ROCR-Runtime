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

set ( HSAKMT "hsakmt" )
set ( HSAKMT_PACKAGE "hsakmt-roct" )
set ( HSAKMT_COMPONENT "lib${HSAKMT}" )
set ( HSAKMT_TARGET "${HSAKMT}" )

project ( ${HSAKMT_TARGET} VERSION 1.9.0)

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

## Set default module path if not already set
if ( NOT DEFINED CMAKE_MODULE_PATH )
    set ( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/" )
endif ()

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

## Verbose output.
#set ( CMAKE_VERBOSE_MAKEFILE on )

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

## Include paths
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/src )

## Add the library
add_library ( ${HSAKMT_TARGET} SHARED ${HSAKMT_SRC} )
target_compile_options(${HSAKMT_TARGET} PRIVATE ${HSAKMT_C_FLAGS})
set_property(TARGET ${HSAKMT_TARGET} PROPERTY LINK_FLAGS ${HSAKMT_LINK_FLAGS})

## Set the VERSION and SOVERSION values
set_property ( TARGET ${HSAKMT_TARGET} PROPERTY VERSION "${LIB_VERSION_STRING}" )
set_property ( TARGET ${HSAKMT_TARGET} PROPERTY SOVERSION "${LIB_VERSION_MAJOR}" )

find_package(PkgConfig)
target_link_libraries ( ${HSAKMT_TARGET}
  pthread rt numa
)

## Define default variable and variables for the optional build target hsakmt-dev
set ( SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "Location of hsakmt source code." )
set ( CMAKE_INSTALL_PREFIX "/opt/rocm"  CACHE STRING "Default installation directory." )
set ( CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"  CACHE STRING "Default packaging prefix." )
set ( CPACK_GENERATOR "DEB;RPM"  CACHE STRING "Default packaging generators." )

## Specify build, install and package targets hsakmt-dev
set ( DEV_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/hsakmt-roct-dev )
configure_file ( hsakmt-dev.txt ${DEV_BUILD_DIR}/CMakeLists.txt @ONLY )
add_custom_target ( build-dev
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR="${SOURCE_DIR}"
        -DCMAKE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
        -DCPACK_RPM_PACKAGE_REQUIRES="hsakmt-roct"
        -DCPACK_PACKAGING_INSTALL_PREFIX="${CPACK_PACKAGING_INSTALL_PREFIX}"
        -DVERSION="${BUILD_VERSION_STRING}"
        -DHSAKMT_BIN_NAME="${HSAKMT}"
    WORKING_DIRECTORY ${DEV_BUILD_DIR} )

## Custom targets for the devel package
add_custom_target ( install-dev DEPENDS build-dev COMMAND make install WORKING_DIRECTORY ${DEV_BUILD_DIR} )
add_custom_target ( package-dev DEPENDS build-dev
    COMMAND rm -rf *.deb *.rpm *.tar.gz
    COMMAND make package
    WORKING_DIRECTORY ${DEV_BUILD_DIR} )

## Add the install directives for the runtime library.
install ( TARGETS ${HSAKMT_TARGET} DESTINATION ${CMAKE_INSTALL_LIBDIR} )
install ( FILES ${SOURCE_DIR}/LICENSE.md DESTINATION ${CMAKE_INSTALL_DOCDIR} )

## Add the packaging directives for the runtime library.
set ( CPACK_PACKAGE_NAME ${HSAKMT_PACKAGE} )
set ( CPACK_PACKAGE_VENDOR "AMD" )
set ( CPACK_PACKAGE_VERSION_MAJOR ${BUILD_VERSION_MAJOR} )
set ( CPACK_PACKAGE_VERSION_MINOR ${BUILD_VERSION_MINOR} )
set ( CPACK_PACKAGE_VERSION_PATCH ${BUILD_VERSION_PATCH} )
set ( CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc." )
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "HSAKMT library for AMD KFD support" )
set ( CPACK_RESOURCE_FILE_LICENSE "${SOURCE_DIR}/LICENSE.md" )
set ( CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.x86_64" )

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
set ( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/postinst;${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/prerm" )
set ( CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libnuma1, zlib1g, libudev1" )

# RPM package specific variables
if( DISTRO_ID MATCHES "sles" )
    set ( CPACK_RPM_PACKAGE_REQUIRES "glibc, libnuma-devel, libgcc_s1")
elseif( DISTRO_ID MATCHES "centos" AND DISTRO_RELEASE MATCHES "6" )
    set ( CPACK_RPM_PACKAGE_REQUIRES "glibc, numactl, libgcc" )
else()
    set ( CPACK_RPM_PACKAGE_REQUIRES "glibc, numactl-libs, libgcc" )
endif()
set ( CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_post" )
set ( CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_postun" )

# CPACK_PACKAGING_INSTALL_PREFIX is needed in libhsakmt.pc.in
configure_file ( libhsakmt.pc.in ${DEV_BUILD_DIR}/libhsakmt.pc @ONLY )

include ( CPack )
