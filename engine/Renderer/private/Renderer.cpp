#include <Renderer.hpp>

void Renderer::RenderFrame(DXDevice& device, DXSwapchain& swapchain_target)
{
    // Command List creation
    auto& command_queue = device.GetCommandQueue();
    auto command_list = command_queue.MakeCommandList(device.Get());

    // Clearing frame
    auto frame_parity = cpu_frame % FRAME_BUFFER_COUNT;
    auto* swapchain_buffer_resource = swapchain_target.GetBufferResource(frame_parity);
    auto& swapchain_view_handle = swapchain_target.GetRenderTargetView(frame_parity);

    auto to_render_target = CD3DX12_RESOURCE_BARRIER::Transition(
        swapchain_buffer_resource,
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    command_list.SetResourceBarriers(1, &to_render_target);

    command_list.ClearRenderTarget(swapchain_view_handle, { 0.3f, 0.3f, 0.3f, 1.0f });

    auto to_present = CD3DX12_RESOURCE_BARRIER::Transition(
        swapchain_buffer_resource,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    command_list.SetResourceBarriers(1, &to_present);

    // dx_command_queue.SubmitCommandList(std::move(command_list));
    // main_swapchain->SwapBuffers(true);

    size_t next_frame_parity = swapchain_target.GetBackbufferIndex();

    if (next_frame_parity == frame_parity)
    {
        // Log("Submitted CPU draw commands {}", cpu_frame);
        frame_futures.at(next_frame_parity) = command_queue.SubmitCommandList(std::move(command_list));

        auto next_parity = (next_frame_parity + 1) % FRAME_BUFFER_COUNT;
        if (frame_futures.at(next_parity).IsComplete())
        {
            // Log("Presented GPU frame {} (non-blocking frame)", gpu_frame++);
            swapchain_target.SwapBuffers(true);
        }
    }
    else
    {
        frame_futures.at(next_frame_parity).Wait();
        // Log("Presented GPU frame {} (blocking frame)", gpu_frame++);
        swapchain_target.SwapBuffers(true);

        // Log("Submitted CPU draw commands {}", cpu_frame);
        frame_futures.at(next_frame_parity) = command_queue.SubmitCommandList(std::move(command_list));
    }

    cpu_frame++;
}