cmake_minimum_required(VERSION 3.0.2)
project(vcodec C)

add_library(vcodec src/vcodec.c)
target_include_directories(vcodec PUBLIC include/)
target_compile_options(vcodec PRIVATE -O3)

add_executable(vcodec-test test/main.c)
target_link_libraries(vcodec-test vcodec)
