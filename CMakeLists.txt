cmake_minimum_required(VERSION 3.0.2)
project(vcodec C)

add_library(vcodec src/vcodec_common.c src/vcodec_med_gr.c src/vcodec_inter.c)
target_include_directories(vcodec PUBLIC include/)
target_compile_options(vcodec PRIVATE -O3)

add_executable(vcodec-test test/main.c test/v4l_capture.c)
target_link_libraries(vcodec-test vcodec)
