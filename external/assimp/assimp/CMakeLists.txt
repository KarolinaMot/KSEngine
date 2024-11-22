file(GLOB_RECURSE headers CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE cpp_headers CONFIGURE_DEPENDS "*.hpp")

add_library(assimp INTERFACE ${headers} ${cpp_headers})

# Define the DLLs file to be copied
set(DLL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Lib/${CMAKE_BUILD_TYPE})
set(ASSIMP "$<$<CONFIG:Debug>:assimp-vc143-mtd>$<$<CONFIG:Release>:assimp-vc143-mt>")

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/${ASSIMP}.dll
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL_DIR}/${ASSIMP}.dll ${CMAKE_BINARY_DIR}/${ASSIMP}.dll
  DEPENDS ${DLL_DIR}/${ASSIMP}.dll
)

add_custom_target(copy_assimp_dll ALL DEPENDS ${CMAKE_BINARY_DIR}/${ASSIMP}.dll)
target_link_libraries(assimp INTERFACE ${DLL_DIR}/${ASSIMP}.lib)

