#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXHeapHandle.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UploadArena.h>
#include <renderer/ShaderInputCollection.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <iostream>

class KS::StorageBuffer::Impl
{
public:
    std::unique_ptr<DXResource> m_resource;
    D3D12_RESOURCE_FLAGS m_flags{};
    DXHeapHandle m_UAV_handle{};
    DXHeapHandle m_SRV_handle{};
    UploadSlice m_slice{};
};

KS::StorageBuffer::StorageBuffer() { m_impl = new Impl(); }

KS::StorageBuffer::~StorageBuffer() { 
    delete m_impl;
}

void KS::StorageBuffer::CreateBuffer(const Device& device, DXCommandList& commandList, const std::string& name,
                                     uint32_t numOfElements)
{
    m_impl = new Impl();
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    if (m_read_write)
        m_impl->m_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    else
        m_impl->m_flags = D3D12_RESOURCE_FLAG_NONE;

    m_name = name.c_str();
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    size_t sizeOfBuffer = m_buffer_stride * m_num_elements;
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeOfBuffer, m_impl->m_flags);
    m_impl->m_resource = std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr, name.c_str());

    AllocateAsReadOnly(device);
    if (m_impl->m_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
        AllocateAsReadWrite(device);
   
   const UINT64 bytes = m_buffer_stride * numOfElements;
   auto upl = device.GetUploadArena();
   m_impl->m_slice = upl->Allocate(device, commandList, bytes, 255);
}

void KS::StorageBuffer::UploadDataBuffer(const Device& device, DXCommandList& commandList, const void* data,
                                         uint32_t numOfElements)
{
    if (!data)
    {
        LOG(Log::Severity::WARN,
            "Data could not be uploaded into buffer {} because the data passed was nullptr. Command ignored.", m_name);
        return;
    }
    D3D12_RESOURCE_STATES destState = D3D12_RESOURCE_STATE_GENERIC_READ;
    if (m_flags & StorageBufferFlags::INDEX_DATA_BUFFER)
        destState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    else if (m_flags & StorageBufferFlags::VERTEX_DATA_BUFFER) 
        destState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

    const UINT64 bytes = m_buffer_stride * numOfElements;
    auto upl = device.GetUploadArena();
    memcpy(m_impl->m_slice.m_cpu + m_impl->m_slice.m_head, data, size_t(bytes));

    auto uploadSource = reinterpret_cast<DXResource*>(upl->GetPageResource(m_impl->m_slice.m_pageID));
    commandList.TransitionResource(*m_impl->m_resource, D3D12_RESOURCE_STATE_COPY_DEST);

    // Copy from arena into DEFAULT heap buffer
    commandList.GetCommandList()->CopyBufferRegion(m_impl->m_resource->GetResource().Get(), 0,
                                                   uploadSource->GetResource().Get(), m_impl->m_slice.m_head,
                                                   bytes);

    commandList.TransitionResource(*m_impl->m_resource, destState);
}

void KS::StorageBuffer::Resize(const Device& device, DXCommandList& commandList, uint32_t newNumOfElements)
{
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    m_num_elements = newNumOfElements;

    if (m_num_elements == newNumOfElements)
    {
        LOG(Log::Severity::WARN, "Buffer {} was not resized, because it is already the size that was passed. Command ignored.",
            m_name);
        return;
    }

    m_num_elements = newNumOfElements;
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    size_t sizeOfBuffer = m_buffer_stride * m_num_elements;
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeOfBuffer, m_impl->m_flags);
    m_impl->m_resource = std::make_unique<DXResource>(engineDevice, heapProperties, resourceDesc, nullptr, m_name.c_str());
    const UINT64 bytes = UINT64(m_buffer_stride) * UINT64(m_num_elements);
    auto upl = device.GetUploadArena();
    m_impl->m_slice = upl->Allocate(device, commandList, bytes, 255);
}

void KS::StorageBuffer::Bind(const Device&, DXCommandList& commandList, const ShaderInputDesc& desc, uint32_t)
{
    if (desc.modifications == ShaderInputMod::READ_ONLY)
    {
        commandList.BindHeapResource(*m_impl->m_resource, m_impl->m_SRV_handle, desc.rootIndex);
    }
    else
    {
        commandList.BindHeapResource(*m_impl->m_resource, m_impl->m_UAV_handle, desc.rootIndex);
    }
}

void KS::StorageBuffer::BindAsVertexData(DXCommandList& commandList, uint32_t inputSlot,
                                         uint32_t elementOffset)
{
    commandList.BindVertexData(*m_impl->m_resource, m_buffer_stride, inputSlot, elementOffset);
}

void KS::StorageBuffer::BindAsIndexData(DXCommandList& commandList, uint32_t elementOffset)
{
    commandList.BindIndexData(*m_impl->m_resource, m_buffer_stride, elementOffset);
}

void KS::StorageBuffer::AllocateAsReadOnly(const Device& device, int slot)
{
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.StructureByteStride = static_cast<UINT>(m_buffer_stride);
    srvDesc.Buffer.NumElements = static_cast<UINT>(m_num_elements);

    if (slot == -1)
        m_impl->m_SRV_handle = heap->AllocateResource(m_impl->m_resource.get(), &srvDesc);
    else
        m_impl->m_SRV_handle = heap->AllocateResource(m_impl->m_resource.get(), &srvDesc, slot);
}

void KS::StorageBuffer::AllocateAsReadWrite(const Device& device, int slot)
{
    if (!m_read_write)
    {
        LOG(Log::Severity::WARN,
            "Storage buffer cannot be allocated as read write because it was not created with that flag. Command ignored.");
        return;
    }

    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());


    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.StructureByteStride = static_cast<UINT>(m_buffer_stride);
    uavDesc.Buffer.NumElements = static_cast<UINT>(m_num_elements);

    if (slot == -1)
        m_impl->m_UAV_handle = heap->AllocateUAV(m_impl->m_resource.get(), &uavDesc);
    else
        m_impl->m_UAV_handle = heap->AllocateUAV(m_impl->m_resource.get(), &uavDesc, slot);
}

size_t KS::StorageBuffer::GetGPUAddress(int elementIndex, int) const
{
    return m_impl->m_resource->GetResource()->GetGPUVirtualAddress() + (m_buffer_stride * elementIndex);
}

void* KS::StorageBuffer::GetRawRealResource() const { return m_impl->m_resource->Get(); }

void* KS::StorageBuffer::GetRawResource() const { return m_impl->m_resource.get(); }

int KS::StorageBuffer::GetAllocationIndex(bool readOnly)
{
    if (readOnly)
    {
        return m_impl->m_SRV_handle.GetIndex();
    }
    else
    {
        return m_impl->m_UAV_handle.GetIndex();
    }
}
