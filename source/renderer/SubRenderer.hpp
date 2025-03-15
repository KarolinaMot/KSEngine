#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace KS
{
class Device;
class Shader;
class RenderTarget;
class DepthStencil;
class Texture;
class ShaderInput;
struct ShaderInputDesc;
class SubRenderer
{
public:
    SubRenderer(const Device& device, std::shared_ptr<Shader> shader,
                std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs)
        : m_shader(shader), m_inputs(inputs){};
    virtual ~SubRenderer() = default;
    virtual void Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<DepthStencil> depthStencil, Texture** previoiusPassResults = nullptr, int numTextures = 0) = 0;
    const Shader* GetShader() const { return m_shader.get(); }

protected:
    std::shared_ptr<Shader> m_shader;
    std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_inputs;
};
}