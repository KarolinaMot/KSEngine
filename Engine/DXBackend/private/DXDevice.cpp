#include <DXDevice.hpp>

DXDevice::DXDevice(ComPtr<ID3D12Device> _device)
{
    _device.As(&device);
    command_queue = std::make_unique<DXCommandQueue>(device.Get(), L"Main command queue");

    constexpr size_t HEAP_MAX_CAPACITY = 2048;

    auto& rtv_heap = descriptor_heaps.at(static_cast<size_t>(DescriptorHeap::RT_HEAP));
    rtv_heap = std::make_unique<DXDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, HEAP_MAX_CAPACITY,
        L"Render Target Descriptor Heap");

    auto& dsv_heap = descriptor_heaps.at(static_cast<size_t>(DescriptorHeap::DEPTH_HEAP));
    dsv_heap = std::make_unique<DXDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, HEAP_MAX_CAPACITY,
        L"Depth Stencil Descriptor Heap");

    auto& res_heap = descriptor_heaps.at(static_cast<size_t>(DescriptorHeap::RESOURCE_HEAP));
    res_heap = std::make_unique<DXDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, HEAP_MAX_CAPACITY,
        L"Resource Descriptor Heap");
}