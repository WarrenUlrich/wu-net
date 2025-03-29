cmake_minimum_required(VERSION 3.18)

project(wu-net)

enable_testing()
include(CTest)

add_library(
    ${PROJECT_NAME} INTERFACE
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

include_directories(
  ${PROJECT_NAME}
    include
)

add_subdirectory(tests/example_test)
add_subdirectory(tests/ipv4_address_test)