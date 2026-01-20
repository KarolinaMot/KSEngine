#pragma once
#include <memory>
#include <vector>
#pragma warning(push, 0)
#include <glm/glm.hpp>
#pragma warning(pop)
#include <renderer/UploadArena.h>

#include <renderer/ShaderInput.hpp>

    class DXCommandList;
namespace KS
{
class Device;
class Mesh;
class Scene;

struct TLASInstance
{
    TLASInstance();

    glm::mat4x4 modelMat{};
    uint32_t id = 0;
    uint32_t hitgroupIndex = 0;

    DrawEntry* m_entry;
};

class TLAS : public ShaderInput
{
public:
    TLAS();
    ~TLAS();

    // Ownership of instances lives here:
    void AddInstance(DrawEntry* entry, glm::mat4x4 modelMat);
    void RemoveInstance(uint32_t instanceHandle);
    void UpdateTransform(uint32_t instanceHandle, glm::mat4x4 mat);
    void Clear();
    virtual void Bind(uint32_t frameIndex, void* resourceHeap, DXCommandList& commandList, const ShaderInputDesc& desc,
                      uint32_t offset = 0) override;
    virtual size_t GetGPUAddress(int elementIndex, int frameIndex) const override;

    void Build(const Device& device, const Scene& scene, DXCommandList& cmd);

    uint32_t GetSRVHandle(uint32_t frameIndex) const;

private:
    // CPU
    std::vector<TLASInstance> m_instances;

    UploadSlice m_slice;

    // GPU
    size_t m_tlasSize = 0;
    size_t m_scratchSize = 0;
    uint32_t m_capacity = 0;  // capacity of instance descs

    void EnsureInstanceCapacity(const Device& device,
                                uint32_t count);  // grow buffers (and set m_dirtyStructure)
    void WriteInstanceDescs(uint32_t frameIndex, bool onlyUpdate,
                            const Scene& scene);  // map & fill D3D12_RAYTRACING_INSTANCE_DESC[]
    void EnsureTLAS(const Device& device, uint64_t neededBytes, bool forceRecreate);
    void EnsureScratch(const Device& device, uint64_t neededBytes);

    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
}  // namespace KS