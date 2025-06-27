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

KS::ModelRenderer::ModelRenderer(const Device& device, SubRendererDesc& desc) : SubRenderer(device, desc) {}

KS::ModelRenderer::~ModelRenderer() {}

void KS::ModelRenderer::Render(Device& device, Scene& scene, std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs,
                               bool clearRT)
{
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList(START_THREAD));

    m_renderTarget->Bind(device, *commandList, m_depthStencil.get());
    if (clearRT)
    {
        m_renderTarget->Clear(device, *commandList);
    }
    m_depthStencil->Clear(device, *commandList);


    auto BindDrawState = [&](DXCommandList* cmdList)
    {
        cmdList->BindPipeline(pipeline);
        cmdList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(m_shader->GetShaderInput()->GetSignature()), false);
        cmdList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

        for (const auto& input : inputs)
        {
            input.first->Bind(device, *cmdList, input.second);
        }

        m_renderTarget->Bind(device, *cmdList, m_depthStencil.get());
    };

    int drawQueueSize = scene.GetDrawQueueSize();
    int drawObjectsPerThread = drawQueueSize / (NUM_DRAW_THREAD);
    int leftOverObjects = drawQueueSize % (NUM_DRAW_THREAD);

    for (int i = FIRST_DRAW_THREAD; i <= LAST_DRAW_THREAD; ++i)
    {
        DXCommandList* cmdList = reinterpret_cast<DXCommandList*>(device.GetCommandList(i));

        BindDrawState(cmdList);

        int startMeshIndex = (i - FIRST_DRAW_THREAD) * drawObjectsPerThread;
        int endMeshIndex = i == LAST_DRAW_THREAD 
            ? endMeshIndex = startMeshIndex + (drawObjectsPerThread + leftOverObjects)
            : startMeshIndex + drawObjectsPerThread;


        for (int j = startMeshIndex; j < endMeshIndex; ++j)
        {
            DrawMesh(device, scene, *cmdList, j);
        }
    }
}

void KS::ModelRenderer::DrawMesh(Device& device, Scene& scene, DXCommandList& commandList, int index)
{

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
