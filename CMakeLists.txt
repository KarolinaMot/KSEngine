# PROJECT

cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(Revita)

# CONFIGURATION
add_library(CompilerSettings INTERFACE)

set(CLANG "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(MSVC "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

target_compile_options(CompilerSettings INTERFACE
  "$<${CLANG}:-Wall;-Wextra;>"
  "$<${MSVC}:-W3>"
)
target_compile_features(CompilerSettings INTERFACE cxx_std_20)

# set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=mold")

# EXTERNAL DIRECTORIES
add_subdirectory(engine)
add_subdirectory(external/glm)
add_subdirectory(external/GLFW)

# MAIN PROJECT

add_executable(Revita main.cpp)
target_link_libraries(Revita PRIVATE CompilerSettings PUBLIC Engine PUBLIC glm PUBLIC glfw)
