#
# Minimum version of cmake required
#
cmake_minimum_required(VERSION 2.8.0)

#
#   Required Defines on cmake command line
#
#   1) Set location of ROCR header files
#
#      ROCM_DIR="Root for RocM install"
#
#   2) Set ROCRTST_BLD_TYPE to either "Debug" or "Release".
#      If not set, the default value is "Debug" is bound.
#
#      ROCRTST_BLD_TYPE=Debug or ROCRTST_BLD_TYPE=Release
#
#   3) Set ROCRTST_BLD_BITS to either "32" or "64"
#      If not set, the default value of "64" is bound.
#
#       ROCRTST_BLD_BITS=32 or ROCRTST_BLD_BITS=64
#
#   4) Set TARGET_DEVICES to indicate gpu types for kernel
#      builds (e.g., "gfx803;gfx900; ...")
#
#   Building rocrtst Suite
#
#
#   1) Create build folder e.g. "rocrtst/build" - any name will do
#   2) Cd into build folder
#   3) Run "cmake .."
#   4) Run "make"
#

cmake_minimum_required(VERSION 3.5.0)

# Set Name for Samples Project
#

set(PROJECT_NAME "rocrtst64")
project (${PROJECT_NAME})

# For DEB/RPM generation
set ( CPACK_PACKAGE_FILE_NAME "rocrtst" )
set ( CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc." )
set ( CPACK_PACKAGE_DESCRIPTION "This package includes rocrtst and a convenience script to run the test suite" )
set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "Test suite for ROCr" )
set ( CPACK_PACKAGE_VERSION_MAJOR "1" )
set ( CPACK_PACKAGE_VERSION_MINOR "0" )
set ( CPACK_PACKAGE_VERSION_PATCH "0" )
set ( CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"  CACHE STRING "Default packaging prefix." )
set ( CPACK_GENERATOR "DEB;RPM"  CACHE STRING "Default packaging generators." )
set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RadeonOpenCompute/ROCR-Runtime" )
set ( CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/RadeonOpenCompute/ROCR-Runtime" )


set(DEFAULT_TARGETS "gfx700;gfx701;gfx702;gfx801;gfx802;gfx803;gfx805;gfx810;gfx900;gfx902;gfx904;gfx906;gfx908;gfx909;gfx90a;gfx90c;gfx1010;gfx1011;gfx1012;gfx1030;gfx1031;gfx1032;gfx1033")

#
# Currently support for Windows platform is not present
#

#############################
# COMMON AREA
#############################
if(WIN32)
  message("rocrtst Suite is not supported on Windows platform")
  return()
endif()

#
# Process input variables
#

# Required Defines first:
find_package(hsa-runtime64 REQUIRED )
message(STATUS "HSA Runtime found at ${hsa-runtime64_DIR} ")

set(ROCR_INC_DIR ${hsa-runtime64_DIR}/../../../include)
set(ROCM_SMI_INC_DIR ${ROCM_DIR}/rocm_smi/include)
set(ROCM_SMI_LIB_DIR ${ROCM_DIR}/rocm_smi/lib)
set (ONLY64STR "64")

if (DEFINED LLVM_DIR)
  set(CLANG ${LLVM_DIR}/clang)
  if (NOT EXISTS ${CLANG})
    # SPK temp until Jenkins script input is corrected.
    set (CLANG ${OPENCL_DIR}/bin/clang)
    if (NOT EXISTS ${CLANG})
    message("ERROR: path to clang (${CLANG}) is not valid. Is define LLVM_DIR correct?")
    return()
    endif()
  endif()
else()
    message("WARNING: LLVM_DIR define is not set. Kernels will not be built.")
endif()

if (DEFINED OPENCL_DIR)
  set(OPENCL_INC_DIR ${OPENCL_DIR}/include)
  set(OPENCL_LIB_DIR ${OPENCL_DIR}/lib)
else()
    message("WARNING: OPENCL_DIR define is not set. Kernels will not be built.")
endif()

if (DEFINED OPENCL_VER)
  set(OPENCL_VER ${OPENCL_VER})
else()
  message("OPENCL_VER define is not set. Using default")
  set(OPENCL_VER "2.0")
endif()

if(NOT EXISTS "${OPENCL_INC_DIR}/opencl-c.h")
  if(DEFINED ENV{WORK_ROOT})
    set(OPENCL_INC_DIR "$ENV{WORK_ROOT}/external/llvm-project/clang/lib/Headers/")
  else()
    set(OPENCL_INC_DIR "${OPENCL_DIR}/../../../external/llvm-project/clang/lib/Headers/")
  endif()
  if(NOT EXISTS "${OPENCL_INC_DIR}/opencl-c.h")
    message(WARNING "opencl-c.h not found.")
  endif()
endif()

if (NOT DEFINED TARGET_DEVICES)
  message("No targets devices provided on command line")
  message("  e.g., cmake -DTARGET_DEVICES=\"gfx803;gfx900;gfx...\" ..")
  message("  Using default target of ${DEFAULT_TARGETS}")
  list(APPEND TARGET_DEVICES ${DEFAULT_TARGETS})
endif()

string(TOLOWER "${ROCRTST_BLD_TYPE}" tmp)
if("${tmp}" STREQUAL release)
  set(BUILD_TYPE "Release")
  set(ISDEBUG 0)
else()
  set(BUILD_TYPE "Debug")
  set(ISDEBUG 1)
endif()

find_path(BITCODE_DIR NAMES "opencl.bc" "opencl.amdgcn.bc"
  PATHS
    "${ROCM_DIR}/amdgcn/bitcode"
    "${ROCM_DIR}/lib/bitcode"
    "${ROCM_DIR}/lib"
    "${ROCM_DIR}/lib/x86_64/bitcode"
    "${OPENCL_DIR}/amdgcn/bitcode"
    "${OPENCL_DIR}/lib/x86_64/bitcode"
    "${LLVM_DIR}/../lib/bitcode"
    "${CMAKE_PREFIX_PATH}/amdgcn/bitcode"
    "${CMAKE_PREFIX_PATH}/lib/bitcode"
    "${CMAKE_PREFIX_PATH}/lib/x86_64/bitcode")

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
message("------Target Devices: ${TARGET_DEVICES}")
message("----------Clang path: " ${CLANG})
message("----------OpenCL Dir: " ${OPENCL_DIR})
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
set(CMAKE_VERBOSE_MAKEFILE on)
#
# Compiler pre-processor definitions.
#
# Define MACRO "DEBUG" if build type is "Debug"
if(${BUILD_TYPE} STREQUAL "Debug")
add_definitions(-DDEBUG)
endif()

if(${EMULATOR_BUILD})
add_definitions(-DROCRTST_EMULATOR_BUILD=1)
endif()


add_definitions(-D__linux__)
add_definitions(-DLITTLEENDIAN_CPU=1)

#
# Linux Compiler options
#
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-math-errno")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmerge-all-constants")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-extensions")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")

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
add_subdirectory(${ROCRTST_ROOT}/gtest "${PROJECT_BINARY_DIR}/gtest" EXCLUDE_FROM_ALL)
set (ROCRTST_LIBS ${ROCRTST_LIBS} ${GOOGLE_TEST_FRWK_NAME})

MESSAGE("ROCRTST_LIBS SET STEP:Done")
#
#
# Other source directories
aux_source_directory(${ROCRTST_ROOT}/common common_srcs)

#
# Specify the directory containing various libraries of ROCR
# to be linked against for building ROC Perf applications
#
LINK_DIRECTORIES(${ROCR_LIB_DIR} ${ROCM_SMI_LIB_DIR})

#
# Extend the list of libraries to be used for linking ROC Perf Apps
#
set(ROCRTST_LIBS ${ROCRTST_LIBS} hsa-runtime64::hsa-runtime64)
set(ROCRTST_LIBS ${ROCRTST_LIBS} rocm_smi${ONLY64STR})


# Set Name for rocrtst
MESSAGE(${ROCRTST_LIBS})
set(ROCRTST "rocrtst${ONLY64STR}")

#
# Source files for building rocrtst
#
aux_source_directory(${ROCRTST_ROOT}/suites/performance performanceSources)
aux_source_directory(${ROCRTST_ROOT}/suites/functional functionalSources)
aux_source_directory(${ROCRTST_ROOT}/suites/negative negativeSources)
aux_source_directory(${ROCRTST_ROOT}/suites/stress stressSources)
aux_source_directory(${ROCRTST_ROOT}/suites/test_common testCommonSources)

# Header file include path

include_directories(${ROCR_INC_DIR})
include_directories(${ROCRTST_ROOT})
include_directories(${ROCRTST_ROOT}/gtest/include)
include_directories(${ROCM_SMI_INC_DIR})

# Use this function to build any samples that have kernels to be built
function(build_kernel S_NAME TARG_DEV)
  set(KERNEL_DIR ${PROJECT_BINARY_DIR}/${TARG_DEV})
  set(SNAME_KERNEL "${S_NAME}_kernels.hsaco")

  set(TARG_NAME "${S_NAME}_hsaco.${TARG_DEV}")
  set(HSACO_TARG_LIST ${HSACO_TARG_LIST} ${TARG_NAME}
                                                 CACHE INTERNAL HSA_TARG_LIST)
  separate_arguments(CLANG_ARG_LIST UNIX_COMMAND
   "-x cl -target amdgcn-amd-amdhsa -include ${OPENCL_INC_DIR}opencl-c.h -mcpu=${TARG_DEV} ${BITCODE_ARGS} -cl-std=CL${OPENCL_VER} ${CL_FILE_LIST} -o ${KERNEL_DIR}/${SNAME_KERNEL}")
  add_custom_target("${TARG_NAME}" ${CLANG} ${CLANG_ARG_LIST} COMMAND
      ${CMAKE_COMMAND} -E create_symlink
        "../${ROCRTST}" "${KERNEL_DIR}/${ROCRTST}"
     COMMENT "BUILDING KERNEL..."
     VERBATIM)
endfunction(build_kernel)

function(build_sample_for_devices S_NAME)
  set(HSACO_TARG_LIST PARENT_SCOPE)

  foreach(t ${TARGET_DEVICES})
    build_kernel(${S_NAME} ${t})
  endforeach(t)
endfunction(build_sample_for_devices)


# Make directories for each possible target device
foreach(td ${TARGET_DEVICES})
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${td})
endforeach(td)

######################
# Kernel Build Section
######################
set (HSACO_TARG_LIST "" CACHE INTERNAL HSACO_TARG_LIST)

set(KERN_SUFFIX "kernels.hsaco")

# Check if device-libs bitcode is following old or new layout
if(EXISTS "${BITCODE_DIR}/opencl.amdgcn.bc")
  set(BITCODE_ARGS "-nogpulib
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/opencl.amdgcn.bc
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/ockl.amdgcn.bc
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/ocml.amdgcn.bc")
else()
  set(BITCODE_ARGS "--hip-device-lib-path=${BITCODE_DIR}")
endif()

# Test Case Template example
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/test_case_template_kernels.cl")
build_sample_for_devices("test_case_template")

# P2P Memory Access
#set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
#set(CL_FILE_LIST "${KERNELS_DIR}/p2p_mem_access_kernels.cl")
#build_sample_for_devices("p2p_mem_access")

# Dispatch Time
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/dispatch_time_kernels.cl")
build_sample_for_devices("dispatch_time")

# gpuReadWrite
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/gpuReadWrite_kernels.cl")
build_sample_for_devices("gpuReadWrite")


# Vector Add Debug Trap
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/vector_add_debug_trap_kernel.cl")
build_sample_for_devices("vector_add_debug_trap")

# Vector Add Memory Fault
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/vector_add_memory_fault_kernel.cl")
build_sample_for_devices("vector_add_memory_fault")

# atomic_add_kernels
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/atomicOperations_kernels.cl")
build_sample_for_devices("atomicOperations")

# Signal Operations
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/signal_operations.cl")
build_sample_for_devices("signal_operations")

# groupMemoryDynamic
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(CL_FILE_LIST "${KERNELS_DIR}/groupMemoryDynamic_kernels.cl")
build_sample_for_devices("groupMemoryDynamic")

# Build rules
add_executable(${ROCRTST} ${performanceSources} ${functionalSources} ${negativeSources} ${stressSources}
                                           ${common_srcs} ${testCommonSources})

target_link_libraries(${ROCRTST} ${ROCRTST_LIBS} c stdc++ dl pthread rt numa hwloc)

add_custom_target(rocrtst_kernels DEPENDS ${HSACO_TARG_LIST})
install(TARGETS ${ROCRTST}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
include ( CPack )
