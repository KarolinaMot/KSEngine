#include <renderer/ModelRenderer.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>

#include <device/Device.hpp>
#include <resources/Texture.hpp>
#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <fileio\ResourceHandle.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <scene/Scene.hpp>
#include <thread>

KS::ModelRenderer::ModelRenderer(const Device& device, SubRendererDesc& desc) : SubRenderer(device, desc) {}

KS::ModelRenderer::~ModelRenderer() {}

void KS::ModelRenderer::Render(Device& device, Scene& scene, std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs,
                               bool clearRT)
{
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    DXCommandList* mainCommandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    m_renderTarget->PrepareToRenderTo(device, *mainCommandList);
    m_depthStencil->PrepareToUse(device, *mainCommandList);
    // m_renderTarget->Bind(device, *mainCommandList, m_depthStencil.get());
    if (clearRT)
    {
        m_renderTarget->Clear(device, *mainCommandList);
    }
    m_depthStencil->Clear(device, *mainCommandList);

    auto BindDrawResources = [&](DXCommandList* cmdList)
    {
        cmdList->BindPipeline(pipeline);
        cmdList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(m_shader->GetShaderInput()->GetSignature()), false);
        cmdList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

        for (const auto& input : inputs)
        {
            input.first->Bind(device, *cmdList, input.second);
        }

        m_renderTarget->Bind(device, *cmdList, m_depthStencil.get());
        cmdList->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    };

    auto RecordDrawCommandList = [&](int threadIndex, int startMeshIndex, int endMeshIndex, DXCommandList& drawCmdList)
    {
        // Reuse the existing lambda
        BindDrawResources(&drawCmdList);

        for (int meshIndex = startMeshIndex; meshIndex < endMeshIndex; meshIndex++)
        {
            DrawMesh(device, scene, drawCmdList, meshIndex);
        }
    };

    int drawQueueSize = scene.GetDrawQueueSize();
    int drawObjectsPerThread = drawQueueSize / NUM_DRAW_THREAD;
    int leftOverObjects = drawQueueSize % NUM_DRAW_THREAD;
    std::vector<std::thread> workerThreads;
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    for (int i = FIRST_DRAW_THREAD; i <= LAST_DRAW_THREAD; ++i)
    {
        int start = (i - FIRST_DRAW_THREAD) * drawObjectsPerThread;
        int count = (i == LAST_DRAW_THREAD) ? drawObjectsPerThread + leftOverObjects : drawObjectsPerThread;
        int end = start + count - 1;

        auto& drawList = *reinterpret_cast<DXCommandList*>(device.GetCommandList(i));
        workerThreads.emplace_back(RecordDrawCommandList, i, start, end, std::ref(drawList));
    }

    for (auto& t : workerThreads)
    {
        t.join();
    }
}

void KS::ModelRenderer::DrawMesh(Device& device, Scene& scene, DXCommandList& commandList, int index)
{
    if (index >= scene.GetDrawQueueSize()) return;

    MeshSet meshSet = scene.GetMeshSet(device, index);
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
    meshSet.normalTex->Bind(device, commandList, m_shader->GetShaderInput()->GetInput("normal_tex"));
    meshSet.emissiveTex->Bind(device, commandList, m_shader->GetShaderInput()->GetInput("emissive_tex"));
    meshSet.roughMetTex->Bind(device, commandList, m_shader->GetShaderInput()->GetInput("roughmet_tex"));
    meshSet.occlusionTex->Bind(device, commandList, m_shader->GetShaderInput()->GetInput("occlusion_tex"));

    commandList.DrawIndexed(indices->GetElementCount());
}
