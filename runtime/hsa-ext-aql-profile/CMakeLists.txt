#
# Minimum version of cmake required
#
cmake_minimum_required ( VERSION 3.5.0 )

#
# Setup flag to be verbose or not
#
set ( CMAKE_VERBOSE_MAKEFILE TRUE CACHE BOOL "Verbose Output" FORCE )

set ( ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
set ( PROJ_DIR ${ROOT_DIR}/src )
set ( TEST_DIR ${ROOT_DIR}/test )

#
# Build sources
#
include ( ${PROJ_DIR}/CMakeLists.txt )

#
# Build tests
#
add_subdirectory ( ${TEST_DIR} ${PROJECT_BINARY_DIR}/test )

#
# Style format
#
execute_process ( COMMAND sh -xc "/usr/bin/find ${ROOT_DIR} -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -exec /usr/bin/clang-format -i -style=file \{\} \;" )
