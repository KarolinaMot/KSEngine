#include "Geometry.hpp"

KS::BoundingBox::BoundingBox(const glm::vec3& m_center, const glm::vec3& m_extents)
    : m_center(m_center)
    , m_extents(glm::vec3(std::fabsf(m_extents.x), std::fabsf(m_extents.y), std::fabsf(m_extents.z)))
{
}

std::array<glm::vec3, 8> KS::BoundingBox::GetEdgePoints() const
{
    return {
        m_center + glm::vec3(-m_extents.x, -m_extents.y, -m_extents.z),
        m_center + glm::vec3(m_extents.x, -m_extents.y, -m_extents.z),
        m_center + glm::vec3(-m_extents.x, m_extents.y, -m_extents.z),
        m_center + glm::vec3(-m_extents.x, -m_extents.y, m_extents.z),
        m_center + glm::vec3(m_extents.x, m_extents.y, -m_extents.z),
        m_center + glm::vec3(-m_extents.x, m_extents.y, m_extents.z),
        m_center + glm::vec3(m_extents.x, -m_extents.y, m_extents.z),
        m_center + glm::vec3(m_extents.x, m_extents.y, m_extents.z),
    };
}

KS::BoundingBox KS::BoundingBox::ApplyTransform(const glm::mat4& transform) const
{
    // There might be a better way of calculating these transformed values
    // https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling

    glm::vec3 center = transform * glm::vec4(m_center, 1.0f);
    glm::vec3 min = center, max = center;

    auto edges = GetEdgePoints();
    for (auto edge : edges)
    {
        glm::vec3 transformed = transform * glm::vec4(edge, 1.0f);
        min = glm::min(transformed, min);
        max = glm::max(transformed, max);
    }

    auto extent = (max - min) * 0.5f;

    return BoundingBox(min + extent, extent);
}

bool KS::BoundingBox::FrustumTest(const std::array<Plane, 6>& frustum) const
{
    // Using separating axis theorem
    // From https://gist.github.com/Kinwailo/d9a07f98d8511206182e50acda4fbc9b

    glm::vec3 boxMin = GetStart();
    glm::vec3 boxMax = GetEnd();

    glm::vec3 vmin {}, vmax {};

    for (auto& plane : frustum)
    {
        auto planeNormal = plane.GetNormal();

        // X axis
        if (planeNormal.x > 0)
        {
            vmin.x = boxMin.x;
            vmax.x = boxMax.x;
        }
        else
        {
            vmin.x = boxMax.x;
            vmax.x = boxMin.x;
        }
        // Y axis
        if (planeNormal.y > 0)
        {
            vmin.y = boxMin.y;
            vmax.y = boxMax.y;
        }
        else
        {
            vmin.y = boxMax.y;
            vmax.y = boxMin.y;
        }
        // Z axis
        if (planeNormal.z > 0)
        {
            vmin.z = boxMin.z;
            vmax.z = boxMax.z;
        }
        else
        {
            vmin.z = boxMax.z;
            vmax.z = boxMin.z;
        }
        if (plane.GetSignedDistance(vmax) < 0.0f)
            return false;
    }

    return true;
}

KS::Plane::Plane(const glm::vec3& normal, const glm::vec3& point)
{
    m_normal = glm::normalize(normal);
    m_signedOriginDistance = glm::dot(m_normal, point);
}

KS::Plane::Plane(const glm::vec3& normal, float m_signedOriginDistance)
    : m_normal(normal)
    , m_signedOriginDistance(m_signedOriginDistance)
{
}

float KS::Plane::GetSignedDistance(const glm::vec3& point) const
{
    return glm::dot(point, m_normal) - m_signedOriginDistance;
}

KS::Camera KS::Camera::Perspective(const glm::vec3& position, const glm::vec3 lookAt, float aspectRatio, float fieldOfView, float nearClip, float farClip)
{
    Camera camera;
    camera.type = CameraType::PERSPECTIVE;
    camera.position = position;
    camera.lookat = lookAt;
    camera.aspectRatio = aspectRatio;
    camera.nearClip = nearClip;
    camera.farClip = farClip;
    camera.fieldOfView = fieldOfView;

    return camera;
}

KS::Camera KS::Camera::Orthographic(const glm::vec3& position, const glm::vec3 lookAt, float aspectRatio, float extents, float nearClip, float farClip)
{
    Camera camera;
    camera.type = CameraType::ORTHOGRAPHIC;
    camera.position = position;
    camera.lookat = lookAt;
    camera.aspectRatio = aspectRatio;
    camera.nearClip = nearClip;
    camera.farClip = farClip;
    camera.boundExtents = extents;

    return camera;
}

glm::mat4 KS::Camera::GetView() const
{
    return glm::lookAtLH(position, lookat, World::UP);
}

glm::mat4 KS::Camera::GetProjection() const
{
    if (type == CameraType::PERSPECTIVE)
    {
        return glm::perspectiveLH_ZO(fieldOfView, aspectRatio, nearClip, farClip);
    }
    else // ORTHOGRAPHIC
    {
        return glm::orthoLH_ZO(
            -boundExtents * aspectRatio,
            boundExtents * aspectRatio,
            -boundExtents,
            boundExtents,
            nearClip, farClip // Depth planes
        );
    }
}

glm::mat3 KS::Camera::GetRotation() const
{
    auto forward = glm::normalize(lookat - position);
    auto right = glm::normalize(glm::cross(forward, World::UP));
    auto up = glm::normalize(glm::cross(right, forward));

    return glm::mat3(right, forward, up);
}

glm::vec3 KS::Camera::GetPosition() const
{
    return position; }

glm::vec3 KS::Camera::GetForward() const { return glm::normalize(lookat - position); }

glm::vec3 KS::Camera::GetRight() const { return glm::normalize(glm::cross(GetForward(), KS::World::UP)); }

std::array<KS::Plane, 6> KS::Camera::GetFrustum() const
{
    auto rotation = GetRotation();

    const auto& right = rotation[0];
    const auto& forward = rotation[1];
    const auto& up = rotation[2];

    // Near and far

    auto nearVector = forward * nearClip;
    auto farVector = forward * farClip;

    Plane nearPlane = Plane(forward, position + nearVector);
    Plane farPlane = Plane(-forward, position + farVector);

    Plane rightPlane, leftPlane, topPlane, bottomPlane;

    if (type == CameraType::PERSPECTIVE)
    {

        float halfVertical = farClip * std::tanf(fieldOfView * 0.5f);
        float halfHorizontal = halfVertical * aspectRatio;

        auto rightNormal = glm::cross(farVector - (right * halfHorizontal), up);
        auto leftNormal = -glm::cross(farVector + (right * halfHorizontal), up);

        auto topNormal = glm::cross(farVector + (up * halfVertical), right);
        auto bottomNormal = -glm::cross(farVector - (up * halfVertical), right);

        rightPlane = Plane(rightNormal, position);
        leftPlane = Plane(leftNormal, position);
        topPlane = Plane(topNormal, position);
        bottomPlane = Plane(bottomNormal, position);
    }
    else // ORTHOGRAPHIC
    {
        auto rightPoint = right * boundExtents * aspectRatio;
        auto leftPoint = -right * boundExtents * aspectRatio;

        auto topPoint = up * boundExtents;
        auto bottomPoint = -up * boundExtents;

        rightPlane = Plane(-right, position + rightPoint);
        leftPlane = Plane(right, position + leftPoint);
        topPlane = Plane(-up, position + topPoint);
        bottomPlane = Plane(up, position + bottomPoint);
    }

    return { nearPlane, farPlane, rightPlane, leftPlane, topPlane, bottomPlane };
}
