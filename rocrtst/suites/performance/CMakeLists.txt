#
# Minimum version of cmake required
#
cmake_minimum_required(VERSION 2.8.0)

#
# GCC 4.8 or higher compiler required.
#
#   Setup build environment
#
#   1) Set env. variable specifying the location of ROCR header files
#
#      export ROCR_DIR="Root for RocR install"
#
#   2) Set env. variable ROCRTST_BLD_TYPE to either "Debug" or "Release".
#      If not set, the default value is "Debug" is bound.
# 
#      export ROCRTST_BLD_TYPE=Debug or ROCRTST_BLD_TYPE=Release
#
#   3) Set env. variable ROCRTST_BLD_BITS to either "32" or "64"
#      If not set, the default value of "64" is bound.
# 
#       export ROCRTST_BLD_BITS=32 or ROCRTST_BLD_BITS=64
#
#   4) Set env. variable TARGET_DEVICE to indicate gpu type (e.g., gfx803,
#      gfx900, ...)
#
#   5) Set env. variables AMDHSAFIN_DIR and and AMDHSAFIN_TARGET to the 
#      directory containing the amd finalizer executable and version
#      (e.g, 8:0:3) respectively.      
#
#   Building rocrtst Suite
# 
#   1) Create build folder e.g. "rocrtst/build" - any name will do
#   2) Cd into build folder
#   3) Run "cmake .."
#   4) Run "make"
#

#
# Currently support for Windows platform is not present
#
if(WIN32)
  MESSAGE("rocrtst Suite is not supported on Windows platform")
  RETURN()
endif()

#
# Process environment variables relating to Build type, size and RT version
#
string(TOLOWER "$ENV{ROCRTST_BLD_TYPE}" tmp)
if("${tmp}" STREQUAL debug)
  set(BUILD_TYPE "Debug")
  set(ISDEBUG 1)
else()
  set(BUILD_TYPE "Release")
  set(ISDEBUG 0)
endif()

if("$ENV{ROCRTST_BLD_BITS}" STREQUAL 32)
  set (ONLY64STR "")
  set (IS64BIT 0)
else()
  set (ONLY64STR "64")
  set (IS64BIT 1)
endif()

set(ROCR_INC_DIR $ENV{ROCR_DIR}/hsa/include) 
set(ROCR_LIB_DIR $ENV{ROCR_DIR}/lib)
 
#
# Determine ROCR Header files are present
#
if(NOT EXISTS ${ROCR_INC_DIR}/hsa/hsa.h)
  MESSAGE("ERROR: ${ROCR_INC_DIR}/hsa/hsa.h does not exist. Check ROCR_DIR env. variable.")
  RETURN()
endif()


# Determine ROCR Library files are present
#
if (${IS64BIT} EQUAL 0)
  if(NOT EXISTS ${ROCR_LIB_DIR}/libhsa-runtime.so)
    MESSAGE("ERROR: Environment variable ROCR_LIB_DIR pointing to ROCR libraries is not set")
    RETURN()
  endif()
else()
  if(NOT EXISTS ${ROCR_LIB_DIR}/libhsa-runtime64.so)
    MESSAGE("ERROR: Environment variable ROCR_LIB_DIR pointing to ROCR libraries is not set")
    RETURN()
  endif()
endif()

#
# Set Name for rocrtst Suite Project
#
set(ROCRTST_SUITE_NAME "rocrtst${ONLY64STR}")
project (${ROCRTST_SUITE_NAME})

#
# Print out the build configuration being used:
#
#   Build Src directory
#   Build Binary directory
#   Build Type: Debug Vs Release, 32 Vs 64
#   Compiler Version, etc
#
MESSAGE("")
MESSAGE("-------------IS64BIT: " ${IS64BIT})
MESSAGE("-----------BuildType: " ${BUILD_TYPE})
MESSAGE("------------Compiler: " ${CMAKE_CXX_COMPILER})
MESSAGE("-------------Version: " ${CMAKE_CXX_COMPILER_VERSION})
MESSAGE("--------Proj Src Dir: " ${PROJECT_SOURCE_DIR})
MESSAGE("--------Proj Bld Dir: " ${PROJECT_BINARY_DIR})
MESSAGE("--------Proj Lib Dir: " ${PROJECT_BINARY_DIR}/lib)
MESSAGE("--------Proj Exe Dir: " ${PROJECT_BINARY_DIR}/bin)
MESSAGE("")

#
# Set the build type based on user input
#
set(CMAKE_BUILD_TYPE ${BUILD_TYPE})
#
# Flag to enable / disable verbose output.
#
SET( CMAKE_VERBOSE_MAKEFILE on )
#
# Compiler pre-processor definitions.
#
# Define MACRO "DEBUG" if build type is "Debug"
if(${BUILD_TYPE} STREQUAL "Debug")
add_definitions(-DDEBUG)
endif()

add_definitions(-D__linux__)
add_definitions(-DLITTLEENDIAN_CPU=1)

#
# Linux Compiler options
#
set(CMAKE_CXX_FLAGS "-std=c++11 ")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-math-errno")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmerge-all-constants")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-extensions")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")


#
# Extend the compiler flags for 64-bit builds
#
if (IS64BIT) 
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64  -msse -msse2")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

#
# Add compiler flags to include symbol information for debug builds
#
if(ISDEBUG)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
endif()
MESSAGE("ISDEBUG STEP:Done")


set(ROCRTST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Set Name for Google Test Framework and build it as a
# static library to be linked with user test programs
#
set(GOOGLE_TEST_FRWK_NAME "google-test-frwk${ONLY64STR}")
add_subdirectory(${ROCRTST_ROOT}/gtest "${PROJECT_BINARY_DIR}/gtest")
set (ROCRTST_LIBS ${ROCRTST_LIBS} ${GOOGLE_TEST_FRWK_NAME}
					hsa-runtime-tools${ONLY64STR})
MESSAGE("ROCRTST_LIBS SET STEP:Done")
#
#
# Other source directories
aux_source_directory(${ROCRTST_ROOT}/common common_srcs)

#
# Specify the directory containing various libraries of ROCR
# to be linked against for building ROC Perf applications
#
LINK_DIRECTORIES(${ROCR_LIB_DIR})

#
# Extend the list of libraries to be used for linking ROC Perf Apps
#
set(ROCRTST_LIBS ${ROCRTST_LIBS} hsa-runtime${ONLY64STR})


# Set Name for rocrtst 
MESSAGE(${ROCRTST_LIBS})
set(ROCRTST "rocrtst${ONLY64STR}")

#
# Sorce files for building rocrtst
#
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} performanceSources)


# Header file include path

include_directories(${ROCR_INC_DIR})
include_directories(${ROCRTST_ROOT})
include_directories(${ROCRTST_ROOT}/gtest/include)

# Build rules

add_executable(${ROCRTST} ${performanceSources} ${common_srcs})
target_link_libraries(${ROCRTST} ${ROCRTST_LIBS} c stdc++ dl pthread rt)

INSTALL(TARGETS ${ROCRTST}
        ARCHIVE DESTINATION ${PROJECT_BINARY_DIR}/lib
        LIBRARY DESTINATION ${PROJECT_BINARY_DIR}/lib
        RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)

