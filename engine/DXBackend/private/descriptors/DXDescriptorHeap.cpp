#include <Log.hpp>
#include <descriptors/DXDescriptorHeap.hpp>

namespace detail
{

void CreateView(ID3D12Device* device, MAYBE_UNUSED ID3D12Resource* resource, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const DXDescriptorHeap::CBVParameters& p)
{
    device->CreateConstantBufferView(&p.desc, handle);
}

void CreateView(ID3D12Device* device, ID3D12Resource* resource, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const DXDescriptorHeap::SRVParameters& p)
{
    device->CreateShaderResourceView(resource, &p.desc, handle);
}

void CreateView(ID3D12Device* device, ID3D12Resource* resource, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const DXDescriptorHeap::UAVParameters& p)
{
    device->CreateUnorderedAccessView(resource, p.counter_resource, &p.desc, handle);
}

void CreateView(ID3D12Device* device, ID3D12Resource* resource, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const DXDescriptorHeap::RTVParameters& p)
{
    device->CreateRenderTargetView(resource, &p.desc, handle);
}

void CreateView(ID3D12Device* device, ID3D12Resource* resource, CD3DX12_CPU_DESCRIPTOR_HANDLE handle, const DXDescriptorHeap::DSVParameters& p)
{
    device->CreateDepthStencilView(resource, &p.desc, handle);
}

}

DXDescriptorHeap::DXDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t size, const wchar_t* name)
{
    capacity = size;
    descriptor_type = type;

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

    for (size_t i = 0; i < capacity; i++)
        free_list.push(i);
}

void DXDescriptorHeap::FreeHandle(CD3DX12_CPU_DESCRIPTOR_HANDLE handle)
{
    size_t index = (handle.ptr - descriptor_heap->GetCPUDescriptorHandleForHeapStart().ptr) / descriptor_stride;
    free_list.push(index);
}

std::optional<DXDescriptorHandle> DXDescriptorHeap::Allocate(ID3D12Device* device, ID3D12Resource* resource, const AllocateParameters& parameters)
{
    if (free_list.empty())
    {
        Log("Warning: Descriptor Heap is out of free descriptor slots");
        return std::nullopt;
    }

    if (parameters.index() == 3 && descriptor_type != D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    {
        Log("Warning: Trying to create a RTV with a non render-target heap");
        return std::nullopt;
    }

    if (parameters.index() == 4 && descriptor_type != D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
    {
        Log("Warning: Trying to create a DSV with a non depth-stencil heap");
        return std::nullopt;
    }

    size_t index = free_list.front();
    free_list.pop();

    auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
        index, descriptor_stride);

    auto create_visitor = [device, resource, handle](const auto& p)
    { detail::CreateView(device, resource, handle, p); };

    std::visit(create_visitor, parameters);

    return DXDescriptorHandle(this, handle);
}
