cmake_minimum_required(VERSION 3.13)

project(googletest-distribution)
set(GOOGLETEST_VERSION 1.15.2)

enable_testing()

add_subdirectory(googletest)
add_subdirectory(googlemock)
