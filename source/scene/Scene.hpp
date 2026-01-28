#pragma once
#include <fileio/ResourceHandle.hpp>
#include <renderer/InfoStructs.hpp>

class DXDescHeap;
class DXShaderTable;
struct aiMesh;
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
    void SetCulledDrawCallIndicesCount(uint32_t count) { m_culledIndicesCount = count; }

    void Tick(Device& device);

    void GetFinalRTInfo(Device& device, DXDescHeap* heap, uint32_t frameIndex, uint64_t& gpuPtr, uint32_t& width,
                        uint32_t& height);
    RenderTarget* GetFinalRT() const { return m_finalRT.get(); };

    MaterialInfo GetMaterialInfo(const Material& material) const;
    FogInfo GetFogValues() const { return m_fogInfo; }
    StorageBuffer* GetStorageBuffer(StorageBuffers buffer) const { return mStorageBuffers[buffer].get(); }
    UniformBuffer* GetUniformBuffer(UniformBuffers buffer) const { return mUniformBuffers[buffer].get(); }
    uint32_t GetDrawQueueSize() const { return m_drawCallCount; }
    LightInfo GetLightInfo() { return m_lightInfo; }
    const DrawEntry& GetDrawEntry(uint32_t index) const { return draw_queue[index]; }
    CullingInfo GetCullingInfo() const { return m_cullInfo; }
    void SetSkydome(Device& device, DXCommandList& commandList, ResourceHandle<Texture> skydomeTexture);
    std::pair<std::shared_ptr<Skydome>, ResourceHandle<Texture>> GetSkydome() const { return m_skyDome; };
    std::pair<std::shared_ptr<Mesh>, ResourceHandle<Mesh>> GetSkydomeMesh() const { return m_skyDomeMesh; };
    std::shared_ptr<Texture> GetTexture(Device& device, DXCommandList* commandList, ResourceHandle<Texture> imgPath,
                                        bool isSrgb = false);
    void SetUpdateSuperSampler() { m_updateSupersampled = 2; }
    void SetUpdateCamera()
    {
        m_cameraUpdated = true;
        SetUpdateSuperSampler();
    }
    TLAS* GetBVH() const { return m_BVH.get(); }
    DXDescHeap* GetResourceHeap() const;
    void UpdateDirLights(int index, DirLightInfo info);
    void UpdatePointLights(int index, PointLightInfo info);
    size_t GetTexWithoutMipmapCount() const { return m_texWithoutMipmaps.size(); }
    std::shared_ptr<RenderTarget> GetRenderTarget(Subrenderers subrender) { return m_renderTargets[subrender]; }
    std::shared_ptr<DepthStencil> GetDepthStencil() { return m_deferredRendererDepthStencil; }
    DirLightInfo GetDirLight(int index) const { return m_directionalLights[index]; }
    PointLightInfo GetPointLight(int index) const { return m_pointLights[index]; }
    glm::vec4& GetAmbientLight() { return m_lightInfo.mAmbientAndIntensity; }
    uint32_t GetCulledIndicesCount() const { return m_culledIndicesCount; }
    void CreateBatches(Device& device, DXCommandList& list);
    const std::vector<BatchRange>& GetBatches() const { return batch_queue; }
    void SetShadowSample(uint32_t value) { m_pathTracingInfo.shadowSampleNumber = value; }
    void SetGISample(uint32_t value) { m_pathTracingInfo.GIsampleNumber = value; }
    Texture* GetDIReservoir(uint32_t index) const { return m_DIReservoirs[index].get(); }

    KS::Texture* GetTextureForMipmapGen(int index) const
    {
        if (auto lock = m_texWithoutMipmaps[index].lock())
            return lock.get();
        else
            return nullptr;
    }

    const std::shared_ptr<Mesh> GetMesh(ResourceHandle<Mesh> meshHandle) const;
    void AddToMipmapQueue(std::weak_ptr<KS::Texture> tex) { m_texWithoutMipmaps.push_back(tex); }
    void ClearMipmapQueue() { m_texWithoutMipmaps.clear(); }
    uint32_t GetShadowSample() const { return m_pathTracingInfo.shadowSampleNumber; }
    uint32_t GetGISample() const { return m_pathTracingInfo.GIsampleNumber; }
    std::string GetName() const { return m_name; }
    ScenesToChoose GetIndex() const { return m_identifyingIndex; }
    std::vector<uint32_t>& GetDrawIndices() { return m_drawIndices; }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    const Model* GetModel(Device& device, DXCommandList& commandList, ResourceHandle<Model> model);

    std::vector<DrawEntry> draw_queue{};
    std::vector<BatchRange> batch_queue{};
    std::vector<MaterialInfo> material_cache{};
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
    std::shared_ptr<Texture> m_DIReservoirs[4];

    std::vector<InstanceData> m_instanceData = std::vector<InstanceData>(MAX_MESHES);
    std::vector<BoundingBox> m_boundingBoxes = std::vector<BoundingBox>(MAX_MESHES);
    std::vector<uint32_t> m_drawIndices = std::vector<uint32_t>(MAX_MESHES);
    uint32_t m_culledIndicesCount = 0;
    uint32_t m_drawCallCount = 0;
    uint32_t m_materialCounter = 0;
    LightInfo m_lightInfo{};
    FogInfo m_fogInfo{};
    CullingInfo m_cullInfo{};
    PathTracingData m_pathTracingInfo{};
    std::pair<std::shared_ptr<Skydome>, ResourceHandle<Texture>> m_skyDome;
    std::vector<std::weak_ptr<KS::Texture>> m_texWithoutMipmaps;
    bool m_updateDirLights = false, m_updatePointLights = false;
    bool m_updateScene = false;
    uint32_t m_updateSupersampled = 2;
    bool m_cameraUpdated=false;
};
}  // namespace KS
