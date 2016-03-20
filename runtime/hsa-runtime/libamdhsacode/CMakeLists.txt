#
# amdhsacode library
#
# This file is expected to be included from top-level CMakeLists.txt.
#
# Dependencies:
# - Compiler definitions
# - elf library
#
# Defines:
# - amdhsacode library and target include directories

file(GLOB sources *.cpp *.hpp)
find_package(LibElf REQUIRED)
add_library(amdhsacode ${sources})
target_include_directories(amdhsacode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(amdhsacode elf)
