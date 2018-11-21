#
# Minimum version of cmake required
#
cmake_minimum_required(VERSION 2.8.0)

#
# GCC 4.8 or higher compiler required.
#

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
#   1) Create build folder e.g. "rocrtst/build" - any name will do
#   2) Cd into build folder
#   3) Run "cmake .."
#   4) Run "make"
#

set(DEFAULT_TARGET "gfx803")

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
# Process input variables
#

# Required Defines first:

set(ROCR_INC_DIR ${ROCM_DIR}/include)
set(ROCR_LIB_DIR ${ROCM_DIR}/lib)
#
# Determine ROCR Header files are present
#
if(NOT EXISTS ${ROCR_INC_DIR}/hsa/hsa.h)
  message("ERROR: ${ROCR_INC_DIR}/hsa/hsa.h does not exist. Check value of ROCM_DIR define")
  return()
endif()

# Determine ROCR Library files are present
#
if("${ROCRTST_BLD_BITS}" STREQUAL 32)
  set (ONLY64STR "")
  set (IS64BIT 0)
else()
  set (ONLY64STR "64")
  set (IS64BIT 1)
endif()
#
if (${IS64BIT} EQUAL 0)
  if(NOT EXISTS ${ROCR_LIB_DIR}/libhsa-runtime.so)
    message("ERROR: ${ROCR_LIB_DIR}/libhsa-runtime.so doesn't exist. Check value of ROCM_DIR define")
    return()
  endif()
else()
  if(NOT EXISTS ${ROCR_LIB_DIR}/libhsa-runtime64.so)
    message("ERROR: Define ROCR_LIB_DIR pointing to ROCR libraries is not set")
    return()
  endif()
endif()

if (DEFINED LLVM_DIR)
  set(CLANG ${LLVM_DIR}/clang)
  if (NOT EXISTS ${CLANG})
    message("ERROR: path to clang (${CLANG}) is not valid. Is define LLVM_DIR correct?")
    return()
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

if (NOT DEFINED TARGET_DEVICES)
  message("WARNING: No targets devices provided on command line")
  message("  e.g., cmake -DTARGET_DEVICES=\"gfx803;gfx900;gfx...\" ..")
  message("  Using default target of $DEFAULT_TARGET")
  list(APPEND TARGET_DEVICES "gfx803")
endif() 

string(TOLOWER "${ROCRTST_BLD_TYPE}" tmp)
if("${tmp}" STREQUAL release)
  set(BUILD_TYPE "Release")
  set(ISDEBUG 0)
else()
  set(BUILD_TYPE "Debug")
  set(ISDEBUG 1)
endif()

if(${EMULATOR_BUILD})
add_definitions(-DROCRTST_EMULATOR_BUILD=1)
endif()

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
message("------Target Devices: ${TARGET_DEVICES}")
message("----------Clang path: " ${CLANG})
message("----------OpenCL Dir: " ${OPENCL_DIR})
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
##
# Specify the directory containing various libraries of ROCR
# to be linked against for building ROC Perf applications
#
link_directories(${ROCR_LIB_DIR})

#
# Extend the list of libraries to be used for linking ROC Perf Apps
#
set(ROCR_LIBS ${ROCR_LIBS} hsa-runtime${ONLY64STR})

include_directories(${ROCR_INC_DIR} ${OPENCL_INC_DIR})

# Use this function to build any samples that have kernels to be built
function(process_sample S_NAME TARG_DEV HAS_KERNEL)
  set(KERNEL_DIR ${PROJECT_BINARY_DIR}/${TARG_DEV})
  set(SNAME_KERNEL "${S_NAME}_kernels.hsaco")
  
  set(TARG_NAME "${S_NAME}_hsaco.${TARG_DEV}")
  set (HSACO_TARG_LIST ${HSACO_TARG_LIST} ${TARG_NAME}
                                               CACHE INTERNAL HSACO_TARG_LIST)
  
  if (${HAS_KERNEL})
    # Build the kernel
    separate_arguments(CLANG_ARG_LIST UNIX_COMMAND
     "-x cl -Xclang -finclude-default-header -target amdgcn-amdh-amdhsa -mcpu=${TARG_DEV} -mno-code-object-v3 ${BITCODE_LIBS} -cl-std=CL${OPENCL_VER} ${CL_FILE_LIST} -o ${KERNEL_DIR}/${SNAME_KERNEL}")
    add_custom_target("${TARG_NAME}" ${CLANG} ${CLANG_ARG_LIST} COMMAND
      ${CMAKE_COMMAND} -E create_symlink
        "../${SNAME_EXE}" "${KERNEL_DIR}/${SNAME_EXE}"
       COMMENT "BUILDING KERNEL..."
      VERBATIM)
   else()
    # No kernel to build, but we need to set up symlinks; we'll use the hsaco
    # targ name, even though we aren't building an hsaco
    add_custom_target("${TARG_NAME}"
       ${CMAKE_COMMAND} -E create_symlink
             "../${SNAME_EXE}" "${KERNEL_DIR}/${SNAME_EXE}"
       COMMENT "NO KERNEL TO BUILD; SYMLINK ONLY..."
      VERBATIM)
   endif()
endfunction(process_sample)

function(build_sample_for_devices S_NAME HAS_KERNEL)
  set(SNAME_EXE "${S_NAME}")

  aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/${S_NAME} S_NAME_SOURCES)
  add_executable(${SNAME_EXE} ${S_NAME_SOURCES})
  target_link_libraries(${SNAME_EXE} ${ROCR_LIBS} c stdc++ dl pthread rt)

  set(HSACO_TARG_LIST PARENT_SCOPE)

  foreach(t ${TARGET_DEVICES})
    process_sample(${S_NAME} ${t} ${HAS_KERNEL})
  endforeach(t)
endfunction(build_sample_for_devices)

function(add_symlink_to_exe DST)
  foreach(td ${TARGET_DEVICES})
  endforeach(td)
endfunction(add_symlink_to_exe)


# Make directories for each possible target device
foreach(td ${TARGET_DEVICES})
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${td})
endforeach(td)

###########################
# SAMPLE SPECIFIC SECTION 
###########################

set (HSACO_TARG_LIST "" CACHE INTERNAL HSACO_TARG_LIST)

set(KERN_SUFFIX "kernels.hsaco")
set(BITCODE_PREF "-Xclang -mlink-bitcode-file -Xclang")
set(BITCODE_PREF "${BITCODE_PREF} ${OPENCL_LIB_DIR}/bitcode")

set(COMMON_BITCODE_LIBS "${BITCODE_PREF}/opencl.amdgcn.bc")
set(COMMON_BITCODE_LIBS
                      "${COMMON_BITCODE_LIBS} ${BITCODE_PREF}/ockl.amdgcn.bc")

# Binary Search
set(BITCODE_LIBS "${COMMON_BITCODE_LIBS}")
set(BITCODE_LIBS "${BITCODE_LIBS} ${BITCODE_PREF}/ocml.amdgcn.bc")
set(CL_FILE_LIST
               "${PROJECT_SOURCE_DIR}/binary_search/binary_search_kernels.cl")
build_sample_for_devices("binary_search" TRUE)

# RocR Info
build_sample_for_devices("rocrinfo" FALSE)

# IPC
build_sample_for_devices("ipc" FALSE)

# Async Mem. Copy
build_sample_for_devices("async_mem_copy" FALSE)

add_custom_target(sample_kernels DEPENDS ${HSACO_TARG_LIST})
install(TARGETS ${SAMPLE_EXE}
        ARCHIVE DESTINATION ${PROJECT_BINARY_DIR}/lib
        LIBRARY DESTINATION ${PROJECT_BINARY_DIR}/lib
        RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)
