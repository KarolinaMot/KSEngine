#include "ComponentTransform.hpp"

glm::mat4 KS::ComponentTransform::GetWorldMatrix() const
{
    auto Result = glm::scale(glm::mat4_cast(m_Rotation), m_Scale);
    Result[3] = { m_Translation.x, m_Translation.y, m_Translation.z, 1.0f };
    return Result;
}
