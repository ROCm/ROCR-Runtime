#
# Minimum version of cmake required
#
cmake_minimum_required(VERSION 2.8.0)

#
# GCC 4.8 or higher compiler required.
#

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
#   5) Set env. variables OPENC_DIR and and OPENC_VER to the OpenCL install
#      root and OpenCL version, respectively.
#
#   Building rocrtst Suite
# 
#   1) Create build folder e.g. "rocrtst/build" - any name will do
#   2) Cd into build folder
#   3) Run "cmake .."
#   4) Run "make"
#

#############################
# COMMON AREA
#############################
#
# Currently support for Windows platform is not present
#
if(WIN32)
  message("This sample is not supported on Windows platform")
  return()
endif()

#
# Process environment variables relating to Build type, size and RT version
#
string(TOLOWER "$ENV{ROCRTST_BLD_TYPE}" tmp)
if("${tmp}" STREQUAL release)
  set(BUILD_TYPE "Release")
  set(ISDEBUG 0)
else()
  set(BUILD_TYPE "Debug")
  set(ISDEBUG 1)
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
  message("ERROR: Environment variable ROCR_INC_DIR pointing to ROCR headers is not set")
  return()
endif()

# Determine ROCR Library files are present
#
if (${IS64BIT} EQUAL 0)
  if(NOT EXISTS ${ROCR_LIB_DIR}/libhsa-runtime.so)
    message("ERROR: Environment variable ROCR_LIB_DIR pointing to ROCR libraries is not set")
    return()
  endif()
else()
  if(NOT EXISTS ${ROCR_LIB_DIR}/libhsa-runtime64.so)
    message("ERROR: Environment variable ROCR_LIB_DIR pointing to ROCR libraries is not set")
    return()
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
# Set Name for Samples Project
#

set(PROJECT_NAME "sample${ONLY64STR}")
project (${PROJECT_NAME})

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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-math-errno")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmerge-all-constants")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-extensions")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

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
message("ISDEBUG STEP:Done")

#
# Linux Linker options
#
#set(CMAKE_EXE_LINKER_FLAGS "-Wl,-Bdynamic -Wl,-z,noexecstack -Wl ")
#set(CMAKE_EXE_LINKER_FLAGS "-Wl,-soname=$(CORE_RUNTIME_NAME).so.1 ")

#
# Specify the directory containing various libraries of ROCR
# to be linked against for building ROC Perf applications
#
link_directories(${ROCR_LIB_DIR})

#
# Extend the list of libraries to be used for linking ROC Perf Apps
#
set(ROCR_LIBS ${ROCR_LIBS} hsa-runtime${ONLY64STR})
set(ROCR_LIBS ${ROCR_LIBS} hsa-runtime-tools${ONLY64STR})
message(${ROCR_LIBS})

include_directories(${ROCR_INC_DIR})
include_directories($ENV{OPENCL_DIR}/include/opencl$ENV{OPENCL_VER})

# Use this function to build any samples that have kernels to be built
function(process_sample S_NAME)
  set(SNAME_EXE "${S_NAME}_${PROJECT_NAME}")
  set(SNAME_KERNEL "${S_NAME}_kernels.hsaco")
  set(TARG_NAME "${S_NAME}_hsaco")
  set(HSACO_TARG_LIST ${HSACO_TARG_LIST} ${TARG_NAME} PARENT_SCOPE)
  separate_arguments(CLANG_ARG_LIST UNIX_COMMAND "-target amdgcn-amdh-amdhsa -mcpu=${TARGET_DEVICE} -include ${OPENCL_DIR}/include/opencl-c.h ${BITCODE_LIBS} -cl-std=CL${OPENCL_VER} ${CL_FILE_LIST} -o ${PROJECT_BINARY_DIR}/${SNAME_KERNEL}")
  add_custom_target(${TARG_NAME} ${CLANG} ${CLANG_ARG_LIST}
     COMMENT "BUILDING KERNEL..."
     VERBATIM)
  aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/${S_NAME} S_NAME_SOURCES)
  add_executable(${SNAME_EXE} ${S_NAME_SOURCES})
  target_link_libraries(${SNAME_EXE} ${ROCR_LIBS} c stdc++ dl pthread rt)
endfunction(process_sample)


###########################
# SAMPLE SPECIFIC SECTION 
###########################
set(KERN_SUFFIX "kernels.hsaco")
set(BITCODE_PREF "-Xclang -mlink-bitcode-file -Xclang")
set(BITCODE_PREF "${BITCODE_PREF} ${OPENCL_DIR}/lib/x86_64/bitcode")

set(COMMON_BITCODE_LIBS "${BITCODE_PREF}/opencl.amdgcn.bc")
set(COMMON_BITCODE_LIBS "${COMMON_BITCODE_LIBS} ${BITCODE_PREF}/ockl.amdgcn.bc")

# Binary Search
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(BITCODE_LIBS "${BITCODE_LIBS} ${BITCODE_PREF}/ocml.amdgcn.bc")
set(CL_FILE_LIST "${PROJECT_SOURCE_DIR}/binary_search/binary_search_kernels.cl")
process_sample("binary_search")

# P2P Memory Access
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${PROJECT_SOURCE_DIR}/p2p_mem_access/p2p_mem_access_kernels.cl")
process_sample("p2p_mem_access")

# RocR Info
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/rocrinfo ROCR_INFO_SOURCES)
add_executable(rocrinfo ${ROCR_INFO_SOURCES})
target_link_libraries(rocrinfo ${ROCR_LIBS} c stdc++ dl pthread rt)

# IPC
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ipc IPC_SOURCES)
add_executable(ipc ${IPC_SOURCES})
target_link_libraries(ipc ${ROCR_LIBS} c stdc++ dl pthread rt)

# Async Mem. Copy
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/async_mem_copy AMC_SOURCES)
add_executable(async_mem_copy ${AMC_SOURCES})
target_link_libraries(async_mem_copy ${ROCR_LIBS} c stdc++ dl pthread rt)

add_custom_target(sample_kernels DEPENDS ${HSACO_TARG_LIST})
install(TARGETS ${SAMPLE_EXE}
        ARCHIVE DESTINATION ${PROJECT_BINARY_DIR}/lib
        LIBRARY DESTINATION ${PROJECT_BINARY_DIR}/lib
        RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)
