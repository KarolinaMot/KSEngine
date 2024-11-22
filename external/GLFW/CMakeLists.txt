add_library(glfw INTERFACE "glfw/glfw3.h" "glfw/glfw3native.h")
target_link_libraries(glfw INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/glfw3.lib")

target_include_directories(glfw INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(glfw 
    INTERFACE "NOMINMAX" 
    INTERFACE "GLFW_EXPOSE_NATIVE_WIN32"
    INTERFACE "WIN32_LEAN_AND_MEAN"
)