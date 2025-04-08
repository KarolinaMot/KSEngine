#include <renderer/ComputeRenderer.hpp>
#include <renderer/RenderTarget.hpp>
#include <resources/Texture.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <device/Device.hpp>

KS::ComputeRenderer::ComputeRenderer(const Device& device, std::shared_ptr<Shader> shader)
    : SubRenderer(device, shader)
{
}

KS::ComputeRenderer::~ComputeRenderer()
{
}

void KS::ComputeRenderer::Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget,
                                      std::shared_ptr<DepthStencil> depthStencil,
                                 std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs, bool clearRenderTarget)
{
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);

    if (clearRenderTarget)
    {
        renderTarget->Bind(device, depthStencil.get());
        renderTarget->Clear(device);
    }
    renderTarget->GetTexture(device, 0)->Bind(device, m_shader->GetShaderInput()->GetInput("PBRRes"));

    for (int i = 0; i < inputs.size(); i++)
    {
        inputs[i].first->Bind(device, inputs[i].second);
    }

    uint32_t texWidth = renderTarget->GetTexture(device, 0)->GetWidth();
    uint32_t texHeight = renderTarget->GetTexture(device, 0)->GetHeight();
    commandList->DispatchShader(texWidth / 8, texHeight / 8, 1);
}