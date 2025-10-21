#pragma once
#include "SubRenderer.hpp"
#include <glm/glm.hpp>
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
        RTRenderer(const Device& device, Scene& scene, SubRendererDesc& desc);
        ~RTRenderer();

        void Render(Device& device, DXCommandContext* commandContext, Scene& scene,
                    std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>& inputs, bool clearRT) override;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
        bool m_raytraced = false;
        int32_t m_frameCount = 0;
    };
}
