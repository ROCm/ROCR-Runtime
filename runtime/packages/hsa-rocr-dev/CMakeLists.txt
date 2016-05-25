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

set ( PROJECT_STRING hsa-rocr-dev )

project( ${PROJECT_STRING} )

list(APPEND CMAKE_MODULE_PATH "${HSA_OPENSOURCE_ROOT}/cmake_modules")

include ( utils )

if(NOT DEFINED VERSION_STRING)
    set (VERSION_STRING "1.0.0")
endif()

parse_version(${VERSION_STRING})

set ( EXTERNAL_DIR ${CMAKE_CURRENT_BINARY_DIR} )

set ( RUNTIME_NAME "libhsa-runtime64" )

set ( RUNTIME_LIBRARY_SOURCE   "${OUT_DIR}/lib/${RUNTIME_NAME}.so.${VERSION_STRING}" )
set ( RUNTIME_LIBRARY_TARGET   "${EXTERNAL_DIR}/hsa/lib/${RUNTIME_NAME}.so.${VERSION_STRING}" )
set ( RUNTIME_LIBRARY_SONAME   "${EXTERNAL_DIR}/hsa/lib/${RUNTIME_NAME}.so.1" )
set ( RUNTIME_LIBRARY_LINKNAME "${EXTERNAL_DIR}/lib/${RUNTIME_NAME}.so" )

set ( PACKAGE_DIRECTORIES
      "include"
      "hsa/include"
      "hsa/lib")
 
set ( RUNTIME_HEADER_NAMES
      "hsa.h"
      "amd_hsa_common.h"
      "amd_hsa_elf.h"
      "amd_hsa_kernel_code.h"
      "amd_hsa_queue.h"
      "amd_hsa_signal.h"
      "Brig.h"
      "hsa_api_trace.h"
      "hsa_ext_amd.h"
      "hsa_ext_finalize.h"
      "hsa_ext_image.h"
      "hsa_ven_amd_loaded_code_object.h"
     )

set ( RUNTIME_TOOLS_HEADER_NAMES
      "hsa_ext_profiler.h"
      "hsa_ext_debugger.h"
      "amd_hsa_tools_interfaces.h"
    )

set ( RUNTIME_TOOLS_INCLUDE_DIR "${AMD_CLOSED_SOURCE_ROOT}/drivers/hsa/runtime/tools/inc" )

set ( HSA_INCLUDE_LINK "${EXTERNAL_DIR}/hsa/include/hsa" )

set ( TOPLEVEL_INCLUDE_LINK "${EXTERNAL_DIR}/include/hsa" )

add_custom_command(OUTPUT ${PACKAGE_DIRECTORIES} WORKING_DIRECTORY ${EXTERNAL_DIR} 
                   COMMAND mkdir -p include 
                   COMMAND mkdir -p lib
                   COMMAND mkdir -p hsa/lib
                   COMMAND mkdir -p hsa/include )

add_custom_target (create_dirs DEPENDS ${PACKAGE_DIRECTORIES} )

add_custom_command( OUTPUT ${HSA_INCLUDE_LINK} WORKING_DIRECTORY ${EXTERNAL_DIR}/hsa/include
                    COMMAND ${CMAKE_COMMAND} -E create_symlink ../include hsa )

add_custom_command( OUTPUT ${TOPLEVEL_INCLUDE_LINK} WORKING_DIRECTORY ${EXTERNAL_DIR}/include
                    COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/include hsa )

add_custom_target (link_dirs DEPENDS create_dirs ${TOPLEVEL_INCLUDE_LINK} ${HSA_INCLUDE_LINK} )

add_custom_command( OUTPUT ${RUNTIME_LIBRARY_TARGET} WORKING_DIRECTORY ${EXTERNAL_DIR} 
                    COMMAND ${CMAKE_COMMAND} -E copy ${RUNTIME_LIBRARY_SOURCE} ${RUNTIME_LIBRARY_TARGET} )

add_custom_command( OUTPUT ${RUNTIME_LIBRARY_SONAME} WORKING_DIRECTORY ${EXTERNAL_DIR}/hsa/lib
                    COMMAND ${CMAKE_COMMAND} -E create_symlink ${RUNTIME_NAME}.so.${VERSION_STRING} ${RUNTIME_NAME}.so.1 )

add_custom_command( OUTPUT ${RUNTIME_LIBRARY_LINKNAME} WORKING_DIRECTORY ${EXTERNAL_DIR}/lib
                    COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/lib/${RUNTIME_NAME}.so.1 ${RUNTIME_NAME}.so )

foreach ( HEADER_FILE ${RUNTIME_HEADER_NAMES} )

    set ( HEADER_TARGET "${EXTERNAL_DIR}/hsa/include/${HEADER_FILE}" )

    add_custom_command( OUTPUT ${HEADER_TARGET} WORKING_DIRECTORY ${EXTERNAL_DIR}
                        COMMAND ${CMAKE_COMMAND} -E copy ${OUT_DIR}/include/${HEADER_FILE} ${HEADER_TARGET} )

    list ( APPEND RUNTIME_HEADERS ${HEADER_TARGET} )

endforeach()

foreach ( HEADER_FILE ${RUNTIME_TOOLS_HEADER_NAMES} )

    set ( HEADER_TARGET "${EXTERNAL_DIR}/hsa/include/${HEADER_FILE}" )

    add_custom_command( OUTPUT ${HEADER_TARGET} WORKING_DIRECTORY ${EXTERNAL_DIR}
                        COMMAND ${CMAKE_COMMAND} -E copy ${RUNTIME_TOOLS_INCLUDE_DIR}/${HEADER_FILE} ${HEADER_TARGET} )

    list ( APPEND RUNTIME_HEADERS ${HEADER_TARGET} )

endforeach()

add_custom_target (copy_headers DEPENDS ${RUNTIME_HEADERS} create_dirs)

add_custom_target (copy_targets DEPENDS create_dirs
                                        ${RUNTIME_LIBRARY_TARGET}
                                        ${RUNTIME_LIBRARY_SONAME}
                                        ${RUNTIME_LIBRARY_LINKNAME}
                                        link_dirs
                                        copy_headers
                                        )

## Install directives
install (DIRECTORY ${EXTERNAL_DIR}/include/ DESTINATION include USE_SOURCE_PERMISSIONS )
install (DIRECTORY ${EXTERNAL_DIR}/lib/ DESTINATION lib USE_SOURCE_PERMISSIONS )
install (DIRECTORY ${EXTERNAL_DIR}/hsa/ DESTINATION hsa USE_SOURCE_PERMISSIONS )
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sample DESTINATION hsa USE_SOURCE_PERMISSIONS )
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt DESTINATION hsa )
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION hsa )

## Packaging directives
set ( CPACK_PACKAGE_NAME ${PROJECT_NAME} )
set ( CPACK_PACKAGE_VENDOR "AMD" )
set ( CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR} )
set ( CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR} )
set ( CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH} )
set ( CPACK_PACKAGE_CONTACT "James Edwards (JamesAdrian.Edwards@amd.com)" )
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "AMD Heterogeneous System Architecture HSA - Linux HSA Runtime for Boltzmann (ROCm) platforms" )
set ( CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/description" )
set ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/copyright" )

# Debian package specific variables
set ( CPACK_DEBIAN_PACKAGE_DEPENDS "hsa-ext-rocr-dev (=${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}), hsakmt-roct-dev (=2.0.0)" )
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCR-Runtime" )
set ( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/postinst;${CMAKE_CURRENT_SOURCE_DIR}/prerm" )

# RPM package specific variables
set ( CPACK_RPM_PACKAGE_DEPENDS "hsa-ext-rocr-dev (=${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}), hsakmt-roct-dev (=2.0.0)" )
set ( CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpm_post" )
set ( CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpm_postun" )

include ( CPack )
