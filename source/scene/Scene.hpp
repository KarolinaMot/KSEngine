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

    uint32_t QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform, std::string name);
    void ApplyModelTransform(uint32_t meshId, const glm::mat4& transfrom);
    void QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius);
    void QueuePointLight(PointLightInfo info);
    void QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity);
    void QueueDirectionalLight(DirLightInfo info);
    void SetAmbientLight(glm::vec3 color, float intensity);
    void SetFogValues(Device& device, const FogInfo& newFogInfo);

    void Tick(Device& device);

    void GetFinalRTInfo(Device& device, DXDescHeap* heap, uint32_t frameIndex, uint64_t& gpuPtr, uint32_t& width,
                        uint32_t& height);
    RenderTarget* GetFinalRT() const { return m_finalRT.get(); };

    uint32_t GetModelCount() const { return m_modelCount; }
    MaterialInfo GetMaterialInfo(const Material& material) const;
    MeshSet GetMeshSet(Device& device, DXCommandList* commandList, int index);
    FogInfo GetFogValues() const { return m_fogInfo; }
    StorageBuffer* GetStorageBuffer(StorageBuffers buffer) const { return mStorageBuffers[buffer].get(); }
    UniformBuffer* GetUniformBuffer(UniformBuffers buffer) const { return mUniformBuffers[buffer].get(); }
    size_t GetDrawQueueSize() { return draw_queue.size(); }
    LightInfo GetLightInfo() { return m_lightInfo; }
    DrawEntry& GetDrawEntry(uint32_t index) { return draw_queue[index]; }
    void SetSkydome(Device& device, DXCommandList& commandList, ResourceHandle<Texture> skydomeTexture);
    std::pair<std::shared_ptr<Skydome>, ResourceHandle<Texture>> GetSkydome() const { return m_skyDome; };
    std::pair<std::shared_ptr<Mesh>, ResourceHandle<Mesh>> GetSkydomeMesh() const { return m_skyDomeMesh; };
    std::shared_ptr<Texture> GetTexture(Device& device, DXCommandList* commandList, ResourceHandle<Texture> imgPath,
                                        bool isSrgb = false);
    TLAS* GetBVH() const { return m_BVH.get(); }
    DXDescHeap* GetResourceHeap() const;
    void UpdateDirLights(int index, DirLightInfo info);
    void UpdatePointLights(int index, PointLightInfo info);
    DXShaderTable* GetShaderTable(int index) const;
    size_t GetTexWithoutMipmapCount() const { return m_texWithoutMipmaps.size(); }
    std::shared_ptr<RenderTarget> GetRenderTarget(Subrenderers subrender) { return m_renderTargets[subrender]; }
    std::shared_ptr<DepthStencil> GetDepthStencil() { return m_deferredRendererDepthStencil; }
    DirLightInfo GetDirLight(int index) const { return m_directionalLights[index]; }
    PointLightInfo GetPointLight(int index) const { return m_pointLights[index]; }
    glm::vec4& GetAmbientLight() { return m_lightInfo.mAmbientAndIntensity; }

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

    const Model* GetModel(Device& device, DXCommandList& commandList, ResourceHandle<Model> model);
    const std::shared_ptr<Mesh> GetMesh(ResourceHandle<Mesh> meshHandle);
    void InitializeShaderTable();

    std::vector<DrawEntry> draw_queue{};
    std::unordered_map<ResourceHandle<Model>, Model> model_cache{};
    std::unordered_map<ResourceHandle<Mesh>, std::shared_ptr<Mesh>> mesh_cache{};
    std::unordered_map<ResourceHandle<Texture>, std::shared_ptr<Texture>> tex_cache{};
    std::pair<std::shared_ptr<Mesh>, ResourceHandle<Mesh>> m_skyDomeMesh;
    std::shared_ptr<StorageBuffer> mStorageBuffers[NUM_SBUFFER];
    std::shared_ptr<UniformBuffer> mUniformBuffers[NUM_UBUFFER];
    std::shared_ptr<RenderTarget> m_renderTargets[NUM_SUBRENDER];
    std::shared_ptr<RenderTarget> m_finalRT;
    std::shared_ptr<DepthStencil> m_deferredRendererDepthStencil;
    std::vector<DirLightInfo> m_directionalLights;
    std::vector<PointLightInfo> m_pointLights;
    std::unique_ptr<TLAS> m_BVH;
    std::string m_name;
    ScenesToChoose m_identifyingIndex;

    std::vector<InstanceData> m_instanceData = std::vector<InstanceData>(MAX_MESHES);
    uint32_t m_modelCount = 0;
    LightInfo m_lightInfo{};
    FogInfo m_fogInfo{};
    std::pair<std::shared_ptr<Skydome>, ResourceHandle<Texture>> m_skyDome;
    std::vector<std::weak_ptr<KS::Texture>> m_texWithoutMipmaps;
    bool m_updateDirLights = false, m_updatePointLights = false;
};
}  // namespace KS
