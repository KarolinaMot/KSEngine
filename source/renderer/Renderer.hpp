#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace KS
{
class Device;
class SubRenderer;
class Shader;
class UniformBuffer;
class Texture;
class DepthStencil;
class RenderTarget;

struct RendererInitParams
{
    std::vector<std::shared_ptr<Shader>> shaders;
};

struct RendererRenderParams
{
    glm::mat4x4 projectionMatrix;
    glm::mat4x4 viewMatrix;
    glm::vec3 cameraPos;
    int cpuFrame;
};

class Renderer
{
public:
    Renderer(Device& device, const RendererInitParams& params);
    ~Renderer();

    void Render(Device& device, const RendererRenderParams& params);

    std::vector<std::unique_ptr<SubRenderer>> m_subrenderers;

private:
    std::shared_ptr<UniformBuffer> m_camera_buffer;

    std::shared_ptr<Texture> m_deferredRendererTex[2][4];
    std::shared_ptr<Texture> m_deferredRendererDepthTex;
    std::shared_ptr<RenderTarget> m_deferredRendererRT;
    std::shared_ptr<DepthStencil> m_deferredRendererDepthStencil;
};
}