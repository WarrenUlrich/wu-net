cmake_minimum_required(VERSION 3.15)

project(wu-net)

add_library(
  ${PROJECT_NAME} INTERFACE
)

target_include_directories(
  ${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(test test/main.cpp)

target_link_libraries(test PRIVATE wu-net)