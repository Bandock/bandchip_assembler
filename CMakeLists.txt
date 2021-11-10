cmake_minimum_required(VERSION 3.10)
project(bandchip_assembler VERSION 0.9 LANGUAGES C CXX)

add_executable(bandchip_assembler src/application.cpp src/main.cpp)
target_include_directories(bandchip_assembler PUBLIC "${PROJECT_BINARY_DIR}/include")
