# PROJECT

cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(KSEngine)

# CONFIGURATION
add_library(CompilerSettings INTERFACE)

set(CLANG "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(MSVC "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

target_compile_options(CompilerSettings INTERFACE
  "$<${CLANG}:-Wall;>"
)
target_compile_features(CompilerSettings INTERFACE cxx_std_20)

# set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=mold")

# MAIN FILES

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "source/*.cpp")
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "source/*.hpp")

# EXTERNAL DIRECTORIES
add_subdirectory(external/GLFW)
add_subdirectory(external/dx12)
add_subdirectory(external/entt)
add_subdirectory(external/glm)
add_subdirectory(external/assimp)
add_subdirectory(external/cereal)
add_subdirectory(external/DXR)

# MAIN PROJECT

add_executable(KSEngine ${sources} ${headers})
target_link_libraries(KSEngine PRIVATE CompilerSettings PUBLIC glm PUBLIC glfw PUBLIC dx12 PUBLIC entt PUBLIC assimp PUBLIC cereal PUBLIC DXR)
target_include_directories(KSEngine PUBLIC source PUBLIC external)
