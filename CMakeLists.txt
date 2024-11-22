# PROJECT

cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(KSEngine)

# OPTIONS

option(ENABLE_TESTS "Compile test executable" ON)
option(ENABLE_PCH "Compile with Precompiled Headers" OFF)
option(ENABLE_UNITY "Compile with Unity Builds" OFF)

# INCLUDES

include(FetchContent)
include(scripts/cmake/code-modules.cmake)

# COMPILE SETTINGS

add_library(CompileFlags INTERFACE)
target_compile_options(CompileFlags INTERFACE -Wall INTERFACE -Wextra)

# PCH

if (ENABLE_PCH)
    add_subdirectory(Engine/PCH)
endif ()

# TEST EXECUTABLE

if (ENABLE_TESTS)
    add_subdirectory(external/googletest)
    add_subdirectory(tests)
endif ()

# MAIN EXECUTABLE

add_subdirectory(engine)
add_subdirectory(external)

add_executable(KSEngine "engine/main.cpp")
target_link_libraries(KSEngine PRIVATE Engine)
