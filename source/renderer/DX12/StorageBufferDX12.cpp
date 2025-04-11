#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXHeapHandle.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/ShaderInputCollection.hpp>

class KS::StorageBuffer::Impl
{
public:
    std::unique_ptr<DXResource> m_resource;
    D3D12_RESOURCE_FLAGS m_flags;
    DXHeapHandle m_UAV_handle;
    DXHeapHandle m_SRV_handle;
};

KS::StorageBuffer::StorageBuffer() { m_impl = new Impl(); }

KS::StorageBuffer::~StorageBuffer() { delete m_impl; }

void KS::StorageBuffer::CreateBuffer(const Device& device, const std::string& name, size_t dataSize, int numOfElements)
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
    m_impl->m_resource->CreateUploadBuffer(engineDevice, sizeOfBuffer, 0);
}

void KS::StorageBuffer::UploadDataBuffer(const Device& device, const void* data, int numOfElements)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    if (!data)
    {
        LOG(Log::Severity::WARN,
            "Data could not be uploaded into buffer {} because the data passed was nullptr. Command ignored.", m_name);
        return;
    }

    D3D12_SUBRESOURCE_DATA subData;
    subData.pData = data;
    subData.RowPitch = m_buffer_stride;
    subData.SlicePitch = m_buffer_stride * m_num_elements;
    m_impl->m_resource->Update(commandList, subData, D3D12_RESOURCE_STATE_GENERIC_READ, 0, 1);
}

void KS::StorageBuffer::Resize(const Device& device, int newNumOfElements)
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
    m_impl->m_resource->CreateUploadBuffer(engineDevice, sizeOfBuffer, 0);
}

void KS::StorageBuffer::Bind(Device& device, const ShaderInputDesc& desc, uint32_t offsetIndex)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    if (desc.modifications == ShaderInputMod::READ_ONLY)
    {
        if (!m_impl->m_SRV_handle.IsValid()) AllocateAsReadOnly(device);

        commandList->BindHeapResource(m_impl->m_resource, m_impl->m_SRV_handle, desc.rootIndex);
    }
    else
    {
        if (!m_impl->m_UAV_handle.IsValid()) AllocateAsReadWrite(device);

        commandList->BindHeapResource(m_impl->m_resource, m_impl->m_UAV_handle, desc.rootIndex);
    }
}

void KS::StorageBuffer::BindAsVertexData(const Device& device, uint32_t inputSlot, uint32_t elementOffset)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->BindVertexData(m_impl->m_resource, m_buffer_stride, inputSlot, elementOffset);
}

void KS::StorageBuffer::BindAsIndexData(const Device& device, uint32_t elementOffset)
{
    auto commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    commandList->BindIndexData(m_impl->m_resource, m_buffer_stride, elementOffset);
}

void KS::StorageBuffer::AllocateAsReadOnly(Device& device, int slot)
{
    auto heap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.StructureByteStride = m_buffer_stride;
    srvDesc.Buffer.NumElements = m_num_elements;

    if (slot == -1)
        m_impl->m_SRV_handle = heap->AllocateResource(m_impl->m_resource.get(), &srvDesc);
    else
        m_impl->m_SRV_handle = heap->AllocateResource(m_impl->m_resource.get(), &srvDesc, slot);
}

void KS::StorageBuffer::AllocateAsReadWrite(Device& device, int slot)
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
    uavDesc.Buffer.StructureByteStride = m_buffer_stride;
    uavDesc.Buffer.NumElements = m_num_elements;

    if (slot == -1)
        m_impl->m_UAV_handle = heap->AllocateUAV(m_impl->m_resource.get(), &uavDesc);
    else
        m_impl->m_UAV_handle = heap->AllocateUAV(m_impl->m_resource.get(), &uavDesc, slot);
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
