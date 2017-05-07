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

## Set the name and project name.
set ( PROJECT_STRING hsa-ext-rocr-dev )
project ( ${PROJECT_STRING} )

list ( APPEND CMAKE_MODULE_PATH "${HSA_OPENSOURCE_ROOT}/cmake_modules" )
include ( utils )

## Get the package version. The defaults to 1.0.0.
get_version ( "1.0.0" )

set ( BUILD_VERSION_MAJOR ${VERSION_MAJOR} )
set ( BUILD_VERSION_MINOR ${VERSION_MINOR} )
set ( BUILD_VERSION_PATCH ${VERSION_PATCH} )
set ( LIB_VERSION_STRING "${BUILD_VERSION_MAJOR}.${BUILD_VERSION_MINOR}.${BUILD_VERSION_PATCH}" )
if ( DEFINED VERSION_BUILD )
    set ( BUILD_VERSION_PATCH "${BUILD_VERSION_PATCH}-${VERSION_BUILD}" )
endif ()
set ( BUILD_VERSION_STRING "${BUILD_VERSION_MAJOR}.${BUILD_VERSION_MINOR}.${BUILD_VERSION_PATCH}" )

set ( PACKAGE_DIRECTORIES
      "bin"
      "hsa/bin"
      "hsa/lib")

add_custom_command ( OUTPUT ${PACKAGE_DIRECTORIES} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMAND mkdir -p bin
                   COMMAND mkdir -p hsa/lib
                   COMMAND mkdir -p hsa/bin )

add_custom_target (create_dirs DEPENDS ${PACKAGE_DIRECTORIES} )

set ( FINALIZER_NAME "libhsa-ext-finalize64" )
set ( TOOLS_NAME     "libhsa-runtime-tools64" )
set ( IMAGE_NAME     "libhsa-ext-image64" )
set ( AMDHSACOD_NAME "amdhsacod" )
set ( AMDHSAFIN_NAME "amdhsafin" )

set ( FINALIZER_LIBRARY_SOURCE "${OUT_DIR}/lib/${FINALIZER_NAME}.so.${LIB_VERSION_STRING}" )
set ( TOOLS_LIBRARY_SOURCE     "${OUT_DIR}/lib/${TOOLS_NAME}.so.${LIB_VERSION_STRING}" )
set ( IMAGE_LIBRARY_SOURCE     "${OUT_DIR}/lib/${IMAGE_NAME}.so.${LIB_VERSION_STRING}" )
set ( AMDHSACOD_SOURCE         "${OUT_DIR}/bin/${AMDHSACOD_NAME}" )
set ( AMDHSAFIN_SOURCE         "${OUT_DIR}/bin/${AMDHSAFIN_NAME}" )

set ( FINALIZER_LIBRARY_TARGET "${CMAKE_CURRENT_BINARY_DIR}/hsa/lib/${FINALIZER_NAME}.so.${LIB_VERSION_STRING}" )
set ( TOOLS_LIBRARY_TARGET     "${CMAKE_CURRENT_BINARY_DIR}/hsa/lib/${TOOLS_NAME}.so.${LIB_VERSION_STRING}" )
set ( IMAGE_LIBRARY_TARGET     "${CMAKE_CURRENT_BINARY_DIR}/hsa/lib/${IMAGE_NAME}.so.${LIB_VERSION_STRING}" )
set ( AMDHSACOD_TARGET         "${CMAKE_CURRENT_BINARY_DIR}/hsa/bin/${AMDHSACOD_NAME}" )
set ( AMDHSAFIN_TARGET         "${CMAKE_CURRENT_BINARY_DIR}/hsa/bin/${AMDHSAFIN_NAME}" )

set ( FINALIZER_LIBRARY_SONAME "${CMAKE_CURRENT_BINARY_DIR}/hsa/lib/${FINALIZER_NAME}.so.${BUILD_VERSION_MAJOR}" )
set ( TOOLS_LIBRARY_SONAME     "${CMAKE_CURRENT_BINARY_DIR}/hsa/lib/${TOOLS_NAME}.so.${BUILD_VERSION_MAJOR}" )
set ( IMAGE_LIBRARY_SONAME     "${CMAKE_CURRENT_BINARY_DIR}/hsa/lib/${IMAGE_NAME}.so.${BUILD_VERSION_MAJOR}" )
set ( AMDHSACOD_LINK           "${CMAKE_CURRENT_BINARY_DIR}/bin/${AMDHSACOD_NAME}" )
set ( AMDHSAFIN_LINK           "${CMAKE_CURRENT_BINARY_DIR}/bin/${AMDHSAFIN_NAME}" )

add_custom_command ( OUTPUT ${FINALIZER_LIBRARY_TARGET} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy ${FINALIZER_LIBRARY_SOURCE} ${FINALIZER_LIBRARY_TARGET} )

add_custom_command ( OUTPUT ${TOOLS_LIBRARY_TARGET} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy ${TOOLS_LIBRARY_SOURCE} ${TOOLS_LIBRARY_TARGET} )

add_custom_command ( OUTPUT ${IMAGE_LIBRARY_TARGET} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy ${IMAGE_LIBRARY_SOURCE} ${IMAGE_LIBRARY_TARGET} )

add_custom_command ( OUTPUT ${AMDHSACOD_TARGET} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy ${AMDHSACOD_SOURCE} ${AMDHSACOD_TARGET} )

add_custom_command ( OUTPUT ${AMDHSAFIN_TARGET} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                     COMMAND ${CMAKE_COMMAND} -E copy ${AMDHSAFIN_SOURCE} ${AMDHSAFIN_TARGET} )

add_custom_command ( OUTPUT ${FINALIZER_LIBRARY_SONAME} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/hsa/lib
                     COMMAND ${CMAKE_COMMAND} -E create_symlink ${FINALIZER_NAME}.so.${VERSION_STRING} ${FINALIZER_NAME}.so.${BUILD_VERSION_MAJOR} )

add_custom_command ( OUTPUT ${TOOLS_LIBRARY_SONAME} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/hsa/lib
                     COMMAND ${CMAKE_COMMAND} -E create_symlink ${TOOLS_NAME}.so.${VERSION_STRING} ${TOOLS_NAME}.so.${BUILD_VERSION_MAJOR} )

add_custom_command ( OUTPUT ${IMAGE_LIBRARY_SONAME} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/hsa/lib
                     COMMAND ${CMAKE_COMMAND} -E create_symlink ${IMAGE_NAME}.so.${VERSION_STRING} ${IMAGE_NAME}.so.${BUILD_VERSION_MAJOR} )

add_custom_command ( OUTPUT ${AMDHSACOD_LINK} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin
                     COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/bin/${AMDHSACOD_NAME} ${AMDHSACOD_NAME} )

add_custom_command ( OUTPUT ${AMDHSAFIN_LINK} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin
                     COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/bin/${AMDHSAFIN_NAME} ${AMDHSAFIN_NAME} )

add_custom_target ( copy_targets ALL DEPENDS create_dirs
                                        ${FINALIZER_LIBRARY_TARGET}
                                        ${TOOLS_LIBRARY_TARGET}
                                        ${IMAGE_LIBRARY_TARGET}
                                        ${AMDHSACOD_TARGET}
                                        ${AMDHSAFIN_TARGET}
                                        ${FINALIZER_LIBRARY_SONAME}
                                        ${TOOLS_LIBRARY_SONAME}
                                        ${IMAGE_LIBRARY_SONAME}
                                        ${AMDHSACOD_LINK}
                                        ${AMDHSAFIN_LINK}
                                        )

install ( DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/ DESTINATION bin USE_SOURCE_PERMISSIONS )
install ( DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/hsa/ DESTINATION hsa USE_SOURCE_PERMISSIONS )

set ( CPACK_PACKAGE_NAME ${PROJECT_NAME} )
set ( CPACK_PACKAGE_VENDOR "AMD" )
set ( CPACK_PACKAGE_VERSION_MAJOR ${BUILD_VERSION_MAJOR} )
set ( CPACK_PACKAGE_VERSION_MINOR ${BUILD_VERSION_MINOR} )
set ( CPACK_PACKAGE_VERSION_PATCH ${BUILD_VERSION_PATCH} )
set ( CPACK_PACKAGE_CONTACT "James Edwards (JamesAdrian.Edwards@amd.com)" )
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "AMD Heterogeneous System Architecture HSA - Linux HSA Runtime extensions for ROCm platforms" )
set ( CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/description" )
set ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/copyright" )

# Debian package specific variables
# set ( CPACK_DEBIAN_PACKAGE_DEPENDS "None" )
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCR-Runtime" )
set ( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/postinst;${CMAKE_CURRENT_SOURCE_DIR}/prerm" )

# RPM package specific variables
# set ( CPACK_RPM_PACKAGE_REQUIRES "None" )
set ( CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpm_post" )
set ( CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpm_postun" )

include ( CPack )
