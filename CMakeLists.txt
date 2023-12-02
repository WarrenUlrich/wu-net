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

add_executable(client test/client.cpp)

target_compile_features(client PUBLIC cxx_std_23)

target_link_libraries(client PRIVATE wu-net)

# Server
add_executable(server test/server.cpp)

target_compile_features(server PUBLIC cxx_std_23)

target_link_libraries(server PRIVATE wu-net)