#
# Minimum version of cmake required
#
cmake_minimum_required(VERSION 3.5.0)

#
#   Required Defines on cmake command line
#
#   1) Set location of OpenCL header files
#        OPENCL_DIR="Root for OpenCL install"
#      If not set, the default value is "/opt/rocm/opencl"
#
#   2) Set location of CLANG/LLVM binary directory
#        LLVM_DIR="Directory contains clang, llvm-link and llvm-dis
#      If not set, the default value is "<PROJECT_BUILD_DIR>/lightning/bin"
#
#   3) Set BITCODE library directory
#        BITCODE_DIR="Directory contains the bitcode library"
#      If not set, the default value is "${OPENCL_DIR}/lib/x86_64/bitcode"
#
#   4) Set TARGET_DEVICES to indicate gpu types for kernel builds (e.g., "gfx803;gfx900; ...")
#      If not set, the target devices are those have the Open Compute Library Controls (OCLC)
#      bitcode file, "oclc_isa_version_*.amdgcn.bc", in the BITCODE directory
#
#   Building - Should be automatic but for manual builds:
#
#   1) *** Create build folder e.g. "blit_src/build" - any name will do
#   2) Go to the build folder
#   3) Run "cmake .."
#   4) Run "make opencl_blit_objects.cpp"
#

## Include the cmake_modules utils.cmake
list ( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules" )
include ( utils )

# Flag to abort before executing after default initialization of cache variables
set (QUIT 0)

# Collect possible LLVM version directories.
set (LLVM_SEARCH_PATHS "")
set (LLVM_SEARCH_ROOT "${CMAKE_INSTALL_PREFIX}/llvm/lib/clang")
listsubdirs(${LLVM_SEARCH_ROOT} FOLDERS)
foreach(ITEM IN LISTS FOLDERS)
    list (APPEND LLVM_SEARCH_PATHS "${LLVM_SEARCH_ROOT}/${ITEM}/include/")
endforeach()

if (NOT DEFINED OPENCL_VER)
  set (OPENCL_VER "2.0")
endif()
set( OPENCL_VER ${OPENCL_VER} CACHE STRING "OpenCL version" FORCE )

get_include_path(BITCODE_DIR "Bitcode library path" RESULT FOUND NAMES "opencl.amdgcn.bc" HINTS "${CMAKE_INSTALL_PREFIX}/lib/bitcode" "${OPENCL_DIR}/lib/x86_64/bitcode")
if (NOT ${FOUND})
  set (QUIT 1)
endif()

set (BITCODE_LIB "${BITCODE_DIR}/opencl.amdgcn.bc")
if (NOT EXISTS ${BITCODE_LIB})
  message("ERROR: path to opencl.amdgcn.bc (${BITCODE_LIB}) is not valid. Is BITCODE_DIR correctly defined?")
  set (QUIT 1)
endif()

get_include_path(LLVM_DIR "LLVM directory" RESULT FOUND NAMES "clang" HINTS "${CMAKE_INSTALL_PREFIX}/llvm/bin")
if (NOT ${FOUND})
  set (QUIT 1)
endif()

set (CLANG "${LLVM_DIR}/clang")
if (NOT EXISTS ${CLANG})
  message("ERROR: path to clang (${CLANG}) is not valid. Is LLVM_DIR correctly defined?")
  set (QUIT 1)
endif()

set (LLVM_LINK "${LLVM_DIR}/llvm-link")
if (NOT EXISTS ${LLVM_LINK})
  message("ERROR: path to llvm-link (${LLVM_LINK}) is not valid. Is LLVM_DIR correctly defined?")
  set (QUIT 1)
endif()

set (LLVM_DIS "${LLVM_DIR}/llvm-dis")
if (NOT EXISTS ${LLVM_DIS})
  message("ERROR: path to llvm-dis (${LLVM_DIS}) is not valid. Is LLVM_DIR correctly defined?")
  set (QUIT 1)
endif()

# Value of Images Src Dir is bound in parent environment
set (KERNELS_DIR "${IMAGE_SOURCE_DIR}/blit_src")

# Define the target devices with xnack enable
if (NOT DEFINED XNACK_DEVS)
  set (XNACK_DEVS "gfx801;gfx902")
endif()
set( XNACK_DEVS ${XNACK_DEVS} CACHE STRING "XNACK targets" FORCE )

# Determine the target devices if not specified
if (NOT DEFINED TARGET_DEVICES)
  set (TARGET_DEVICES "gfx700;gfx701;gfx702;gfx801;gfx802;gfx803;gfx900;gfx902;gfx904;gfx906;gfx908;gfx1010;gfx1011;gfx1012")
endif()
set( TARGET_DEVICES ${TARGET_DEVICES} CACHE STRING "Build targets" FORCE )

# End of default configuration and path checking.
# Quit if configuration is incomplete.
if (QUIT)
  message(FATAL_ERROR "Configuration halted.")
  return()
endif()

set(TARGET_TRIPLE "amdgcn-amd-amdhsa")

message("")
message("Build Setting:")
message("   Target Devices: ${TARGET_DEVICES}")
message("    Proj. Src Dir: ${PROJECT_SOURCE_DIR}")
message("    Proj. Bld Dir: ${PROJECT_BINARY_DIR}")
message(" Image Source Dir: ${IMAGE_SOURCE_DIR}")
message("         LLVM Dir: ${LLVM_DIR}")
message("       Clang path: ${CLANG}")
message("       OpenCL Dir: ${OPENCL_DIR}")
message("   OpenCL version: ${OPENCL_VER}")
message("      Bitcode Dir: ${BITCODE_DIR}")
message("    Target Triple: ${TARGET_TRIPLE}")

##==========================================
##  Generate Kernel Bitcode
##==========================================
function(gen_kernel_bc TARGET_DEV XNACK_OPT FPREFIX INPUT_FILE OUTPUT_FILE)

  string (REPLACE "gfx" "" GFXIP "${TARGET_DEV}")
  separate_arguments(CLANG_ARG_LIST UNIX_COMMAND
    "-O2 -x cl -target ${TARGET_TRIPLE} -Xclang -finclude-default-header -mcpu=${TARGET_DEV} -m${XNACK_OPT}
    -nogpulib
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/opencl.amdgcn.bc
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/ockl.amdgcn.bc
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/ocml.amdgcn.bc
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/oclc_daz_opt_on.amdgcn.bc
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/oclc_isa_version_${GFXIP}.amdgcn.bc
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/oclc_unsafe_math_off.amdgcn.bc
    -Xclang -mlink-bitcode-file -Xclang ${BITCODE_DIR}/oclc_finite_only_off.amdgcn.bc
    -cl-std=CL${OPENCL_VER} -o ${OUTPUT_FILE} ${INPUT_FILE}")

  add_custom_target("${FPREFIX}" ${CLANG} ${CLANG_ARG_LIST}
    COMMENT "BUILDING bitcode for ${FPREFIX}..."
    VERBATIM)

  message("      Kernel Source: " ${INPUT_FILE})
  message("     Kernel Bitcode: " ${OUTPUT_FILE})

endfunction(gen_kernel_bc)

##==========================================
## Build the kernel for a device
##==========================================
function(build_kernel BLIT_NAME TARG_DEV)
  
  list (FIND XNACK_DEVS ${TARG_DEV} XNACK_IDX)
  if (${XNACK_IDX} GREATER -1)
    set (XNACK_OPT "xnack")
  else()
    set (XNACK_OPT "no-xnack")
  endif()

  set (FILE_PREFIX "${BLIT_NAME}_${TARG_DEV}")
  set (HSACO_TARG_LIST ${HSACO_TARG_LIST} "${FILE_PREFIX}" CACHE INTERNAL HSACO_TARG_LIST)

  ##  generate kernel bitcodes
  ##
  set (CL_FILE  "${KERNELS_DIR}/imageblit_kernels.cl")
  set (KERNEL_BC_FILE "${FILE_PREFIX}")
  gen_kernel_bc(${TARG_DEV} ${XNACK_OPT} ${FILE_PREFIX} ${CL_FILE} ${KERNEL_BC_FILE})

endfunction(build_kernel)


##==========================================
## Build the kernel for a list of devices
##==========================================
function(build_kernel_for_devices BLIT_NAME)

  set(HSACO_TARG_LIST PARENT_SCOPE)

  foreach(dev ${TARGET_DEVICES})
    message("\n  Working on: ${dev} ...")
    build_kernel(${BLIT_NAME} ${dev})
  endforeach(dev)

endfunction(build_kernel_for_devices)

##==========================================
## Create BLIT Code Object blobs file
##==========================================
function(generate_blit_file BFILE)

  file(REMOVE ${IMAGE_SOURCE_DIR}/${BFILE})

  add_custom_command(OUTPUT ${IMAGE_SOURCE_DIR}/${BFILE}
                     COMMAND ${KERNELS_DIR}/create_hsaco_ascii_file.sh ${IMAGE_SOURCE_DIR}/${BFILE})

  message("\n  Will create ASCII bitcodes in ${BFILE} for ${TARGET_DEVICES} ... \n")
  add_custom_target(${BFILE} DEPENDS ${HSACO_TARG_LIST} ${IMAGE_SOURCE_DIR}/${BFILE})

endfunction(generate_blit_file)

build_kernel_for_devices("ocl_blit_object")
generate_blit_file("opencl_blit_objects.cpp")
