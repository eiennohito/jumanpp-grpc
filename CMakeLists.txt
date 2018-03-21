project(jumanpp-grpc)
cmake_minimum_required(VERSION 3.1)
set(PROJECT_VERSION 0.1-dev)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)

option(JPP_GRPC_PYTHON OFF)

set(JPP_GRPC_BASE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(jumanpp EXCLUDE_FROM_ALL)
add_subdirectory(src)
if (${JPP_GRPC_PYTHON})
  add_subdirectory(python)
endif()