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
    int frameIndex = 0;
    m_frameIndex = std::make_unique<UniformBuffer>(device, "FRAME INDEX BUFFER", frameIndex, 1);
    auto heap = static_cast<DXDescHeap*>(device.GetResourceHeap());
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    DXRTPipeline* pipeline = reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline());

    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        m_impl->m_shaderTable[i] = std::make_unique<DXShaderTable>();
        m_impl->m_shaderTable[i]->AddHitGroup(L"HitGroup");
        m_impl->m_shaderTable[i]->AddMiss(L"Miss");

        int frame = i;
        uint32_t texUAVIndex = desc.renderTarget->GetTexture(frame, 0)->GetHandleIndex(false);

        D3D12_GPU_DESCRIPTOR_HANDLE outputHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
        outputHandle.ptr += static_cast<uint64_t>(texUAVIndex * heap->GetDescriptorSize());

        D3D12_GPU_DESCRIPTOR_HANDLE tlasHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
        tlasHandle.ptr += static_cast<uint64_t>((BVH_SLOT + frame) * heap->GetDescriptorSize());

        std::vector<void*> heapPointers(3);
        heapPointers[0] = reinterpret_cast<void*>(cameraBuffer->GetGPUAddress(0, frame));
        heapPointers[1] = reinterpret_cast<void*>(tlasHandle.ptr);
        heapPointers[2] = reinterpret_cast<void*>(outputHandle.ptr);
        m_impl->m_shaderTable[i]->AddRayGen(L"RayGen", &heapPointers[0], sizeof(void*)*3);

        m_impl->m_shaderTable[i]->Build(engineDevice, pipeline->m_stateObjectProps.Get());
    }
}

KS::RTRenderer::~RTRenderer() {}

void KS::RTRenderer::Render(Device& device, DXCommandContext* commandContext, Scene&,
                            std::vector<std::pair<ShaderInput*, ShaderInputDesc>>&, bool)
{
    //int i = 0;
    int cpuFrameIndex = device.GetFrameIndex();
    auto& commandList = commandContext->m_commandList;
    auto width = m_renderTarget->GetWidth();
    auto height = m_renderTarget->GetHeight();
    auto pipeline = reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline())->m_pipeline.Get();
    m_frameIndex->Update(device, cpuFrameIndex);

    auto desc = m_impl->m_shaderTable[cpuFrameIndex]->FillDispatchDesc(width, height);

    // Bind the raytracing pipeline
    commandList->GetCommandList()->SetPipelineState1(pipeline);

    // Dispatch the rays and write to the raytracing output
    commandList->GetCommandList()->DispatchRays(&desc);

    //auto& sbtInfo = m_impl->m_SBTinfo[cpuFrameIndex];


    //m_renderTarget->GetTexture(device, cpuFrameIndex)->TransitionToRW(device, *commandList);

    //// Setup the raytracing task
    //D3D12_DISPATCH_RAYS_DESC desc = {};
    //// The layout of the SBT is as follows: ray generation shader, miss
    //// shaders, hit groups. As described in the CreateShaderBindingTable method,
    //// all SBT entries of a given type have the same size to allow a fixed stride.

    //// The ray generation shaders are always at the beginning of the SBT.
    //uint32_t rayGenerationSectionSizeInBytes = sbtInfo.RayGenSectionSize;
    //desc.RayGenerationShaderRecord.StartAddress = sbtInfo.GPUAddress;
    //desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

    //// The miss shaders are in the second SBT section, right after the ray
    //// generation shader. We have one miss shader for the camera rays and one
    //// for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
    //// also indicate the stride between the two miss shaders, which is the size
    //// of a SBT entry
    //uint32_t missSectionSizeInBytes = sbtInfo.MissSectionSize;
    //desc.MissShaderTable.StartAddress = sbtInfo.GPUAddress + rayGenerationSectionSizeInBytes;
    //desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
    //desc.MissShaderTable.StrideInBytes = sbtInfo.MissEntrySize;

    //// The hit groups section start after the miss shaders. In this sample we
    //// have one 1 hit group for the triangle
    //uint32_t hitGroupsSectionSize = sbtInfo.HitGroupSectionSize;
    //desc.HitGroupTable.StartAddress = sbtInfo.GPUAddress + rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
    //desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
    //desc.HitGroupTable.StrideInBytes = sbtInfo.HitGroupEntrySize;

    //// Dimensions of the image to render, identical to a kernel launch dimension
    //desc.Width = device.GetWidth();
    //desc.Height = device.GetHeight();
    //desc.Depth = 1;
}
