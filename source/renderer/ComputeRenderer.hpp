#pragma once
#include "SubRenderer.hpp"

namespace KS
{
class ComputeRenderer : public SubRenderer
{
public:
    ComputeRenderer(const Device& device, std::shared_ptr<Shader> shader);
    ~ComputeRenderer();

    void Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget,
                std::shared_ptr<DepthStencil> depthStencil,
                std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs) override;
private:
};
} // namespace KS
