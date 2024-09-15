#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace KS
{
class Device;
class SubRenderer;
class Shader;
class UniformBuffer;
struct RendererInitParams
{
    std::vector<std::shared_ptr<Shader>> shaders;
};

struct RendererRenderParams
{
    glm::mat4x4 projectionMatrix;
    glm::mat4x4 viewMatrix;
    glm::vec3 cameraPos;
    int cpuFrame;
};

class Renderer
{
public:
    Renderer(const Device& device, const RendererInitParams& params);
    ~Renderer();

    void Render(Device& device, const RendererRenderParams& params);

    std::vector<std::unique_ptr<SubRenderer>> m_subrenderers;

private:
    std::shared_ptr<UniformBuffer> m_camera_buffer;
};
}