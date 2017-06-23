#
# Build is not supported on Windows plaform
#
if ( WIN32 )
  message ( FATAL_ERROR "Windows build is not supported." )
endif ()

#
# External dependencies for Rocr Header files
#
if ( NOT DEFINED ENV{ROCR_INC_DIR} )
  message ( FATAL_ERROR "ERROR: Environment variable ROCR_INC_DIR is not set" )
  return ()
endif ()

#
# External dependencies for Rocr Library files
#
if ( NOT DEFINED ENV{ROCR_LIB_DIR} )
  message ( FATAL_ERROR "ERROR: Environment variable ROCR_LIB_DIR is not set" )
  return ()
endif ()

#
# Process Env to determine build type
#
string ( TOLOWER "$ENV{ROCR_BLD_TYPE}" type )
if ( "${type}" STREQUAL debug )
  set ( ISDEBUG 1 )
  set ( BUILD_TYPE "Debug" )
else ()
  set ( ISDEBUG 0 )
  set ( BUILD_TYPE "Release" )
endif ()

#
# Determine build is 32-bit or 64-bit
# @note: By default it is not set
#
if ( "$ENV{ROCR_BLD_BITS}" STREQUAL 32 )
    set ( ONLY64STR "" )
    set ( IS64BIT 0 )
else ()
    set ( ONLY64STR "64" )
    set ( IS64BIT 1 )
endif ()

#
# Build information
#
message ( "---------ROCR-HdrDir: " $ENV{ROCR_INC_DIR} )
message ( "---------ROCR-LibDir: " $ENV{ROCR_LIB_DIR} )
