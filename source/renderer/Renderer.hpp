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

    void Render(Device& device, const RendererRenderParams& params, bool raytraced = false);
    void QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius);
    void QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity);
    void SetAmbientLight(glm::vec3 color, float intensity);

    std::vector<std::unique_ptr<SubRenderer>> m_subrenderers;

private:
    void UpdateLights(const Device& device);
    void Rasterize(Device& device, const RendererRenderParams& params);
    void Raytrace(Device& device, const RendererRenderParams& params);

    std::shared_ptr<UniformBuffer> m_camera_buffer;

    std::shared_ptr<Texture> m_deferredRendererTex[2][4];
    std::shared_ptr<Texture> m_deferredRendererDepthTex;
    std::shared_ptr<Texture> m_pbrResTex[2];
    std::shared_ptr<RenderTarget> m_deferredRendererRT;
    std::shared_ptr<RenderTarget> m_pbrResRT;
    std::shared_ptr<DepthStencil> m_deferredRendererDepthStencil;

    std::shared_ptr<StorageBuffer> mStorageBuffers[KS::NUM_SBUFFER];
    std::shared_ptr<UniformBuffer> mUniformBuffers[KS::NUM_UBUFFER];
    std::vector<DirLightInfo> m_directionalLights;
    std::vector<PointLightInfo> m_pointLights;
    LightInfo m_lightInfo {};
};
}