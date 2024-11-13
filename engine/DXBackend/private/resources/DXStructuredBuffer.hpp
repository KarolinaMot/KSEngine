#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <commands/DXCommandList.hpp>
#include <descriptors/DXDescriptorHeap.hpp>
#include <memory>
#include <optional>
#include <resources/DXResource.hpp>

class DXStructuredBuffer
{
public:
    DXStructuredBuffer(ID3D12Device* device, size_t dataSize, int numOfElements, const char* bufferDebugName, int frameNumber, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    ~DXStructuredBuffer();

    size_t GetGPUVirtualAddress() const;

    void Resize(ID3D12Device* device, int numberOfElements);
    void Update(DXCommandList* commandList, const void* data);

    void Bind(DXCommandList* commandList, int rootSlot, bool readOnly, DXDescriptorHeap* descriptorHeap);

    const DXDescriptorHandle& GetUAVHandle() { return m_UAV_handle; }
    const DXDescriptorHandle& GetSRVHandle() { return m_SRV_handle; }
    const std::unique_ptr<DXResource>& GetResource() const { return m_resource; }

private:
    void AllocateAsUAV(ID3D12Device* device, DXDescriptorHeap* descriptorHeap);
    void AllocateAsSRV(ID3D12Device* device, DXDescriptorHeap* descriptorHeap);

    std::unique_ptr<DXResource> m_resource;
    DXDescriptorHandle m_UAV_handle;
    DXDescriptorHandle m_SRV_handle;

    size_t m_dataSize = 0;
    size_t m_numberOfElements = 0;
    D3D12_RESOURCE_FLAGS m_flags;
    const char* m_name;
};
