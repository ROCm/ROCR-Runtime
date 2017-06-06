#
# Source files for Rocr ThreadTrace
#
set ( LIB_SRC thread_trace.cpp )
set ( LIB_SRC ${LIB_SRC} gfx8_thread_trace.cpp )
set ( LIB_SRC ${LIB_SRC} gfx9_thread_trace.cpp )

#
# Header files include path(s).
#
include_directories ( $ENV{ROCR_INC_DIR} )
include_directories ( ${PROJ_DIR}/commandwriter )
include_directories ( ${HSA_RUNTIME_OSC_DIR} )

#
# Build ThreadTrace as a Static Library object
#
add_library ( ${SQTT_LIB} STATIC ${LIB_SRC} )
