#pragma once
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <renderer/UploadArena.h>

class DXCommandList;
namespace KS
{
class Device;
class Mesh;

struct TLASInstance
{
    TLASInstance();

    glm::mat4x4 modelMat{};
    uint32_t id = 0;

    std::weak_ptr<Mesh> m_mesh;
};

class TLAS
{
public:

    TLAS();
    ~TLAS();

    // Ownership of instances lives here:
    void AddInstance(const Device& device, DXCommandList& cmd, std::shared_ptr<Mesh>& mesh, glm::mat4x4 modelMat);
    void RemoveInstance(uint32_t instanceHandle);
    void UpdateTransform(uint32_t instanceHandle, glm::mat4x4 mat);
    void Clear();

    void Build(const Device& device, DXCommandList& cmd);

private:
    // CPU
    std::vector<TLASInstance> m_instances;

    bool m_updateStructure = true;     // add/remove/reorder → need rebuild
    bool m_updateTransforms = false;  // transforms changed → can refit
    UploadSlice m_slice;

    // GPU
    size_t m_tlasSize = 0;
    size_t m_scratchSize = 0;
    size_t m_capacity = 0;  // capacity of instance descs

    void EnsureInstanceCapacity(const Device& device, DXCommandList& cmd,
                                size_t count);  // grow buffers (and set m_dirtyStructure)
    void WriteInstanceDescs();                // map & fill D3D12_RAYTRACING_INSTANCE_DESC[]
    void EnsureTLAS(const Device& device, uint64_t neededBytes, bool forceRecreate);
    void EnsureScratch(const Device& device, uint64_t neededBytes);

    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
}  // namespace KS
