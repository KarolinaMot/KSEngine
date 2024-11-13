#pragma once
#include <Common.hpp>
#include <DXDevice.hpp>
#include <display/DXSwapchain.hpp>
#include <shader/DXShaderCompiler.hpp>
#include <shader/DXShaderInputs.hpp>
#include <sync/DXFuture.hpp>


class Renderer
{
public:
    Renderer(DXDevice& device, DXShaderCompiler& shader_compiler);
    ~Renderer() = default;

    NON_COPYABLE(Renderer);
    NON_MOVABLE(Renderer);

    void RenderFrame(DXDevice& device, DXSwapchain& swapchain_target);

private:
    DXShaderInputs shader_inputs {};
    std::array<DXFuture, FRAME_BUFFER_COUNT> frame_futures {};
    size_t cpu_frame {};
};