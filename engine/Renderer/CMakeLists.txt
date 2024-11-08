add_library(Renderer)
module_default_init(Renderer)

target_link_libraries(Renderer 
    PUBLIC Core
    PUBLIC DXBackend    
    PUBLIC cereal
    PUBLIC glfw
    PUBLIC assimp
    PUBLIC stb
    PUBLIC entt
)