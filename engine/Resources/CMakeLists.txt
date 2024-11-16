add_library(Utility)
module_default_init(Utility)

target_link_libraries(
    PUBLIC Utility
    PUBLIC cereal
    PUBLIC assimp
    PUBLIC stb
)