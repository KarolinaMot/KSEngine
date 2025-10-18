#include <device/Device.hpp>
#include <renderer/ComputeRenderer.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <resources/Texture.hpp>

KS::ComputeRenderer::ComputeRenderer(const Device& device, SubRendererDesc& desc) : SubRenderer(device, desc) {}

KS::ComputeRenderer::~ComputeRenderer() {}

void KS::ComputeRenderer::Render(Device& device, DXCommandContext* commandContext, Scene& scene,
                                 std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>& inputs, bool clearRT)
{
    auto& commandList = commandContext->m_commandList;

    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(m_shader->GetShaderInput()->GetSignature()), true);
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);
    auto frameIndex = device.GetCPUFrameIndex();

    for (int i = 0; i < inputs.size(); i++)
    {
        auto& input = inputs[i];
        if (input.first)
            input.first->Bind(device, *commandList, input.second.desc, input.second.bindOffset);
        else
            LOG(Log::Severity::WARN, "One of the inputs {} in a compute renderer was empty command will be ignored", i);
    }

    uint32_t texWidth = 0;
    uint32_t texHeight = 0;

    if (m_renderTarget)
    {
        if (clearRT)
        {
            m_renderTarget->Bind(*commandList, frameIndex, m_depthStencil.get());
            m_renderTarget->Clear(*commandList, frameIndex);
        }
        m_renderTarget->GetTexture(frameIndex, 0)->Bind(device, *commandList, m_shader->GetShaderInput()->GetInput("PBRRes"));
        texWidth = m_renderTarget->GetTexture(frameIndex, 0)->GetWidth();
        texHeight = m_renderTarget->GetTexture(frameIndex, 0)->GetHeight();
    }
    else
    {
        texWidth = m_dispatchWidth;
        texHeight = m_dispatchHeight;
    }
    
    if (texWidth == 0 || texHeight == 0)
    {
        LOG(Log::Severity::WARN, "Trying to dispatch compute shader with invalid dispatch sizes. Did you forget to assign a render target or a dispatch size? Command ignored.");
        return;
    }
    commandList->DispatchShader(texWidth / 8, texHeight / 8, 1);
}
