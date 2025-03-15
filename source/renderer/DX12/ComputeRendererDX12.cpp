#include <renderer/ComputeRenderer.hpp>
#include <renderer/RenderTarget.hpp>
#include <resources/Texture.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <device/Device.hpp>

KS::ComputeRenderer::ComputeRenderer(const Device& device, std::shared_ptr<Shader> shader,
                                     std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs)
    : SubRenderer(device, shader, inputs)
{
}

KS::ComputeRenderer::~ComputeRenderer()
{
}

void KS::ComputeRenderer::Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<DepthStencil> depthStencil, Texture** previoiusPassResults, int numTextures)
{
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);
    renderTarget->GetTexture(device, 0)->Bind(device, m_shader->GetShaderInput()->GetInput("PBRRes"));

    for (int i = 0; i < numTextures; i++)
    {
        previoiusPassResults[i]->Bind(device, m_shader->GetShaderInput()->GetInput("GBuffer" + std::to_string(i + 1)));
    }

    commandList->DispatchShader(static_cast<uint32_t>(device.GetWidth() / 8), static_cast<uint32_t>(device.GetHeight() / 8), 1);
}