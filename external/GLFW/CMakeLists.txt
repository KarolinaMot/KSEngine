add_library(glfw INTERFACE "glfw3.h" "glfw3native.h")
target_link_libraries(glfw INTERFACE "${CMAKE_SOURCE_DIR}/external/GLFW/glfw3.lib")
target_include_directories(glfw INTERFACE "")