#pragma once
#include <memory>
#include <string>
#include <vector>
#include <renderer/Shader.hpp>

struct DXCommandContext;
namespace KS
{
class Device;
class RenderTarget;
class DepthStencil;
class Texture;
class Scene;
struct ShaderInputBindDesc;
class ShaderInput;

struct SubRendererDesc
{
    std::shared_ptr<Shader> shader = nullptr;
    std::shared_ptr<RenderTarget> renderTarget = nullptr;
    std::shared_ptr<DepthStencil> depthStencil = nullptr;
};

class SubRenderer
{
public:
    SubRenderer(const Device&, SubRendererDesc& desc)
        : m_shader(desc.shader), m_renderTarget(desc.renderTarget), m_depthStencil(desc.depthStencil){};
    virtual ~SubRenderer() = default;
    virtual void Render(Device& device, DXCommandContext* commandContext, Scene& scene,
                        std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>& inputs, bool clearRT) = 0;
    const Shader* GetShader() const { return m_shader.get(); }

    void SetRenderTarget(std::shared_ptr<RenderTarget> rt) { m_renderTarget = rt; }

    void Recompile(const Device& device) { m_shader->Compile(device); }

protected:
    std::shared_ptr<Shader> m_shader;
    std::shared_ptr<RenderTarget> m_renderTarget;
    std::shared_ptr<DepthStencil> m_depthStencil;
};
}  // namespace KS