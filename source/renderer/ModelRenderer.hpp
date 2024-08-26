#pragma once

#include "SubRenderer.hpp"
#include <fileio/ResourceHandle.hpp>

#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <resources/Model.hpp>

#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include "InfoStructs.hpp"

namespace KS
{
class StorageBuffer;
class RenderTarget;
class DepthStencil;
class UniformBuffer;
class Device;
class Shader;
class Mesh;
class Model;
class Texture;
struct DrawEntry;

class ModelRenderer : public SubRenderer
{
public:
    ModelRenderer(const Device& device, std::shared_ptr<Shader> shader);
    ~ModelRenderer();

    void QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius);
    void QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity);
    void SetAmbientLight(glm::vec3 color, float intensity);
    void QueueModel(ResourceHandle<Model> model, const glm::mat4& transform);
    void Render(Device& device, int cpuFrameIndex) override;

private:
    const Mesh* GetMesh(const Device& device, ResourceHandle<Mesh> mesh);
    const Model* GetModel(ResourceHandle<Model> model);
    std::shared_ptr<Texture> GetTexture(const Device& device, ResourceHandle<Texture> imgPath);
    void UpdateLights(const Device& device);
    MaterialInfo GetMaterialInfo(const DrawEntry& drawEntry);
    void GetMaterial(const Device& device, MaterialTextures& mat, MaterialInfo& materialInfo, const DrawEntry& drawEntry);
    void BindMaterial(const Device& device, const MaterialTextures& mat);

    std::unordered_map<ResourceHandle<Model>, Model> model_cache {};
    std::unordered_map<ResourceHandle<Mesh>, Mesh> mesh_cache {};
    std::unordered_map<ResourceHandle<Texture>, std::shared_ptr<Texture>> tex_cache {};

    std::vector<DrawEntry> draw_queue {};
    std::shared_ptr<StorageBuffer> mStorageBuffers[NUM_S_BUFFERS];
    std::shared_ptr<UniformBuffer> mUniformBuffers[NUM_U_BUFFERS];
    std::shared_ptr<RenderTarget> mRenderTargets[NUM_U_BUFFERS];
    std::vector<DirLightInfo> m_directionalLights;
    std::vector<PointLightInfo> m_pointLights;
    LightInfo m_lightInfo {};
};
} // namespace KS
