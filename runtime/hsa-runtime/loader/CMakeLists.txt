#
# loader library
#
# This file is expected to be included from top-level CMakeLists.txt.
#
# Dependencies:
# - Compiler definitions
# - amdhsacode library
#
# Defines:
# - amdhsaloader library and target include directories

file(GLOB sources *.cpp *.hpp)
add_library(amdhsaloader ${sources})
target_include_directories(amdhsaloader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(amdhsaloader amdhsacode)
