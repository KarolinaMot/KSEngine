add_library(DXBackend)
module_default_init(DXBackend)

target_link_libraries(DXBackend
    PUBLIC Core
    PUBLIC DirectX12
)