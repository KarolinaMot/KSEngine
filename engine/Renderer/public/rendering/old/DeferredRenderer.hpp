#pragma once
#include <Common.hpp>
#include <DXDevice.hpp>

#include <Geometry.hpp>
#include <display/DXSwapchain.hpp>
#include <gpu_resources/DXResource.hpp>
#include <rendering/GPUMesh.hpp>
#include <shader/DXPipeline.hpp>
#include <shader/DXShaderCompiler.hpp>
#include <shader/DXShaderInputs.hpp>
#include <sync/DXFuture.hpp>

class DeferredRenderer
{
public:
    DeferredRenderer(DXDevice& device, DXShaderCompiler& shader_compiler, glm::uvec2 screen_size);
    ~DeferredRenderer() = default;

    NON_COPYABLE(DeferredRenderer);
    NON_MOVABLE(DeferredRenderer);

    void RenderFrame(const Camera& camera, DXDevice& device, DXSwapchain& swapchain_target);
    void QueueModel(const glm::mat4& transform, const GPUMesh* mesh) { models_to_render.emplace_back(transform, mesh); }

private:
    std::vector<std::pair<glm::mat4, const GPUMesh*>> models_to_render;

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
