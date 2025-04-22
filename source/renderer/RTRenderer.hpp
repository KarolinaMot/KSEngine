#pragma once
#include "SubRenderer.hpp"

namespace KS
{
class UniformBuffer;
class Scene;
class Device;

struct RTShaderInfo
{
    size_t GPUAddress = 0;
    uint32_t RayGenSectionSize = 0;
    uint32_t RayGenEntrySize = 0;
    uint32_t MissSectionSize = 0;
    uint32_t MissEntrySize = 0;
    uint32_t HitGroupSectionSize = 0;
    uint32_t HitGroupEntrySize = 0;
};

class RTRenderer : public SubRenderer
{
public:
    RTRenderer(const Device& device, SubRendererDesc& desc, UniformBuffer* cameraBuffer);
    ~RTRenderer();
    void Render(Device& device, Scene& scene, std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs,
                bool clearRT) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::unique_ptr<UniformBuffer> m_frameIndex;
};
}  // namespace KS
