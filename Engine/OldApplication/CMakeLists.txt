add_library(OldApplication)
module_default_init(OldApplication)

target_include_directories(OldApplication PUBLIC "private")

target_link_libraries(OldApplication 
    PUBLIC cereal
    PUBLIC Core
    PUBLIC glfw
    PUBLIC dx12
    PUBLIC assimp
    PUBLIC stb
    PUBLIC entt
)