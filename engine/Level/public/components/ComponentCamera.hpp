#pragma once
#include <Geometry.hpp>
#include <glm/trigonometric.hpp>

struct ComponentFirstPersonCamera
{
    enum class Type : uint32_t
    {
        PERSPECTIVE,
        ORTHOGRAPHIC
    };

    glm::vec3 euler_rotation = {};
    float field_of_view = glm::radians(90.0f);
    float near_clip = 0.01f;
    float far_clip = 1000.0f;
    Type type = Type::PERSPECTIVE;
};