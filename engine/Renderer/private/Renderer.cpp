#include <Renderer.hpp>

Renderer::Renderer(DXDevice& device, DXShaderCompiler& shader_compiler)
{
    // Root Signature Setup
    {
        auto builder = DXShaderInputsBuilder();

        builder
            // Uniforms
            .AddStorageBuffer("camera_matrix", 0, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL)
            .AddStorageBuffer("model_index", 1, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL)
            .AddStorageBuffer("light_info", 2, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL)

            // Textures (RO)
            .AddStorageBuffer("base_tex", 0, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
            .AddStorageBuffer("normal_tex", 1, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
            .AddStorageBuffer("emissive_tex", 2, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
            .AddStorageBuffer("roughmet_tex", 3, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
            .AddStorageBuffer("occlusion_tex", 4, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)

            // Data Buffers (RO)
            .AddStorageBuffer("dir_lights", 5, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_ALL)
            .AddStorageBuffer("point_lights", 6, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_ALL)
            .AddStorageBuffer("model_matrix", 7, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_VERTEX)
            .AddStorageBuffer("material_info", 8, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)

            // Render Targets (RW)
            .AddDescriptorTable("PBRRes", 0, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
            .AddDescriptorTable("GBuffer1", 1, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
            .AddDescriptorTable("GBuffer2", 2, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
            .AddDescriptorTable("GBuffer3", 3, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
            .AddDescriptorTable("GBuffer4", 4, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)

            // Static Sampler
            .AddStaticSampler("main_sampler", 0, {}, D3D12_SHADER_VISIBILITY_ALL);

        shader_inputs = builder.Build(device.Get(), L"Deferred Pipeline Inputs").value();
    }

    // Pipeline Setup
    {
        shader_compiler.AddIncludeDirectory(L"assets/shaders");

        auto deferred_vs = shader_compiler
                               .CompileFromPath("assets/shaders/Deferred.hlsl", DXShader::Type::VERTEX, L"mainVS")
                               .value();

        auto deferred_ps = shader_compiler
                               .CompileFromPath("assets/shaders/Deferred.hlsl", DXShader::Type::VERTEX, L"mainPS")
                               .value();

        auto resolve_cs = shader_compiler
                              .CompileFromPath("assets/shaders/Main.hlsl", DXShader::Type::COMPUTE, L"main")
                              .value();

        auto builder = DXPipelineBuilder();

        builder
            // Inputs
            .AddInput(0, "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, false)
            .AddInput(1, "NORMALS", 0, DXGI_FORMAT_R32G32B32_FLOAT, false)
            .AddInput(2, "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, false)
            .AddInput(3, "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, false)

            // Shaders
            .SetRootSignature(shader_inputs.GetSignature())
            .AttachShader(deferred_ps)
            .AttachShader(deferred_vs)
            .AttachShader(resolve_cs)

            // Output formats (for graphics pass)
            .AddRenderTarget(DXGI_FORMAT_R32G32B32_FLOAT)
            .AddRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM)
            .AddRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM)
            .AddRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM);

        graphics_deferred_pipeline = builder.BuildGraphicsPipeline(device.Get(), L"Deferred Graphics Pipeline").value();

        builder
            // Output formats (for compute pass)
            .ClearOutputDescription()
            .AddRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM);

        compute_pbr_pipeline = builder.BuildComputePipeline(device.Get(), L"Compute PBR Pipeline").value();
    }
}

// auto builder = DXPipelineBuilder()
//                            .AddInput("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, VDS_POSITIONS)
//                            .AddInput("NORMALS", DXGI_FORMAT_R32G32B32_FLOAT, VDS_NORMALS)
//                            .AddInput("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, VDS_UV)
//                            .AddInput("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, VDS_TANGENTS)
//                            .SetVertexAndPixelShaders(v->GetBufferPointer(), v->GetBufferSize(), p->GetBufferPointer(), p->GetBufferSize());

//         if (!rtFormats)
//         {
//             LOG(Log::Severity::WARN, "Render target format has not been passed. A default format of R8G8B8A8_UNORM will be added instead");
//             builder.AddRenderTarget(KSFormatsToDXGI(Formats::R8G8B8A8_UNORM));
//         }
//         else
//         {
//             for (int i = 0; i < numFormats; i++)
//             {
//                 builder.AddRenderTarget(KSFormatsToDXGI(rtFormats[i]));
//             }
//         }

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