cmake_minimum_required(VERSION 2.6)

project(Pass-ThroughFS)

set(CMAKE_C_FLAGS "-Wall -Wextra -std=c99")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/")

find_package(Fuse REQUIRED)

add_definitions(${FUSE_DEFINITIONS})
include_directories(${FUSE_INCLUDE_DIRS})
add_executable(ptfs ptfs.c)
target_link_libraries(ptfs ${FUSE_LIBRARIES})

install(TARGETS ptfs DESTINATION bin)
