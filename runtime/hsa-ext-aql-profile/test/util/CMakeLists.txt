#
# Source files for Rocr Utils library
#
file( GLOB MODULE_SRC "*.cpp" )

#
# Header files include path(s).
#
include_directories ( $ENV{ROCR_INC_DIR} )

#
# Build Utils as a Static Library object
#
add_library( ${UTIL_LIB} STATIC ${MODULE_SRC} )
target_link_libraries( ${UTIL_LIB} c stdc++ dl pthread rt )
