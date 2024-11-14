#pragma once
#include <Common.hpp>
#include <DXDevice.hpp>

#include <Geometry.hpp>
#include <display/DXSwapchain.hpp>
#include <gpu_resources/DXResource.hpp>
#include <shader/DXPipeline.hpp>
#include <shader/DXShaderCompiler.hpp>
#include <shader/DXShaderInputs.hpp>
#include <sync/DXFuture.hpp>

struct Mesh
{
    DXResource position {};
    DXResource normals {};
    DXResource uvs {};
    DXResource tangents {};
    DXResource indices{};
    size_t index_count{};
};

class Renderer
{
public:
    Renderer(DXDevice& device, DXShaderCompiler& shader_compiler, glm::uvec2 screen_size);
    ~Renderer() = default;

    NON_COPYABLE(Renderer);
    NON_MOVABLE(Renderer);

    void RenderFrame(const Camera& camera, DXDevice& device, DXSwapchain& swapchain_target);
    void QueueModel(const glm::mat4& transform, const Mesh* mesh) { models_to_render.emplace_back(transform, mesh); }

private:
    std::vector<std::pair<glm::mat4, const Mesh*>> models_to_render;

    // Pipelines

    DXShaderInputs shader_inputs {};
    DXPipeline graphics_deferred_pipeline {};
    DXPipeline compute_pbr_pipeline {};

    // Uniforms

    DXResource camera_data {};
    DXResource model_matrix_data {};
    DXResource material_info_data {};
    DXResource light_data {};

    // RenderTargets

    DXDescriptorHeap<DSV> depth_heap {};
    DXResource depth_stencil {};

    DXDescriptorHeap<RTV> render_target_heap {};
    DXResource position_rt {};
    DXResource albedo_rt {};
    DXResource normal_rt {};
    DXResource emissive_rt {};

    DXDescriptorHeap<UAV> unordered_access_heap {};
    DXResource result_frame {};
};
