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
    struct ASBuffers
    {
        std::shared_ptr<DXResource> pScratch[2] = {nullptr, nullptr};
        std::shared_ptr<DXResource> pResult[2] = {nullptr, nullptr};
        std::shared_ptr<DXResource> pInstanceDesc[2] = {nullptr, nullptr};
    };
    struct HitInfo
    {
        glm::vec4 colorAndDistance;
    };

    nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
    std::unique_ptr<DXShaderTable> m_shaderTable[FRAME_BUFFER_COUNT];

    ASBuffers m_BLBuffers[200];
    std::pair<UINT, DirectX::XMMATRIX> m_instances[200];
    ASBuffers m_topLevelASBuffers;
    int m_BLCount = 0;
    DXHeapHandle m_BHVHandle[2];
    bool m_updateBVH = false;

    std::unique_ptr<UniformBuffer> m_frameIndex;
};

KS::RTRenderer::RTRenderer(const Device& device, SubRendererDesc& desc, UniformBuffer* cameraBuffer) : SubRenderer(device, desc)
{
    m_impl = std::make_unique<Impl>();
    auto engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());

    int32_t frameIndex = 0;
    m_impl->m_frameIndex = std::make_unique<UniformBuffer>(device, "FRAME INDEX BUFFER", frameIndex, 1);

    auto heap = static_cast<DXDescHeap*>(device.GetResourceHeap());
    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle =
        static_cast<DXDescHeap*>(device.GetResourceHeap())->Get()->GetGPUDescriptorHandleForHeapStart();
    auto heapPointer = srvUavHeapHandle.ptr;

    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        m_impl->m_shaderTable[i] = std::make_unique<DXShaderTable>();
        m_impl->m_shaderTable[i]->AddHitGroup(L"HitGroup");
        m_impl->m_shaderTable[i]->AddMiss(L"Miss");

        D3D12_GPU_DESCRIPTOR_HANDLE outputHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
        outputHandle.ptr += static_cast<uint64_t>((RAYTRACE_RT_SLOT + i) * heap->GetDescriptorSize());

        D3D12_GPU_DESCRIPTOR_HANDLE tlasHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
        tlasHandle.ptr += static_cast<uint64_t>((BVH_SLOT + i) * heap->GetDescriptorSize());

        std::vector<void*> heapPointers(4);
        heapPointers[0] = reinterpret_cast<void*>(outputHandle.ptr);
        heapPointers[1] = reinterpret_cast<void*>(tlasHandle.ptr);
        heapPointers[2] = reinterpret_cast<void*>(m_impl->m_frameIndex->GetGPUAddress(0, i));
        heapPointers[3] = reinterpret_cast<void*>(cameraBuffer->GetGPUAddress(0, i));

        m_impl->m_shaderTable[i]->AddRayGen(L"RayGen", heapPointers.data(),
                                            static_cast<UINT>(sizeof(void*) * heapPointers.size()));
        
        auto pipeline = reinterpret_cast<DXRTPipeline*>(m_shader->GetPipeline());
        m_impl->m_shaderTable[i]->Build(engineDevice, pipeline->m_stateObjectProps.Get());
    }
}

KS::RTRenderer::~RTRenderer() {}

void KS::RTRenderer::Render(Device& device, DXCommandContext* commandContext, Scene& scene,
                            std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>& inputs, bool clearRT)
{
     if (!m_impl->m_updateBVH)
    {
         memset(m_impl->m_BLBuffers, 0, 200 * sizeof(Impl::ASBuffers));
         m_impl->m_BLCount = 0;
     }
    auto commandList = commandContext->m_commandList.get();
    auto cpuFrameIndex = device.GetCPUFrameIndex();

    if (m_frameCount < 2)
    {
         int queueSize = static_cast<int>(scene.GetDrawQueueSize());
         for (int i = 0; i < queueSize; i++)
         {
             auto meshSet = scene.GetMeshSet(device, commandList, i);
             auto mesh = meshSet.mesh;

            if (mesh == nullptr) continue;

            CreateBVHBotomLevelInstance(device, mesh, meshSet.transform, cpuFrameIndex);
        }
    }

     CreateTopLevelAS(device, commandContext, m_impl->m_updateBVH, cpuFrameIndex);

    // m_impl->m_frameIndexBuffer->Update(&cpuFrameIndex, sizeof(uint32_t), 0, cpuFrameIndex);
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

void KS::RTRenderer::CreateBVHBotomLevelInstance(const Device& device, const Mesh* mesh, const glm::mat4x4& modelMat,
                                                 int cpuFrame)
{
    // if (!updateOnly)
    //{
    if (!mesh) return;

    // CreateBottomLevelAS(device, mesh, cpuFrame);
    auto& inst = m_impl->m_instances[m_impl->m_BLCount];
    inst.first = mesh->BLASAddress();
    inst.second = Conversion::GLMToXMMATRIX(modelMat);

    // auto BLASAddress =
    // m_impl->m_BLBuffers[m_impl->m_BLCount].pResult[cpuFrame]->GetResource()->GetGPUVirtualAddress();
    m_impl->m_topLevelASGenerator.AddInstance(inst.first, m_impl->m_instances[m_impl->m_BLCount].second,
                                              static_cast<uint32_t>(m_impl->m_BLCount), static_cast<uint32_t>(0));

    m_impl->m_BLCount++;
    //}
    // else
    //{
    //    m_impl->m_instances[entryIndex].second = Conversion::GLMToXMMATRIX(draw_entry.modelMat);
    //}
}

void KS::RTRenderer::CreateTopLevelAS(const Device& device, DXCommandContext* commandContext, bool updateOnly, int cpuFrame)
{
    ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());
    auto commandList = commandContext->m_commandList.get();

    if (!updateOnly)
    {
        // As for the bottom-level AS, the building the AS requires some scratch space
        // to store temporary data in addition to the actual AS. In the case of the
        // top-level AS, the instance descriptors also need to be stored in GPU
        // memory. This call outputs the memory requirements for each (scratch,
        // results, instance descriptors) so that the application can allocate the
        // corresponding memory
        UINT64 scratchSize, resultSize, instanceDescsSize;

        m_impl->m_topLevelASGenerator.ComputeASBufferSizes(engineDevice, true, &scratchSize, &resultSize, &instanceDescsSize);

        //// Create the scratch and result buffers. Since the build is all done on GPU,
        //// those can be allocated on the default heap

        auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        heapProps.CreationNodeMask = 0;
        heapProps.VisibleNodeMask = 0;

        m_impl->m_topLevelASBuffers.pScratch[cpuFrame] =
            std::make_shared<DXResource>(engineDevice, heapProps, bufDesc, nullptr, "TOP LEVEL BVH SCRATCH");
        bufDesc.Width = resultSize;
        m_impl->m_topLevelASBuffers.pResult[cpuFrame] =
            std::make_shared<DXResource>(engineDevice, heapProps, bufDesc, nullptr, "TOP LEVEL BVH RESULT",
                                         D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

        // The buffer describing the instances: ID, shader binding information,
        // matrices ... Those will be copied into the buffer by the helper through
        // mapping, so the buffer has to be allocated on the upload heap.
        bufDesc.Width = instanceDescsSize;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        heapProps.CreationNodeMask = 0;
        heapProps.VisibleNodeMask = 0;
        m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame] =
            std::make_shared<DXResource>(engineDevice, heapProps, bufDesc, nullptr, "TOP LEVEL BVH DESC");

        commandList->TransitionResource(*m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->Get(),
                                        m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->GetState(),
                                        D3D12_RESOURCE_STATE_GENERIC_READ);
        m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->ChangeState(D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    // After all the buffers are allocated, or if only an update is required, we
    // can build the acceleration structure. Note that in the case of the update
    // we also pass the existing AS as the 'previous' AS, so that it can be
    // refitted in place.
    m_impl->m_topLevelASGenerator.Generate(
        commandList->GetCommandList().Get(), m_impl->m_topLevelASBuffers.pScratch[cpuFrame]->Get(),
        m_impl->m_topLevelASBuffers.pResult[cpuFrame]->Get(), m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->Get(),
        updateOnly, m_impl->m_topLevelASBuffers.pResult[cpuFrame]->Get());

    if (!updateOnly)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location =
            m_impl->m_topLevelASBuffers.pResult[cpuFrame]->Get()->GetGPUVirtualAddress();

        m_impl->m_BHVHandle[cpuFrame] =
            reinterpret_cast<DXDescHeap*>(device.GetResourceHeap())
                ->AllocateResource(m_impl->m_topLevelASBuffers.pResult[cpuFrame].get(), &srvDesc, BVH_SLOT + cpuFrame);
    }

    commandList->TrackResource(m_impl->m_topLevelASBuffers.pScratch[cpuFrame]->GetResource());
    commandList->TrackResource(m_impl->m_topLevelASBuffers.pResult[cpuFrame]->GetResource());
    commandList->TrackResource(m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->GetResource());
}
