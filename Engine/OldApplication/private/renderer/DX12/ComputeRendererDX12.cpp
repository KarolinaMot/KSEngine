#include <Device.hpp>
#include <renderer/ComputeRenderer.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputs.hpp>
#include <resources/Texture.hpp>

ComputeRenderer::ComputeRenderer(const Device& device, std::shared_ptr<Shader> shader)
    : SubRenderer(device, shader)
{
}

ComputeRenderer::~ComputeRenderer()
{
}

void ComputeRenderer::Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<DepthStencil> depthStencil, Texture** previoiusPassResults, int numTextures)
{
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);
    renderTarget->GetTexture(device, 0)->Bind(device, m_shader->GetShaderInput()->GetInput("PBRRes").rootIndex, false);

    for (int i = 0; i < numTextures; i++)
    {
        previoiusPassResults[i]->Bind(device, m_shader->GetShaderInput()->GetInput("GBuffer" + std::to_string(i + 1)).rootIndex, false);
    }

    commandList->DispatchShader(static_cast<uint32_t>(device.GetWidth() / 8), static_cast<uint32_t>(device.GetHeight() / 8), 1);
}