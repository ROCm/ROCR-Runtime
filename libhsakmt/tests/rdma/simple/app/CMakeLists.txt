cmake_minimum_required (VERSION 2.6)

project (rdma_test)

link_directories($ENV{ROOT_OF_ROOTS}/out/lib)

include_directories($ENV{LIBHSAKMT_ROOT}/include)
include_directories(../drv)

add_executable(rdma_test rdma_test.cpp)
target_link_libraries(rdma_test libhsakmt.so)

