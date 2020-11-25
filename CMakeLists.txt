cmake_minimum_required(VERSION 3.0.2)
project(vcodec C)

add_library(vcodec src/vcodec_common.c src/vcodec_med_gr.c src/vcodec_inter.c
                   src/vcodec_vec.c src/vec_table.c src/vcodec_dct.c src/vcodec_transform.c src/vcodec_decoder.c)
target_include_directories(vcodec PUBLIC include)
target_include_directories(vcodec PRIVATE src)
target_compile_options(vcodec PRIVATE -ggdb3)

add_executable(vcodec-test app/main.c src/tools/source.c src/tools/y4m.c)
target_link_libraries(vcodec-test vcodec m)
add_executable(vcodec-dec-test app/decoder_test.c)
target_link_libraries(vcodec-dec-test vcodec m)

add_subdirectory(test)
