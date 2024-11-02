# PROJECT

cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(KSEngine)

# OPTIONS

option(ENABLE_PCH "Compile with Precompiled Headers" ON)
option(UNITY_BUILDS "Compile with Unity Builds" ON)

# INCLUDES

include(FetchContent)
include(scripts/cmake/code-modules.cmake)

# MAIN EXECUTABLE

add_subdirectory(Engine)

add_executable(KSEngine "engine/main.cpp")
target_link_libraries(KSEngine PRIVATE Engine)
