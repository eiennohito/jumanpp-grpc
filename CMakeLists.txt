project(jumanpp-grpc)
cmake_minimum_required(VERSION 3.1)
set(PROJECT_VERSION 0.1-dev)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)

add_subdirectory(jumanpp EXCLUDE_FROM_ALL)
add_subdirectory(src)