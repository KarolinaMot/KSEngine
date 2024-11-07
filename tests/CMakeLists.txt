add_executable(KSTests "example_tests.cpp")
target_link_libraries(KSTests PRIVATE gmock-main)
target_output_directory(KSTests ${CMAKE_BINARY_DIR})