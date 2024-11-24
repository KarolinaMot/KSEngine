add_library(Level)
module_default_init(Level)

target_link_libraries(Level
    PUBLIC Core
    PUBLIC entt
)