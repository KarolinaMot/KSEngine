#pragma once
#include <memory>
#include <glm/glm.hpp>
#include <vector>
#include <renderer/InfoStructs.hpp>

namespace KS
{
class Device;
class SubRenderer;
class DepthStencil;
class RenderTarget;
class ShaderInputCollection;
class ShaderInput;
class UniformBuffer;
class Scene;
struct ShaderInputDesc;

struct RenderTickParams
{
    glm::mat4x4 projectionMatrix;
    glm::mat4x4 viewMatrix;
    glm::vec3 cameraPos;
    glm::vec3 cameraRight;
    int cpuFrame;
};

class Renderer
{
public:
    Renderer(Device& device);
    ~Renderer();

    void Render(Device& device, Scene& scene, const RenderTickParams& params, bool raytraced = false);

private:
    std::shared_ptr<ShaderInputCollection> m_mainInputs;
    std::unique_ptr<SubRenderer> m_subrenderers[NUM_SUBRENDER];
    std::shared_ptr<RenderTarget> m_renderTargets[NUM_SUBRENDER];
    std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_inputs[NUM_SUBRENDER];
    std::shared_ptr<DepthStencil> m_deferredRendererDepthStencil;
    std::shared_ptr<UniformBuffer> m_camera_buffer;

    // std::shared_ptr<Texture> m_deferredRendererTex[2][4];E
    // std::shared_ptr<Texture> m_deferredRendererDepthTex;
    // std::shared_ptr<Texture> m_pbrResTex[2];
    // std::shared_ptr<Texture> m_raytracingResTex[2];
    // std::shared_ptr<Texture> m_lightRenderingTex[2];
    // std::shared_ptr<Texture> m_lightShaftTex[2];
    // std::shared_ptr<Texture> m_upscaledLightShaftTex[2];

    // std::shared_ptr<RenderTarget> m_deferredRendererRT;
    // std::shared_ptr<RenderTarget> m_raytracedRendererRT;
    // std::shared_ptr<RenderTarget> m_pbrResRT;
    // std::shared_ptr<RenderTarget> m_lightRenderRT;
    // std::shared_ptr<RenderTarget> m_lightShaftRT;
    // std::shared_ptr<RenderTarget> m_upscaledLightShaftRT;
    // std::shared_ptr<DepthStencil> m_deferredRendererDepthStencil;

    // std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_mainComputeShaderInputs;
    // std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_deferredShaderInputs;
    // std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_lightRenderInputs;
    // std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_lightOccluderInputs;
    // std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_lightShaftInputs;
    // std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_upscaleLightShaftInputs;
};
}  // namespace KS