#include <device/Device.hpp>
#include <fileio\ResourceHandle.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>
#include <renderer/ModelRenderer.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <resources/Texture.hpp>
#pragma warning(push, 0)
#include <glm/gtc/matrix_transform.hpp>
#include <DXR/DXRHelper.h>
#include <DXR/nv_helpers_dx12/TopLevelASGenerator.h>
#include <DXR/nv_helpers_dx12/BottomLevelASGenerator.h>
#include <DXR/nv_helpers_dx12/RaytracingPipelineGenerator.h>
#include <DXR/nv_helpers_dx12/RootSignatureGenerator.h>
#include <DXR/nv_helpers_dx12/ShaderBindingTableGenerator.h>
#pragma warning(pop)
#include <renderer/InfoStructs.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <scene/Scene.hpp>

#include <renderer/ShaderInputBlueprintBuilder.hpp>
#include <renderer/DX12/Helpers/DXShaderTable.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DXRTPipeline.hpp>

KS::ModelRenderer::ModelRenderer(const Device& device, SubRendererDesc& desc, bool onlyCubemap)
: SubRenderer(device, desc)
{
    m_onlyCubemap = onlyCubemap;
}

KS::ModelRenderer::~ModelRenderer() {}

bool SplitEven(int total, int parts, int i, int& start, int& end)
{
    const int used = std::min(parts, static_cast<int>(total));
    const int base = used ? total / used : 0;
    const int rem = used ? total % used : 0;
    if (i >= used)
    {
        start = end = 0;
        return true;
    }
    start = i * base + std::min(i, rem);
    const int count = base + (i < rem ? 1 : 0);
    end = start + count;  // end is exclusive

    return count != 0;
}

void KS::ModelRenderer::Render(Device& device, DXCommandContext* commandContext, Scene& scene,
                               std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>& inputs, bool clearRT)
{
    int drawQueueSize = static_cast<int>(scene.GetDrawQueueSize());
    if (drawQueueSize == 0) return;

    auto commandList = commandContext->m_commandList.get();

    auto pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    auto rootSignature = m_shader->GetShaderInput();
    auto frameIndex = device.GetCPUFrameIndex();

    m_renderTarget->Bind(*commandList, frameIndex, m_depthStencil.get());

    if (clearRT)
    {
        m_renderTarget->Clear(*commandList, frameIndex);
    }
    m_depthStencil->Clear(*commandList);

    auto BindDrawResources = [&](DXCommandList* cmdList)
    {
        cmdList->BindPipeline(pipeline);
        cmdList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(m_shader->GetShaderInput()->GetSignature()), false);
        cmdList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

        for (int i=0; i<inputs.size(); i++)
        {
            auto &input = inputs[i];
            if (input.first) 
                input.first->Bind(device, *cmdList, input.second.desc, input.second.bindOffset);
            else
                LOG(Log::Severity::WARN,
                    "One of the inputs {} in a model renderer was empty command will be ignored", i);
        }

        m_renderTarget->Bind(*cmdList, frameIndex, m_depthStencil.get());
        cmdList->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    };

    if (m_onlyCubemap)
    {
        BindDrawResources(commandList);
        auto skydomeMesh = scene.GetSkydomeMesh().first;
        using namespace MeshConstants;
        auto positions = skydomeMesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
        auto indices = skydomeMesh->GetAttribute(ATTRIBUTE_INDICES_NAME);

        positions->BindAsVertexData(*commandList, 0);
        indices->BindAsIndexData(*commandList);

        commandList->DrawIndexed(indices->GetElementCount());
        device.CloseCommandContext(std::move(*commandContext));
        return;
    }

    device.CloseCommandContext(std::move(*commandContext));

    auto RecordDrawCommandList = [&](int startMeshIndex, int endMeshIndex, Device& device)
    {
        // Reuse the existing lambda
        auto context = device.GetCommandContext();
        BindDrawResources(context.m_commandList.get());

        for (int meshIndex = startMeshIndex; meshIndex < endMeshIndex; meshIndex++)
        {
            DrawMesh(device, scene, *context.m_commandList.get(), meshIndex);
        }
        device.CloseCommandContext(std::move(context));
    };

    std::vector<std::thread> workerThreads;

    for (int i = 0; i < NUM_DRAW_THREAD; ++i)
    {
        int start, end = 0;
        bool enoughMeshes = SplitEven(drawQueueSize, NUM_DRAW_THREAD, i, start, end);
        if (!enoughMeshes) break;

        workerThreads.emplace_back(RecordDrawCommandList, start, end, std::ref(device));
    }

    for (auto& t : workerThreads)
    {
        t.join();
    }
}

void KS::ModelRenderer::DrawMesh(Device & device, Scene & scene, DXCommandList & commandList, int index)
{
    if (index >= scene.GetDrawQueueSize()) return;

    MeshSet meshSet = scene.GetMeshSet(device, &commandList, index);
    if (meshSet.mesh == nullptr || meshSet.baseTex == nullptr) return;

    using namespace MeshConstants;

    auto positions = meshSet.mesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
    auto normals = meshSet.mesh->GetAttribute(ATTRIBUTE_NORMALS_NAME);
    auto uvs = meshSet.mesh->GetAttribute(ATTRIBUTE_TEXTURE_UVS_NAME);
    auto tangents = meshSet.mesh->GetAttribute(ATTRIBUTE_TANGENTS_NAME);
    auto indices = meshSet.mesh->GetAttribute(ATTRIBUTE_INDICES_NAME);

    scene.GetUniformBuffer(MODEL_INDEX_BUFFER)
        ->Bind(device, commandList, m_shader->GetShaderInput()->GetInput("model_index"), meshSet.modelIndex);

    int shaderFlags = m_shader->GetFlags();

    if (shaderFlags & Shader::MeshInputFlags::HAS_POSITIONS) positions->BindAsVertexData(commandList, 0);
    if (shaderFlags & Shader::MeshInputFlags::HAS_NORMALS) normals->BindAsVertexData(commandList, 1);
    if (shaderFlags & Shader::MeshInputFlags::HAS_UVS) uvs->BindAsVertexData(commandList, 2);
    if (shaderFlags & Shader::MeshInputFlags::HAS_TANGENTS) tangents->BindAsVertexData(commandList, 3);

    indices->BindAsIndexData(commandList);

    meshSet.baseTex->Bind(device, commandList, m_shader->GetShaderInput()->GetInput("base_tex"));
    meshSet.normalTex->Bind(device, commandList,m_shader->GetShaderInput()->GetInput("normal_tex"));
    meshSet.emissiveTex->Bind(device,commandList, m_shader->GetShaderInput()->GetInput("emissive_tex"));
    meshSet.roughMetTex->Bind(device, commandList,m_shader->GetShaderInput()->GetInput("roughmet_tex"));
    meshSet.occlusionTex->Bind(device, commandList,m_shader->GetShaderInput()->GetInput("occlusion_tex"));

    commandList.DrawIndexed(indices->GetElementCount());
}