file(GLOB_RECURSE headers CONFIGURE_DEPENDS "stb/*.h")
add_library(stb INTERFACE ${headers})

target_include_directories(stb INTERFACE ".")