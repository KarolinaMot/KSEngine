#include "Mesh.hpp"
#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/InfoStructs.hpp>

void KS::MeshData::AddAttribute(const std::string& name, ByteBuffer&& data)
{
    attribute_data.emplace(name, std::move(data));
}

const KS::ByteBuffer* KS::MeshData::GetAttribute(const std::string& name) const
{
    if (auto it = attribute_data.find(name); it != attribute_data.end())
    {
        return &it->second;
    }
    return nullptr;
}

class KS::Mesh::Impl
{
public:
    std::shared_ptr<DXResource> m_BLAS;
    D3D12_RAYTRACING_GEOMETRY_DESC m_geom{};
    size_t m_BLASSize;

    void FillGeometryDesc(const StorageBuffer& vb, UINT vbStride, UINT vbCount, const StorageBuffer* ib, UINT ibStride,
                          UINT ibCount, D3D12_RAYTRACING_GEOMETRY_FLAGS flags);
};

static DXGI_FORMAT IndexFormatFromStride(UINT stride)
{
    if (stride == 2) return DXGI_FORMAT_R16_UINT;
    if (stride == 4) return DXGI_FORMAT_R32_UINT;
    return DXGI_FORMAT_UNKNOWN;
}

static UINT64 Align256(UINT64 v) { return (v + 255ull) & ~255ull; }

KS::Mesh::Mesh(Device& device, void* resourceHeap, DXCommandList& commandList, const MeshData& data, const char* name,
               uint32_t meshIndex)
{
    m_impl = std::make_unique<Impl>();

    for (const auto& [attrName, attributes] : data)
    {
        auto view = attributes.GetView<uint8_t>();

        auto* start = view.begin();
        size_t size = view.count();
        size_t stride = MeshConstants::ATTRIBUTE_STRIDES.find(attrName)->second;

        ASSERT(size % stride == 0 && "Attribute stride is not divisible by provided data");

        auto buffer =
            std::make_shared<KS::StorageBuffer>(device, resourceHeap, commandList, attrName, start, static_cast<uint32_t>(stride),
                                                          static_cast<uint32_t>(size / stride), false);

        if (attrName == MeshConstants::ATTRIBUTE_NORMALS_NAME)
        {
            buffer->AllocateAsReadOnly(resourceHeap, NORMALS_SLOT + meshIndex);
            m_attributes.normals = buffer.get();
        }
        else if (attrName == MeshConstants::ATTRIBUTE_INDICES_NAME)
        {
            buffer->AllocateAsReadOnly(resourceHeap, INDICES_SLOT + meshIndex);
            m_attributes.indices = buffer.get();
        }
        else if (attrName == MeshConstants::ATTRIBUTE_POSITIONS_NAME)
        {
            buffer->AllocateAsReadOnly(resourceHeap, VPOS_SLOT + meshIndex);
            m_attributes.positions = buffer.get();
        }
        else if (attrName == MeshConstants::ATTRIBUTE_TEXTURE_UVS_NAME)
        {
            buffer->AllocateAsReadOnly(resourceHeap, UVS_SLOT + meshIndex);
            m_attributes.uvs = buffer.get();
        }
        else if (attrName == MeshConstants::ATTRIBUTE_TANGENTS_NAME)
        {
            buffer->AllocateAsReadOnly(resourceHeap, TAN_SLOT + meshIndex);
            m_attributes.tangents = buffer.get();
        }

        m_data.emplace(attrName, buffer);
    }

    m_name = name;
    m_meshIndex = meshIndex;
    BuildBLAS(device, commandList);

    if (!m_impl->m_BLAS)
    {
        throw std::runtime_error("Mesh::BLASAddress: there is no blas.");
    }
}

KS::Mesh::~Mesh() {}

KS::Mesh::Mesh(Mesh&& other) noexcept
{
    m_impl = std::move(other.m_impl);
    other.m_impl = nullptr;
    m_data = std::move(other.m_data);
    m_meshIndex = other.m_meshIndex;
}

KS::Mesh& KS::Mesh::operator=(Mesh&& other) noexcept
{
    m_impl = std::move(other.m_impl);
    other.m_impl = nullptr;
    m_data = std::move(other.m_data);
    m_meshIndex = other.m_meshIndex;
    return *this;
}

std::shared_ptr<KS::StorageBuffer> KS::Mesh::GetAttribute(const std::string& name) const
{
    if (auto it = m_data.find(name); it != m_data.end())
    {
        return it->second;
    }
    return nullptr;
}

size_t KS::Mesh::BLASAddress() const
{
    if (!m_impl->m_BLAS)
    {
        throw std::runtime_error("Mesh::BLASAddress: there is no blas.");
    }

    return m_impl->m_BLAS->Get()->GetGPUVirtualAddress();
}

std::shared_ptr<DXResource> KS::Mesh::GetBLASRes() const { return m_impl->m_BLAS; }

void KS::Mesh::BuildBLAS(const Device& device, DXCommandList& cmd)
{
    auto vb = GetAttribute(MeshConstants::ATTRIBUTE_POSITIONS_NAME);
    if (!vb)
    {
        LOG(Log::Severity::WARN, "Can't build BLAS from mesh, because it has no vertices");
        return;
    }

    auto ib = GetAttribute(MeshConstants::ATTRIBUTE_INDICES_NAME);
    const auto vbStride = vb->GetBufferStride();
    const auto vbCount = vb->GetElementCount();

    uint32_t ibStride = 0, ibCount = 0;
    KS::StorageBuffer* ibRaw = nullptr;
    if (ib)
    {
        ibStride = ib->GetBufferStride();
        ibCount = ib->GetElementCount();
        ibRaw = ib.get();

        if (IndexFormatFromStride(ibStride) == DXGI_FORMAT_UNKNOWN)
            throw std::runtime_error("Mesh::BuildBLAS: index buffer must be 16 or 32-bit.");
    }

    m_impl->FillGeometryDesc(*vb, vbStride, vbCount, ibRaw, ibStride, ibCount, D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE);

    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = 1;
    inputs.pGeometryDescs = &m_impl->m_geom;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO pre{};
    engineDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &pre);

    // Allocate / ensure BLAS
    const UINT64 need = Align256(pre.ResultDataMaxSizeInBytes);
    m_impl->m_BLAS =
        std::make_shared<DXResource>(engineDevice, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                     CD3DX12_RESOURCE_DESC::Buffer(need, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), nullptr,
                                     "Mesh BLAS", D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    const UINT64 scratchSize = Align256(pre.ScratchDataSizeInBytes);
    std::shared_ptr<DXResource> scratch;
    scratch =
        std::make_shared<DXResource>(engineDevice, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                     CD3DX12_RESOURCE_DESC::Buffer(scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
                                     nullptr, "Mesh BLAS scratch", D3D12_RESOURCE_STATE_COMMON);

    cmd.TransitionResource(*scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    m_impl->m_BLASSize = need;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
    build.Inputs = inputs;
    build.DestAccelerationStructureData = m_impl->m_BLAS->Get()->GetGPUVirtualAddress();
    build.ScratchAccelerationStructureData = scratch->Get()->GetGPUVirtualAddress();
    build.SourceAccelerationStructureData = 0;

    auto vbResource = reinterpret_cast<DXResource*>(vb->GetRawResource());
    auto ibResource = reinterpret_cast<DXResource*>(ib->GetRawResource());

    cmd.TransitionResource(*vbResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    cmd.TransitionResource(*ibResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    cmd.GetCommandList()->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
    cmd.ResourceBarrier(*m_impl->m_BLAS, D3D12_RESOURCE_BARRIER_TYPE_UAV);

    cmd.TransitionResource(*vbResource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    cmd.TransitionResource(*ibResource, D3D12_RESOURCE_STATE_INDEX_BUFFER);

    cmd.TrackResource(m_impl->m_BLAS->GetResource());
}

void KS::Mesh::Impl::FillGeometryDesc(const StorageBuffer& vb, UINT vbStride, UINT vbCount, const StorageBuffer* ib,
                                      UINT ibStride, UINT ibCount, D3D12_RAYTRACING_GEOMETRY_FLAGS flags)
{
    m_geom = {};
    m_geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    m_geom.Flags = flags;

    auto& t = m_geom.Triangles;
    t.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;  // position format (assumed)
    t.VertexBuffer.StartAddress = vb.GetGPUAddress(0, 0);
    t.VertexBuffer.StrideInBytes = vbStride;
    t.VertexCount = vbCount;

    if (ib && ibCount > 0)
    {
        t.IndexFormat = IndexFormatFromStride(ibStride);
        t.IndexBuffer = ib->GetGPUAddress(0, 0);
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