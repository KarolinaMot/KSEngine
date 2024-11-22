file(GLOB_RECURSE sources CONFIGURE_DEPENDS "*.hpp")
add_library(cereal INTERFACE ${sources})