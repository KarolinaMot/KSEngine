#include "ComponentCamera.hpp"

Camera ComponentFirstPersonCamera::GenerateCamera(const glm::mat4& world_matrix) const
{
    Camera Result;
    if (isOrtho)
    {
        Result = Camera::Orthographic(
            world_matrix[3],
            world_matrix[3] + world_matrix * glm::vec4(World::FORWARD, 0.0f),
            aspectRatio,
            extentSize,
            nearPlane,
            farPlane);
    }
    else
    {
        Result = Camera::Perspective(
            world_matrix[3],
            world_matrix[3] + world_matrix * glm::vec4(World::FORWARD, 0.0f),
            aspectRatio,
            fieldOfView,
            nearPlane,
            farPlane);
    }

    return Result;
}