#pragma once
#include <memory>
#include "SubRenderer.hpp"
#include <glm/glm.hpp>

namespace KS
{
    class Buffer;
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
        void Render(Device& device, int cpuFrameIndex) override;

    private:
        std::shared_ptr<Buffer> mQuadVResource;
        std::shared_ptr<Buffer> mQuadUVResource;
        std::shared_ptr<Buffer> mIndicesResource;

        std::shared_ptr<Buffer> mModelMatBuffer;
        std::shared_ptr<Buffer> mColorBuffer;
    };
} // namespace KS
