#include "DXResource.hpp"
#include <cassert>
#include <commands/DXCommandList.hpp>
#include <iostream>

DXResource::DXResource(const ComPtr<ID3D12Device5>& device, const CD3DX12_HEAP_PROPERTIES& heapProperties, const CD3DX12_RESOURCE_DESC& descr, D3D12_CLEAR_VALUE* clearValue, const char* name, D3D12_RESOURCE_STATES state)
{
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &descr,
        state,
        clearValue,
        IID_PPV_ARGS(&mResource));
    mState = state;
    mDesc = descr;

    if (FAILED(hr))
        assert(false && "Resource creation failed");

    device->GetCopyableFootprints(&descr, 0, 1, 0, nullptr, nullptr, nullptr, &mResourceSize);

    wchar_t wString[4096];
    MultiByteToWideChar(CP_ACP, 0, name, -1, wString, 4096);
    mResource->SetName(wString);
}

DXResource::DXResource(const ComPtr<ID3D12Device5>& device, ComPtr<ID3D12Resource> res, D3D12_RESOURCE_STATES resState)
{
    mResource = res;
    mState = resState;
    auto description = mResource->GetDesc();
    device->GetCopyableFootprints(&description, 0, 1, 0, nullptr, nullptr, nullptr, &mResourceSize);
}

DXResource::~DXResource()
{
    for (size_t i = 0; i < mUploadBuffers.size(); i++)
    {
        mUploadBuffers[i] = nullptr;
    }
}

void DXResource::ChangeState(D3D12_RESOURCE_STATES dstState)
{
    if (dstState == mState)
        return;

    mState = dstState;
}

void DXResource::CreateUploadBuffer(const ComPtr<ID3D12Device5>& device, int dataSize, int currentSubresource)
{
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
    if (mUploadBuffers.size() <= currentSubresource)
        mUploadBuffers.resize(currentSubresource + 1);

    mUploadBuffers[currentSubresource] = std::make_unique<DXResource>(device, heapProperties, resourceDesc, nullptr, "Upload buffer", D3D12_RESOURCE_STATE_GENERIC_READ);
}

void DXResource::Update(DXCommandList* list, D3D12_SUBRESOURCE_DATA data, D3D12_RESOURCE_STATES dstState, int currentSubresource, int totalSubresources)
{
    list->ResourceBarrier(mResource, mState, D3D12_RESOURCE_STATE_COPY_DEST);
    UpdateSubresources(list->GetCommandList().Get(), mResource.Get(), mUploadBuffers[currentSubresource]->mResource.Get(), 0, currentSubresource, totalSubresources, &data);
    list->ResourceBarrier(mResource, D3D12_RESOURCE_STATE_COPY_DEST, mState);
}
