#include "DXResource.hpp"
#include <code_utility.hpp>
#include <iostream>
#include "DXCommandList.hpp"

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
        ASSERT(false && "Resource creation failed");

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

}

void DXResource::ChangeState(D3D12_RESOURCE_STATES dstState)
{
    if (dstState == mState)
        return;

    mState = dstState;
}