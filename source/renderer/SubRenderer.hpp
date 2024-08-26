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
class Texture;
class DepthStencil;
class SubRenderer
{
public:
    SubRenderer(const Device& device, std::shared_ptr<Shader> shader, std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<DepthStencil> depthStencil, std::shared_ptr<Texture>* resultOfPreviousPasses = nullptr, int numTextures = 0)
        : m_shader(shader)
        , m_renderTarget(renderTarget)
        , m_depthStencil(depthStencil)
        , m_resultOfPreviousPasses(std::move(resultOfPreviousPasses))
        , m_numTextures(numTextures) {};
    virtual ~SubRenderer() = default;
    virtual void Render(Device& device, int cpuFrameIndex) = 0;
    const Shader* GetShader() const { return m_shader.get(); }

protected:
    std::shared_ptr<Shader> m_shader;
    std::shared_ptr<RenderTarget> m_renderTarget;
    std::shared_ptr<DepthStencil> m_depthStencil;
    std::shared_ptr<Texture>* m_resultOfPreviousPasses;
    int m_numTextures;
};
}