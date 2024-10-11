add_library(DXR)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "*.cpp")
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "*.h")

target_sources(DXR PRIVATE ${sources} ${headers})