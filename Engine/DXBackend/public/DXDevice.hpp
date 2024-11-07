#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>

#include <dxgi1_6.h>

class DXDevice
{
public:
    DXDevice(ComPtr<ID3D12Device> device);
    ~DXDevice() = default;

private:
    ComPtr<ID3D12Device5> m_device;
    ComPtr<IDXGISwapChain3> swapchain;

    std::unique_ptr<DXCommandQueue> m_command_queue;
    std::unique_ptr<DXCommandList> m_command_list;
    std::shared_ptr<DXCommandAllocator> m_command_allocator[FRAME_BUFFER_COUNT];
    DXGPUFuture m_fence_values[FRAME_BUFFER_COUNT];

    std::shared_ptr<DXDescHeap> m_descriptor_heaps[NUM_DESC_HEAPS];
};