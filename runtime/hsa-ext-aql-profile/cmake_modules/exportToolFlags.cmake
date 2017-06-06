#
# Compiler Preprocessor definitions.
#
add_definitions ( -D__linux__ )
add_definitions ( -DUNIX_OS )
add_definitions ( -DLINUX )
add_definitions ( -D__AMD64__ )
add_definitions ( -D__x86_64__ )
add_definitions ( -DAMD_INTERNAL_BUILD )
add_definitions ( -DLITTLEENDIAN_CPU=1 )
add_definitions ( -DHSA_LARGE_MODEL= )
add_definitions ( -DHSA_DEPRECATED= )

#
# Linux Compiler options
#
set ( CMAKE_CXX_FLAGS "-std=c++11")
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=sign-compare" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=enum-compare" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=comment " )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=pointer-arith" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-comment" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pointer-arith" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion-null" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-math-errno" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fms-extensions" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmerge-all-constants" )
set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC" )

#
# Extend Compiler flags based on build type
#
set ( CMAKE_BUILD_TYPE ${BUILD_TYPE} )
if ( "${CMAKE_BUILD_TYPE}" STREQUAL Debug )
  set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb" )
endif ()

#
# Extend Compiler flags based on Processor architecture
#
if ( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
  set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64  -msse -msse2" )
elseif ( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86" )
  set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" )
endif ()

#
# Basic Tool Chain Information
#
message ( "-------------IS64BIT: " ${IS64BIT} )
message ( "-----------BuildType: " ${BUILD_TYPE} )
message ( " -----------Compiler: " ${CMAKE_CXX_COMPILER} )
message ( " ------------Version: " ${CMAKE_CXX_COMPILER_VERSION} )
message ( " ------------ProjDir: " ${PROJ_DIR} )
message ( " ------------TestDir: " ${PROJ_DIR} )
message ( "------HSA-RuntimeDir: " ${HSA_RUNTIME_DIR} )
message ( " -----------CoreUtil: " ${CORE_UTIL_DIR} )
