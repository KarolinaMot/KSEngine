#pragma once
#include <memory>
#include <glm/glm.hpp>
#include <vector>
#include <renderer/InfoStructs.hpp>

namespace KS
{
class Device;
class SubRenderer;
class DepthStencil;
class RenderTarget;
class ShaderInputCollection;
class ShaderInput;
class UniformBuffer;
class Scene;
struct ShaderInputDesc;

struct RenderTickParams
{
    glm::mat4x4 projectionMatrix;
    glm::mat4x4 viewMatrix;
    glm::vec3 cameraPos;
    glm::vec3 cameraRight;
    int cpuFrame;
};

class Renderer
{
public:
    Renderer(Device& device);
    ~Renderer();

    void Render(Device& device, Scene& scene, const RenderTickParams& params, bool raytraced = false);

private:
    std::shared_ptr<ShaderInputCollection> m_mainInputs;
    std::shared_ptr<ShaderInputCollection> m_rtInputs;
    std::unique_ptr<SubRenderer> m_subrenderers[NUM_SUBRENDER];
    std::shared_ptr<RenderTarget> m_renderTargets[NUM_SUBRENDER];
    std::vector<std::pair<ShaderInput*, ShaderInputDesc>> m_inputs[NUM_SUBRENDER];
    std::shared_ptr<DepthStencil> m_deferredRendererDepthStencil;
    std::shared_ptr<UniformBuffer> m_camera_buffer;
};
}  // namespace KS