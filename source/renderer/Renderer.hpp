#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <renderer/ShaderInput.hpp>
#include <renderer/ShaderInputCollection.hpp>
namespace KS
{
class Device;
class SubRenderer;
class Shader;
class UniformBuffer;
class Texture;
class DepthStencil;
class RenderTarget;

struct SubRendererDesc
{
    std::shared_ptr<Shader> shader;
    std::vector<std::pair<ShaderInput*, ShaderInputDesc>> inputs;
};

struct RendererInitParams
{
    std::vector<SubRendererDesc> subRenderers;
};

struct RendererRenderParams
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
    Renderer(Device& device, RendererInitParams& params);
    ~Renderer();

    void Render(Device& device, const RendererRenderParams& params, bool raytraced = false);
    void QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius);
    void QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity);
    void SetAmbientLight(glm::vec3 color, float intensity);

    std::vector<std::unique_ptr<SubRenderer>> m_subrenderers;
    void QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform);

private:
    void UpdateLights(const Device& device);

    std::shared_ptr<UniformBuffer> m_camera_buffer;

    std::shared_ptr<Texture> m_deferredRendererTex[2][4];
    std::shared_ptr<Texture> m_deferredRendererDepthTex;
    std::shared_ptr<Texture> m_pbrResTex[2];
    std::shared_ptr<Texture> m_raytracingResTex[2];
    std::shared_ptr<Texture> m_lightRenderingTex[2];
    std::shared_ptr<Texture> m_lightShaftTex[2];

    std::shared_ptr<RenderTarget> m_deferredRendererRT;
    std::shared_ptr<RenderTarget> m_raytracedRendererRT;
    std::shared_ptr<RenderTarget> m_pbrResRT;
    std::shared_ptr<RenderTarget> m_lightRenderRT;
    std::shared_ptr<RenderTarget> m_lightShaftRT;
    std::shared_ptr<DepthStencil> m_deferredRendererDepthStencil;

    std::shared_ptr<StorageBuffer> mStorageBuffers[KS::NUM_SBUFFER];
    std::shared_ptr<UniformBuffer> mUniformBuffers[KS::NUM_UBUFFER];
    std::vector<DirLightInfo> m_directionalLights;
    std::vector<PointLightInfo> m_pointLights;

    std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_mainComputeShaderInputs;
    std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_deferredShaderInputs;
    std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_lightRenderInputs;
    std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_lightOccluderInputs;
    std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_lightShaftInputs;
    LightInfo m_lightInfo {};
    FogInfo m_fogInfo {};


};
}