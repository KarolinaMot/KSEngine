#pragma once
#include <memory>
#pragma warning(push, 0)
#include <glm/glm.hpp>
#pragma warning(pop)
#include <renderer/InfoStructs.hpp>
#include <vector>

namespace KS
{
class Device;
class SubRenderer;
class DepthStencil;
class RenderTarget;
class ShaderInputBlueprint;
class ShaderInput;
struct ShaderInputBindDesc;
class Shader;
class Texture;
class UniformBuffer;
class Scene;

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

    void Render(Device& device, Scene& scene, const RenderTickParams& params, bool raytraced = false, bool recompileShaders = false);

private:
    void GodRays(Device& device, Scene& scene);
    void Main(Device& device, Scene& scene);
    void GenerateMipmaps(Device& device, Scene& scene);
    void Raytrace(Device& device, Scene& scene);
    void GenCubemap(Device& device, Scene& scene);
    void RenderCubemap(Device& device, Scene& scene);

    std::shared_ptr<ShaderInputBlueprint> m_mainInputs;
    std::shared_ptr<ShaderInputBlueprint> m_rtInputs;
    std::unique_ptr<SubRenderer> m_subrenderers[NUM_SUBRENDER];
    std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>> m_inputs[NUM_SUBRENDER];
    std::shared_ptr<ShaderInputBlueprint> m_mipMapShaderInputs;
};
}  // namespace KS