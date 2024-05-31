file(GLOB_RECURSE headers CONFIGURE_DEPENDS "*.hpp")

add_library(entt INTERFACE ${headers})
target_include_directories(entt INTERFACE "")