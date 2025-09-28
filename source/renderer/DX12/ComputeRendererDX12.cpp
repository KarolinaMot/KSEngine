#include <renderer/ComputeRenderer.hpp>
#include <renderer/RenderTarget.hpp>
#include <resources/Texture.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <device/Device.hpp>

KS::ComputeRenderer::ComputeRenderer(const Device& device, SubRendererDesc& desc) : SubRenderer(device, desc) {}

KS::ComputeRenderer::~ComputeRenderer() {}

void KS::ComputeRenderer::Render(Device& device, DXCommandContext* commandContext, Scene&,
                                 std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs, bool clearRT)
{
    auto& commandList = commandContext->m_commandList;
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(m_shader->GetShaderInput()->GetSignature()), true);
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

    if (clearRT)
    {
        m_renderTarget->PrepareToRenderTo(device, *commandList);
        m_renderTarget->Bind(device, *commandList, m_depthStencil.get());
        m_renderTarget->Clear(device, *commandList);
    }
    m_renderTarget->GetTexture(device, 0)->Bind(device, *commandList, m_shader->GetShaderInput()->GetInput("PBRRes"));

    for (int i = 0; i < inputs.size(); i++)
    {
        inputs[i].first->Bind(device, *commandList, inputs[i].second);
    }

    uint32_t texWidth = m_renderTarget->GetTexture(device, 0)->GetWidth();
    uint32_t texHeight = m_renderTarget->GetTexture(device, 0)->GetHeight();
    commandList->DispatchShader(texWidth / 8, texHeight / 8, 1);
}