#pragma once
#include <memory>
#include <string>
#include <unordered_map>

class DXCommandList;
class DXResource;

namespace KS
{
class MeshData;
class Device;
class Mesh;
class StorageBuffer;
class MeshPool
{
public:
    MeshPool(Device& device, DXCommandList& commandList, uint32_t maxMeshes, uint32_t maxVerticesPerMesh,
             uint32_t maxIndicesPerMesh);
    ~MeshPool();

    std::shared_ptr<Mesh> AllocateMesh(Device& device, DXCommandList& commandList, const MeshData& data, const char* name);
    void DeallocateMesh(std::shared_ptr<Mesh> mesh);
    void BindMesh(DXCommandList& commandList, const Mesh* mesh, int inputFlags);
    std::shared_ptr<KS::StorageBuffer> GetAttribute(const std::string& name) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<DXResource> CreateBLAS(const Device& device, DXCommandList& commandList);

    std::unordered_map<std::string, std::shared_ptr<StorageBuffer>> m_attributeBuffers;

    uint32_t m_vOffset = 0;
    uint32_t m_iOffset = 0;

    uint32_t m_maxVerticesPerMesh = 0;
    uint32_t m_maxIndicesPerMesh = 0;
    uint32_t m_maxMeshes = 0;
};
}
