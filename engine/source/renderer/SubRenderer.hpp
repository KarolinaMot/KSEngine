#pragma once
#include <string>
#include <memory>
#include "../../../external/glm/glm.hpp"

namespace KS
{
    struct SubRendererInitParams
    {
        std::string shader;
    };

    class Device;
    class Shader;
    class SubRenderer
    {
    public:
        SubRenderer(const Device &device, std::shared_ptr<Shader> shader) : m_shader(shader) {};
        virtual ~SubRenderer() = default;
        virtual void Render(const Device &device, int cpuFrameIndex) = 0;
        const Shader *GetShader() const { return m_shader.get(); }

    protected:
        std::shared_ptr<Shader> m_shader;
    };
}