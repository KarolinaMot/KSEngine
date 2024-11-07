# PROJECT

cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(KSEngine)

# OPTIONS

option(ENABLE_TESTS "Compile test executable" ON)
option(ENABLE_PCH "Compile with Precompiled Headers" ON)
option(ENABLE_UNITY "Compile with Unity Builds" ON)

# INCLUDES

include(FetchContent)
include(scripts/cmake/code-modules.cmake)

# PCH

if (ENABLE_PCH)
    add_subdirectory(Engine/PCH)
endif ()

# TEST EXECUTABLE

if (ENABLE_TESTS)

    add_subdirectory(ThirdParty/googletest)
    add_executable(KSTests)
    target_link_libraries(KSTests PRIVATE GTest::gtest_main)

    if (ENABLE_PCH)
        target_link_libraries(KSTests PRIVATE PCH)
        target_precompile_headers(KSTests REUSE_FROM PCH)
    endif ()

    if (ENABLE_UNITY)
        target_enable_unity(KSTests)
    endif ()
    
endif ()

# MAIN EXECUTABLE

add_subdirectory(Engine)
add_subdirectory(ThirdParty)

add_executable(KSEngine "Engine/main.cpp")
target_link_libraries(KSEngine PRIVATE Engine)
