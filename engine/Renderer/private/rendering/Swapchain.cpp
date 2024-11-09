#include <rendering/Swapchain.hpp>

Swapchain::Swapchain(ComPtr<IDXGISwapChain> swapchain_handle, ID3D12Device* device, DXDescriptorHeap& rendertarget_heap)
{
    CheckDX(swapchain_handle.As(&swapchain));

    for (size_t i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        ComPtr<ID3D12Resource> buffer;
        CheckDX(swapchain->GetBuffer(i, IID_PPV_ARGS(&buffer)));
        buffers.at(i) = buffer;

        D3D12_RENDER_TARGET_VIEW_DESC desc {};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;

        render_views.at(i) = rendertarget_heap.Allocate(device, buffer.Get(), ViewParams::RTV { desc }).value();
    }
}