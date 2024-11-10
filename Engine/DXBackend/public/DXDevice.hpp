#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>
#include <array>
#include <commands/DXCommandQueue.hpp>
#include <descriptors/DXDescriptorHeap.hpp>

class DXDevice
{
public:
    enum class DescriptorHeap : size_t
    {
        RT_HEAP,
        DEPTH_HEAP,
        RESOURCE_HEAP,
        NUM_HEAPS
    };

    DXDevice(ComPtr<ID3D12Device> device);
    ~DXDevice() = default;

    ID3D12Device* Get() { return device.Get(); }
    DXCommandQueue& GetCommandQueue() { return *command_queue; }
    DXDescriptorHeap& GetDescriptorHeap(DescriptorHeap heap) { return *descriptor_heaps.at(static_cast<size_t>(heap)); }

private:
    ComPtr<ID3D12Device5> device;
    std::unique_ptr<DXCommandQueue> command_queue;
    std::array<std::unique_ptr<DXDescriptorHeap>, static_cast<size_t>(DescriptorHeap::NUM_HEAPS)> descriptor_heaps;
};