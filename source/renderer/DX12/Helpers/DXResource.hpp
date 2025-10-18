#pragma once
#include <memory>
#include <vector>

#include "DXIncludes.hpp"

class DXCommandList;
class DXResource
{
public:
    DXResource(){};
    DXResource(const ComPtr<ID3D12Device5>& device, const CD3DX12_HEAP_PROPERTIES& heapProperties,
               const CD3DX12_RESOURCE_DESC& descr, D3D12_CLEAR_VALUE* clearValue, const char* name,
               D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
    DXResource(const ComPtr<ID3D12Device5>& device, ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES resState);
    ~DXResource();

    ComPtr<ID3D12Resource> GetResource() const { return mResource; }
    ID3D12Resource* Get() const { return mResource.Get(); }
    void SetResource(ComPtr<ID3D12Resource> res) { mResource = res; }

    CD3DX12_RESOURCE_DESC GetDesc() const { return mDesc; }
    D3D12_RESOURCE_STATES GetState() const { return mState; }
    size_t GetResourceSize() const { return mResourceSize; }

    void ChangeState(D3D12_RESOURCE_STATES dstState);
    bool mResizeBuffer = false;

private:
    D3D12_RESOURCE_STATES mState = D3D12_RESOURCE_STATE_COMMON;
    CD3DX12_RESOURCE_DESC mDesc{};
    ComPtr<ID3D12Resource> mResource;
    size_t mResourceSize = 0;
    // std::vector<std::unique_ptr<DXResource>> mUploadBuffers;
};
