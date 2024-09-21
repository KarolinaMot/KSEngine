#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace KS
{
struct SubRendererInitParams
{
    std::string shader;
};

class Device;
class Shader;
class RenderTarget;
class DepthStencil;
class Texture;
class SubRenderer
{
public:
    SubRenderer(const Device& device, std::shared_ptr<Shader> shader)
        : m_shader(shader) {};
    virtual ~SubRenderer() = default;
    virtual void Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<DepthStencil> depthStencil, Texture** previoiusPassResults = nullptr, int numTextures = 0) = 0;
    const Shader* GetShader() const { return m_shader.get(); }

protected:
    std::shared_ptr<Shader> m_shader;
};
}