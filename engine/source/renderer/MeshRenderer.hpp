#pragma once
#include <memory>
#include "SubRenderer.hpp"
#include <glm/glm.hpp>

namespace KS
{
    struct ModelMat
    {
        glm::mat4 mModel;
        glm::mat4 mTransposed;
    };

    class Device;
    class Shader;
    class
        MeshRenderer : public SubRenderer
    {
    public:
        MeshRenderer(const Device &device, std::shared_ptr<Shader> shader);
        ~MeshRenderer();
        void Render(const Device &device, int cpuFrameIndex) override;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace KS
