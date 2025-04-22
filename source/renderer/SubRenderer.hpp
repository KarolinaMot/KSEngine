#pragma once
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
class Scene;
class ShaderInput;
struct ShaderInputDesc;

struct SubRendererDesc
{
    std::shared_ptr<Shader> shader;
    std::shared_ptr<RenderTarget> renderTarget;
    std::shared_ptr<DepthStencil> depthStencil;
};



class SubRenderer
{
public:
    SubRenderer(const Device& device, SubRendererDesc& desc)
        : m_shader(desc.shader), m_renderTarget(desc.renderTarget), m_depthStencil(desc.depthStencil){};
    virtual ~SubRenderer() = default;
    virtual void Render(Device& device, Scene& scene, std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs, bool clearRT) = 0;
    const Shader* GetShader() const { return m_shader.get(); }

protected:
    std::shared_ptr<Shader> m_shader;
    std::shared_ptr<RenderTarget> m_renderTarget;
    std::shared_ptr<DepthStencil> m_depthStencil;
};
}  // namespace KS