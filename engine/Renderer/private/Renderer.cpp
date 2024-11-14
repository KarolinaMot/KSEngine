#include <InfoStructs.hpp>
#include <Renderer.hpp>
#include <resources/DXResourceBuilder.hpp>

namespace detail
{

template <typename T>
void WriteResource(DXResource& resource, const T& source)
{
    auto mapped_ptr = resource.Map(0);
    std::memcpy(mapped_ptr.Get(), &source, sizeof(T));
    resource.Unmap(std::move(mapped_ptr));
}

}

Renderer::Renderer(DXDevice& device, DXShaderCompiler& shader_compiler)
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
            .WithRootSignature(shader_inputs.GetSignature())
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

    // Constant Buffer Setup
    {
        // Uniforms
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
    }
}

void Renderer::RenderFrame(const Camera& camera, DXDevice& device, DXSwapchain& swapchain_target)
{
    // Command List creation
    auto& command_queue = device.GetCommandQueue();
    auto command_list = command_queue.MakeCommandList(device.Get());

    // Clearing swapchain
    {
        auto current_backbuffer = swapchain_target.GetBackbufferIndex();

        auto* swapchain_buffer_resource = swapchain_target.GetBufferResource(current_backbuffer);
        auto swapchain_view_handle = swapchain_target.GetRTV(current_backbuffer).value();

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

    command_list.Get()->SetGraphicsRootSignature(shader_inputs.GetSignature());
    command_list.Get()->SetPipelineState(graphics_deferred_pipeline.state.Get());

    // Update Camera Data
    {
        CameraData data {};
        data.m_view = camera.GetView();
        data.m_proj = camera.GetProjection();
        data.m_cameraPos = -data.m_view[3];
        data.m_camera = data.m_proj * data.m_view;

        detail::WriteResource(camera_data, data);
        command_list.BindRootCBV(camera_data, 0);
    }
    // Update Light Info
    {
        LightInfo data {};

        detail::WriteResource(light_data, data);
        command_list.BindRootCBV(light_data, 3);
    }

    // For each model, draw
    {
        for (const auto& mat : models_to_render)
        {
            // Set model matrix
            {
                ModelMatrixData data {};
                data.mModel = mat;
                data.mTransposed = glm::transpose(mat);

                detail::WriteResource(model_matrix_data, data);
                command_list.BindRootCBV(model_matrix_data, 1);
            }
            // Set material data
            {
                MaterialInfo data {};

                detail::WriteResource(material_info_data, data);
                command_list.BindRootCBV(model_matrix_data, 2);
            }
            // Set
        }

        models_to_render.clear();
    }

    // Submit and Sync
    {
        // TODO: correctly implement frames in flight
        command_queue.SubmitCommandList(std::move(command_list)).Wait();
        swapchain_target.SwapBuffers(true);
    }
}