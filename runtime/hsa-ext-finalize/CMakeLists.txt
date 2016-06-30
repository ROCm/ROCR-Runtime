cmake_minimum_required ( VERSION 2.8.0 ) ## GCC 4.8 or higher compiler required.

if ( WIN32 )
    MESSAGE ( FATAL_ERROR "Windows build is not supported." )
endif ()

## Process build type.
if( NOT DEFINED CMAKE_BUILD_TYPE )
    set( CMAKE_BUILD_TYPE "Debug" )
endif()

if( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    set( BUILD_TYPE "Release" )
    set( ISDEBUG 0 )
    set( BUILD_FOLDER "B_rel" )
else()
    set( BUILD_TYPE "Debug" )
    set( ISDEBUG 1)
    set( BUILD_FOLDER "B_dbg" )
endif()

## Process build target.
if ( "${HSA_BUILD_TARGET_BITS}" STREQUAL 32 )
    set ( ONLY64STR "" )
    set ( IS64BIT 0 )
    set ( BUILD_FOLDER "lnx/${BUILD_FOLDER}" )
else ()
    set ( ONLY64STR "64" )
    set ( IS64BIT 1 )
    set ( BUILD_FOLDER "lnx64a/${BUILD_FOLDER}" )
endif ()

## Check that the libhsail include and library directories are defined.
if ( NOT DEFINED LIBHSAIL_BUILD_FOLDER )
    set ( LIBHSAIL_BUILD_FOLDER ${BUILD_FOLDER} )
endif()

## Used for packaging.
set ( CORE_RUNTIME_NAME "hsa-runtime" )
set ( CORE_RUNTIME_COMPONENT "lib${CORE_RUNTIME_NAME}" )
set ( CORE_RUNTIME_TARGET "${CORE_RUNTIME_NAME}${ONLY64STR}" )
set ( CORE_RUNTIME_LIB "lib${CORE_RUNTIME_NAME}${ONLY64STR}.so" )

MESSAGE ( ------IS64BIT: ${IS64BIT} )
MESSAGE ( ------Compiler: ${CMAKE_CXX_COMPILER} )
MESSAGE ( ------Version: ${CMAKE_CXX_COMPILER_VERSION} )

## Verbose output.
set ( CMAKE_VERBOSE_MAKEFILE on )

## Compiler preproc definitions.
add_definitions ( -D__linux__ )
add_definitions ( -DUNIX_OS )
add_definitions ( -DLINUX )
add_definitions ( -D__AMD64__ )
add_definitions ( -D__x86_64__ )
add_definitions ( -DAMD_INTERNAL_BUILD )
add_definitions ( -DLITTLEENDIAN_CPU=1 )

## Set ext runtime module name and project name.
set ( FINALIZE_NAME "hsa-ext-finalize" )
set ( FINALIZE_COMPONENT "lib${FINALIZE_NAME}" )
set ( FINALIZE_TARGET "${FINALIZE_NAME}${ONLY64STR}" )
project ( ${FINALIZE_TARGET} )

## External dependencies and directories
if ( NOT DEFINED REG_INCLUDE )
  set(REG_INCLUDE ${HSA_CLOSED_SOURCE_DIR}/drivers/inc/asic_reg)
endif()

if ( NOT EXISTS ${REG_INCLUDE}/si_id.h )
    MESSAGE ( FATAL_ERROR "Environment variable REG_INCLUDE is not set appropriately. REG_INCLUDE=${REG_INCLUDE}" )
else ()
    set ( REG_INCLUDE ${REG_INCLUDE} )
endif ()

if( NOT DEFINED EXT_SOURCE_DIR )
  set ( EXT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
endif()

if( NOT DEFINED OPEN_SOURCE_DIR )
  set ( OPEN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." )
endif()

## ------------------------- Linux Compiler and Linker options -------------------------
set ( CMAKE_CXX_FLAGS "-std=c++11 " )

set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fexceptions -fno-rtti -fvisibility=hidden -Wno-error=sign-compare -Wno-sign-compare -Wno-write-strings -Wno-deprecated-declarations -Wno-conversion-null -fno-math-errno -fno-threadsafe-statics -fmerge-all-constants -fms-extensions -Wno-error=comment -Wno-comment -Wno-error=pointer-arith -Wno-pointer-arith -fPIC" )

if ( IS64BIT )
    set  ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64  -msse -msse2" )
else  ()
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" )
endif ()

if ( "${CMAKE_BUILD_TYPE}" STREQUAL Debug )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb" )
endif ()

set ( DRVDEF "${EXT_SOURCE_DIR}/make/finalize.so.def" )

set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bdynamic -Wl,-z,noexecstack -Wl,--version-script=${DRVDEF}" )

set ( CMAKE_SKIP_BUILD_RPATH TRUE )

## Library path(s).
include_directories ( ${REG_INCLUDE} )
include_directories ( ${EXT_SOURCE_DIR}/.. )
include_directories ( ${EXT_SOURCE_DIR}/inc )
include_directories ( ${OPEN_SOURCE_DIR}/hsa-runtime )
include_directories ( ${OPEN_SOURCE_DIR}/hsa-runtime/inc )
include_directories ( ${OPEN_SOURCE_DIR}/hsa-runtime/core/inc )
include_directories ( ${HSA_CLOSED_SOURCE_DIR}/drivers/hsa/compiler/ext_finalize/amdhsafin/Interface )
include_directories ( ${HSA_CLOSED_SOURCE_DIR}/drivers/hsa/compiler/finalizer/HSAIL/include )
include_directories ( ${HSA_CLOSED_SOURCE_DIR}/drivers/hsa/compiler/finalizer/HSAIL/hsail-tools/libHSAIL-AMD )
include_directories ( ${HSA_CLOSED_SOURCE_DIR}/drivers/hsa/compiler/finalizer/HSAIL/hsail-tools/libHSAIL )
include_directories ( ${HSA_CLOSED_SOURCE_DIR}/drivers/hsa/compiler/finalizer/HSAIL/hsail-tools/libHSAIL/build/${LIBHSAIL_BUILD_FOLDER} )

set ( FINALIZE_SRCS ${EXT_SOURCE_DIR}/finalize/hsa_ext_finalize.cpp
                    ${EXT_SOURCE_DIR}/finalize/program_context.cpp
                    ${EXT_SOURCE_DIR}/finalize/finalizer_manager.cpp
)

add_library ( ${FINALIZE_TARGET} SHARED ${FINALIZE_SRCS} )

## Set the VERSION and SOVERSION values
if ( DEFINED VERSION_STRING )
    set_property ( TARGET ${FINALIZE_TARGET} PROPERTY VERSION "${VERSION_STRING}" )
endif ()

set_property ( TARGET ${FINALIZE_TARGET} PROPERTY SOVERSION "${VERSION_MAJOR}" )

## Add the core runtime in the link
set ( LIBHSAIL -Wl,--no-whole-archive PRIVATE ${HSA_BUILD_LIB_PATH}/libhsail.a )
set ( LIBHSAILAMD -Wl,--no-whole-archive PRIVATE ${HSA_BUILD_LIB_PATH}/libhsail-amd.a )
set ( LIBAMDHSAFIN -Wl,--no-whole-archive PRIVATE ${HSA_BUILD_LIB_PATH}/amdhsafin64.a )
set ( LIBAMDHSACODE -Wl,--no-whole-archive PRIVATE ${HSA_BUILD_LIB_PATH}/libamdhsacode.a )
set ( LIBCACHING -Wl,--no-whole-archive PRIVATE ${HSA_BUILD_LIB_PATH}/libcaching.a )
set ( LIBDWARF -Wl,--no-whole-archive PRIVATE ${HSA_BUILD_LIB_PATH}/libdwarf.a )
set ( LIBELF -Wl,--no-whole-archive PRIVATE ${HSA_BUILD_LIB_PATH}/libelf.a )
set ( LIBSCP3 -Wl,--no-whole-archive PRIVATE ${HSA_BUILD_LIB_PATH}/scSP3_R1000.a )

## Add the core runtime in the link
link_directories(${HSA_BUILD_LIB_PATH})
target_link_libraries (
    ${FINALIZE_TARGET}
    PRIVATE ${LIBHSAIL}
    PRIVATE ${LIBHSAILAMD}
    PRIVATE ${LIBAMDHSAFIN}
    PRIVATE ${LIBAMDHSACODE}
    PRIVATE ${LIBCACHING}
    PRIVATE ${LIBDWARF}
    PRIVATE ${LIBELF}
    PRIVATE ${LIBSCP3}
    PRIVATE ${HSA_BUILD_LIB_PATH}/${CORE_RUNTIME_LIB}
    c dl pthread rt
)

## If the build is Release, strip the target library
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    add_custom_command ( TARGET ${FINALIZE_TARGET} POST_BUILD COMMAND ${CMAKE_STRIP} *.so )
endif ()

## Set install information
install ( TARGETS ${FINALIZE_TARGET} LIBRARY DESTINATION lib COMPONENT ${FINALIZE_COMPONENT})
