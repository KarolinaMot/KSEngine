set(GLM_BUILD_LIBRARY ON)
add_subdirectory(glm)
target_include_directories(glm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")