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

project ( ${HSAKMT_TARGET} )

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
set ( LIB_VERSION_PATCH 6)
set ( LIB_VERSION_STRING "${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH}" )

if ( DEFINED VERSION_BUILD AND NOT ${VERSION_BUILD} STREQUAL "" )
    message ( "VERSION BUILD DEFINED ${VERSION_BUILD}" )
    set ( BUILD_VERSION_PATCH "${BUILD_VERSION_PATCH}-${VERSION_BUILD}" )
endif ()
set ( BUILD_VERSION_STRING "${BUILD_VERSION_MAJOR}.${BUILD_VERSION_MINOR}.${BUILD_VERSION_PATCH}" )

## Verbose output.
#set ( CMAKE_VERBOSE_MAKEFILE on )

## Compiler flags
set ( CMAKE_C_FLAGS "-fPIC -W -Wall -Wextra -Wno-unused-parameter -Wformat-security -Wswitch-default -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wlogical-op -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wunreachable-code -std=gnu99 -fvisibility=hidden" )
if ( "${CMAKE_C_COMPILER_VERSION}" STRGREATER "4.8.0")
    set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror" )
endif ()

if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2" )
else ()
    set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g" )
endif ()

set ( HSAKMT_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/libhsakmt.ver" )

## Linker Flags
set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,--version-script=${HSAKMT_LINKER_SCRIPT} -Wl,-soname=${HSAKMT_COMPONENT}.so.${LIB_VERSION_MAJOR} -Wl,-z,nodelete -Wl,-no-undefined" )

## Source files
set ( HSAKMT_SRC "src/debug.c"
                 "src/events.c"
                 "src/fmm.c"
                 "src/globals.c"
                 "src/libhsakmt.c"
                 "src/memory.c"
                 "src/openclose.c"
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

## Set the VERSION and SOVERSION values
set_property ( TARGET ${HSAKMT_TARGET} PROPERTY VERSION "${LIB_VERSION_STRING}" )
set_property ( TARGET ${HSAKMT_TARGET} PROPERTY SOVERSION "${LIB_VERSION_MAJOR}" )

find_package(PkgConfig)
pkg_check_modules(PC_LIBPCI REQUIRED libpci)
include_directories ( ${PC_LIBPCI_INCLUDEDIR} )
target_link_libraries ( ${HSAKMT_TARGET}
  pthread rt numa ${PC_LIBPCI_LIBRARIES}
)

## If the library is a release, strip the target library
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    add_custom_command ( TARGET ${HSAKMT_TARGET} POST_BUILD COMMAND ${CMAKE_STRIP} ${HSAKMT_COMPONENT}.so )
endif ()

## Define default variable and variables for the optional build target hsakmt-dev
set ( SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "Location of hsakmt source code." )
set ( CMAKE_INSTALL_PREFIX "/opt/rocm"  CACHE STRING "Default installation directory." )
set ( CPACK_PACKAGING_INSTALL_PREFIX "/opt/rocm"  CACHE STRING "Default packaging prefix." )
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
    WORKING_DIRECTORY ${DEV_BUILD_DIR} )

## Custom targets for the devel package 
add_custom_target ( install-dev DEPENDS build-dev COMMAND make install WORKING_DIRECTORY ${DEV_BUILD_DIR} )
add_custom_target ( package-dev DEPENDS build-dev
    COMMAND rm -rf *.deb *.rpm *.tar.gz
    COMMAND make package
    WORKING_DIRECTORY ${DEV_BUILD_DIR} )

## Add the install directives for the runtime library.
install ( TARGETS ${HSAKMT_TARGET} DESTINATION ${CMAKE_INSTALL_LIBDIR} )
install ( FILES ${SOURCE_DIR}/LICENSE.md DESTINATION libhsakmt )

## Add the packaging directives for the runtime library.
set ( CPACK_PACKAGE_NAME ${HSAKMT_PACKAGE} )
set ( CPACK_PACKAGE_VENDOR "AMD" )
set ( CPACK_PACKAGE_VERSION_MAJOR ${BUILD_VERSION_MAJOR} )
set ( CPACK_PACKAGE_VERSION_MINOR ${BUILD_VERSION_MINOR} )
set ( CPACK_PACKAGE_VERSION_PATCH ${BUILD_VERSION_PATCH} )
set ( CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc." )
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "HSAKMT library for AMD KFD support" )
set ( CPACK_RESOURCE_FILE_LICENSE "${SOURCE_DIR}/LICENSE.md" )

# Debian package specific variables
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCT-Thunk-Interface" )
set ( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/postinst;${CMAKE_CURRENT_SOURCE_DIR}/DEBIAN/prerm" )
set ( CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libnuma1, libpci3, zlib1g, libudev1" )

# RPM package specific variables
set ( CPACK_RPM_PACKAGE_REQUIRES "glibc, numactl-libs, pciutils-libs, libgcc" )
set ( CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_post" )
set ( CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RPM/rpm_postun" )

# CPACK_PACKAGING_INSTALL_PREFIX is needed in libhsakmt.pc.in
configure_file ( libhsakmt.pc.in ${DEV_BUILD_DIR}/libhsakmt.pc @ONLY )

include ( CPack )
