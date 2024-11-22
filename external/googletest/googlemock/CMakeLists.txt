add_library(gmock "src/gmock-all.cc")
target_link_libraries(gmock PUBLIC gtest)
target_include_directories(gtest PUBLIC "include" PRIVATE "src")

add_library(gmock-main "src/gmock_main.cc")
target_link_libraries(gmock-main PUBLIC gmock)