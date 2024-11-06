# Adds all subdirectories to external directories

add_subdirectory(glfw)
add_subdirectory(DirectX12)
add_subdirectory(entt)

set(GLM_BUILD_LIBRARY ON)
add_subdirectory(glm)

add_subdirectory(stb)
add_subdirectory(assimp)
add_subdirectory(cereal)
