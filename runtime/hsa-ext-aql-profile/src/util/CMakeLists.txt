#
# Source files for Rocr Utils library
#
set ( MODULE_SRC ${CORE_UTIL_DIR}/lnx/os_linux.cpp )

#
# Header files include path(s).
#
include_directories ( $ENV{ROCR_INC_DIR} )
include_directories ( ${HSA_RUNTIME_OSC_DIR} )
include_directories ( ${CORE_UTIL_DIR} )

#
# Build Utils as a Static Library object
#
add_library( ${UTIL_LIB} STATIC ${MODULE_SRC} )
target_link_libraries( ${UTIL_LIB} c stdc++ dl pthread rt )
