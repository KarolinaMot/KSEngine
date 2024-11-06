#pragma once

#include "SubRenderer.hpp"
#include <fileio/ResourceHandle.hpp>

#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <resources/Model.hpp>

#include "InfoStructs.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>


class StorageBuffer;
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

    void QueueModel(const Device& device, ResourceHandle<Model> model, const glm::mat4& transform);
    void Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<DepthStencil> depthStencil, Texture** previoiusPassResults = nullptr, int numTextures = 0) override;

private:
    const Mesh* GetMesh(const Device& device, ResourceHandle<Mesh> mesh);
    const Model* GetModel(ResourceHandle<Model> model);
    std::shared_ptr<Texture> GetTexture(const Device& device, ResourceHandle<Texture> imgPath);
    void UpdateLights(const Device& device);
    MaterialInfo GetMaterialInfo(const Material& material);

    std::unordered_map<ResourceHandle<Model>, Model> model_cache {};
    std::unordered_map<ResourceHandle<Mesh>, Mesh> mesh_cache {};
    std::unordered_map<ResourceHandle<Texture>, std::shared_ptr<Texture>> tex_cache {};
    std::unique_ptr<StorageBuffer> m_modelMatsBuffer;
    std::unique_ptr<StorageBuffer> m_materialInfoBuffer;
    std::unique_ptr<UniformBuffer> m_modelIndexBuffer;
    bool m_raytraced = false;
    std::vector<DrawEntry> draw_queue {};

    ModelMat m_modelMatrices[200] {};
    MaterialInfo m_materialInstances[200] {};
    int32_t m_modelCount = 0;
};