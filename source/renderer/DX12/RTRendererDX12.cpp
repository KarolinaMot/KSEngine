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
#include <resources/Skydome.hpp>
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

        std::vector<void*> heapPointers;
        heapPointers.reserve(15);

        if (!m_shader->GetShaderInput())
        {
            D3D12_GPU_DESCRIPTOR_HANDLE outputHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            outputHandle.ptr += static_cast<uint64_t>((RAYTRACE_RT_SLOT + i) * heap->GetDescriptorSize());

            D3D12_GPU_DESCRIPTOR_HANDLE tlasHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            tlasHandle.ptr += static_cast<uint64_t>((BVH_SLOT + i) * heap->GetDescriptorSize());

            D3D12_GPU_DESCRIPTOR_HANDLE materialHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            materialHandle.ptr += static_cast<uint64_t>(scene.GetStorageBuffer(MATERIAL_INFO_BUFFER)->GetHandle(true)) *
                                  heap->GetDescriptorSize();

            D3D12_GPU_DESCRIPTOR_HANDLE normalsHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            normalsHandle.ptr += static_cast<uint64_t>(NORMALS_SLOT) * heap->GetDescriptorSize();

            D3D12_GPU_DESCRIPTOR_HANDLE modelMatHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            modelMatHandle.ptr +=
                static_cast<uint64_t>(scene.GetStorageBuffer(MODEL_MAT_BUFFER)->GetHandle(true)) * heap->GetDescriptorSize();

            D3D12_GPU_DESCRIPTOR_HANDLE dirLights = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            dirLights.ptr +=
                static_cast<uint64_t>(scene.GetStorageBuffer(DIR_LIGHT_BUFFER)->GetHandle(true)) * heap->GetDescriptorSize();

            D3D12_GPU_DESCRIPTOR_HANDLE pointLights = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            pointLights.ptr +=
                static_cast<uint64_t>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER)->GetHandle(true)) * heap->GetDescriptorSize();

            D3D12_GPU_DESCRIPTOR_HANDLE textures = heap->Get()->GetGPUDescriptorHandleForHeapStart();

            D3D12_GPU_DESCRIPTOR_HANDLE indexHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            indexHandle.ptr += static_cast<uint64_t>(INDICES_SLOT) * heap->GetDescriptorSize();

            D3D12_GPU_DESCRIPTOR_HANDLE vPosHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            vPosHandle.ptr += static_cast<uint64_t>(VPOS_SLOT) * heap->GetDescriptorSize();

            D3D12_GPU_DESCRIPTOR_HANDLE uvHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            uvHandle.ptr += static_cast<uint64_t>(UVS_SLOT) * heap->GetDescriptorSize();

            D3D12_GPU_DESCRIPTOR_HANDLE tanHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            tanHandle.ptr += static_cast<uint64_t>(TAN_SLOT) * heap->GetDescriptorSize();

            auto skyboxHandleID = scene.GetSkydome().first->GetHandleIndex(true);
            D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
            skyboxHandle.ptr += static_cast<uint64_t>(skyboxHandleID) * heap->GetDescriptorSize();

            heapPointers.push_back(reinterpret_cast<void*>(outputHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(tlasHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER)->GetGPUAddress(0, i)));
            heapPointers.push_back(reinterpret_cast<void*>(materialHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(modelMatHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(dirLights.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(pointLights.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(skyboxHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(normalsHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(indexHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(vPosHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(uvHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(tanHandle.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(textures.ptr));
            heapPointers.push_back(reinterpret_cast<void*>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER)->GetGPUAddress(0, i)));
        }

        m_impl->m_shaderTable[i]->AddRayGen(L"RayGen", heapPointers.data(),
                                            static_cast<UINT>(sizeof(void*) * heapPointers.size()));

        m_impl->m_shaderTable[i]->AddHitGroup(L"HitGroup", heapPointers.data(),
                                              static_cast<UINT>(sizeof(void*) * heapPointers.size()));

        m_impl->m_shaderTable[i]->AddMiss(L"Miss", heapPointers.data(), static_cast<UINT>(sizeof(void*) * heapPointers.size()));

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

    //if (clearRT)
    //{
    //    m_renderTarget->Clear(*commandList, cpuFrameIndex);
    //}

    m_renderTarget->GetTexture(cpuFrameIndex, 0)->TransitionToRW(device, *commandList);

    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    auto shaderInputs = m_shader->GetShaderInput();

    if (shaderInputs)
    {
        commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(shaderInputs->GetSignature()), true);

        for (const auto& input : inputs)
        {
            input.first->Bind(device, *commandList, input.second.desc);
        }

        commandList->BindHeapSlot(*resourceHeap, NORMALS_SLOT, m_shader->GetShaderInput()->GetInput("normals").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, INDICES_SLOT, m_shader->GetShaderInput()->GetInput("indices").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, VPOS_SLOT, m_shader->GetShaderInput()->GetInput("vertexPos").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, UVS_SLOT, m_shader->GetShaderInput()->GetInput("uvs").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, TAN_SLOT, m_shader->GetShaderInput()->GetInput("tangents").rootIndex);
        commandList->BindHeapSlot(*resourceHeap, 0, m_shader->GetShaderInput()->GetInput("textures").rootIndex);
    }

     D3D12_DISPATCH_RAYS_DESC desc =
         m_impl->m_shaderTable[cpuFrameIndex]->FillDispatchDesc(device.GetSwapchainWidth(), device.GetSwapchainHeight(), 1);
     auto pipeline = reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline())->m_pipeline.Get();

    // Bind the raytracing pipeline
     commandList->GetCommandList()->SetPipelineState1(pipeline);
    // Dispatch the rays and write to the raytracing output
     commandList->GetCommandList()->DispatchRays(&desc);
    
     m_frameCount++;
}