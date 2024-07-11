#include "DXStructuredBuffer.hpp"
#include "../../../tools/Log.hpp"
#include "DXDescHeap.hpp"
#include "DXResource.hpp"

DXStructuredBuffer::DXStructuredBuffer(const ComPtr<ID3D12Device5>& device, size_t dataSize, int numOfElements, const char* bufferDebugName, int frameNumber, D3D12_RESOURCE_FLAGS flags)
{
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    size_t sizeOfBuffer = dataSize * numOfElements;
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeOfBuffer, flags);
    m_resource = std::make_unique<DXResource>(device, heapProperties, resourceDesc, nullptr, bufferDebugName);
    m_resource->CreateUploadBuffer(device, sizeOfBuffer, 0);
    m_flags = flags;
    m_dataSize = dataSize;
    m_numberOfElements = numOfElements;
    m_name = bufferDebugName;
}

DXStructuredBuffer::~DXStructuredBuffer()
{
}

size_t DXStructuredBuffer::GetGPUVirtualAddress() const
{
    return m_resource->Get()->GetGPUVirtualAddress();
}

void DXStructuredBuffer::Resize(const ComPtr<ID3D12Device5>& device, int numberOfElements)
{
    if (m_numberOfElements == numberOfElements)
        return;

    m_numberOfElements = numberOfElements;
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    size_t sizeOfBuffer = m_dataSize * m_numberOfElements;
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeOfBuffer, m_flags);
    m_resource = std::make_unique<DXResource>(device, heapProperties, resourceDesc, nullptr, m_name);
    m_resource->CreateUploadBuffer(device, sizeOfBuffer, 0);
}

void DXStructuredBuffer::Update(ID3D12GraphicsCommandList4* commandList, const void* data)
{
    if (!data)
        return;

    D3D12_SUBRESOURCE_DATA subData;
    subData.pData = data;
    subData.RowPitch = m_dataSize;
    subData.SlicePitch = m_dataSize * m_numberOfElements;
    m_resource->Update(commandList, subData, D3D12_RESOURCE_STATE_GENERIC_READ, 0, 1);
}

void DXStructuredBuffer::AllocateAsUAV(DXDescHeap* descriptorHeap)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.StructureByteStride = m_dataSize;
    uavDesc.Buffer.NumElements = m_numberOfElements;

    m_UAV_handle = descriptorHeap->AllocateUAV(m_resource.get(), &uavDesc);
}

void DXStructuredBuffer::AllocateAsSRV(DXDescHeap* descriptorHeap)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.StructureByteStride = m_dataSize;
    srvDesc.Buffer.NumElements = m_numberOfElements;
    m_SRV_handle = descriptorHeap->AllocateResource(m_resource.get(), &srvDesc);
}

void DXStructuredBuffer::BindToGraphics(ID3D12GraphicsCommandList4* commandList, int rootSlot, bool readOnly, DXDescHeap* descriptorHeap)
{
    if (readOnly)
    {
        if (!m_SRV_handle.IsValid())
            AllocateAsSRV(descriptorHeap);

        descriptorHeap->BindToGraphics(commandList, rootSlot, m_SRV_handle);
    }
    else
    {
        if (!m_UAV_handle.IsValid())
            AllocateAsUAV(descriptorHeap);

        descriptorHeap->BindToGraphics(commandList, rootSlot, m_UAV_handle);
    }
}

void DXStructuredBuffer::BindToCompute(ID3D12GraphicsCommandList4* commandList, int rootSlot, bool readOnly, DXDescHeap* descriptorHeap)
{
    if (readOnly)
    {
        if (!m_SRV_handle.IsValid())
            AllocateAsSRV(descriptorHeap);

        descriptorHeap->BindToCompute(commandList, rootSlot, m_SRV_handle);
    }
    else
    {
        if (!m_UAV_handle.IsValid())
            AllocateAsUAV(descriptorHeap);

        descriptorHeap->BindToCompute(commandList, rootSlot, m_UAV_handle);
    }
}
