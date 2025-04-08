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
    SubRenderer(const Device& device, std::shared_ptr<Shader> shader)
        : m_shader(shader){};
    virtual ~SubRenderer() = default;
    virtual void Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget,
                        std::shared_ptr<DepthStencil> depthStencil,
                        std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs, bool clearRenderTarget=true) = 0;
    const Shader* GetShader() const { return m_shader.get(); }

protected:
    std::shared_ptr<Shader> m_shader;
};
}