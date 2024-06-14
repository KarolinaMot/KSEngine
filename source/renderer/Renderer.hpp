#pragma once
#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace KS
{
    class Device;
    class SubRenderer;
    class Shader;
    struct RendererInitParams
    {
        std::vector<std::shared_ptr<Shader>> shaders;
    };

    struct RendererRenderParams
    {
        glm::mat4x4 projectionMatrix;
        glm::mat4x4 viewMatrix;
        int cpuFrame;
    };

    class Renderer
    {
    public:
        Renderer(const Device &device, const RendererInitParams &params);
        ~Renderer();

        void Render(const Device &device, const RendererRenderParams &params);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
        std::vector<std::unique_ptr<SubRenderer>> m_subrenderers;
    };
}