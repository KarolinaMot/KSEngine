#include <device/Device.hpp>
#include <scene/Scene.hpp>
#include <renderer/ComputeRenderer.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <resources/Texture.hpp>

KS::ComputeRenderer::ComputeRenderer(const Device& device, std::shared_ptr<Shader>& shader) : SubRenderer(device, shader) {}

KS::ComputeRenderer::~ComputeRenderer() {}

void KS::ComputeRenderer::Render(Device& device, DXCommandContext* commandContext, RenderParameters& par)
{
    auto& commandList = commandContext->m_commandList;

    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(m_shader->GetShaderInput()->GetSignature()), true);
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(par.scene->GetResourceHeap());
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);
    auto frameIndex = device.GetFrameIndex();
    auto prevFrameIndex = device.GetPrevFrameIndex();

    for (int i = 0; i < par.inputs->size(); i++)
    {
        auto& input = (*par.inputs)[i];
        if (input.first)
            input.first->Bind(input.second.prev ? prevFrameIndex : frameIndex, resourceHeap, *commandList, input.second.desc,
                              input.second.bindOffset);
        else
            LOG(Log::Severity::WARN, "One of the inputs {} in a compute renderer was empty command will be ignored", i);
    }


    if (par.rt)
    {
        if (par.clearRt)
        {
            par.rt->Bind(*commandList, frameIndex, par.ds.get());
            par.rt->Clear(*commandList, frameIndex);
        }
        par.rt->GetTexture(frameIndex, 0)->Bind(frameIndex, resourceHeap, *commandList, m_shader->GetShaderInput()->GetInput("compute_res"));
        m_dispatchWidth = par.rt->GetTexture(frameIndex, 0)->GetWidth();
        m_dispatchHeight = par.rt->GetTexture(frameIndex, 0)->GetHeight();
    }
    
    if (m_dispatchWidth == 0 || m_dispatchHeight == 0 || m_dispatchDepth == 0)
    {
        LOG(Log::Severity::WARN,
            "Trying to dispatch compute shader with invalid dispatch sizes [{}, {}, {}]. Did you forget to assign a render "
            "target or a dispatch size? Command ignored.",
            m_dispatchWidth, m_dispatchHeight, m_dispatchDepth);
        return;
    }

    uint32_t dispatchWidth = (m_dispatchWidth + 8u - 1u) / 8u;
    uint32_t dispatchHeight = (m_dispatchHeight + 8u - 1u) / 8u;
    commandList->DispatchShader(dispatchWidth, dispatchHeight, m_dispatchDepth);
}
