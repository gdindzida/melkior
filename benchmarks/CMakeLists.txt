add_executable(bench_opencv blur_example_benchmark.cpp)

target_link_libraries(bench_opencv PRIVATE ${OpenCV_LIBS})