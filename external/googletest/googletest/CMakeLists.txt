add_library(gtest)
target_sources(gtest PRIVATE "src/gtest-all.cc")
target_include_directories(gtest PUBLIC "include" PRIVATE "src")

add_library(gtest-main)
target_sources(gtest-main PRIVATE "src/gtest_main.cc")
target_link_libraries(gtest-main PUBLIC gtest)