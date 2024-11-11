#pragma once
#include <Common.hpp>
#include <DXDevice.hpp>
#include <display/DXSwapchain.hpp>
#include <sync/DXFuture.hpp>

class Renderer
{
public:
    Renderer() = default;
    ~Renderer() = default;

    NON_COPYABLE(Renderer);
    NON_MOVABLE(Renderer);

    void RenderFrame(DXDevice& device, DXSwapchain& swapchain_target);

private:
    std::array<DXFuture, FRAME_BUFFER_COUNT> frame_futures {};
    size_t cpu_frame {};
};