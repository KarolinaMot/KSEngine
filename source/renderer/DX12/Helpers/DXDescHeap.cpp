#include "DXDescHeap.hpp"
#include "DXHeapHandle.hpp"
#include "device/Device.hpp"

DXDescHeap::DXDescHeap(ComPtr<ID3D12Device5> device, int numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, LPCWSTR name, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    D3D12_DESCRIPTOR_HEAP_DESC renderTargetDesc = {};
    renderTargetDesc.NumDescriptors = numDescriptors;
    renderTargetDesc.Type = type;
    renderTargetDesc.Flags = flags;

    mType = type;
    mMaxResources = numDescriptors;

    // Leaving space for ImGUI resources
    if (mType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
        mResourceCount = 4;

    HRESULT hr = device->CreateDescriptorHeap(&renderTargetDesc, IID_PPV_ARGS(&mDescriptorHeap));
    if (FAILED(hr))
    {
        // LOG(LogCore, Fatal, "Failed to create descriptor heap");
        assert(false && "Failed to create descriptor heap");
    }
    mDescriptorSize = device->GetDescriptorHandleIncrementSize(type);
    mDescriptorHeap->SetName(name);
    m_device = device;
}

DXHeapHandle DXDescHeap::AllocateResource(DXResource* resource, D3D12_SHADER_RESOURCE_VIEW_DESC* desc)
{
    int slot = -1;

    if (mType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
        // LOG(LogCore, Warning, "Trying to allocate an SRV in the wrong heap");
        assert(false && "Trying to allocate an SRV in the wrong heap");
        return DXHeapHandle();
    }
    if (mClearList.size() > 0)
    {
        slot = mClearList[0];
        mClearList.erase(mClearList.begin());
    }
    else if (mResourceCount <= mMaxResources)
    {
        slot = mResourceCount;
        mResourceCount++;
    }
    else
    {
        // LOG(LogCore, Fatal, "Descriptor heap maximum reached");
        assert(false && "Descriptor heap maximum reached");
        return DXHeapHandle();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), slot, mDescriptorSize);
    m_device->CreateShaderResourceView(resource->Get(), desc, handle);

    return DXHeapHandle(slot, shared_from_this());
}

DXHeapHandle DXDescHeap::AllocateUAV(DXResource* resource, D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, DXResource* counterResource)
{
    int slot = -1;

    if (mType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
        assert(false && "Trying to allocate an SRV in the wrong heap");
        return DXHeapHandle();
    }
    if (mClearList.size() > 0)
    {
        slot = mClearList[0];
        mClearList.erase(mClearList.begin());
    }
    else if (mResourceCount <= mMaxResources)
    {
        slot = mResourceCount;
        mResourceCount++;
    }
    else
    {
        // LOG(LogCore, Fatal, "Descriptor heap maximum reached");
        assert(false && "Descriptor heap maximum reached");
        return DXHeapHandle();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), slot, mDescriptorSize);

    if (!counterResource)
        m_device->CreateUnorderedAccessView(resource->Get(), nullptr, desc, handle);
    else
        m_device->CreateUnorderedAccessView(resource->Get(), counterResource->Get(), desc, handle);

    return DXHeapHandle(slot, shared_from_this());
}

DXHeapHandle DXDescHeap::AllocateRenderTarget(DXResource* resource, D3D12_RENDER_TARGET_VIEW_DESC* desc)
{
    int slot = -1;

    if (mType != D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    {
        // LOG(LogCore, Warning, "Trying to allocate an RTV in the wrong heap");
        assert(false && "Trying to allocate an RTV in the wrong heap");
        return DXHeapHandle();
    }

    if (mClearList.size() > 0)
    {
        slot = mClearList[0];
        mClearList.erase(mClearList.begin());
    }
    else if (mResourceCount < mMaxResources)
    {
        slot = mResourceCount;
        mResourceCount++;
    }
    else
    {
        // LOG(LogCore, Fatal, "Descriptor heap maximum reached");
        assert(false && "Descriptor heap maximum reached");
        return DXHeapHandle();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), slot, mDescriptorSize);
    m_device->CreateRenderTargetView(resource->Get(), desc, handle);

    return DXHeapHandle(slot, shared_from_this());
}

DXHeapHandle DXDescHeap::AllocateRenderTarget(DXResource* resource, ID3D12Device5* device, D3D12_RENDER_TARGET_VIEW_DESC* desc)
{
    int slot = -1;

    if (mType != D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    {
        // LOG(LogCore, Warning, "Trying to allocate an RTV in the wrong heap");
        assert(false && "Trying to allocate an RTV in the wrong heap");
        return DXHeapHandle();
    }

    if (mClearList.size() > 0)
    {
        slot = mClearList[0];
        mClearList.erase(mClearList.begin());
    }
    else if (mResourceCount <= mMaxResources)
    {
        slot = mResourceCount;
        mResourceCount++;
    }
    else
    {
        // LOG(LogCore, Fatal, "Descriptor heap maximum reached");
        assert(false && "Descriptor heap maximum reached");
        return DXHeapHandle();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), slot, mDescriptorSize);
    m_device->CreateRenderTargetView(resource->Get(), desc, handle);

    return DXHeapHandle(slot, shared_from_this());
}

DXHeapHandle DXDescHeap::AllocateDepthStencil(DXResource* resource, D3D12_DEPTH_STENCIL_VIEW_DESC* desc)
{
    int slot = -1;

    if (mType != D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
    {
        // LOG(LogCore, Warning, "Trying to allocate a DSV in the wrong heap");
        assert(false && "Trying to allocate an DSV in the wrong heap");
        return DXHeapHandle();
    }

    if (mClearList.size() > 0)
    {
        slot = mClearList[0];
        mClearList.erase(mClearList.begin());
    }
    else if (mResourceCount <= mMaxResources)
    {
        slot = mResourceCount;
        mResourceCount++;
    }
    else
    {
        // LOG(LogCore, Fatal, "Descriptor heap maximum reached");
        assert(false && "Descriptor heap maximum reached");
        return DXHeapHandle();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), slot, mDescriptorSize);
    m_device->CreateDepthStencilView(resource->Get(), desc, handle);

    return DXHeapHandle(slot, shared_from_this());
}

DXHeapHandle DXDescHeap::AllocateDepthStencil(DXResource* resource, ID3D12Device5* device, D3D12_DEPTH_STENCIL_VIEW_DESC* desc)
{
    int slot = -1;

    if (mType != D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
    {
        // LOG(LogCore, Warning, "Trying to allocate a DSV in the wrong heap");
        assert(false && "Trying to allocate an DSV in the wrong heap");
        return DXHeapHandle();
    }

    if (mClearList.size() > 0)
    {
        slot = mClearList[0];
        mClearList.erase(mClearList.begin());
    }
    else if (mResourceCount <= mMaxResources)
    {
        slot = mResourceCount;
        mResourceCount++;
    }
    else
    {
        // LOG(LogCore, Fatal, "Descriptor heap maximum reached");
        assert(false && "Descriptor heap maximum reached");
        return DXHeapHandle();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), slot, mDescriptorSize);
    m_device->CreateDepthStencilView(resource->Get(), desc, handle);

    return DXHeapHandle(slot, shared_from_this());
}

void DXDescHeap::DeallocateResource(int slot)
{
    for (size_t i = 0; i < mClearList.size(); i++)
    {
        if (mClearList[i] == slot)
            return;
    }

    mClearList.push_back(slot);
}