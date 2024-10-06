# PROJECT

cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(KSEngine)

# PROJECT GENERAL FLAGS (COMPILER SPECIFIC IN TOOLCHAIN)

add_library(CompilerSettings INTERFACE)
target_compile_features(CompilerSettings INTERFACE cxx_std_20)
target_compile_definitions(CompilerSettings INTERFACE "WIN32_LEAN_AND_MEAN" "NOMINMAX")

# MAIN FILES

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "source/*.cpp")
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "source/*.hpp")

# EXTERNAL DIRECTORIES
add_subdirectory(external)

# MAIN PROJECT
add_executable(KSEngine ${sources} ${headers})
target_link_libraries(KSEngine PRIVATE CompilerSettings PUBLIC glm PUBLIC glfw PUBLIC dx12 PUBLIC entt PUBLIC assimp PUBLIC cereal)
target_include_directories(KSEngine PUBLIC source PUBLIC external)
