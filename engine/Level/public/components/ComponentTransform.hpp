#pragma once
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

struct ComponentTransform3D
{
    glm::quat rotation { 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 translation { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale { 1.0f, 1.0f, 1.0f };

    // Helpers

    glm::mat4 ToMatrix() const
    {
        glm::mat4 out = glm::scale(glm::mat4_cast(rotation), scale);
        out[3] = { translation, 1.0f };
        return out;
    }
};