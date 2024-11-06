add_library(Application)
module_default_init(Application)

target_include_directories(Application PUBLIC "private")

target_link_libraries(Application 
    PUBLIC cereal
    PUBLIC Core
    PUBLIC glfw
    PUBLIC dx12
    PUBLIC assimp
    PUBLIC stb
    PUBLIC entt
)