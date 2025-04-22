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
    for (int i = 0; i < inputs.size(); i++)
    {
        inputs[i].first->Bind(device, inputs[i].second);
    }

    m_renderTarget->Bind(device, m_depthStencil.get());
    if (clearRT)
    {
        m_renderTarget->Clear(device);
    }
    m_depthStencil->Clear(device);

    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);

    //scene.GetModelMatrixlBuffer().Bind(device, m_shader->GetShaderInput()->GetInput("model_matrix"));
    //scene.GetModelMaterialBuffer().Bind(device, m_shader->GetShaderInput()->GetInput("material_info"));
    int drawQueueSize = scene.GetDrawQueueSize();

    for (int i = 0; i < drawQueueSize; i++)
    {
        MeshSet meshSet = scene.GetMeshSet(device, i);
        if (meshSet.mesh == nullptr || meshSet.baseTex == nullptr) continue;

        using namespace MeshConstants;

        auto positions = meshSet.mesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
        auto normals = meshSet.mesh->GetAttribute(ATTRIBUTE_NORMALS_NAME);
        auto uvs = meshSet.mesh->GetAttribute(ATTRIBUTE_TEXTURE_UVS_NAME);
        auto tangents = meshSet.mesh->GetAttribute(ATTRIBUTE_TANGENTS_NAME);
        auto indices = meshSet.mesh->GetAttribute(ATTRIBUTE_INDICES_NAME);

        scene.GetUniformBuffer(MODEL_INDEX_BUFFER)->Bind(device, m_shader->GetShaderInput()->GetInput("model_index"), meshSet.modelIndex);

        int shaderFlags = m_shader->GetFlags();

        if (shaderFlags & Shader::MeshInputFlags::HAS_POSITIONS) positions->BindAsVertexData(device, 0);
        if (shaderFlags & Shader::MeshInputFlags::HAS_NORMALS) normals->BindAsVertexData(device, 1);
        if (shaderFlags & Shader::MeshInputFlags::HAS_UVS) uvs->BindAsVertexData(device, 2);
        if (shaderFlags & Shader::MeshInputFlags::HAS_TANGENTS) tangents->BindAsVertexData(device, 3);

        indices->BindAsIndexData(device);

        meshSet.baseTex->Bind(device, m_shader->GetShaderInput()->GetInput("base_tex"));
        meshSet.normalTex->Bind(device, m_shader->GetShaderInput()->GetInput("normal_tex"));
        meshSet.emissiveTex->Bind(device, m_shader->GetShaderInput()->GetInput("emissive_tex"));
        meshSet.roughMetTex->Bind(device, m_shader->GetShaderInput()->GetInput("roughmet_tex"));
        meshSet.occlusionTex->Bind(device, m_shader->GetShaderInput()->GetInput("occlusion_tex"));

        commandList->DrawIndexed(indices->GetElementCount());
    }
}