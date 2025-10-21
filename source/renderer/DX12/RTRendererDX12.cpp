#include "../RTRenderer.hpp"
#include <device/Device.hpp>
#include <fileio\ResourceHandle.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <resources/Texture.hpp>
#pragma warning(push, 0)
#include <DXR/DXRHelper.h>
#include <DXR/nv_helpers_dx12/BottomLevelASGenerator.h>
#include <DXR/nv_helpers_dx12/RaytracingPipelineGenerator.h>
#include <DXR/nv_helpers_dx12/RootSignatureGenerator.h>
#include <DXR/nv_helpers_dx12/ShaderBindingTableGenerator.h>
#include <DXR/nv_helpers_dx12/TopLevelASGenerator.h>

#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DXRTPipeline.hpp>
#include <renderer/DX12/Helpers/DXShaderTable.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/ShaderInputBlueprintBuilder.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <scene/Scene.hpp>

class KS::RTRenderer::Impl
{
public:
    std::unique_ptr<DXShaderTable> m_shaderTable[FRAME_BUFFER_COUNT];
};

KS::RTRenderer::RTRenderer(const Device& device, Scene& scene, SubRendererDesc& desc) : SubRenderer(device, desc)
{
    m_impl = std::make_unique<Impl>();
    auto engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());

    int32_t frameIndex = 0;

    auto heap = static_cast<DXDescHeap*>(device.GetResourceHeap());
    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle =
        static_cast<DXDescHeap*>(device.GetResourceHeap())->Get()->GetGPUDescriptorHandleForHeapStart();
    auto heapPointer = srvUavHeapHandle.ptr;

    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        m_impl->m_shaderTable[i] = std::make_unique<DXShaderTable>();
        m_impl->m_shaderTable[i]->AddMiss(L"Miss");

        D3D12_GPU_DESCRIPTOR_HANDLE outputHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
        outputHandle.ptr += static_cast<uint64_t>((RAYTRACE_RT_SLOT + i) * heap->GetDescriptorSize());

        D3D12_GPU_DESCRIPTOR_HANDLE tlasHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
        tlasHandle.ptr += static_cast<uint64_t>((BVH_SLOT + i) * heap->GetDescriptorSize());

       D3D12_GPU_DESCRIPTOR_HANDLE materialHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
        materialHandle.ptr += static_cast<uint64_t>(scene.GetStorageBuffer(MATERIAL_INFO_BUFFER)->GetHandle(true)) * heap->GetDescriptorSize();

        std::vector<void*> heapPointers(4);
        heapPointers[0] = reinterpret_cast<void*>(outputHandle.ptr);
        heapPointers[1] = reinterpret_cast<void*>(tlasHandle.ptr);
        heapPointers[2] = reinterpret_cast<void*>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER)->GetGPUAddress(0, i));
        heapPointers[3] = reinterpret_cast<void*>(materialHandle.ptr);

        m_impl->m_shaderTable[i]->AddRayGen(L"RayGen", heapPointers.data(),
                                            static_cast<UINT>(sizeof(void*) * heapPointers.size()));

        m_impl->m_shaderTable[i]->AddHitGroup(L"HitGroup", heapPointers.data(),
                                              static_cast<UINT>(sizeof(void*) * heapPointers.size()));

        auto pipeline = reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline());
        m_impl->m_shaderTable[i]->Build(engineDevice, pipeline->m_stateObjectProps.Get());
    }
}

KS::RTRenderer::~RTRenderer() {}

void KS::RTRenderer::Render(Device& device, DXCommandContext* commandContext, Scene& scene,
                            std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>& inputs, bool clearRT)
{
    auto commandList = commandContext->m_commandList.get();
    auto cpuFrameIndex = device.GetCPUFrameIndex();

    m_renderTarget->GetTexture(cpuFrameIndex, 0)->TransitionToRW(device, *commandList);

     D3D12_DISPATCH_RAYS_DESC desc =
         m_impl->m_shaderTable[cpuFrameIndex]->FillDispatchDesc(device.GetWidth(), device.GetHeight(), 1);
     auto pipeline = reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline())->m_pipeline.Get();

    // Bind the raytracing pipeline
     commandList->GetCommandList()->SetPipelineState1(pipeline);
    // Dispatch the rays and write to the raytracing output
     commandList->GetCommandList()->DispatchRays(&desc);
    
     m_frameCount++;
}