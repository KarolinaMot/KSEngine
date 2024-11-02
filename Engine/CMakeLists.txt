
# ENGINE
add_library(Engine INTERFACE)

if (ENABLE_PCH)
    add_subdirectory(PCH)
endif ()

# ENGINE MODULES

message(STATUS "### Linking Engine Modules:")
target_add_module(Engine Core)
target_add_module(Engine Utility)
