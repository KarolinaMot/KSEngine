# Adds all subdirectories and finds all system packages

add_subdirectory(GLFW)
add_subdirectory(dx12)
add_subdirectory(entt)
add_subdirectory(glm)
add_subdirectory(assimp)

# set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "C:/Program Files/Assimp/cmake-modules")
# find_package(assimp-plain REQUIRED)

add_subdirectory(cereal)