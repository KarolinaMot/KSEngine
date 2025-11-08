#include <renderer/UploadArena.h>

#include <code_utility.hpp>
#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXHeapHandle.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <renderer/TLAS.hpp>

struct KS::TLAS::Impl
{
    std::shared_ptr<DXResource> m_scratch[FRAME_BUFFER_COUNT];
    std::shared_ptr<DXResource> m_desc[FRAME_BUFFER_COUNT];
    std::shared_ptr<DXResource> m_tlas[FRAME_BUFFER_COUNT];
    //std::shared_ptr<DXResource> m_defaultPerFrame[FRAME_BUFFER_COUNT];
    DXHeapHandle m_SRVHandle[FRAME_BUFFER_COUNT];
    bool m_updateStructure[FRAME_BUFFER_COUNT] = {true, true};     // add/remove/reorder → need rebuild
    bool m_updateTransforms[FRAME_BUFFER_COUNT] = {false, false};  // transforms changed → can refit
    D3D12_RAYTRACING_INSTANCE_DESC* m_instanceDescs[FRAME_BUFFER_COUNT];
};

KS::TLASInstance::TLASInstance() {}

KS::TLAS::TLAS() { m_Impl = std::make_unique<Impl>(); }

KS::TLAS::~TLAS() {}

void KS::TLAS::AddInstance(DrawEntry* entry, glm::mat4x4 modelMat)
{
    uint32_t instanceCount = static_cast<uint32_t>(m_instances.size());
    TLASInstance inst{};
    inst.m_entry = entry;
    inst.modelMat = glm::transpose(modelMat);
    inst.id = instanceCount;
    m_instances.push_back(inst);

    m_Impl->m_updateStructure[0] = true;
    m_Impl->m_updateStructure[1] = true;

    inst.m_entry->tlasHandle = inst.id;
}

void KS::TLAS::RemoveInstance(uint32_t instanceHandle)
{
    uint32_t instanceCount = static_cast<uint32_t>(m_instances.size());

    if (instanceHandle >= instanceCount) return;

    m_instances[instanceHandle] = std::move(m_instances.back());
    m_instances.pop_back();
    m_instances[instanceHandle].m_entry->tlasHandle = instanceHandle;
    m_Impl->m_updateStructure[0] = true;
    m_Impl->m_updateStructure[1] = true;
}

void KS::TLAS::UpdateTransform(uint32_t instanceHandle, glm::mat4x4 mat)
{
    uint32_t instanceCount = static_cast<uint32_t>(m_instances.size());

    if (instanceHandle >= instanceCount) return;
    m_instances[instanceHandle].modelMat = glm::transpose(mat);
    m_Impl->m_updateTransforms[0] = true;  // safe; worst case we rebuild
    m_Impl->m_updateTransforms[1] = true;  // safe; worst case we rebuild
}

void KS::TLAS::Clear()
{
    m_instances.clear();
    m_Impl->m_updateStructure[0] = true;
    m_Impl->m_updateStructure[1] = true;
}

void KS::TLAS::Bind(const Device& device, DXCommandList& commandList, const ShaderInputDesc& desc, uint32_t)
{
    auto frame = device.GetCPUFrameIndex();
    auto& tlas = m_Impl->m_tlas[frame];
    commandList.BindHeapResource(*tlas, m_Impl->m_SRVHandle[frame], desc.rootIndex);
}

size_t KS::TLAS::GetGPUAddress(int, int frameIndex) const
{
    auto& tlas = m_Impl->m_tlas[frameIndex];
    return tlas->GetResource()->GetGPUVirtualAddress();
}

static UINT64 Align256(UINT64 v) { return (v + 255ull) & ~255ull; }

void KS::TLAS::EnsureInstanceCapacity(const Device& device, uint32_t count)
{
    UINT newCap = m_capacity <= 1 ? 2 : m_capacity;
    auto frameIndex = device.GetCPUFrameIndex();

    if (count >= m_capacity)
    {
        while (newCap < count) newCap *= newCap;
    }

    const UINT64 bytes = Align256(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * UINT64(newCap));

    if (m_Impl->m_desc[frameIndex] && m_Impl->m_desc[frameIndex]->GetDesc().Width >= bytes) return;

    ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bytes);

    if (m_Impl->m_desc[frameIndex]) m_Impl->m_desc[frameIndex]->GetResource()->Unmap(0, nullptr);

    m_Impl->m_desc[frameIndex] = std::make_shared<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr, "TLAS desc",
                                                              D3D12_RESOURCE_STATE_COMMON);

    m_Impl->m_desc[frameIndex]->GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&m_Impl->m_instanceDescs[frameIndex]));
}

void KS::TLAS::WriteInstanceDescs(uint32_t frameIndex, bool onlyUpdate)
{
    const UINT count = static_cast<UINT>(m_instances.size());
    auto& instanceDescs = m_Impl->m_instanceDescs[frameIndex];

    if (!onlyUpdate)
    {
        std::memset(instanceDescs, 0, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * count);
    }

    for (UINT i = 0; i < m_instances.size(); ++i)
    {
        const auto& s = m_instances[i];
        if (s.m_entry->mesh)
        {
            D3D12_RAYTRACING_INSTANCE_DESC d{};
            std::memcpy(d.Transform, &s.modelMat[0], sizeof(float) * 12);
            d.InstanceID = s.id;
            d.InstanceMask = 0xFF;
            d.InstanceContributionToHitGroupIndex = s.hitgroupIndex;
            d.Flags = D3D12_RAYTRACING_INSTANCE_FLAGS::D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
            d.AccelerationStructure = s.m_entry->mesh->BLASAddress();
            instanceDescs[i] = d;
        }
    }

}

void KS::TLAS::EnsureTLAS(const Device& device, uint64_t neededBytes, bool forceRecreate)
{
    auto frameIndex = device.GetCPUFrameIndex();

    if (!forceRecreate && m_Impl->m_tlas[frameIndex] && m_Impl->m_tlas[frameIndex]->GetDesc().Width >= neededBytes) return;

    ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(Align256(neededBytes), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    m_Impl->m_tlas[frameIndex] =
        std::make_shared<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr, "TLAS buffer",
                                     D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
    m_tlasSize = m_Impl->m_tlas[frameIndex]->GetDesc().Width;
}

void KS::TLAS::EnsureScratch(const Device& device, uint64_t neededBytes)
{
    auto frameIndex = device.GetCPUFrameIndex();

    if (m_Impl->m_scratch[frameIndex] && m_Impl->m_scratch[frameIndex]->GetDesc().Width >= neededBytes) return;

    ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(Align256(neededBytes), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    m_Impl->m_scratch[frameIndex] = std::make_shared<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr,
                                                                 "TLAS scratch", D3D12_RESOURCE_STATE_COMMON);

    m_scratchSize = m_Impl->m_scratch[frameIndex]->GetDesc().Width;
}

void KS::TLAS::Build(const Device& device, DXCommandList& cmd)
{
    auto frameIndex = device.GetCPUFrameIndex();
    m_Impl->m_updateStructure[frameIndex] = true;
    if (!m_Impl->m_updateTransforms[frameIndex] && !m_Impl->m_updateStructure[frameIndex]) return;

    const UINT count = static_cast<UINT>(m_instances.size());
    ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());

    if (count == 0)
    {
        return;
    }
    const bool doUpdate = m_Impl->m_updateTransforms[frameIndex] && !m_Impl->m_updateStructure[frameIndex] &&
                          (m_Impl->m_tlas[frameIndex] != nullptr);

    EnsureInstanceCapacity(device, count);
    WriteInstanceDescs(frameIndex, doUpdate);

    // Prebuild info
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = count;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
                   D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                   (doUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE
                             : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE);
    inputs.InstanceDescs = m_Impl->m_desc[frameIndex]->GetResource()->GetGPUVirtualAddress();


    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO pre{};
    engineDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &pre);

    // Ensure TLAS and scratch
    EnsureTLAS(device, pre.ResultDataMaxSizeInBytes, !doUpdate);
    EnsureScratch(device, pre.ScratchDataSizeInBytes);

    cmd.TransitionResource(*m_Impl->m_scratch[frameIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // Build / Refit
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
    build.Inputs = inputs;
    build.DestAccelerationStructureData = m_Impl->m_tlas[frameIndex]->GetResource()->GetGPUVirtualAddress();
    build.ScratchAccelerationStructureData = m_Impl->m_scratch[frameIndex]->GetResource()->GetGPUVirtualAddress();
    build.SourceAccelerationStructureData = doUpdate ? m_Impl->m_tlas[frameIndex]->GetResource()->GetGPUVirtualAddress() : 0;

    cmd.GetCommandList()->BuildRaytracingAccelerationStructure(&build, 0, nullptr);

    cmd.ResourceBarrier(*m_Impl->m_tlas[frameIndex], D3D12_RESOURCE_BARRIER_TYPE_UAV);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV tlasSrv{};
    tlasSrv.Location = m_Impl->m_tlas[frameIndex]->GetResource()->GetGPUVirtualAddress();

    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.RaytracingAccelerationStructure = tlasSrv;

    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    m_Impl->m_SRVHandle[frameIndex] = heap->AllocateResource(m_Impl->m_tlas[frameIndex].get(), &desc, BVH_SLOT + frameIndex);

    if (m_Impl->m_updateTransforms[frameIndex]) m_Impl->m_updateTransforms[frameIndex] = false;
    if (m_Impl->m_updateStructure[frameIndex]) m_Impl->m_updateStructure[frameIndex] = false;

    cmd.TrackResource(m_Impl->m_tlas[frameIndex]->GetResource());
}

uint32_t KS::TLAS::GetSRVHandle(uint32_t frameIndex) const { return m_Impl->m_SRVHandle[frameIndex].GetIndex(); }
