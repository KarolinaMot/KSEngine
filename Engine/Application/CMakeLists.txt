add_library(Application)
module_default_init(Application)

target_link_libraries(Application
    PUBLIC Core
    PUBLIC glfw
)