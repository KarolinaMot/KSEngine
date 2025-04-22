#pragma once
#include <fileio/ResourceHandle.hpp>
#include <renderer/InfoStructs.hpp>

namespace KS
{
struct DrawEntry;
class Device;
class UniformBuffer;
class StorageBuffer;
class Texture;
class Model;
class Mesh;
class Image;

struct SBTInfo
{
    size_t GPUAddress = 0;
    uint32_t RayGenSectionSize = 0;
    uint32_t RayGenEntrySize = 0;
    uint32_t MissSectionSize = 0;
    uint32_t MissEntrySize = 0;
    uint32_t HitGroupSectionSize = 0;
    uint32_t HitGroupEntrySize = 0;
};

struct MeshSet
{
    const Mesh* mesh;
    std::shared_ptr<Texture> baseTex;
    std::shared_ptr<Texture> normalTex;
    std::shared_ptr<Texture> emissiveTex;
    std::shared_ptr<Texture> roughMetTex;
    std::shared_ptr<Texture> occlusionTex;
    int modelIndex;
};

class Scene
{
public:
    Scene(const Device& device);
    ~Scene();

    void QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform, std::string name);
    void ApplyModelTransform(Device& device, std::string name, const glm::mat4& transfrom);
    void QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius);
    void QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity);
    void SetAmbientLight(glm::vec3 color, float intensity);

    void Tick(Device& device);

    int32_t GetModelCount() const { return m_modelCount; }
    MaterialInfo GetMaterialInfo(const Material& material) const;
    MeshSet GetMeshSet(Device& device, int index);

    StorageBuffer* GetStorageBuffer(StorageBuffers buffer) { return mStorageBuffers[buffer].get(); }
    UniformBuffer* GetUniformBuffer(UniformBuffers buffer) { return mUniformBuffers[buffer].get(); }
    size_t GetDrawQueueSize() { return draw_queue.size(); }
    std::unordered_map<std::string, DrawEntry>& GetQueue() { return draw_queue; }

private:
    void CreateBottomLevelAS(const Device& device, const Mesh* mesh, int cpuFrame);
    void CreateBVHBotomLevelInstance(const Device& device, const DrawEntry& draw_entry, bool updateOnly, int entryIndex,
                                     int cpuFrame);
    void CreateTopLevelAS(const Device& device, bool updateOnly, int cpuFrame);

    const Mesh* GetMesh(const Device& device, ResourceHandle<Mesh> mesh);
    const Model* GetModel(ResourceHandle<Model> model);
    std::shared_ptr<Texture> GetTexture(Device& device, ResourceHandle<Texture> imgPath);

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::unordered_map<std::string, DrawEntry> draw_queue{};

    std::unordered_map<ResourceHandle<Model>, Model> model_cache{};
    std::unordered_map<ResourceHandle<Mesh>, Mesh> mesh_cache{};
    std::unordered_map<ResourceHandle<Texture>, std::shared_ptr<Texture>> tex_cache{};
    std::shared_ptr<StorageBuffer> mStorageBuffers[KS::NUM_SBUFFER];
    std::shared_ptr<UniformBuffer> mUniformBuffers[KS::NUM_UBUFFER];
    std::vector<DirLightInfo> m_directionalLights;
    std::vector<PointLightInfo> m_pointLights;

    ModelMat m_modelMatrices[200]{};
    MaterialInfo m_materialInstances[200]{};
    int32_t m_modelCount = 0;
    LightInfo m_lightInfo{};
    FogInfo m_fogInfo{};
};
}  // namespace KS
