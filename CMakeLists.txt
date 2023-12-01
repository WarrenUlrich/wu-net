cmake_minimum_required(VERSION 3.15)

project(wu-net)

add_library(
  ${PROJECT_NAME} INTERFACE
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_23)

target_include_directories(
  ${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(test test/main.cpp)

target_compile_features(test PUBLIC cxx_std_23)

target_link_libraries(test PRIVATE wu-net)