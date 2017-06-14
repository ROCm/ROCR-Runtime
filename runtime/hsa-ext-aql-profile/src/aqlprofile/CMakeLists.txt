#
# Source files for Rocr Service Manager
#
set ( LIB_SRC aql_profile.cpp populate_aql.cpp gfx8_factory.cpp gfx9_factory.cpp legacy_pm4.cpp )

#
# Header files include path(s).
#
include_directories ( $ENV{ROCR_INC_DIR} )
include_directories ( ${PROJ_DIR}/perfcounter )
include_directories ( ${PROJ_DIR}/threadtrace )
include_directories ( ${PROJ_DIR}/commandwriter )
include_directories ( ${API_DIR} )

#
# Build Service Manager as a dynamic Library object
#
set ( LIB_LIST ${PMC_LIB} ${SQTT_LIB} ${CMDWRITER_LIB} )
add_library ( ${TARGET_LIB} SHARED ${LIB_SRC} )
target_link_libraries( ${TARGET_LIB} ${LIB_LIST} c stdc++ dl pthread rt )
