#include <Common.hpp>
#include <DXDevice.hpp>

#include <DXDevice.hpp>
#include <shader/DXPipeline.hpp>
#include <shader/DXShaderCompiler.hpp>
#include <shader/DXShaderInputs.hpp>

#include <Geometry.hpp>
#include <display/DXSwapchain.hpp>
#include <glm/vec2.hpp>
#include <rendering/GPUMesh.hpp>

class ForwardRenderer
{
public:
    ForwardRenderer(DXDevice& device, DXShaderCompiler& shader_compiler, glm::uvec2 screen_size);
    ~ForwardRenderer() = default;

    NON_COPYABLE(ForwardRenderer);
    NON_MOVABLE(ForwardRenderer);

    void RenderFrame(const Camera& camera, DXDevice& device, DXSwapchain& swapchain_target);
    void QueueModel(const glm::mat4& transform, const GPUMesh* mesh) { models_to_render.emplace_back(transform, mesh); }

private:
    std::vector<std::pair<glm::mat4, const GPUMesh*>> models_to_render;

    DXResource triangle_verts {};
    DXResource triangle_indices {};

    // Pipelines
    DXShaderInputs shader_inputs {};
    DXPipeline pipeline {};

    // Uniforms
    DXResource camera_data {};
    DXResource model_matrix_data {};

    // RenderTargets
    DXDescriptorHeap<DSV> depth_heap {};
    DXResource depth_stencil {};
};
