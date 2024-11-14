#include <Log.hpp>
#include <resources/DXDescriptorHeap.hpp>

namespace detail
{

void CreateView(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const CBV& p)
{
    device->CreateConstantBufferView(&p.desc, handle);
}

void CreateView(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const SRV& p)
{
    device->CreateShaderResourceView(p.resource, &p.desc, handle);
}

void CreateView(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const UAV& p)
{
    device->CreateUnorderedAccessView(p.resource, p.counter_resource, &p.desc, handle);
}

void CreateView(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const RTV& p)
{
    device->CreateRenderTargetView(p.resource, &p.desc, handle);
}

void CreateView(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const DSV& p)
{
    device->CreateDepthStencilView(p.resource, &p.desc, handle);
}

}

DXDescriptorHeapBase::DXDescriptorHeapBase(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t size, const wchar_t* name)
{
    max_size = size;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = size;
    desc.Type = type;
    desc.Flags = {};

    if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    {
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }

    CheckDX(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap)));
    descriptor_heap->SetName(name);

    descriptor_stride = device->GetDescriptorHandleIncrementSize(type);
}

std::optional<CD3DX12_CPU_DESCRIPTOR_HANDLE> DXDescriptorHeapBase::GetCPUAddress(size_t index)
{
    if (index >= max_size)
    {
        return std::nullopt;
    }

    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
        index, descriptor_stride);
}
std::optional<CD3DX12_GPU_DESCRIPTOR_HANDLE> DXDescriptorHeapBase::GetGPUAddress(size_t index)
{
    if (index >= max_size)
    {
        return std::nullopt;
    }

    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
        index, descriptor_stride);
}

template <typename T>
    requires IsDescriptorType<T>
bool DXDescriptorHeap<T>::Allocate(ID3D12Device* device, const T& description, size_t index)
{
    if (index >= max_size)
    {
        Log(L"DXDescriptor Heap Error: Attempting to allocate beyond {} bounds ({}), size is {}", index, max_size);
        return false;
    }

    detail::CreateView(device, GetCPUAddress(index).value(), description);
    return true;
}

template class DXDescriptorHeap<CBV>;
template class DXDescriptorHeap<SRV>;
template class DXDescriptorHeap<UAV>;
template class DXDescriptorHeap<RTV>;
template class DXDescriptorHeap<DSV>;
