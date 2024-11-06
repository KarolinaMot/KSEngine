#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// TODO: Still no support for transform hierarchies
class ComponentTransform
{
public:
    ComponentTransform(
        const glm::vec3& translation = {},
        const glm::quat& rotation = { 1.0f, 0.0f, 0.0f, 0.0f },
        const glm::vec3& scale = { 1.0f, 1.0f, 1.0f })
        : m_Translation(translation)
        , m_Rotation(rotation)
        , m_Scale(scale)
    {
    }

    void SetLocalTranslation(const glm::vec3& translation) { m_Translation = translation; }
    void SetLocalRotation(const glm::quat& rotation) { m_Rotation = rotation; }
    void SetLocalScale(const glm::vec3& scale) { m_Scale = scale; }

    auto GetLocalTranslation() const { return m_Translation; }
    auto GetLocalRotation() const { return m_Translation; }
    auto GetLocalScale() const { return m_Translation; }

    glm::mat4 GetWorldMatrix() const;

private:
    glm::vec3 m_Translation;
    glm::quat m_Rotation;
    glm::vec3 m_Scale;
};
