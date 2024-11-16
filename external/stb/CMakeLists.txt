file(GLOB_RECURSE headers CONFIGURE_DEPENDS "stb/*.h")
add_library(stb ${headers} "stb_impl.cpp")

target_include_directories(stb PUBLIC ".")