#pragma once

#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

class Plane;
class BoundingBox;
class Camera;

// Coordinate system constants (Left handed, Y is up)
namespace World
{

constexpr glm::vec3 FORWARD = { 0.0f, 0.0f, 1.0f };
constexpr glm::vec3 RIGHT = { 1.0f, 0.0f, 0.0f };
constexpr glm::vec3 UP = { 0.0f, 1.0f, 0.0f };

}

// Axis aligned 3D bounding box
class BoundingBox
{
public:
    BoundingBox() = default;
    BoundingBox(const glm::vec3& m_center, const glm::vec3& m_extents);

    glm::vec3 GetStart() const { return m_center - m_extents; }
    glm::vec3 GetEnd() const { return m_center + m_extents; }
    glm::vec3 GetSize() const { return m_extents * 2.0f; }

    std::array<glm::vec3, 8> GetEdgePoints() const;
    BoundingBox ApplyTransform(const glm::mat4& transform) const;
    bool FrustumTest(const std::array<Plane, 6>& frustum) const;
    glm::vec3 GetCenter() const { return m_center; }
    void SetExtents(glm::vec3 extents) { m_extents = extents; }
    glm::vec3 GetExtents() const { return m_extents; }

private:
    glm::vec3 m_center;
    glm::vec3 m_extents; // size / 2
};

// 3D plane
class Plane
{
public:
    Plane() = default;

    // From a normal and a point belonging on the plane
    Plane(const glm::vec3& normal, const glm::vec3& point);

    // From normal and signed distance to the origin
    Plane(const glm::vec3& normal, float m_signedOriginDistance);

    // signed distance between a point and a plane (positive means it is in the normal points towards)
    float GetSignedDistance(const glm::vec3& point) const;
    glm::vec3 GetNormal() const { return m_normal; }

private:
    glm::vec3 m_normal = glm::vec3(1.0f, 0.0f, 0.0f); //(A, B, C)
    float m_signedOriginDistance {}; // D
};

// Camera data, used for rendering systems
// Contains functionality for creating the view and projection matrices, as well as frustum planes.
// Meant to be created for every single frame and be immutable
// Not to be confused with CameraComponent
class Camera
{
public:
    Camera() = default;

    // Look at, perspective camera
    static Camera Perspective(
        const glm::vec3& position,
        const glm::vec3 lookAt,
        float aspectRatio,
        float fieldOfView,
        float nearClip,
        float farClip);

    // Look at, orthographic camera
    static Camera Orthographic(
        const glm::vec3& position,
        const glm::vec3 lookAt,
        float aspectRatio,
        float extents,
        float nearClip,
        float farClip);

    glm::mat4 GetView() const;
    glm::mat4 GetProjection() const;

    glm::mat3 GetRotation() const; // Returns a 3x3 matrix, which can be converted to quat
    glm::vec3 GetPosition() const;

    std::array<Plane, 6> GetFrustum() const;

private:
    enum class CameraType
    {
        PERSPECTIVE,
        ORTHOGRAPHIC
    } type;

    glm::vec3 position {};
    glm::vec3 lookat {};

    float aspectRatio {};
    float fieldOfView {};
    float boundExtents {};
    float nearClip {};
    float farClip {};
};
