add_library(unity ../third-party/Unity/src/unity.c ../third-party/Unity/extras/fixture/src/unity_fixture.c)
target_include_directories(unity PUBLIC ../third-party/Unity/src/ ../third-party/Unity/extras/fixture/src/ ../third-party/Unity/extras/memory/src/)

add_executable(vcodec-tests vcodec_test_main.c bitstream_test.c entropy_coding_test.c)
target_link_libraries(vcodec-tests vcodec unity)
target_include_directories(vcodec-tests PRIVATE ../src/)
