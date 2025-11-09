#pragma once
#include <fileio/ResourceHandle.hpp>
#include <renderer/InfoStructs.hpp>

class DXDescHeap;
class DXShaderTable;
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
class RenderTarget;
class DepthStencil;
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
    Scene();
    Scene(Device& device, std::string name, ScenesToChoose id);
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
    std::pair<std::shared_ptr<Skydome>, ResourceHandle<Texture>> GetSkydome() const { return m_skyDome; };
    std::pair<std::shared_ptr<Mesh>, ResourceHandle<Mesh>> GetSkydomeMesh() const { return m_skyDomeMesh; };
    std::shared_ptr<Texture> GetTexture(Device& device, DXCommandList* commandList, ResourceHandle<Texture> imgPath);
    TLAS* GetBVH() const { return m_BVH.get(); }
    DXDescHeap* GetResourceHeap() const;
    DXShaderTable* GetShaderTable(int index) const;
    size_t GetTexWithoutMipmapCount() const { return m_texWithoutMipmaps.size(); }
    std::shared_ptr<RenderTarget> GetRenderTarget(Subrenderers subrender) { return m_renderTargets[subrender]; }
    std::shared_ptr<DepthStencil> GetDepthStencil() { return m_deferredRendererDepthStencil; }

    KS::Texture* GetTextureForMipmapGen(int index) const
    {
        if (auto lock = m_texWithoutMipmaps[index].lock())
            return lock.get();
        else
            return nullptr;
    }

    void AddToMipmapQueue(std::weak_ptr<KS::Texture> tex) { m_texWithoutMipmaps.push_back(tex); }
    void ClearMipmapQueue() { m_texWithoutMipmaps.clear(); }
    std::string GetName() const { return m_name; }
    ScenesToChoose GetIndex() const { return m_identifyingIndex; }

    private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    const Model* GetModel(ResourceHandle<Model> model);
    const std::shared_ptr<Mesh> GetMesh(Device& device, DXCommandList* commandList, ResourceHandle<Mesh> mesh);
    void InitializeShaderTable();

    std::unordered_map<std::string, DrawEntry> draw_queue{};
    std::unordered_map<ResourceHandle<Model>, Model> model_cache{};
    std::unordered_map<ResourceHandle<Mesh>, std::shared_ptr<Mesh>> mesh_cache{};
    std::unordered_map<ResourceHandle<Texture>, std::shared_ptr<Texture>> tex_cache{};
    std::pair<std::shared_ptr<Mesh>, ResourceHandle<Mesh>> m_skyDomeMesh;
    std::shared_ptr<StorageBuffer> mStorageBuffers[NUM_SBUFFER];
    std::shared_ptr<UniformBuffer> mUniformBuffers[NUM_UBUFFER];
    std::shared_ptr<RenderTarget> m_renderTargets[NUM_SUBRENDER];
    std::shared_ptr<DepthStencil> m_deferredRendererDepthStencil;
    std::vector<DirLightInfo> m_directionalLights;
    std::vector<PointLightInfo> m_pointLights;
    std::unique_ptr<TLAS> m_BVH;
    std::string m_name;
    ScenesToChoose m_identifyingIndex;

    std::vector<ModelMat> m_modelMatrices = std::vector<ModelMat>(MAX_MESHES);
    std::vector<MaterialInfo> m_materialInstances = std::vector<MaterialInfo>(MAX_MESHES);
    int32_t m_modelCount = 0;
    LightInfo m_lightInfo{};
    FogInfo m_fogInfo{};
    std::pair<std::shared_ptr<Skydome>, ResourceHandle<Texture>> m_skyDome;
    std::vector<std::weak_ptr<KS::Texture>> m_texWithoutMipmaps;
};
}  // namespace KS
