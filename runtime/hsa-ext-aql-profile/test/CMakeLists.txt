#
# Header files include path(s).
#
include_directories ( $ENV{ROCR_INC_DIR} )
include_directories ( ${API_DIR} )
include_directories ( ${TEST_DIR}/util )
include_directories ( ${TEST_DIR}/ctrl )

#
# Specify the directory containing the libraries of HsaRt
# to be linked against for building a Hsa Perf application
#
LINK_DIRECTORIES($ENV{ROCR_LIB_DIR})
find_library ( ROCR_LIB NAMES hsa-runtime64 PATHS $ENV{ROCR_LIB_DIR} )

#
# Set Name for Common library and build it as a
# static library to be linked with others
#
set ( UTIL_LIB "util${ONLY64STR}" )
add_subdirectory ( ${TEST_DIR}/util "${PROJECT_BINARY_DIR}/util" )

#
# Build the test library
#
set ( TEST_NAME simple_convolution )
include_directories ( ${TEST_DIR}/${TEST_NAME} )
set ( LIB_NAME "${TEST_NAME}${ONLY64STR}" )
add_library ( ${LIB_NAME} STATIC ${TEST_DIR}/${TEST_NAME}/${TEST_NAME}.cpp )
target_link_libraries( ${LIB_NAME} c stdc++ )
execute_process ( COMMAND sh -xc "cp ${TEST_DIR}/${TEST_NAME}/*.hsaco ${PROJECT_BINARY_DIR}" )
set ( TEST_LIBS ${LIB_NAME} )

#
# Build the test control
#
set ( SRC_LIST ${TEST_DIR}/ctrl/test.cpp )
set ( SRC_LIST ${SRC_LIST} ${TEST_DIR}/ctrl/test_pmgr.cpp )
set ( SRC_LIST ${SRC_LIST} ${TEST_DIR}/ctrl/test_hsa.cpp )
set ( LIB_LIST ${TEST_LIBS} ${UTIL_LIB} ${CORE_UTILS_LIB} ${ROCR_LIB} ${TARGET_LIB} )
set ( EXE_NAME "ctrl" )
add_executable ( ${EXE_NAME} ${SRC_LIST} )
target_link_libraries( ${EXE_NAME} ${LIB_LIST} c stdc++ dl pthread rt atomic )
