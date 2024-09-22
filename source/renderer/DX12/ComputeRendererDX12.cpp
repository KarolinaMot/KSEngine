#include <renderer/ComputeRenderer.hpp>

KS::ComputeRenderer::ComputeRenderer(const Device& device, std::shared_ptr<Shader> shader)
    : SubRenderer(device, shader)
{
}

KS::ComputeRenderer::~ComputeRenderer()
{
}

void KS::ComputeRenderer::Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<DepthStencil> depthStencil, Texture** previoiusPassResults, int numTextures)
{
}