cmake_minimum_required ( VERSION 3.5.0 )

## Verbose output.
set ( CMAKE_VERBOSE_MAKEFILE on )

## Set ext runtime module name and project name.
set ( TOOLS_NAME "hsa-runtime-tools" )
set ( TOOLS_TARGET "${TOOLS_NAME}64" )
project ( ${TOOLS_TARGET} )

## Include the cmake_modules utils.cmake
list ( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules" )
include ( utils )

## Compiler preproc definitions.
add_definitions ( -D__linux__ )
add_definitions ( -DUNIX_OS )
add_definitions ( -DLINUX )
add_definitions ( -D__AMD64__ )
add_definitions ( -D__x86_64__ )
add_definitions ( -DAMD_INTERNAL_BUILD )
add_definitions ( -DLITTLEENDIAN_CPU=1 )
add_definitions ( -D HSA_DEPRECATED= )

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

## Find the hsakmt library and include files
find_file ( HSAKMT_INC "libhsakmt/hsakmt.h" )
find_library ( HSAKMT_LIB libhsakmt.so )
get_filename_component ( HSAKMT_LIB_PATH ${HSAKMT_LIB} DIRECTORY )
get_filename_component ( HSAKMT_INC_PATH ${HSAKMT_INC} DIRECTORY )
include_directories ( ${HSAKMT_BUILD_INC_PATH} )
link_directories (${HSAKMT_BUILD_LIB_PATH})

## Find the hsa-runtime and include files
find_file ( HSA_INC "hsa/hsa.h" )
find_library ( HSA_LIB libhsa-runtime64.so )
get_filename_component ( HSA_LIB_PATH ${HSA_LIB} DIRECTORY )
get_filename_component ( HSA_INC_PATH ${HSA_INC} DIRECTORY )
include_directories ( ${HSA_INC_PATH} )
link_directories (${HSA_LIB_PATH})

## External dependencies
if ( NOT DEFINED REG_INCLUDE )
  set(REG_INCLUDE ${HSA_CLOSED_SOURCE_DIR}/drivers/inc/asic_reg)
endif()

if ( NOT EXISTS ${REG_INCLUDE}/si_id.h )
  MESSAGE ( FATAL_ERROR "Environment variable REG_INCLUDE is not set appropriately." )
endif ()

if ( NOT DEFINED HSAKMT_INCLUDE )
  set(HSAKMT_INCLUDE ${HSA_CLOSED_SOURCE_DIR}/drivers/hsathk/include)
endif()

if ( NOT EXISTS ${HSAKMT_INCLUDE}/hsakmt.h )
  MESSAGE ( FATAL_ERROR "Environment variable REG_INCLUDE is not set appropriately." )
endif ()

if( NOT DEFINED TOOLS_SOURCE_DIR )
  set ( TOOLS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
endif()

if( NOT DEFINED OPEN_SOURCE_DIR )
  set ( OPEN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." )
endif()

## ------------------------- Linux Compiler and Linker options -------------------------
set ( CMAKE_CXX_FLAGS "-std=c++11 ")

set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Werror -fexceptions -fno-rtti -fvisibility=hidden -Wno-error=sign-compare -Wno-error=enum-compare -Wno-sign-compare -Wno-write-strings -Wno-deprecated-declarations -Wno-conversion-null -fno-math-errno -fno-threadsafe-statics -fmerge-all-constants -fms-extensions -Wno-error=comment -Wno-comment -Wno-error=pointer-arith -Wno-pointer-arith -fPIC" )

if ( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64  -msse -msse2" )
elseif ( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86" )
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" )
endif ()

if ( "${CMAKE_BUILD_TYPE}" STREQUAL Debug )
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb" )
endif ()

set ( DRVDEF "${TOOLS_SOURCE_DIR}/make/hsatools.so.def" )

set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bdynamic -Wl,-z,noexecstack -Wl,--version-script=${DRVDEF}" )

set ( CMAKE_SKIP_BUILD_RPATH TRUE )

## Library path(s).
include_directories ( ${REG_INCLUDE} )
include_directories ( ${HSAKMT_INCLUDE} )
include_directories ( ${TOOLS_SOURCE_DIR} )
include_directories ( ${TOOLS_SOURCE_DIR}/.. )
include_directories ( ${OPEN_SOURCE_DIR}/hsa-runtime )
include_directories ( ${OPEN_SOURCE_DIR}/hsa-runtime/inc )
include_directories ( ${OPEN_SOURCE_DIR}/hsa-runtime/core/inc )
include_directories ( ${TOOLS_SOURCE_DIR}/inc )
include_directories ( ${TOOLS_SOURCE_DIR}/commandwriter )
include_directories ( ${TOOLS_SOURCE_DIR}/commandwriter/include/si )
include_directories ( ${TOOLS_SOURCE_DIR}/common )
include_directories ( ${TOOLS_SOURCE_DIR}/debugger )
include_directories ( ${TOOLS_SOURCE_DIR}/intercept )
include_directories ( ${TOOLS_SOURCE_DIR}/profiler )
include_directories ( ${TOOLS_SOURCE_DIR}/threadtrace )
include_directories ( ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000 )
include_directories ( ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/ci )
include_directories ( ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/si )
include_directories ( ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gen )
include_directories ( ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx8 )
include_directories ( ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx81 )
include_directories ( ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx9 )

set ( CORE_SRC   ${OPEN_SOURCE_DIR}/hsa-runtime/core/common/shared.cpp
                 ${OPEN_SOURCE_DIR}/hsa-runtime/core/common/hsa_table_interface.cpp )

set ( CMDWRITER_SRC   ${TOOLS_SOURCE_DIR}/commandwriter/aql_hw_cmdwriter.cpp
                      ${TOOLS_SOURCE_DIR}/commandwriter/ci_aql_common.cpp
                      ${TOOLS_SOURCE_DIR}/commandwriter/gfx9_cmdwriter.cpp
                      ${TOOLS_SOURCE_DIR}/commandwriter/gfx9_factory.cpp
                      ${TOOLS_SOURCE_DIR}/commandwriter/pre_gfx9_factory.cpp
                      ${TOOLS_SOURCE_DIR}/commandwriter/cmdwriter.cpp )

set ( COMMON_SRC      ${TOOLS_SOURCE_DIR}/common/amd_asic_type.cpp
                      ${TOOLS_SOURCE_DIR}/common/amd_tools_interface.cpp )

set ( DEBUGGER_SRC    ${TOOLS_SOURCE_DIR}/debugger/cwsr_trapstring_perf.cpp
                      ${TOOLS_SOURCE_DIR}/debugger/gpu_trap_event.cpp
                      ${TOOLS_SOURCE_DIR}/debugger/hsa_ext_debugger.cpp
                      ${TOOLS_SOURCE_DIR}/debugger/kfd_event.cpp
                      ${TOOLS_SOURCE_DIR}/debugger/pm4_queue.cpp
                      ${TOOLS_SOURCE_DIR}/debugger/runtime_trapstring.cpp
                      ${TOOLS_SOURCE_DIR}/debugger/shader_event.cpp
                      ${TOOLS_SOURCE_DIR}/debugger/trap_finalizer.cpp
                      ${TOOLS_SOURCE_DIR}/debugger/trap_handler.cpp
                      ${TOOLS_SOURCE_DIR}/debugger/trap_manager.cpp )

set ( INTERCEPT_SRC   ${TOOLS_SOURCE_DIR}/intercept/amd_sw_aql_command_processor.cpp
                      ${TOOLS_SOURCE_DIR}/intercept/hsa_amd_tools.cpp
                      ${TOOLS_SOURCE_DIR}/intercept/profiler.cpp )

set ( PROFILER_SRC    ${TOOLS_SOURCE_DIR}/profiler/gpu_countergroup.cpp 
                      ${TOOLS_SOURCE_DIR}/profiler/gpu_counter.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/var_data.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/info_set.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/parameter_set.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/ci_blockinfo.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/ci_pmu.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/vi_blockinfo.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/vi_pmu.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/ai_blockinfo.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/ai_pmu.cpp
                      ${TOOLS_SOURCE_DIR}/profiler/hsa_ext_profiler.cpp )

set ( THREAD_TRACE_SRC    ${TOOLS_SOURCE_DIR}/threadtrace/thread_trace.cpp 
                          ${TOOLS_SOURCE_DIR}/threadtrace/gfx9_factory.cpp
                          ${TOOLS_SOURCE_DIR}/threadtrace/gfx9_thread_trace.cpp
                          ${TOOLS_SOURCE_DIR}/threadtrace/pre_gfx9_factory.cpp
                          ${TOOLS_SOURCE_DIR}/threadtrace/pre_gfx9_thread_trace.cpp )

set ( SP3_R1000_SRC   ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-asic.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-dispatch.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-eval.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-gc.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-int.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-lib.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-main.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-merge-shaders.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-native.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/sp3-vm.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gen/sp3-parse.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gen/sp3-lex.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/ci/sp3-ci-asic.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/ci/sp3-ci-dis.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/ci/sp3-ci-gen.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/ci/sp3-ci-inst-info.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/ci/sp3-ci-regs.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/ci/sp3-ci-tables.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/si/sp3-si-asic.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/si/sp3-si-dis.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/si/sp3-si-gen.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/si/sp3-si-inst-info.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/si/sp3-si-regs.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/si/sp3-si-tables.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx81/sp3-gfx81-asic.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx81/sp3-gfx81-dis.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx81/sp3-gfx81-gen.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx81/sp3-gfx81-inst-info.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx81/sp3-gfx81-regs.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx8/sp3-gfx8-asic.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx8/sp3-gfx8-dis.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx8/sp3-gfx8-gen.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx8/sp3-gfx8-inst-info.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx8/sp3-gfx8-regs.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx8/sp3-gfx8-tables.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx9/sp3-gfx9-asic.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx9/sp3-gfx9-dis.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx9/sp3-gfx9-gen.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx9/sp3-gfx9-inst-info.c
                      ${TOOLS_SOURCE_DIR}/sp3/Chip/R1000/gfx9/sp3-gfx9-regs.c )

set ( UTIL_SRC   ${OPEN_SOURCE_DIR}/hsa-runtime/core/util/timer.cpp
                 ${OPEN_SOURCE_DIR}/hsa-runtime/core/util/small_heap.cpp
                 ${OPEN_SOURCE_DIR}/hsa-runtime/core/util/lnx/os_linux.cpp )
 

## This is the main shared library.
add_library ( ${TOOLS_TARGET} SHARED ${CORE_SRC}
                                     ${COMMON_SRC}
                                     ${CMD_WRITER_SRC}
                                     ${CMDWRITER_SRC}
                                     ${DEBUGGER_SRC}
                                     ${INTERCEPT_SRC}
                                     ${PROFILER_SRC}
                                     ${THREAD_TRACE_SRC}
                                     ${SP3_R1000_SRC}
                                     ${UTIL_SRC} )

## Set the VERSION and SOVERSION values
set_property ( TARGET ${TOOLS_TARGET} PROPERTY VERSION "${LIB_VERSION_STRING}" )
set_property ( TARGET ${TOOLS_TARGET} PROPERTY SOVERSION "${BUILD_VERSION_MAJOR}" )

## Add the required link libraries
target_link_libraries (
    ${TOOLS_TARGET}
    PRIVATE hsa-runtime64
    PRIVATE hsakmt
    c dl pthread rt
)

## If the build is Release, strip the target library
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    add_custom_command ( TARGET ${TOOLS_TARGET} POST_BUILD COMMAND ${CMAKE_STRIP} *.so )
endif ()

## Set install information
install ( TARGETS ${TOOLS_TARGET} LIBRARY DESTINATION hsa/lib )
install ( DIRECTORY ${TOOLS_SOURCE_DIR}/inc/ DESTINATION hsa/include/hsa FILES_MATCHING PATTERN "*.h" )
