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
#pragma warning(pop)
#include <renderer/InfoStructs.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <scene/Scene.hpp>

#include <renderer/ShaderInputBlueprintBuilder.hpp>
#include <renderer/DX12/Helpers/DXShaderTable.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DXRTPipeline.hpp>

KS::ModelRenderer::ModelRenderer(const Device& device, std::shared_ptr<Shader>& shader, bool onlyCubemap)
    : SubRenderer(device, shader)
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

void KS::ModelRenderer::Render(Device& device, DXCommandContext* commandContext, RenderParameters& par)
{
    auto drawQueueSize = par.scene->GetCulledIndicesCount();
    if (drawQueueSize == 0) return;

    auto* commandList = commandContext->m_commandList.get();
    auto* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());
    auto shaderInput = m_shader->GetShaderInput();
    auto* resourceHeap = reinterpret_cast<DXDescHeap*>(par.scene->GetResourceHeap());
    auto* modelIndexUBO = par.scene->GetUniformBuffer(MODEL_INDEX_BUFFER);
    auto modelIndexInp = shaderInput->GetInput("model_index");
    auto texturesRoot = shaderInput->GetInput("textures").rootIndex;
    int shaderFlags = m_shader->GetFlags();
    auto frameIndex = device.GetCPUFrameIndex();

    par.rt->Bind(*commandList, frameIndex, par.ds.get());

    if (par.clearRt)
    {
        par.rt->Clear(*commandList, frameIndex);
    }
    par.ds->Clear(*commandList);

    auto BindDrawResources = [&](DXCommandList* cmdList)
    {
        cmdList->BindPipeline(pipeline);
        cmdList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(shaderInput->GetSignature()), false);
        cmdList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

        for (int i = 0; i < par.inputs->size(); i++)
        {
            auto& input = (*par.inputs)[i];
            if (input.first)
                input.first->Bind(device, resourceHeap, *cmdList, input.second.desc, input.second.bindOffset);
            else
                LOG(Log::Severity::WARN, "One of the inputs {} in a model renderer was empty", i);
        }

        par.rt->Bind(*cmdList, frameIndex, par.ds.get());
        cmdList->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    };

    BindDrawResources(commandList);

    if (m_onlyCubemap)
    {
        auto skydomeMeshHandle = par.scene->GetSkydomeMesh().second;
        auto skydomeMesh = par.scene->GetMesh(skydomeMeshHandle);
        using namespace MeshConstants;
        auto positions = skydomeMesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
        auto indices = skydomeMesh->GetAttribute(ATTRIBUTE_INDICES_NAME);
        BindDrawResources(commandList);

        positions->BindAsVertexData(*commandList, 0);
        indices->BindAsIndexData(*commandList);

        commandList->DrawIndexed(indices->GetElementCount());
        commandContext->Close();
        return;
    }


    auto RecordDrawCommandList = [&](int startMeshIndex, int endMeshIndex, Device& device, std::vector<uint32_t>& indices)
    {
        // Reuse the existing lambda
        auto context = device.GetCommandContext();
        BindDrawResources(context.m_commandList.get());

        for (int meshIndex = startMeshIndex; meshIndex < endMeshIndex; meshIndex++)
        {
            DrawMesh(device, *par.scene, *context.m_commandList.get(), indices[meshIndex], modelIndexInp, resourceHeap,
                     modelIndexUBO,
                     shaderFlags, texturesRoot);
        }
        context.Close();
    };

    std::vector<std::thread> workerThreads;
    auto& culledMeshIndices = par.scene->GetCulledDrawIndices();
    //for (int i = 0; i < NUM_DRAW_THREAD; ++i)
    //{

    //    int start, end = 0;
    //    bool enoughMeshes = SplitEven(drawQueueSize, NUM_DRAW_THREAD, i, start, end);
    //    if (!enoughMeshes) break;

    //    workerThreads.emplace_back(RecordDrawCommandList, start, end, std::ref(device), std::ref(culledMeshIndices));
    //}

    for (uint32_t i = 0; i < par.scene->GetCulledIndicesCount(); i++)
    {
        DrawMesh(device, *par.scene, *commandList, culledMeshIndices[i], modelIndexInp, resourceHeap,
                 modelIndexUBO, shaderFlags, texturesRoot);

    }

    //for (auto& t : workerThreads)
    //{
    //    t.join();
    //}
    commandContext->Close();
}

void KS::ModelRenderer::DrawMesh(Device& device, const Scene& scene, DXCommandList& commandList, uint32_t index,
                                 const ShaderInputDesc& modelIndexInputDesc, DXDescHeap* resourceHeap,
                                 UniformBuffer* modelIndexUBO, int shaderFlags, uint32_t texturesRootIndex)
{
    if (index >= scene.GetDrawQueueSize()) return;

    const DrawEntry& drawEntry = scene.GetDrawEntry(index);
    auto mesh = scene.GetMesh(drawEntry.meshHandle);
    if (mesh == nullptr) return;

    using namespace MeshConstants;

    auto positions = mesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
    auto normals = mesh->GetAttribute(ATTRIBUTE_NORMALS_NAME);
    auto uvs = mesh->GetAttribute(ATTRIBUTE_TEXTURE_UVS_NAME);
    auto tangents = mesh->GetAttribute(ATTRIBUTE_TANGENTS_NAME);
    auto indices = mesh->GetAttribute(ATTRIBUTE_INDICES_NAME);

    modelIndexUBO->Bind(device, resourceHeap, commandList, modelIndexInputDesc, drawEntry.modelIndex);

    if (shaderFlags & Shader::MeshInputFlags::HAS_POSITIONS && positions) positions->BindAsVertexData(commandList, 0);
    if (shaderFlags & Shader::MeshInputFlags::HAS_NORMALS && normals) normals->BindAsVertexData(commandList, 1);
    if (shaderFlags & Shader::MeshInputFlags::HAS_UVS && uvs) uvs->BindAsVertexData(commandList, 2);
    if (shaderFlags & Shader::MeshInputFlags::HAS_TANGENTS && tangents) tangents->BindAsVertexData(commandList, 3);

    indices->BindAsIndexData(commandList);
    commandList.BindHeapSlot(*resourceHeap, 0, texturesRootIndex);

    commandList.DrawIndexed(indices->GetElementCount());
}