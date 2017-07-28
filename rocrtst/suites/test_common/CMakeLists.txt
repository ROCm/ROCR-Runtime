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

if (DEFINED ENV{OPENCL_DIR})
  set(CLANG $ENV{OPENCL_DIR}/bin/x86_64/clang)
  set(OPENCL_DIR $ENV{OPENCL_DIR})
  if (NOT EXISTS ${CLANG})
    message("ERROR: path to clang (${CLANG}) is not valid. Is env. variable OPENCL_DIR correct?")
    return()
  endif()

  if (DEFINED ENV{OPENCL_VER})
    set(OPENCL_VER $ENV{OPENCL_VER})
  else()
    message("OPENCL_VER environment variable is not set. Using default")
    set(OPENCL_VER "2.0")
  endif()
else()
    message("WARNING: OPENCL_DIR environment variable is not set. Kernels will not be built.")
endif()

if (DEFINED ENV{TARGET_DEVICE})
  set(TARGET_DEVICE $ENV{TARGET_DEVICE})
else()
  message("ERROR: TARGET_DEVICE environment variable is not defined.")
  message("Please define a valid clang target (e.g., gfx803, gfx900,...).")
  return()
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
message("")
message("Build Configuration:")
message("-------------IS64BIT: " ${IS64BIT})
message("-----------BuildType: " ${BUILD_TYPE})
message("------------Compiler: " ${CMAKE_CXX_COMPILER})
message("-------------Version: " ${CMAKE_CXX_COMPILER_VERSION})
message("--------Proj Src Dir: " ${PROJECT_SOURCE_DIR})
message("--------Proj Bld Dir: " ${PROJECT_BINARY_DIR})
message("--------Proj Lib Dir: " ${PROJECT_BINARY_DIR}/lib)
message("--------Proj Exe Dir: " ${PROJECT_BINARY_DIR}/bin)
message("-------Target Device: " ${TARGET_DEVICE})
message("----------Clang path: " ${CLANG})
message("-------OpenCL version " ${OPENCL_VER})
message("")

set(KERNELS_DIR ${PROJECT_SOURCE_DIR}/kernels)
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
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")


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
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -O0")
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
aux_source_directory(${ROCRTST_ROOT}/common/rocm_smi common_smi_srcs)

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
# Source files for building rocrtst
#
aux_source_directory(${ROCRTST_ROOT}/suites/performance performanceSources)
aux_source_directory(${ROCRTST_ROOT}/suites/functional functionalSources)
aux_source_directory(${ROCRTST_ROOT}/suites/test_common testCommonSources)

# Header file include path

include_directories(${ROCR_INC_DIR})
include_directories(${ROCRTST_ROOT})
include_directories(${ROCRTST_ROOT}/gtest/include)

# Use this function to build any samples that have kernels to be built
function(build_kernel S_NAME)
  set(SNAME_KERNEL "${S_NAME}_kernels.hsaco")
  set(TARG_NAME "${S_NAME}_hsaco")
  set(HSACO_TARG_LIST ${HSACO_TARG_LIST} ${TARG_NAME} PARENT_SCOPE)
  separate_arguments(CLANG_ARG_LIST UNIX_COMMAND "-target amdgcn-amdh-amdhsa -mcpu=${TARGET_DEVICE} -include ${OPENCL_DIR}/include/opencl-c.h ${BITCODE_LIBS} -cl-std=CL${OPENCL_VER} ${CL_FILE_LIST} -o ${PROJECT_BINARY_DIR}/${SNAME_KERNEL}")
  add_custom_target(${TARG_NAME} ${CLANG} ${CLANG_ARG_LIST}
     COMMENT "BUILDING KERNEL..."
     VERBATIM)
endfunction(build_kernel)

######################
# Kernel Build Section
######################
set(KERN_SUFFIX "kernels.hsaco")
set(BITCODE_PREF "-Xclang -mlink-bitcode-file -Xclang")
set(BITCODE_PREF "${BITCODE_PREF} ${OPENCL_DIR}/lib/x86_64/bitcode")

set(COMMON_BITCODE_LIBS "${BITCODE_PREF}/opencl.amdgcn.bc")
set(COMMON_BITCODE_LIBS "${COMMON_BITCODE_LIBS} ${BITCODE_PREF}/ockl.amdgcn.bc")

# To build kernels, repeat the pattern used below for the P2P kernel; this
# pattern sets the bitcode libraries required by the kernel which will be 
# used in the build_kernel() call, which builds the kernel.

# Test Case Template example
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/test_case_template_kernels.cl")
build_kernel("test_case_template")

# P2P Memory Access
#set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
#set(CL_FILE_LIST "${KERNELS_DIR}/p2p_mem_access_kernels.cl")
#build_kernel("p2p_mem_access")

# Dispatch Time 
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/dispatch_time_kernels.cl")
build_kernel("dispatch_time")

# Build rules
add_executable(${ROCRTST} ${performanceSources} ${functionalSources} ${common_srcs}
                                ${common_smi_srcs} ${testCommonSources})

target_link_libraries(${ROCRTST} ${ROCRTST_LIBS} c stdc++ dl pthread rt)

add_custom_target(rocrtst_kernels DEPENDS ${HSACO_TARG_LIST})
INSTALL(TARGETS ${ROCRTST}
        ARCHIVE DESTINATION ${PROJECT_BINARY_DIR}/lib
        LIBRARY DESTINATION ${PROJECT_BINARY_DIR}/lib
        RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)

