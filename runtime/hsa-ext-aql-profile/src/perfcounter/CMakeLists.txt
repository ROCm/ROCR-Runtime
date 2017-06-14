#
# Source files for Rocr PerfCntr
#
set ( LIB_SRC gfx8_perf_counter.cpp )
set ( LIB_SRC ${LIB_SRC} gfx9_perf_counter.cpp )
set ( LIB_SRC ${LIB_SRC} gfx8_block_info.cpp )
set ( LIB_SRC ${LIB_SRC} gfx9_block_info.cpp )

#
# Header files include path(s).
#
include_directories ( ${PROJ_DIR}/commandwriter )

#
# Build PerfCntr as a Static Library object
#
add_library ( ${PMC_LIB} STATIC ${LIB_SRC} )
