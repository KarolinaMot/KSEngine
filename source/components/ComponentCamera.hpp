#pragma once
#include "../math/Geometry.hpp"
#pragma warning(push, 0)
#include <glm/glm.hpp>
#pragma warning(pop)

namespace KS
{

class ComponentFirstPersonCamera
{
public:
    Camera GenerateCamera(const glm::mat4& world_matrix) const;

    glm::vec3 eulerAngles {};
    bool isOrtho = false;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float aspectRatio = 16.0f / 9.0f;
    float fieldOfView = glm::radians(75.0f); // for perspective
    float extentSize = 100.0f; // for orthographic
};

}