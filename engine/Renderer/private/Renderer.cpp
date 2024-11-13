#include <InfoStructs.hpp>
#include <Renderer.hpp>

Renderer::Renderer(DXDevice& device, DXShaderCompiler& shader_compiler)
{
    // Root Signature Setup
    {
        auto builder = DXShaderInputsBuilder();

        builder
            // Uniforms
            .AddStorageBuffer("camera_matrix", 0, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL)
            //.AddStorageBuffer("model_index", 1, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL)
            .AddStorageBuffer("model_matrix", 1, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_VERTEX)
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
            //.AddStorageBuffer("model_matrix", 7, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_VERTEX)
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

    // Buffer Setup
    {
        auto& resource_heap = device.GetDescriptorHeap(DXDevice::DescriptorHeap::RESOURCE_HEAP);

        // Camera
        {
            size_t requested_size = sizeof(CameraData) * 2;
            size_t bufferSize = (requested_size + 255) & ~255;

            auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto buffer_props = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

            CheckDX(device.Get()->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &buffer_props,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&camera_data)));
        }
    }
}

void Renderer::RenderFrame(const Camera& camera, DXDevice& device, DXSwapchain& swapchain_target)
{
    // Command List creation
    auto& command_queue = device.GetCommandQueue();
    auto command_list = command_queue.MakeCommandList(device.Get());
    auto frame_parity = cpu_frame % FRAME_BUFFER_COUNT;

    // Clearing swapchain
    {

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
    }

    // Update Camera Data
    {
        CameraData data {};
        data.m_view = camera.GetView();
        data.m_proj = camera.GetProjection();
        data.m_cameraPos = -data.m_view[3];
        data.m_camera = data.m_proj * data.m_view;

        // Map the buffer and copy data
        void* mappedData;

        uint64_t camera_data_start = frame_parity * sizeof(CameraData);
        CD3DX12_RANGE read_range { camera_data_start, camera_data_start + sizeof(CameraData) };
        camera_data->Map(0, &read_range, &mappedData);

        std::memcpy(mappedData, &data, sizeof(CameraData));
        camera_data->Unmap(0, &read_range);

        command_list.Get()->SetGraphicsRootConstantBufferView(0, camera_data->GetGPUVirtualAddress() + camera_data_start);
    }

    // Submit and Sync
    {
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
    }

    cpu_frame++;
}