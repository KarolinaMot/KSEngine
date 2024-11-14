#include <InfoStructs.hpp>
#include <Renderer.hpp>
#include <gpu_resources/DXResourceBuilder.hpp>

namespace detail
{
template <typename T>
void WriteResource(DXResource& resource, const T& source)
{
    auto mapped_ptr = resource.Map(0);
    std::memcpy(mapped_ptr.Get(), &source, sizeof(T));
    resource.Unmap(std::move(mapped_ptr));
}

template <size_t S>
std::array<CD3DX12_RESOURCE_BARRIER, S> MakeTransitionBarriers(const std::array<ID3D12Resource*, S>& resources, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    std::array<CD3DX12_RESOURCE_BARRIER, S> out {};
    for (size_t i = 0; i < S; i++)
    {
        out.at(i) = CD3DX12_RESOURCE_BARRIER::Transition(resources.at(i), before, after);
    }
    return out;
}
}

Renderer::Renderer(DXDevice& device, DXShaderCompiler& shader_compiler, glm::uvec2 screen_size)
{
    // Root Signature Setup
    {
        auto builder = DXShaderInputsBuilder();

        builder
            // Uniforms
            .AddRootDescriptor("camera_matrix", 0, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL)
            .AddRootDescriptor("model_matrix", 1, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_VERTEX)
            .AddRootDescriptor("material_info", 2, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_PIXEL)
            .AddRootDescriptor("light_info", 3, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL)

            // Textures (RO)
            .AddDescriptorTable("base_tex", 0, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
            .AddDescriptorTable("normal_tex", 1, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
            .AddDescriptorTable("emissive_tex", 2, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
            .AddDescriptorTable("roughmet_tex", 3, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
            .AddDescriptorTable("occlusion_tex", 4, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)

            // Data Buffers (RO)
            .AddRootDescriptor("dir_lights", 5, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_ALL)
            .AddRootDescriptor("point_lights", 6, D3D12_ROOT_PARAMETER_TYPE_SRV, D3D12_SHADER_VISIBILITY_ALL)

            // Render Targets (RW)
            .AddDescriptorTable("GBuffer1", 0, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
            .AddDescriptorTable("GBuffer2", 1, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
            .AddDescriptorTable("GBuffer3", 2, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
            .AddDescriptorTable("GBuffer4", 3, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
            .AddDescriptorTable("PBRRes", 4, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)

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
            .WithPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)

            // Inputs
            .AddInput(0, "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, false)
            .AddInput(1, "NORMALS", 0, DXGI_FORMAT_R32G32B32_FLOAT, false)
            .AddInput(2, "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, false)
            .AddInput(3, "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, false)

            // Shaders
            .WithRootSignature(shader_inputs.GetSignature())
            .AttachShader(deferred_ps)
            .AttachShader(deferred_vs)
            .AttachShader(resolve_cs)

            // Depth-Stencil (Stencil is disabled in this setup)
            .WithDepthFormat(DXGI_FORMAT_D32_FLOAT)
            .WithDepthState(CD3DX12_DEPTH_STENCIL_DESC2(D3D12_DEFAULT))

            // Output formats (for graphics pass)
            .AddRenderTarget(DXGI_FORMAT_R32G32B32A32_FLOAT)
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

    // Constant Buffer Setup
    {
        DXResourceBuilder builder = DXResourceBuilder();

        builder
            .WithHeapType(D3D12_HEAP_TYPE_UPLOAD)
            .WithInitialState(D3D12_RESOURCE_STATE_GENERIC_READ);

        camera_data = builder.MakeBuffer(device.Get(), sizeof(CameraData), L"Camera Uniform Buffer").value();
        model_matrix_data = builder.MakeBuffer(device.Get(), sizeof(ModelMatrixData), L"Model Matrix Uniform Buffer").value();
        material_info_data = builder.MakeBuffer(device.Get(), sizeof(MaterialInfo), L"Material Info Uniform Buffer").value();
        light_data = builder.MakeBuffer(device.Get(), sizeof(LightInfo), L"Light Info Uniform Buffer").value();
    }

    // Depth Stencil Buffer
    {
        float depth_clear_colour = 1.0f;

        DXResourceBuilder builder = DXResourceBuilder();
        builder
            .WithInitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE)
            .WithResourceFlags(D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            .WithOptimizedClearColour(CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, &depth_clear_colour));

        depth_stencil = builder.MakeTexture2D(device.Get(), screen_size, DXGI_FORMAT_D32_FLOAT, L"Depth Stencil").value();

        depth_heap = DXDescriptorHeap<DSV>(device.Get(), 1);

        D3D12_DEPTH_STENCIL_VIEW_DESC desc {};
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        desc.Format = DXGI_FORMAT_D32_FLOAT;

        depth_heap.Allocate(device.Get(), DSV { desc, depth_stencil.Get() }, 0);
    }

    // Render Targets (Position, Albedo, Normal, Emissive)
    {
        glm::vec4 black = { 0.0f, 0.0f, 0.0f, 1.0f };
        CD3DX12_CLEAR_VALUE clear_colour_unorm = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, &black.x);
        CD3DX12_CLEAR_VALUE clear_colour_float = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R32G32B32A32_FLOAT, &black.x);

        DXResourceBuilder builder = DXResourceBuilder();
        builder
            .WithResourceFlags(D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
            .WithInitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            .WithOptimizedClearColour(clear_colour_float);

        position_rt = builder.MakeTexture2D(device.Get(), screen_size, DXGI_FORMAT_R32G32B32A32_FLOAT, L"Position GBuffer").value();

        builder
            .WithOptimizedClearColour(clear_colour_unorm);

        albedo_rt = builder.MakeTexture2D(device.Get(), screen_size, DXGI_FORMAT_R8G8B8A8_UNORM, L"Albedo GBuffer").value();
        normal_rt = builder.MakeTexture2D(device.Get(), screen_size, DXGI_FORMAT_R8G8B8A8_UNORM, L"Normal GBuffer").value();
        emissive_rt = builder.MakeTexture2D(device.Get(), screen_size, DXGI_FORMAT_R8G8B8A8_UNORM, L"Emissive GBuffer").value();

        render_target_heap = DXDescriptorHeap<RTV>(device.Get(), 4);

        D3D12_RENDER_TARGET_VIEW_DESC desc {};
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

        render_target_heap.Allocate(device.Get(), { desc, position_rt.Get() }, 0);

        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        render_target_heap.Allocate(device.Get(), { desc, albedo_rt.Get() }, 1);
        render_target_heap.Allocate(device.Get(), { desc, normal_rt.Get() }, 2);
        render_target_heap.Allocate(device.Get(), { desc, emissive_rt.Get() }, 3);
    }

    // Final Result buffer
    {
        DXResourceBuilder builder = DXResourceBuilder();
        builder
            .WithResourceFlags(D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
            .WithInitialState(D3D12_RESOURCE_STATE_COPY_SOURCE);

        result_frame = builder.MakeTexture2D(device.Get(), screen_size, DXGI_FORMAT_R8G8B8A8_UNORM, L"Final Result").value();

        unordered_access_heap = DXDescriptorHeap<UAV>(device.Get(), 5);

        D3D12_UNORDERED_ACCESS_VIEW_DESC desc {};
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

        unordered_access_heap.Allocate(device.Get(), { desc, position_rt.Get() }, 0);

        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        unordered_access_heap.Allocate(device.Get(), { desc, albedo_rt.Get(), nullptr }, 1);
        unordered_access_heap.Allocate(device.Get(), { desc, normal_rt.Get(), nullptr }, 2);
        unordered_access_heap.Allocate(device.Get(), { desc, emissive_rt.Get(), nullptr }, 3);
        unordered_access_heap.Allocate(device.Get(), { desc, result_frame.Get(), nullptr }, 4);
    }
}

void Renderer::RenderFrame(const Camera& camera, DXDevice& device, DXSwapchain& swapchain_target)
{
    // Command List creation
    auto& command_queue = device.GetCommandQueue();
    auto command_list = command_queue.MakeCommandList(device.Get());

    // Set Render targets
    {
        std::array<ID3D12Resource*, 4> render_targets = {
            position_rt.Get(),
            albedo_rt.Get(),
            normal_rt.Get(),
            emissive_rt.Get()
        };

        auto to_render_target = detail::MakeTransitionBarriers(
            render_targets,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        command_list.SetResourceBarriers(to_render_target.size(), to_render_target.data());

        CD3DX12_CPU_DESCRIPTOR_HANDLE begin_range = render_target_heap.GetCPUAddress(0).value();
        CD3DX12_CPU_DESCRIPTOR_HANDLE depth_descriptor = depth_heap.GetCPUAddress(0).value();

        command_list.Get()->OMSetRenderTargets(4, &begin_range, true, &depth_descriptor);

        glm::vec4 gbuffer_clear_colour = { 0.0f, 0.0f, 0.0f, 1.0f };

        for (size_t i = 0; i < 4; i++)
        {
            command_list.ClearRenderTarget(
                render_target_heap.GetCPUAddress(i).value(),
                gbuffer_clear_colour);
        }

        command_list.ClearDepthStencil(depth_descriptor, 1.0f, 0);
    }

    command_list.Get()->SetGraphicsRootSignature(shader_inputs.GetSignature());
    command_list.Get()->SetPipelineState(graphics_deferred_pipeline.state.Get());
    command_list.Get()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Update Camera Data
    {
        CameraData data {};
        data.m_view = camera.GetView();
        data.m_proj = camera.GetProjection();
        data.m_cameraPos = -data.m_view[3];
        data.m_camera = data.m_proj * data.m_view;

        detail::WriteResource(camera_data, data);
        command_list.BindRootCBV(camera_data, shader_inputs.GetInputIndex("camera_matrix").value());
    }
    // Update Light Info
    {
        LightInfo data {};

        detail::WriteResource(light_data, data);
        command_list.BindRootCBV(light_data, shader_inputs.GetInputIndex("light_info").value());
    }

    // For each model, draw in GBuffers
    {
        for (const auto& [mat, mesh] : models_to_render)
        {
            // Set model matrix
            {
                ModelMatrixData data {};
                data.mModel = mat;
                data.mTransposed = glm::transpose(mat);

                detail::WriteResource(model_matrix_data, data);
                command_list.BindRootCBV(model_matrix_data, shader_inputs.GetInputIndex("model_matrix").value());
            }
            // Set material data
            {
                MaterialInfo data {};

                detail::WriteResource(material_info_data, data);
                command_list.BindRootCBV(model_matrix_data, shader_inputs.GetInputIndex("material_info").value());
            }
            // Set Input Assembly
            {
                D3D12_VERTEX_BUFFER_VIEW vertex_views[4] = {
                    { mesh->position.GetAddress(), mesh->position.GetDimensions().x, sizeof(glm::vec3) },
                    { mesh->normals.GetAddress(), mesh->normals.GetDimensions().x, sizeof(glm::vec3) },
                    { mesh->uvs.GetAddress(), mesh->uvs.GetDimensions().x, sizeof(glm::vec2) },
                    { mesh->tangents.GetAddress(), mesh->tangents.GetDimensions().x, sizeof(glm::vec3) },
                };

                command_list.Get()->IASetVertexBuffers(0, 4, vertex_views);

                D3D12_INDEX_BUFFER_VIEW index_view {};
                index_view.BufferLocation = mesh->indices.GetAddress();
                index_view.SizeInBytes = mesh->indices.GetDimensions().x;
                index_view.Format = DXGI_FORMAT_R32_UINT;

                command_list.Get()->IASetIndexBuffer(&index_view);
                command_list.Get()->DrawIndexedInstanced(mesh->index_count, 1, 0, 0, 0);
            }
        }

        models_to_render.clear();
    }

    command_list.Get()->SetComputeRootSignature(shader_inputs.GetSignature());
    command_list.Get()->SetPipelineState(compute_pbr_pipeline.state.Get());

    auto* uav_heap = unordered_access_heap.Get();
    command_list.Get()->SetDescriptorHeaps(1, &uav_heap);

    // Resolve PBR shading
    {
        std::array<ID3D12Resource*, 4> gbuffers = {
            position_rt.Get(),
            albedo_rt.Get(),
            normal_rt.Get(),
            emissive_rt.Get()
        };

        auto to_uav = detail::MakeTransitionBarriers(
            gbuffers,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        command_list.SetResourceBarriers(to_uav.size(), to_uav.data());

        // auto* uav_heap = unordered_access_heap.Get();
        // command_list.Get()->SetDescriptorHeaps(1, &uav_heap);

        // for (size_t i = 0; i < 5; i++)
        // {
        //     command_list.Get()->SetComputeRootDescriptorTable(
        //         shader_inputs.GetInputIndex("GBuffer1").value() + i,
        //         unordered_access_heap.GetGPUAddress(i).value());
        // }

        // auto to_uav_frame = CD3DX12_RESOURCE_BARRIER::Transition(
        //     result_frame.Get(),
        //     D3D12_RESOURCE_STATE_COPY_SOURCE,
        //     D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // command_list.SetResourceBarriers(1, &to_uav_frame);

        // glm::vec4 clear_colour = { 1.0f, 1.0f, 1.0f, 1.0f };

        // command_list.Get()->ClearUnorderedAccessViewFloat(
        //     unordered_access_heap.GetGPUAddress(4).value(),
        //     CD3DX12_CPU_DESCRIPTOR_HANDLE {},
        //     result_frame.Get(), &clear_colour.x,
        //     0, nullptr);

        // auto screen_size = swapchain_target.GetResolution();
        // command_list.Get()->Dispatch(screen_size.x / 8, screen_size.y / 8, 1);
    }

    // Copy to swapchain
    {
        auto current_backbuffer = swapchain_target.GetBackbufferIndex();
        auto* swapchain_buffer_resource = swapchain_target.GetBufferResource(current_backbuffer);

        CD3DX12_RESOURCE_BARRIER copy_barriers[2] {};

        copy_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            albedo_rt.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);

        copy_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            swapchain_buffer_resource,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_COPY_DEST);

        command_list.SetResourceBarriers(2, copy_barriers);

        D3D12_TEXTURE_COPY_LOCATION dest_location {};
        dest_location.pResource = swapchain_buffer_resource;
        dest_location.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src_location {};
        src_location.pResource = albedo_rt.Get();
        src_location.SubresourceIndex = 0;

        auto screen_size = swapchain_target.GetResolution();
        CD3DX12_BOX box = CD3DX12_BOX { 0, 0, static_cast<int>(screen_size.x), static_cast<int>(screen_size.y) };
        command_list.Get()->CopyTextureRegion(&dest_location, 0, 0, 0, &src_location, &box);

        auto to_present = CD3DX12_RESOURCE_BARRIER::Transition(
            swapchain_buffer_resource,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PRESENT);

        // TODO REMOVE
        auto albedo_test = CD3DX12_RESOURCE_BARRIER::Transition(albedo_rt.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        command_list.SetResourceBarriers(1, &to_present);
        command_list.SetResourceBarriers(1, &albedo_test);
    }

    // Submit and Sync
    {
        // TODO: correctly implement frames in flight
        command_queue.SubmitCommandList(std::move(command_list)).Wait();
        swapchain_target.SwapBuffers(true);
    }
}