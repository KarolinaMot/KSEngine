#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>
#include <array>
#include <commands/DXCommandList.hpp>
#include <descriptors/DXDescriptorHeap.hpp>
#include <dxgi1_6.h>
#include <glm/vec2.hpp>

// TODO: no function to recreate swapchain (for resizeable windows)
class DXSwapchain
{
public:
    DXSwapchain(ComPtr<IDXGISwapChain> swapchain_handle, ID3D12Device* device, DXDescriptorHeap& rendertarget_heap);
    ~DXSwapchain() = default;

    glm::uvec2 GetResolution() const;
    DXDescriptorHandle& GetRenderTargetView(size_t frame_index) { return render_views.at(frame_index); }

    uint32_t GetBackbufferIndex() { return swapchain->GetCurrentBackBufferIndex(); }
    ID3D12Resource* GetBufferResource(size_t frame_index) { return buffers.at(frame_index).Get(); }

    void SwapBuffers(bool vsync) const;

    NON_COPYABLE(DXSwapchain);
    NON_MOVABLE(DXSwapchain);

private:
    ComPtr<IDXGISwapChain3> swapchain {};
    std::array<ComPtr<ID3D12Resource>, FRAME_BUFFER_COUNT> buffers {};
    std::array<DXDescriptorHandle, FRAME_BUFFER_COUNT> render_views {};
};