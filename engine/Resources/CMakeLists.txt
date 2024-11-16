add_library(Resources)
module_default_init(Resources)

target_link_libraries(Resources
    PUBLIC Utility
    PUBLIC cereal
    PUBLIC assimp
    PUBLIC stb
)