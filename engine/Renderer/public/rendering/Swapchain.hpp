#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>
#include <array>
#include <commands/DXCommandList.hpp>
#include <descriptors/DXDescriptorHeap.hpp>
#include <dxgi1_6.h>
#include <glm/vec2.hpp>

// TODO: no function to recreate swapchain (for resizeable windows)
class Swapchain
{
    Swapchain(ComPtr<IDXGISwapChain> swapchain_handle, ID3D12Device* device, DXDescriptorHeap& rendertarget_heap);
    ~Swapchain();

    DXDescriptorHandle& GetRTV(size_t frame_index) { return render_views.at(frame_index); }

    NON_COPYABLE(Swapchain);
    NON_MOVABLE(Swapchain);

private:
    ComPtr<IDXGISwapChain3> swapchain {};
    std::array<ComPtr<ID3D12Resource>, FRAME_BUFFER_COUNT> buffers {};
    std::array<DXDescriptorHandle, FRAME_BUFFER_COUNT> render_views {};
};