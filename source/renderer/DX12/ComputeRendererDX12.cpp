#include <renderer/ComputeRenderer.hpp>
#include <renderer/RenderTarget.hpp>
#include <resources/Texture.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <device/Device.hpp>

KS::ComputeRenderer::ComputeRenderer(const Device& device, SubRendererDesc& desc) : SubRenderer(device, desc) {}

KS::ComputeRenderer::~ComputeRenderer() {}

void KS::ComputeRenderer::Render(Device& device, Scene& scene, std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs,
                                 bool clearRT)
{
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);

    if (clearRT)
    {
        m_renderTarget->Bind(device, m_depthStencil.get());
        m_renderTarget->Clear(device);
    }
    m_renderTarget->GetTexture(device, 0)->Bind(device, m_shader->GetShaderInput()->GetInput("PBRRes"));

    for (int i = 0; i < inputs.size(); i++)
    {
        inputs[i].first->Bind(device, inputs[i].second);
    }

    uint32_t texWidth = m_renderTarget->GetTexture(device, 0)->GetWidth();
    uint32_t texHeight = m_renderTarget->GetTexture(device, 0)->GetHeight();
    commandList->DispatchShader(texWidth / 8, texHeight / 8, 1);
}