file(GLOB_RECURSE headers CONFIGURE_DEPENDS "*.h")
add_library(DirectX12 INTERFACE ${headers})

# Define the DLL file to be copied
set(DLL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_BUILD_TYPE}")

# Custom command to copy the DLL to the binary directory
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/dxcompiler.dll ${CMAKE_BINARY_DIR}/dxil.dll
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL_DIR}/dxcompiler.dll ${CMAKE_BINARY_DIR}/dxcompiler.dll
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL_DIR}/dxil.dll ${CMAKE_BINARY_DIR}/dxil.dll
  DEPENDS ${DLL_DIR}/dxcompiler.dll ${DLL_DIR}/dxil.dll
)

# Custom target to execute the custom command
add_custom_target(copy_dll ALL DEPENDS ${CMAKE_BINARY_DIR}/dxcompiler.dll ${CMAKE_BINARY_DIR}/dxil.dll)
target_link_libraries(DirectX12 INTERFACE d3d12.lib dxgi.lib dxguid.lib)
target_include_directories(DirectX12 INTERFACE ".")

target_compile_definitions(DirectX12 
    INTERFACE "NOMINMAX" 
    INTERFACE "GLFW_EXPOSE_NATIVE_WIN32"
    INTERFACE "WIN32_LEAN_AND_MEAN"
)