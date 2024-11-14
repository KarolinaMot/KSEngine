#include <display/DXSwapchain.hpp>

DXSwapchain::DXSwapchain(ComPtr<IDXGISwapChain> swapchain_handle, ID3D12Device* device)
{
    CheckDX(swapchain_handle.As(&swapchain));
    render_target_heap = DXDescriptorHeap<RTV>(device, FRAME_BUFFER_COUNT);

    for (size_t i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        ComPtr<ID3D12Resource> buffer;
        CheckDX(swapchain->GetBuffer(i, IID_PPV_ARGS(&buffer)));

        std::wstring name = L"Swapchain Resource ";
        name += std::to_wstring(i);
        buffer->SetName(name.c_str());

        buffers.at(i) = buffer;

        D3D12_RENDER_TARGET_VIEW_DESC desc {};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;

        render_target_heap.Allocate(device, RTV { desc, buffer.Get() }, i);
    }
}

glm::uvec2 DXSwapchain::GetResolution() const
{
    glm::uvec2 res;
    swapchain->GetSourceSize(&res.x, &res.y);
    return res;
}

void DXSwapchain::SwapBuffers(bool vsync) const
{
    auto flags = vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    swapchain->Present(static_cast<uint32_t>(vsync), flags);
}