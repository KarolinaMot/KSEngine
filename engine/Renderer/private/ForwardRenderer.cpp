#include <ForwardRenderer.hpp>

#include <InfoStructs.hpp>
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

ForwardRenderer::ForwardRenderer(DXDevice& device, DXShaderCompiler& shader_compiler, glm::uvec2 screen_size)
{
    // Root Signature Setup
    {
        auto builder = DXShaderInputsBuilder();

        builder
            // Uniforms
            .AddRootDescriptor("camera_matrix", 0, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL)
            .AddRootDescriptor("model_matrix", 1, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_VERTEX);

        shader_inputs = builder.Build(device.Get(), L"Forward Pipeline Inputs").value();
    }

    // Pipeline Setup
    {
        shader_compiler.AddIncludeDirectory(L"assets/shaders");

        auto vs = shader_compiler
                      .CompileFromPath("assets/shaders/SimpleForward.hlsl", DXShader::Type::VERTEX, L"mainVS")
                      .value();

        auto ps = shader_compiler
                      .CompileFromPath("assets/shaders/SimpleForward.hlsl", DXShader::Type::PIXEL, L"mainPS")
                      .value();

        auto builder = DXPipelineBuilder();

        builder
            .WithPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)

            // Inputs
            .AddInput(0, "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, false)

            // Shaders
            .WithRootSignature(shader_inputs.GetSignature())
            .AttachShader(vs)
            .AttachShader(ps)

            // Depth-Stencil (Stencil is disabled in this setup)
            .WithDepthFormat(DXGI_FORMAT_D32_FLOAT)
            .WithDepthState(CD3DX12_DEPTH_STENCIL_DESC2(D3D12_DEFAULT))

            // Output formats (for graphics pass)
            .AddRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM);

        pipeline = builder.BuildGraphicsPipeline(device.Get(), L"Forward Graphics Pipeline").value();
    }

    // Constant Buffer Setup
    {
        DXResourceBuilder builder = DXResourceBuilder();

        builder
            .WithHeapType(D3D12_HEAP_TYPE_UPLOAD)
            .WithInitialState(D3D12_RESOURCE_STATE_GENERIC_READ);

        camera_data = builder.MakeBuffer(device.Get(), sizeof(CameraData), L"Camera Uniform Buffer").value();
        model_matrix_data = builder.MakeBuffer(device.Get(), sizeof(ModelMatrixData), L"Model Matrix Uniform Buffer").value();
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

    // Triangle buffers
    {
        float vertices[] = {
            -0.5f, -0.5f, 0.5f,
            0.5f, -0.5f, 0.5f,
            0.0f, 0.5f, 0.5f
        };

        uint32_t indices[] = {
            0, 1, 2
        };

        DXResourceBuilder builder = DXResourceBuilder();

        builder
            .WithHeapType(D3D12_HEAP_TYPE_UPLOAD)
            .WithInitialState(D3D12_RESOURCE_STATE_COMMON);

        triangle_verts = builder.MakeBuffer(device.Get(), sizeof(vertices)).value();
        triangle_indices = builder.MakeBuffer(device.Get(), sizeof(indices)).value();

        detail::WriteResource(triangle_verts, vertices);
        detail::WriteResource(triangle_indices, indices);
    }
}

void ForwardRenderer::RenderFrame(const Camera& camera, DXDevice& device, DXSwapchain& swapchain_target)
{
    // Command List creation
    auto& command_queue = device.GetCommandQueue();
    auto command_list = command_queue.MakeCommandList(device.Get());

    // Graphics Pass
    command_list.Get()->SetGraphicsRootSignature(shader_inputs.GetSignature());
    command_list.Get()->SetPipelineState(pipeline.state.Get());
    command_list.Get()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Update Scissor and Rect
    {
        glm::vec2 resolution = swapchain_target.GetResolution();

        CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, resolution.x, resolution.y);
        command_list.Get()->RSSetViewports(1, &viewport);

        CD3DX12_RECT scissors = CD3DX12_RECT(0, 0, resolution.x, resolution.y);
        command_list.Get()->RSSetScissorRects(1, &scissors);
    }

    // Clear Swapchain Render Target
    {
        auto current_backbuffer = swapchain_target.GetBackbufferIndex();
        auto* swapchain_buffer_resource = swapchain_target.GetBufferResource(current_backbuffer);

        auto swapchain_rtv = swapchain_target.GetRTV(current_backbuffer).value();
        auto dsv = depth_heap.GetCPUAddress(0).value();

        auto to_render_target = CD3DX12_RESOURCE_BARRIER::Transition(
            swapchain_buffer_resource,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        command_list.SetResourceBarriers(1, &to_render_target);

        command_list.ClearRenderTarget(swapchain_rtv, glm::vec4(0.3f, 0.3f, 0.3f, 0.0f));
        command_list.ClearDepthStencil(dsv, 1.0f, 0);

        command_list.Get()->OMSetRenderTargets(1, &swapchain_rtv, false, &dsv);
    }

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

    {
        for (const auto& [mat, mesh] : models_to_render)
        {

            // Set model matrix
            {
                ModelMatrixData data {};
                data.mModel = glm::identity<glm::mat4>();
                data.mTransposed = glm::identity<glm::mat4>();

                detail::WriteResource(model_matrix_data, data);
                command_list.BindRootCBV(model_matrix_data, shader_inputs.GetInputIndex("model_matrix").value());
            }

            // Set Input Assembly
            {
                D3D12_VERTEX_BUFFER_VIEW vertex_views[1] = {
                    { mesh->position.GetAddress(), mesh->position.GetDimensions().x, sizeof(glm::vec3) }
                };

                command_list.Get()->IASetVertexBuffers(0, 1, vertex_views);

                D3D12_INDEX_BUFFER_VIEW index_view {};
                index_view.BufferLocation = mesh->indices.GetAddress();
                index_view.SizeInBytes = mesh->indices.GetDimensions().x;
                index_view.Format = DXGI_FORMAT_R32_UINT;

                command_list.Get()->IASetIndexBuffer(&index_view);
                command_list.Get()->DrawIndexedInstanced(mesh->index_count, 1, 0, 0, 0);
            }
        }
    }

    // Submit and Sync
    {
        // TODO: correctly implement frames in flight

        auto current_backbuffer = swapchain_target.GetBackbufferIndex();
        auto* swapchain_buffer_resource = swapchain_target.GetBufferResource(current_backbuffer);

        auto to_render_target = CD3DX12_RESOURCE_BARRIER::Transition(
            swapchain_buffer_resource,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);

        command_list.SetResourceBarriers(1, &to_render_target);

        command_queue.SubmitCommandList(std::move(command_list)).Wait();
        swapchain_target.SwapBuffers(true);
    }
}