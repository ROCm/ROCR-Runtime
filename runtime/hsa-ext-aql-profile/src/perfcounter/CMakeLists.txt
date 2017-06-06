#
# Source files for Rocr PerfCntr
#
set ( LIB_SRC var_data.cpp )
set ( LIB_SRC ${LIB_SRC} info_set.cpp )
set ( LIB_SRC ${LIB_SRC} parameter_set.cpp )
set ( LIB_SRC ${LIB_SRC} gpu_counter.cpp )
set ( LIB_SRC ${LIB_SRC} gpu_countergroup.cpp )
set ( LIB_SRC ${LIB_SRC} vi_blockinfo.cpp )
set ( LIB_SRC ${LIB_SRC} vi_pmu.cpp )
set ( LIB_SRC ${LIB_SRC} ai_blockinfo.cpp )
set ( LIB_SRC ${LIB_SRC} ai_pmu.cpp )

#
# Header files include path(s).
#
include_directories ( $ENV{ROCR_INC_DIR} )
include_directories ( ${PROJ_DIR}/commandwriter )
include_directories ( ${CORE_UTIL_DIR} )

#
# Build PerfCntr as a Static Library object
#
add_library ( ${PMC_LIB} STATIC ${LIB_SRC} )
