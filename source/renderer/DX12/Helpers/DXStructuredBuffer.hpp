#pragma once
#include "DXHeapHandle.hpp"
#include "DXIncludes.hpp"
#include <memory>
#include <optional>

class DXResource;
class DXDescHeap;
class DXCommandList;
class DXStructuredBuffer
{
public:
    DXStructuredBuffer(const ComPtr<ID3D12Device5>& device, size_t dataSize, int numOfElements, const char* bufferDebugName, int frameNumber, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    ~DXStructuredBuffer();

    size_t GetGPUVirtualAddress() const;

    void Resize(const ComPtr<ID3D12Device5>& device, int numberOfElements);
    void Update(DXCommandList* commandList, const void* data);

    void Bind(DXCommandList* commandList, int rootSlot, bool readOnly, DXDescHeap* descriptorHeap);

    const DXHeapHandle& GetUAVHandle() { return m_UAV_handle; }
    const DXHeapHandle& GetSRVHandle() { return m_SRV_handle; }
    const std::unique_ptr<DXResource>& GetResource() const { return m_resource; }

private:
    void AllocateAsUAV(DXDescHeap* descriptorHeap);
    void AllocateAsSRV(DXDescHeap* descriptorHeap);

    std::unique_ptr<DXResource> m_resource;
    DXHeapHandle m_UAV_handle;
    DXHeapHandle m_SRV_handle;

    size_t m_dataSize = 0;
    size_t m_numberOfElements = 0;
    D3D12_RESOURCE_FLAGS m_flags;
    const char* m_name;
};
