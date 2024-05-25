file(GLOB_RECURSE sources CONFIGURE_DEPENDS "*.cpp")
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "*.hpp")

add_library(Engine ${sources} ${headers})

target_include_directories(Engine PUBLIC "source" "../external")
target_link_libraries(Engine PRIVATE CompilerSettings)