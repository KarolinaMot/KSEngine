#pragma once
#include "SubRenderer.hpp"

class ComputeRenderer : public SubRenderer
{
public:
    ComputeRenderer(const Device& device, std::shared_ptr<Shader> shader);
    ~ComputeRenderer();

    void Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<DepthStencil> depthStencil, Texture** previoiusPassResults = nullptr, int numTextures = 0) override;

private:
};
