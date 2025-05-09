#include "../RTRenderer.hpp"

#include <vector>

#include <device/Device.hpp>

#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXRTPipeline.hpp>
#include <renderer/UniformBuffer.hpp>
#include <renderer/Shader.hpp>
#include <DXR/DXRHelper.h>
#include <DXR/nv_helpers_dx12/ShaderBindingTableGenerator.h>
#include <scene/Scene.hpp>

class KS::RTRenderer::Impl
{
public:
    struct HitInfo
    {
        glm::vec4 colorAndDistance;
    };

    nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_sbtStorage[2];
    RTShaderInfo m_SBTinfo[2];
};

KS::RTRenderer::RTRenderer(const Device& device, SubRendererDesc& desc, UniformBuffer* cameraBuffer)
    : SubRenderer(device, desc)
{
    m_impl = std::make_unique<Impl>();
    int frameIndex = 0;
    m_frameIndex = std::make_unique<UniformBuffer>(device, "FRAME INDEX BUFFER", frameIndex, 1);

    ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());

    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle =
        static_cast<DXDescHeap*>(device.GetResourceHeap())->Get()->GetGPUDescriptorHandleForHeapStart();
    auto heapPointer = srvUavHeapHandle.ptr;

    for (int i = 1; i > -1; i--)
    {
        // The ray generation only uses heap data
        std::vector<void*> heapPointers(3);
        heapPointers[0] = reinterpret_cast<void*>(heapPointer);
        heapPointers[1] = reinterpret_cast<void*>(m_frameIndex->GetGPUAddress(0, i));
        heapPointers[2] = reinterpret_cast<void*>(cameraBuffer->GetGPUAddress(0, i));

        m_impl->m_sbtHelper[i].AddRayGenerationProgram(L"RayGen", heapPointers);

       // The miss and hit shaders do not access any external resources: instead they
        // communicate their results through the ray payload
        m_impl->m_sbtHelper[i].AddMissProgram(L"Miss", heapPointers);

        // Adding the triangle hit shader
        m_impl->m_sbtHelper[i].AddHitGroup(L"HitGroup", {});
        uint32_t sbtSize = m_impl->m_sbtHelper[i].ComputeSBTSize();

        m_impl->m_sbtStorage[i] =
            nv_helpers_dx12::CreateBuffer(engineDevice, sbtSize, D3D12_RESOURCE_FLAG_NONE,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
        if (!m_impl->m_sbtStorage[i])
        {
            throw std::logic_error("Could not allocate the shader binding table");
        }

       m_impl->m_sbtHelper[i].Generate(m_impl->m_sbtStorage[i].Get(),
                                        reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline())->m_stateObjectProps.Get());
    }

    for (int i = 0; i < 2; i++)
    {
        auto& sbtInfo = m_impl->m_SBTinfo[i];
        auto& sbtHelper = m_impl->m_sbtHelper[i];
        sbtInfo.GPUAddress = m_impl->m_sbtStorage[i]->GetGPUVirtualAddress();
        sbtInfo.HitGroupEntrySize = sbtHelper.GetHitGroupEntrySize();
        sbtInfo.HitGroupSectionSize = sbtHelper.GetHitGroupSectionSize();
        sbtInfo.MissEntrySize = sbtHelper.GetMissEntrySize();
        sbtInfo.MissSectionSize = sbtHelper.GetMissSectionSize();
        sbtInfo.RayGenEntrySize = sbtHelper.GetRayGenEntrySize();
        sbtInfo.RayGenSectionSize = sbtHelper.GetRayGenSectionSize();
    }
}

KS::RTRenderer::~RTRenderer() {}

void KS::RTRenderer::Render(Device& device, Scene& scene, std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs,
                            bool clearRT)
{
    int i = 0;
    int cpuFrameIndex = device.GetFrameIndex();
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    auto& sbtInfo = m_impl->m_SBTinfo[cpuFrameIndex];

    m_frameIndex->Update(device, cpuFrameIndex);
    m_renderTarget->GetTexture(device, 0)->TransitionToRW(device);

    // Setup the raytracing task
    D3D12_DISPATCH_RAYS_DESC desc = {};
    // The layout of the SBT is as follows: ray generation shader, miss
    // shaders, hit groups. As described in the CreateShaderBindingTable method,
    // all SBT entries of a given type have the same size to allow a fixed stride.

    // The ray generation shaders are always at the beginning of the SBT.
    uint32_t rayGenerationSectionSizeInBytes = sbtInfo.RayGenSectionSize;
    desc.RayGenerationShaderRecord.StartAddress = sbtInfo.GPUAddress;
    desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

    // The miss shaders are in the second SBT section, right after the ray
    // generation shader. We have one miss shader for the camera rays and one
    // for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
    // also indicate the stride between the two miss shaders, which is the size
    // of a SBT entry
    uint32_t missSectionSizeInBytes = sbtInfo.MissSectionSize;
    desc.MissShaderTable.StartAddress = sbtInfo.GPUAddress + rayGenerationSectionSizeInBytes;
    desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
    desc.MissShaderTable.StrideInBytes = sbtInfo.MissEntrySize;

    // The hit groups section start after the miss shaders. In this sample we
    // have one 1 hit group for the triangle
    uint32_t hitGroupsSectionSize = sbtInfo.HitGroupSectionSize;
    desc.HitGroupTable.StartAddress = sbtInfo.GPUAddress + rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
    desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
    desc.HitGroupTable.StrideInBytes = sbtInfo.HitGroupEntrySize;

    // Dimensions of the image to render, identical to a kernel launch dimension
    desc.Width = device.GetWidth();
    desc.Height = device.GetHeight();
    desc.Depth = 1;

    // Bind the raytracing pipeline
    commandList->GetCommandList()->SetPipelineState1(
        reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline())->m_pipeline.Get());
    // Dispatch the rays and write to the raytracing output
    commandList->GetCommandList()->DispatchRays(&desc);
}
