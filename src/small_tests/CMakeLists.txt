add_subdirectory(perf_uva)


add_executable(test_multi_thread_cht test_multi_thread_cht.cc)
target_link_libraries(test_multi_thread_cht base)


# add_executable(test_print test_print.cu)