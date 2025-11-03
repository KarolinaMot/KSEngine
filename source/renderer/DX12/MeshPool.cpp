#include "../MeshPool.hpp"
#include <resources/Mesh.hpp>
#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/Shader.hpp>


class KS::MeshPool::Impl
{
public:
    void FillGeometryDesc(const StorageBuffer& vb, UINT vbStride, UINT vbCount, UINT vbOffset, const StorageBuffer* ib,
                          UINT ibStride, UINT ibCount, UINT ibOffset, D3D12_RAYTRACING_GEOMETRY_FLAGS flags,
                          uint32_t meshIndex);
    D3D12_RAYTRACING_GEOMETRY_DESC m_geom[MAX_MESHES]{};
};

KS::MeshPool::MeshPool(Device& device, DXCommandList& commandList, uint32_t maxMeshes, uint32_t maxVerticesPerMesh,
                       uint32_t maxIndicesPerMesh)
{
    m_impl = std::make_unique<Impl>();
    m_maxMeshes = maxMeshes;
    m_maxVerticesPerMesh = maxVerticesPerMesh;
    m_maxIndicesPerMesh = maxIndicesPerMesh;

    for (int i = 0; i < 6; i++)
    {
        std::string name = MeshConstants::ATTRIBUTE_ARRAY[i];
        uint32_t elementCount = name == MeshConstants::ATTRIBUTE_INDICES_NAME ? m_maxIndicesPerMesh * m_maxMeshes
                                                                              : m_maxVerticesPerMesh * m_maxMeshes;
        auto buffer = std::make_shared<KS::StorageBuffer>(
            device, commandList, name + " storage buffer",
            MeshConstants::ATTRIBUTE_STRIDES.find(name)->second, elementCount, false);
        m_attributeBuffers.emplace(name, buffer);

        if (name == MeshConstants::ATTRIBUTE_NORMALS_NAME)
            buffer->AllocateAsReadOnly(device, NORMALS_SLOT);
        else if (name == MeshConstants::ATTRIBUTE_INDICES_NAME)
            buffer->AllocateAsReadOnly(device, INDICES_SLOT);
        else if (name == MeshConstants::ATTRIBUTE_POSITIONS_NAME)
            buffer->AllocateAsReadOnly(device, VPOS_SLOT);
        else if (name == MeshConstants::ATTRIBUTE_TEXTURE_UVS_NAME)
            buffer->AllocateAsReadOnly(device, UVS_SLOT);
        else if (name == MeshConstants::ATTRIBUTE_TANGENTS_NAME)
            buffer->AllocateAsReadOnly(device, TAN_SLOT);
    }
}

KS::MeshPool::~MeshPool() {}

std::shared_ptr<KS::Mesh> KS::MeshPool::AllocateMesh(Device& device, DXCommandList& commandList, const MeshData& data,
                                                 const char* meshName)
{
    if (m_meshCounter >= MAX_MESHES)
    {
        LOG(Log::Severity::WARN, "Maximum number of meshes {} has been reached. Command ignored.", MAX_MESHES);
        return nullptr;
    }

    for (const auto& [name, attributes] : data)
    {
        auto view = attributes.GetView<uint8_t>();

        auto* start = view.begin();
        size_t size = view.count();
        size_t stride = MeshConstants::ATTRIBUTE_STRIDES.find(name)->second;

        ASSERT(size % stride == 0 && "Attribute stride is not divisible by provided data");
    
        uint32_t elementOffset = name == MeshConstants::ATTRIBUTE_INDICES_NAME ? m_iOffset : m_vOffset;
        elementOffset *= stride;

        m_attributeBuffers[name]->Update(device, commandList, start, size/stride, elementOffset, true);
    }

    auto vView = data.GetAttribute(MeshConstants::ATTRIBUTE_POSITIONS_NAME)->GetView<glm::vec3>();
    size_t vSize = vView.count();
    size_t vStride = MeshConstants::ATTRIBUTE_STRIDES.find(MeshConstants::ATTRIBUTE_POSITIONS_NAME)->second;

    auto iView = data.GetAttribute(MeshConstants::ATTRIBUTE_INDICES_NAME)->GetView<uint32_t>();
    auto iSize = iView.count();
    auto iStride = MeshConstants::ATTRIBUTE_STRIDES.find(MeshConstants::ATTRIBUTE_INDICES_NAME)->second;

    auto blas = CreateBLAS(device, commandList, vSize, iSize);
    auto mesh = make_shared<Mesh>(meshName, m_vOffset, vSize, m_iOffset, iSize, m_meshCounter, blas);

    m_vOffset += vSize;
    m_iOffset += iSize;
    m_meshCounter++;

    return mesh;
    //return nullptr;
}

std::shared_ptr<KS::StorageBuffer> KS::MeshPool::GetAttribute(const std::string& name) const
{
    if (auto it = m_attributeBuffers.find(name); it != m_attributeBuffers.end())
    {
        return it->second;
    }
    return nullptr;
}

void KS::MeshPool::DeallocateMesh(std::shared_ptr<Mesh> mesh)
{

}

void KS::MeshPool::BindMesh(DXCommandList& commandList, const Mesh* mesh, int inputFlags)
{
    auto positions = GetAttribute(MeshConstants::ATTRIBUTE_POSITIONS_NAME);
    auto normals = GetAttribute(MeshConstants::ATTRIBUTE_NORMALS_NAME);
    auto uvs = GetAttribute(MeshConstants::ATTRIBUTE_TEXTURE_UVS_NAME);
    auto tangents = GetAttribute(MeshConstants::ATTRIBUTE_TANGENTS_NAME);
    auto indices = GetAttribute(MeshConstants::ATTRIBUTE_INDICES_NAME);

    if (inputFlags & Shader::MeshInputFlags::HAS_POSITIONS) 
        positions->BindAsVertexData(commandList, 0, mesh->GetVDataOffset(), mesh->GetVCount());
    if (inputFlags & Shader::MeshInputFlags::HAS_NORMALS)
        normals->BindAsVertexData(commandList, 1, mesh->GetVDataOffset(), mesh->GetVCount());
    if (inputFlags & Shader::MeshInputFlags::HAS_UVS)
        uvs->BindAsVertexData(commandList, 2, mesh->GetVDataOffset(), mesh->GetVCount());
    if (inputFlags & Shader::MeshInputFlags::HAS_TANGENTS)
        tangents->BindAsVertexData(commandList, 3, mesh->GetVDataOffset(), mesh->GetVCount());

    indices->BindAsIndexData(commandList, mesh->GetIDataOffset(), mesh->GetICount());
}

static UINT64 Align256(UINT64 v) { return (v + 255ull) & ~255ull; }
static DXGI_FORMAT IndexFormatFromStride(UINT stride)
{
    if (stride == 2) return DXGI_FORMAT_R16_UINT;
    if (stride == 4) return DXGI_FORMAT_R32_UINT;
    return DXGI_FORMAT_UNKNOWN;
}

std::shared_ptr<DXResource> KS::MeshPool::CreateBLAS(const Device& device, DXCommandList& commandList, uint32_t vCount, uint32_t iCount)
{
    auto vb = GetAttribute(MeshConstants::ATTRIBUTE_POSITIONS_NAME);
    if (!vb)
    {
        LOG(Log::Severity::WARN, "Can't build BLAS from mesh, because it has no vertices");
        return nullptr;
    }
    
    auto ib = GetAttribute(MeshConstants::ATTRIBUTE_INDICES_NAME);
    const auto vbStride = vb->GetBufferStride();
    
    uint32_t ibStride = 0;
    KS::StorageBuffer* ibRaw = nullptr;
    if (ib)
    {
        ibStride = ib->GetBufferStride();
        ibRaw = ib.get();
    
        if (IndexFormatFromStride(ibStride) == DXGI_FORMAT_UNKNOWN)
        {
            throw std::runtime_error("Mesh::BuildBLAS: index buffer must be 16 or 32-bit.");
            return nullptr;
        }
    }
    
    m_impl->FillGeometryDesc(*vb, vbStride, vCount, m_vOffset, ibRaw, ibStride, iCount, m_iOffset, D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE, m_meshCounter);
    
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = 1;
    inputs.pGeometryDescs = &m_impl->m_geom[m_meshCounter];
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO pre{};
    engineDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &pre);
    
    // Allocate / ensure BLAS
    const UINT64 need = Align256(pre.ResultDataMaxSizeInBytes);
    auto m_BLAS =
        std::make_shared<DXResource>(engineDevice, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                        CD3DX12_RESOURCE_DESC::Buffer(need, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
                                        nullptr, "Mesh BLAS", D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
    
    const UINT64 scratchSize = Align256(pre.ScratchDataSizeInBytes);
    std::shared_ptr<DXResource> scratch;
    scratch = std::make_shared<DXResource>(engineDevice, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                        CD3DX12_RESOURCE_DESC::Buffer(scratchSize,
                                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), nullptr, "Mesh BLAS scratch",
                                        D3D12_RESOURCE_STATE_COMMON);
    
    commandList.TransitionResource(*scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
    build.Inputs = inputs;
    build.DestAccelerationStructureData = m_BLAS->Get()->GetGPUVirtualAddress();
    build.ScratchAccelerationStructureData = scratch->Get()->GetGPUVirtualAddress();
    build.SourceAccelerationStructureData = 0;
    
    auto vbResource = reinterpret_cast<DXResource*>(vb->GetRawResource());
    auto ibResource = reinterpret_cast<DXResource*>(ib->GetRawResource());
    
    commandList.TransitionResource(*vbResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList.TransitionResource(*ibResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    
    commandList.GetCommandList()->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
    commandList.ResourceBarrier(*m_BLAS, D3D12_RESOURCE_BARRIER_TYPE_UAV);
    
    commandList.TransitionResource(*vbResource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    commandList.TransitionResource(*ibResource, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    
    commandList.TrackResource(m_BLAS->GetResource());
    commandList.TrackResource(scratch->GetResource());

    return m_BLAS;
}

void KS::MeshPool::Impl::FillGeometryDesc(const StorageBuffer& vb, UINT vbStride, UINT vbCount, UINT vbOffset,
                                          const StorageBuffer* ib, UINT ibStride, UINT ibCount, UINT ibOffset,
                                          D3D12_RAYTRACING_GEOMETRY_FLAGS flags, uint32_t meshIndex)
{
    auto& geom = m_geom[meshIndex];
    geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geom.Flags = flags;

    auto& t = geom.Triangles;
    t.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;  // position format (assumed)
    t.VertexBuffer.StartAddress = vb.GetGPUAddress(vbOffset, 0);
    t.VertexBuffer.StrideInBytes = vbStride;
    t.VertexCount = vbCount;

    if (ib && ibCount > 0)
    {
        t.IndexFormat = IndexFormatFromStride(ibStride);
        t.IndexBuffer = ib->GetGPUAddress(ibOffset, 0);
        t.IndexCount = ibCount;
    }
    else
    {
        t.IndexFormat = DXGI_FORMAT_UNKNOWN;
        t.IndexBuffer = 0;
        t.IndexCount = 0;
    }

    t.Transform3x4 = 0;  // no per-geometry pre-transform
}
