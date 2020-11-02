#
# Copyright (C) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
#

# If environment variable DRM_DIR or LIBHSAKMT_PATH is set, the script
# will pick up the corresponding libraries from those pathes.

cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(KFDTest)

# For DEB/RPM generation
set ( CPACK_PACKAGE_FILE_NAME "kfdtest" )
set ( CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc." )
set ( CPACK_PACKAGE_DESCRIPTION "This package includes kfdtest, the list of excluded tests for each ASIC, and a convenience script to run the test suite" )
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "Test suite for ROCK/KFD" )
set ( CPACK_PACKAGE_VERSION_MAJOR "1" )
set ( CPACK_PACKAGE_VERSION_MINOR "0" )
set ( CPACK_PACKAGE_VERSION_PATCH "0" )
set ( CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/RadeonOpenCompute/ROCT-Thunk-Interface" )

#set ( CMAKE_VERBOSE_MAKEFILE on )

find_package(PkgConfig)

if( DEFINED ENV{DRM_DIR} )
#assume header files and libraries are under the same path
    set ( DRM_DIR $ENV{DRM_DIR} )
    set ( DRM_INCLUDE_DIRS ${DRM_DIR}/include )
    link_directories(${DRM_DIR}/lib64)
    set ( DRM_LIBRARIES drm )
    set ( DRM_AMDGPU_LIBRARIES drm_amdgpu )
else()
# The module name passed to pkg_check_modules() is determined by the
# name of file *.pc
    pkg_check_modules(DRM REQUIRED libdrm)
    pkg_check_modules(DRM_AMDGPU REQUIRED libdrm_amdgpu)
    include_directories(${DRM_AMDGPU_INCLUDE_DIRS})
endif()

## Define default variable and variables for the optional build target hsakmt-dev
set ( SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "Location of hsakmt source code." )
set ( CMAKE_INSTALL_PREFIX "/opt/rocm"  CACHE STRING "Default installation directory." )
set ( CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"  CACHE STRING "Default packaging prefix." )
set ( CPACK_GENERATOR "DEB;RPM"  CACHE STRING "Default packaging generators." )

# Debian package specific variables
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCT-Thunk-Interface" )

if( DEFINED ENV{LIBHSAKMT_PATH} )
    set ( LIBHSAKMT_PATH $ENV{LIBHSAKMT_PATH} )
    message ( "LIBHSAKMT_PATH environment variable is set" )
else()
    if ( ${ROCM_INSTALL_PATH} )
       set ( ENV{PKG_CONFIG_PATH} ${ROCM_INSTALL_PATH} )
       pkg_check_modules(HSAKMT libhsakmt)
    else()
       set ( ENV{PKG_CONFIG_PATH} /opt/rocm/libhsakmt/ )
       pkg_check_modules(HSAKMT libhsakmt)
    endif()
    if( NOT HSAKMT_FOUND )
       set ( ENV{PKG_CONFIG_PATH} /opt/rocm/libhsakmt/ )
       pkg_check_modules(HSAKMT libhsakmt)
       set ( LIBHSAKMT_PATH $ENV{OUT_DIR} )
    endif()
endif()

if( DEFINED LIBHSAKMT_PATH )
    set ( HSAKMT_LIBRARY_DIRS ${LIBHSAKMT_PATH}/lib )
    set ( HSAKMT_LIBRARIES hsakmt )
endif()

message ( "Find libhsakmt at ${HSAKMT_LIBRARY_DIRS}" )

set ( SP3_DIR ${PROJECT_SOURCE_DIR}/sp3 )

include_directories(${PROJECT_SOURCE_DIR}/gtest-1.6.0)
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/../../include)
include_directories(${SP3_DIR})

include_directories(${DRM_INCLUDE_DIRS})

set (SRC_FILES gtest-1.6.0/gtest-all.cpp

  src/AqlQueue.cpp
  src/BasePacket.cpp
  src/BaseQueue.cpp
  src/Dispatch.cpp
  src/GoogleTestExtension.cpp
  src/IndirectBuffer.cpp
  src/IsaGenerator.cpp
  src/IsaGenerator_Gfx10.cpp
  src/IsaGenerator_Gfx72.cpp
  src/IsaGenerator_Gfx8.cpp
  src/IsaGenerator_Gfx9.cpp
  src/LinuxOSWrapper.cpp
  src/PM4Packet.cpp
  src/PM4Queue.cpp
  src/RDMAUtil.cpp
  src/SDMAPacket.cpp
  src/SDMAQueue.cpp
  src/KFDBaseComponentTest.cpp
  src/KFDMultiProcessTest.cpp
  src/KFDTestMain.cpp
  src/KFDTestUtil.cpp
  src/KFDTestUtilQueue.cpp

  src/KFDOpenCloseKFDTest.cpp
  src/KFDTopologyTest.cpp
  src/KFDMemoryTest.cpp
  src/KFDLocalMemoryTest.cpp
  src/KFDEventTest.cpp
  src/KFDQMTest.cpp
  src/KFDCWSRTest.cpp
  src/KFDExceptionTest.cpp
  src/KFDGraphicsInterop.cpp
  src/KFDPerfCounters.cpp
  src/KFDDBGTest.cpp
  src/KFDGWSTest.cpp
  src/KFDIPCTest.cpp

  src/KFDEvictTest.cpp
  src/KFDHWSTest.cpp
  src/KFDPerformanceTest.cpp
  src/KFDPMTest.cpp

  src/KFDRASTest.cpp
  src/RDMATest.cpp)

message( STATUS "PROJECT_SOURCE_DIR:" ${PROJECT_SOURCE_DIR} )
#message( STATUS "SRC_FILES: ")
#foreach(file ${SRC_FILES})
#  message(STATUS "${file}")
#endforeach()

#add_definitions(-Wall -std=c++11)

if ( "${CMAKE_C_COMPILER_VERSION}" STRGREATER "4.8.0")
## Add --enable-new-dtags to generate DT_RUNPATH
set ( CMAKE_CXX_FLAGS "-std=gnu++11 -Wl,--enable-new-dtags" )
endif()
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2" )
else ()
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g" )
endif ()

## Address Sanitize Flag
if ( ${ADDRESS_SANITIZER} )
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" -fsanitize=address )
    set ( CMAKE_EXE_LINKER_FLAGS -fsanitize=address )
endif ()

# link_directories() has to be put before add_executable()
# The modules found by pkg_check_modules() in the default pkg config
# path do not need to use link_directories() here.
link_directories(${HSAKMT_LIBRARY_DIRS})
link_directories(${SP3_DIR})

add_executable(kfdtest ${SRC_FILES})

target_link_libraries(kfdtest ${HSAKMT_LIBRARIES} ${DRM_LDFLAGS} ${DRM_AMDGPU_LDFLAGS} pthread m stdc++ rt amdsp3 numa)

configure_file ( scripts/kfdtest.exclude kfdtest.exclude COPYONLY )
configure_file ( scripts/run_kfdtest.sh run_kfdtest.sh COPYONLY )

install( FILES ${CMAKE_CURRENT_BINARY_DIR}/kfdtest ${CMAKE_CURRENT_BINARY_DIR}/run_kfdtest.sh
	PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ
	DESTINATION bin )
install( FILES ${CMAKE_CURRENT_BINARY_DIR}/kfdtest.exclude
	PERMISSIONS OWNER_WRITE OWNER_READ GROUP_WRITE GROUP_READ WORLD_READ
	DESTINATION share/kfdtest )
include ( CPack )
