
# ENGINE
add_library(Engine INTERFACE)

# ENGINE MODULES

message(STATUS "### Linking Engine Modules:")
target_add_module(Engine Core)
target_add_module(Engine Utility)
target_add_module(Engine Application)
target_add_module(Engine DXBackend)
