#pragma once
#include "SubRenderer.hpp"
#pragma warning(push, 0)
#include <glm/glm.hpp>
#pragma warning(pop)
#include <vector>
#include <memory>

struct DXCommandContext;

namespace KS
{
class Device;
class Scene;
class Mesh;
class ShaderInput;
struct ShaderInputBindDesc;
struct SubRendererDesc;
class UniformBuffer;

    class RTRenderer : public SubRenderer
    {
    public:
        RTRenderer(const Device& device, std::shared_ptr<Shader>& shader);
        ~RTRenderer();

        void Render(Device& device, DXCommandContext* commandContext, RenderParameters& par) override;

    private:
        bool m_raytraced = false;
        int32_t m_frameCount = 0;
    };
}
