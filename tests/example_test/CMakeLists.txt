cmake_minimum_required(VERSION 3.16)

project(example_test)

enable_testing()
include(CTest)

add_executable(
    ${PROJECT_NAME}
    src/main.cpp
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(
    ${PROJECT_NAME} PRIVATE
    wu-net
)

add_test(
  NAME ${PROJECT_NAME}
  COMMAND ${PROJECT_NAME}
)