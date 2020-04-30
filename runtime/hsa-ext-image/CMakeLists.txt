cmake_minimum_required ( VERSION 3.5.0 )

## Set ext runtime module name and project name.
set ( IMAGE_NAME "hsa-ext-image" )
set ( IMAGE_TARGET "${IMAGE_NAME}64" )
set ( IMAGE_LIBRARY "lib${IMAGE_TARGET}" )
project ( ${IMAGE_TARGET} )

# Optionally, build with ccache.
set(ROCM_CCACHE_BUILD OFF CACHE BOOL "Set to ON for a ccache enabled build")
if (ROCM_CCACHE_BUILD)
  find_program(CCACHE_PROGRAM ccache)
  if (CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PROGRAM})
  else()
    message(WARNING "Unable to find ccache. Falling back to real compiler")
  endif() # if (CCACHE_PROGRAM)
endif() # if (ROCM_CCACHE_BUILD)

## Include the cmake_modules utils.cmake
list ( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../hsa-runtime/cmake_modules" )
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
add_definitions ( -D BRAHMA_BUILD=1 )

## Get the package version.
get_version( "1.1.9")
set(SO_MAJOR 1)
set(SO_MINOR 1)
if ( ${ROCM_PATCH_VERSION} )
    set ( SO_PATCH ${ROCM_PATCH_VERSION})
    set ( VERSION_PATCH ${ROCM_PATCH_VERSION})
else ()
    set(SO_PATCH 9)
endif ()

set( SO_VERSION_STRING "${SO_MAJOR}.${SO_MINOR}.${SO_PATCH}" )
set( PACKAGE_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_COMMIT_COUNT}-${VERSION_JOB}-${VERSION_HASH}" )

## Find the hsakmt library and include files
get_include_path( HSAKMT_INC_PATH "libhsakmt include path" NAMES "hsakmt.h" "libhsakmt/hsakmt.h" HINTS "${CMAKE_BINARY_DIR}/../../include" "${CMAKE_CURRENT_SOURCE_DIR}/../../../../libhsakmt/include" PATHS "/opt/rocm/include")
get_library_path( HSAKMT_LIB_PATH "libhsakmt library path" NAMES "libhsakmt.so" HINTS "${CMAKE_BINARY_DIR}/../../lib" "${CMAKE_BINARY_DIR}/../roct" PATHS "/opt/rocm/lib")
include_directories( ${HSAKMT_INC_PATH} )
link_directories( ${HSAKMT_LIB_PATH} )

## Find the hsa-runtime and include files
get_include_path( HSA_INC_PATH "ROCr include path" NAMES "inc/hsa.h" HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../hsa-runtime" "${CMAKE_BINARY_DIR}/../../include" PATHS "/opt/rocm/include")
get_library_path( HSA_LIB_PATH "ROCr library path" NAMES "libhsa-runtime64.so" HINTS "${CMAKE_BINARY_DIR}/../../lib" "${CMAKE_BINARY_DIR}/../hsa-core" "${CMAKE_CURRENT_SOURCE_DIR}/../hsa-runtime/build" PATHS "/opt/rocm/lib")
include_directories( ${HSA_INC_PATH} )
link_directories( ${HSA_LIB_PATH} )

## Find self
if( "${IMAGE_SOURCE_DIR}" STREQUAL "" )
    get_include_path( IMG_SOURCE_FILE null NAMES "image_runtime.h" HINTS "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../hsa-runtime/image/" )
    get_filename_component( IMAGE_SOURCE_DIR "${IMG_SOURCE_FILE}" ABSOLUTE )
    unset( IMG_SOURCE_FILE CACHE )
endif()
set( IMAGE_SOURCE_DIR ${IMAGE_SOURCE_DIR} CACHE PATH "Image lib source dir" FORCE )

get_filename_component( OPEN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE CACHE )
set( OPEN_SOURCE_DIR ${OPEN_SOURCE_DIR} CACHE PATH "Open source root dir" FORCE )

## Set RUNPATH - ../../lib covers use of the legacy symlink in /hsa/lib/
set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/../../lib;$ORIGIN/../../lib64;$ORIGIN/../lib64")

## ------------------------- Linux Compiler and Linker options -------------------------
set ( CMAKE_CXX_FLAGS "-std=c++11 " )

set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fexceptions -fno-rtti -fvisibility=hidden -Wno-error=sign-compare -Wno-sign-compare -Wno-write-strings -Wno-deprecated-declarations -Wno-conversion-null -fno-math-errno -fno-threadsafe-statics -fmerge-all-constants -fms-extensions -Wno-error=comment -Wno-comment -Wno-error=pointer-arith -Wno-pointer-arith -fPIC" )

if ( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
    set  ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64  -msse -msse2" )
elseif ( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86" )
    set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" )
endif ()

if ( "${CMAKE_BUILD_TYPE}" STREQUAL Debug )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb" )
endif ()

set ( DRVDEF "${IMAGE_SOURCE_DIR}/image.so.def" )

set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bdynamic -Wl,-z,noexecstack -Wl,--version-script=${DRVDEF} -Wl,--enable-new-dtags" )

## Library path(s).
include_directories(${IMAGE_SOURCE_DIR}/)
include_directories(${OPEN_SOURCE_DIR}/hsa-runtime)
include_directories(${OPEN_SOURCE_DIR}/hsa-runtime/inc)
include_directories(${OPEN_SOURCE_DIR}/hsa-runtime/core/inc)
include_directories(${IMAGE_SOURCE_DIR}/addrlib)
include_directories(${IMAGE_SOURCE_DIR}/addrlib/inc)
include_directories(${IMAGE_SOURCE_DIR}/addrlib/src)
include_directories(${IMAGE_SOURCE_DIR}/addrlib/src/core)
include_directories(${IMAGE_SOURCE_DIR}/addrlib/src/r800)
include_directories(${IMAGE_SOURCE_DIR}/addrlib/src/gfx9)
include_directories(${IMAGE_SOURCE_DIR}/addrlib/src/gfx10)
include_directories(${IMAGE_SOURCE_DIR}/addrlib/src/chip/r800)
include_directories(${IMAGE_SOURCE_DIR}/addrlib/src/chip/gfx9)
include_directories(${IMAGE_SOURCE_DIR}/addrlib/src/chip/gfx10)

set ( IMAGE_SRCS ${IMAGE_SOURCE_DIR}/addrlib/src/addrinterface.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/core/coord.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/core/addrlib.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/core/addrlib1.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/core/addrlib2.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/core/addrobject.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/core/addrelemlib.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/r800/ciaddrlib.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/r800/egbaddrlib.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/r800/siaddrlib.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/gfx9/gfx9addrlib.cpp
                 ${IMAGE_SOURCE_DIR}/addrlib/src/gfx10/gfx10addrlib.cpp
                 ${IMAGE_SOURCE_DIR}/amd_ext.cpp
                 ${IMAGE_SOURCE_DIR}/device_info.cpp
                 ${IMAGE_SOURCE_DIR}/hsa_ext_image.cpp
                 ${IMAGE_SOURCE_DIR}/image_runtime.cpp
                 ${IMAGE_SOURCE_DIR}/image_manager.cpp
                 ${IMAGE_SOURCE_DIR}/image_manager_kv.cpp
                 ${IMAGE_SOURCE_DIR}/image_manager_ai.cpp
                 ${IMAGE_SOURCE_DIR}/image_manager_nv.cpp
                 ${IMAGE_SOURCE_DIR}/image_lut_kv.cpp
                 ${IMAGE_SOURCE_DIR}/blit_object_gfx7xx.cpp
                 ${IMAGE_SOURCE_DIR}/blit_object_gfx8xx.cpp
                 ${IMAGE_SOURCE_DIR}/blit_object_gfx9xx.cpp
                 ${IMAGE_SOURCE_DIR}/opencl_blit_objects.cpp
                 ${IMAGE_SOURCE_DIR}/blit_kernel.cpp
                 ${OPEN_SOURCE_DIR}/hsa-runtime/core/common/shared.cpp
                 ${OPEN_SOURCE_DIR}/hsa-runtime/core/common/hsa_table_interface.cpp
)

add_subdirectory(${IMAGE_SOURCE_DIR}/blit_src ${CMAKE_CURRENT_BINARY_DIR}/image_blit)
set_source_files_properties(${IMAGE_SOURCE_DIR}/opencl_blit_objects.cpp PROPERTIES GENERATED 1)

add_library ( ${IMAGE_TARGET} SHARED ${IMAGE_SRCS} )
add_dependencies( ${IMAGE_TARGET} opencl_blit_objects.cpp )

## Set the VERSION and SOVERSION values
set_property ( TARGET ${IMAGE_TARGET} PROPERTY VERSION "${SO_VERSION_STRING}" )
set_property ( TARGET ${IMAGE_TARGET} PROPERTY SOVERSION "${SO_MAJOR}" )

## Add the core runtime in the link
target_link_libraries (
    ${IMAGE_TARGET}
    PRIVATE hsa-runtime64
    PRIVATE hsakmt
    c dl pthread rt
)

## If the build is Release, strip the target library
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Release )
    add_custom_command ( TARGET ${IMAGE_TARGET} POST_BUILD COMMAND ${CMAKE_STRIP} $<TARGET_FILE_NAME:${IMAGE_TARGET}> )
endif ()

## Create symlinks for legacy packaging and install
add_custom_target ( hsa_images_lib_link ALL WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${CMAKE_COMMAND} -E create_symlink ../hsa/lib/${IMAGE_LIBRARY}.so ${IMAGE_LIBRARY}-link.so )

## Set install information
install ( TARGETS ${IMAGE_TARGET} LIBRARY DESTINATION hsa/lib )
install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/${IMAGE_LIBRARY}-link.so DESTINATION lib PERMISSIONS OWNER_WRITE OWNER_READ RENAME ${IMAGE_LIBRARY}.so )
