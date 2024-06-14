#pragma once
#include <glm/glm.hpp>
#include "../math/Geometry.hpp"

namespace KS {

class ComponentFirstPersonCamera {
public:
    Camera GenerateCamera(const glm::mat4& world_matrix) const;

    glm::vec3 eulerAngles {};
    bool isOrtho = false;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float aspectRatio = 16.0f / 9.0f;
    float fieldOfView = glm::radians(90.0f); // for perspective
    float extentSize = 100.0f; // for orthographic
};

}