#
# Minimum version of cmake required
#
cmake_minimum_required ( VERSION 3.5.0 )

#
# Setup flag to be verbose or not
#
set ( CMAKE_VERBOSE_MAKEFILE TRUE CACHE BOOL "Verbose Output" FORCE )

#
# Set name for the project
# @note: Must come before adding any sub-directories
#
set ( TARGET_NAME "aqlprofile" )
project ( ${TARGET_NAME} )

if ( NOT DEFINED PROJ_DIR )
  set ( PROJ_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
  set ( ROOT_DIR ${PROJ_DIR}/.. )
endif ()

set ( HSA_RUNTIME_DIR ${PROJ_DIR}/../../hsa-runtime )
set ( API_DIR ${HSA_RUNTIME_DIR}/inc )
set ( CORE_UTIL_DIR ${HSA_RUNTIME_DIR}/core/util )

include_directories ( ${ROOT_DIR} )

#
# Validate required build environment is setup correctly
#
include ( ${ROOT_DIR}/cmake_modules/validateBldEnv.cmake )

#
# Setup tool chain flags - preprocessor, compiler and linker
#
include ( ${ROOT_DIR}/cmake_modules/exportToolFlags.cmake )

#
# Set Name for Cmdwriter library and build it as a
# static library to be linked with others
#
set ( CMDWRITER_LIB "commandwriter${ONLY64STR}" )
add_subdirectory ( ${PROJ_DIR}/commandwriter "${PROJECT_BINARY_DIR}/commandwriter" )

#
# Set Name for ThreadTrace library and build it as a
# static library to be linked with others
#
set ( SQTT_LIB "sqtt${ONLY64STR}" )
add_subdirectory ( ${PROJ_DIR}/threadtrace "${PROJECT_BINARY_DIR}/threadtrace" )

#
# Set Name for Profiler library and build it as a
# static library to be linked with others
#
set ( PMC_LIB "pmc${ONLY64STR}" )
add_subdirectory ( ${PROJ_DIR}/perfcounter "${PROJECT_BINARY_DIR}/perfcounter" )

#
# Build the library and link it with other static
# libraries that have been built in this regard
#
set ( TARGET_LIB "${TARGET_NAME}${ONLY64STR}" )
add_subdirectory ( ${PROJ_DIR}/core "${PROJECT_BINARY_DIR}/core" )

#
# Creating the library link
#
execute_process ( COMMAND sh -xc "/bin/ln -s core/lib${TARGET_LIB}.so libhsa-amd-${TARGET_LIB}.so.1" )
