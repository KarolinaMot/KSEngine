#include "../RTRenderer.hpp"

#include <vector>

#include <device/Device.hpp>

#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DXRTPipeline.hpp>
#include <renderer/UniformBuffer.hpp>
#include <renderer/Shader.hpp>
#include <scene/Scene.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <renderer/DX12/Helpers/DXShaderTable.h>

class KS::RTRenderer::Impl
{
public:
    struct HitInfo
    {
        glm::vec4 colorAndDistance;
    };

    std::unique_ptr<DXShaderTable> m_shaderTable[FRAME_BUFFER_COUNT];
    //nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper[2];
    //Microsoft::WRL::ComPtr<ID3D12Resource> m_sbtStorage[2];
    //RTShaderInfo m_SBTinfo[2];
};

KS::RTRenderer::RTRenderer(const Device& device, SubRendererDesc& desc, UniformBuffer* cameraBuffer)
    : SubRenderer(device, desc)
{

    m_impl = std::make_unique<Impl>();
    auto heap = static_cast<DXDescHeap*>(device.GetResourceHeap());
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    DXRTPipeline* pipeline = reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline());

    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        m_impl->m_shaderTable[i] = std::make_unique<DXShaderTable>();
        m_impl->m_shaderTable[i]->AddHitGroup(L"HitGroup");
        m_impl->m_shaderTable[i]->AddMiss(L"Miss");

        
        auto inputs = m_shader->GetShaderInput();
        std::vector<void*> heapPointers;

        if (!inputs->GetIsGlobal())
        {
            int frame = i;
            uint32_t texUAVIndex = desc.renderTarget->GetTexture(frame, 0)->GetHandleIndex(false);

            D3D12_GPU_DESCRIPTOR_HANDLE outputHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            outputHandle.ptr += static_cast<uint64_t>(texUAVIndex * heap->GetDescriptorSize());

            D3D12_GPU_DESCRIPTOR_HANDLE tlasHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            tlasHandle.ptr += static_cast<uint64_t>((BVH_SLOT + frame) * heap->GetDescriptorSize());

            heapPointers.push_back(reinterpret_cast<void*>(cameraBuffer->GetGPUAddress(0, frame)));
            heapPointers.push_back(reinterpret_cast<void*>(tlasHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(outputHandle.ptr));
        }

        m_impl->m_shaderTable[i]->AddRayGen(L"RayGen", heapPointers.data(), static_cast<UINT>(sizeof(void*) * heapPointers.size()));

        m_impl->m_shaderTable[i]->Build(engineDevice, pipeline->m_stateObjectProps.Get());
    }
}

KS::RTRenderer::~RTRenderer() {}

void KS::RTRenderer::Render(Device& device, DXCommandContext* commandContext, Scene&,
                            std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& shaderInputs, bool clearRT)
{
    int cpuFrameIndex = device.GetFrameIndex();
    auto& commandList = commandContext->m_commandList;
    auto width = m_renderTarget->GetWidth();
    auto height = m_renderTarget->GetHeight();
    auto pipeline = reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline())->m_pipeline.Get();
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    auto inputs = m_shader->GetShaderInput();

    if (inputs->GetIsGlobal())
    {
        commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(inputs->GetSignature()), true);

        commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

        for (const auto& input : shaderInputs)
        {
            input.first->Bind(device, *commandList, input.second);

            if (clearRT)
            {
                m_renderTarget->PrepareToRenderTo(device, *commandList);
                m_renderTarget->Bind(device, *commandList, m_depthStencil.get());
                m_renderTarget->Clear(device, *commandList);
            }
            m_renderTarget->GetTexture(cpuFrameIndex, 0)
                ->Bind(device, *commandList, m_shader->GetShaderInput()->GetInput("output"));
        }
    }

    auto desc = m_impl->m_shaderTable[cpuFrameIndex]->FillDispatchDesc(width, height);

    // Bind the raytracing pipeline
    commandList->GetCommandList()->SetPipelineState1(pipeline);

    // Dispatch the rays and write to the raytracing output
    commandList->GetCommandList()->DispatchRays(&desc);
}
