#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>
#include <array>
#include <commands/DXCommandQueue.hpp>
#include <dxgi1_6.h>


class DXDevice
{
public:
    DXDevice(ComPtr<ID3D12Device> device);
    ~DXDevice() = default;

private:
    ComPtr<ID3D12Device5> device;
    ComPtr<IDXGISwapChain3> swapchain;

    std::unique_ptr<DXCommandQueue> command_queue;
    std::array<DXFuture, FRAME_BUFFER_COUNT> frame_fence_values;

    // std::shared_ptr<DXDescHeap> m_descriptor_heaps[NUM_DESC_HEAPS];
};