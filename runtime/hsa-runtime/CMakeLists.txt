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

cmake_minimum_required(VERSION 2.8)

## Set core runtime module name and project name.
set ( CORE_RUNTIME_NAME "hsa-runtime" )
set ( CORE_RUNTIME_COMPONENT "lib${CORE_RUNTIME_NAME}" )
set ( CORE_RUNTIME_TARGET "${CORE_RUNTIME_NAME}64" )
project( ${CORE_RUNTIME_TARGET} )

list ( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules" )

add_definitions ( -D HSA_DEPRECATED= )

include ( utils )
include ( hsa_common )

## Parse the package version
if( NOT DEFINED VERSION_STRING )
    set ( VERSION_STRING "1.0.0" )
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

add_subdirectory ( libamdhsacode )
add_subdirectory ( loader )
add_subdirectory ( core )

## Set install information
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
