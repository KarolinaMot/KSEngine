#pragma once
#include <Common.hpp>
#include <DXDevice.hpp>

#include <Geometry.hpp>
#include <display/DXSwapchain.hpp>
#include <resources/DXResource.hpp>
#include <shader/DXPipeline.hpp>
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

    void RenderFrame(const Camera& camera, DXDevice& device, DXSwapchain& swapchain_target);
    void QueueModel(const glm::mat4& transform) { models_to_render.emplace_back(transform); }

private:
    std::vector<glm::mat4> models_to_render;
    DXResource TestCube {};

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
};