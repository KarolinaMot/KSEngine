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
class Skydome;
class Model;
class CommandList;
class TLAS;
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
    glm::mat4x4 transform;
};

class Scene
{
public:
    Scene(Device& device);
    ~Scene();

    void QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform, std::string name);
    void ApplyModelTransform(std::string name, const glm::mat4& transfrom);
    void QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius);
    void QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity);
    void SetAmbientLight(glm::vec3 color, float intensity);
    void SetFogValues(Device& device, const FogInfo& newFogInfo);

    void Tick(Device& device);

    int32_t GetModelCount() const { return m_modelCount; }
    MaterialInfo GetMaterialInfo(const Material& material) const;
    MeshSet GetMeshSet(Device& device, DXCommandList* commandList, int index);
    FogInfo GetFogValues() const { return m_fogInfo; }
    StorageBuffer* GetStorageBuffer(StorageBuffers buffer) const { return mStorageBuffers[buffer].get(); }
    UniformBuffer* GetUniformBuffer(UniformBuffers buffer) const { return mUniformBuffers[buffer].get(); }
    size_t GetDrawQueueSize() { return draw_queue.size(); }
    LightInfo GetLightInfo() { return m_lightInfo; }
    std::unordered_map<std::string, DrawEntry>& GetQueue() { return draw_queue; }
    void SetSkydome(Device& device, DXCommandList& commandList, ResourceHandle<Texture> skydomeTexture);
    std::pair<std::shared_ptr<Skydome>, ResourceHandle<Texture>> GetSkydome() const
    {
        return m_skyDome;
    };
    std::shared_ptr<Texture> GetTexture(Device& device, DXCommandList* commandList, ResourceHandle<Texture> imgPath);

    
private:
    const Model* GetModel(ResourceHandle<Model> model);
    const std::shared_ptr<Mesh> GetMesh(Device& device, DXCommandList* commandList, ResourceHandle<Mesh> mesh);

    std::unordered_map<std::string, DrawEntry> draw_queue{};
    std::unordered_map<ResourceHandle<Model>, Model> model_cache{};
    std::unordered_map<ResourceHandle<Mesh>, std::shared_ptr<Mesh>> mesh_cache{};
    std::unordered_map<ResourceHandle<Texture>, std::shared_ptr<Texture>> tex_cache{};
    std::shared_ptr<StorageBuffer> mStorageBuffers[KS::NUM_SBUFFER];
    std::shared_ptr<UniformBuffer> mUniformBuffers[KS::NUM_UBUFFER];
    std::vector<DirLightInfo> m_directionalLights;
    std::vector<PointLightInfo> m_pointLights;
    std::unique_ptr<TLAS> m_BVH;

    std::vector<ModelMat> m_modelMatrices = std::vector<ModelMat>(MAX_MESHES);
    std::vector<MaterialInfo> m_materialInstances = std::vector<MaterialInfo>(MAX_MESHES);
    int32_t m_modelCount = 0;
    LightInfo m_lightInfo{};
    FogInfo m_fogInfo{};
    std::pair<std::shared_ptr<Skydome>, ResourceHandle<Texture>> m_skyDome;

    uint32_t m_vDataOffset = 0;
    uint32_t m_uvDataOffset = 0;
    uint32_t m_indexDataOffset = 0;
};
}  // namespace KS
