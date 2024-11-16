# Adds all subdirectories

add_subdirectory(glfw)
add_subdirectory(DirectX12)
add_subdirectory(entt)
add_subdirectory(glm)
add_subdirectory(stb)
add_subdirectory(assimp)
add_subdirectory(cereal)

set(TRACY_ENABLE ON)
add_subdirectory(tracy)