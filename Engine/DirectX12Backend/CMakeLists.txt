add_library(DirectX12Backend)
module_default_init(DirectX12Backend)

target_link_libraries(DirectX12Backend
    PUBLIC Core
    PUBLIC DirectX12
)