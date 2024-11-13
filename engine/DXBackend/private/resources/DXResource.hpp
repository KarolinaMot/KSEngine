#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <commands/DXCommandList.hpp>
#include <memory>
#include <vector>

class DXResource
{
public:
    DXResource() = default;
    DXResource(ID3D12Device* device, const CD3DX12_HEAP_PROPERTIES& heapProperties, const CD3DX12_RESOURCE_DESC& descr, D3D12_CLEAR_VALUE* clearValue, const char* name, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
    DXResource(ID3D12Device* device, ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES resState);
    ~DXResource();

    ID3D12Resource* GetResource() { return mResource.Get(); }
    ID3D12Resource* GetUploadResource(int subresource) { return mUploadBuffers[subresource]->GetResource(); }

    CD3DX12_RESOURCE_DESC GetDesc() const { return mDesc; }
    D3D12_RESOURCE_STATES GetState() const { return mState; }
    size_t GetResourceSize() const { return mResourceSize; }

    void SetResource(ComPtr<ID3D12Resource> res) { mResource = res; }

    void ChangeState(D3D12_RESOURCE_STATES dstState);
    void CreateUploadBuffer(ID3D12Device* device, int dataSize, int currentSubresource);
    void Update(DXCommandList* list, D3D12_SUBRESOURCE_DATA data, D3D12_RESOURCE_STATES dstState, int currentSubresource, int totalSubresources);

private:
    bool mResizeBuffer = false;
    D3D12_RESOURCE_STATES mState = D3D12_RESOURCE_STATE_COMMON;
    CD3DX12_RESOURCE_DESC mDesc {};
    ComPtr<ID3D12Resource> mResource;
    size_t mResourceSize = 0;
    std::vector<std::unique_ptr<DXResource>> mUploadBuffers;
};
