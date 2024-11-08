#include "../ModelRenderer.hpp"
#include "../Shader.hpp"
#include "../ShaderInputs.hpp"
#include "Helpers/DXPipeline.hpp"
#include <Device.hpp>
#include <commands/DXCommandList.hpp>
#include <descriptors/DXDescriptorHandle.hpp>


#include <fileio/FileIO.hpp>
#include <fileio/Serialization.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <resources/Image.hpp>
#include <resources/Texture.hpp>

ModelRenderer::ModelRenderer(const Device& device, std::shared_ptr<Shader> shader)
    : SubRenderer(device, shader)
{
    m_modelMatsBuffer = std::make_unique<StorageBuffer>(device, "MODEL MATRIX RESOURCE", &m_modelMatrices[0], sizeof(ModelMat), 200, false);
    m_materialInfoBuffer = std::make_unique<StorageBuffer>(device, "MATERIAL INFO RESOURCE", &m_materialInstances[0], sizeof(MaterialInfo), 200, false);
    m_modelIndexBuffer = std::make_unique<UniformBuffer>(device, "MODEL INDEX BUFFER", m_modelCount, 200);
}

ModelRenderer::~ModelRenderer()
{
}

void ModelRenderer::QueueModel(const Device& device, ResourceHandle<Model> model, const glm::mat4& transform)
{
    if (auto* ptr = GetModel(model))
    {
        for (auto node : ptr->nodes)
        {
            auto scene_transform = transform * node.transform;

            for (auto [mesh, material] : node.mesh_material_indices)
            {
                if (m_modelCount >= 200)
                {
                    Log("Maximum number of meshes {} has been reached. Command ignored.", 200);
                    return;
                }

                draw_queue.emplace_back(
                    ptr->meshes[mesh],
                    ptr->materials[material],
                    m_modelCount);

                auto mat = ptr->materials[material];
                auto meshHandle = ptr->meshes[mesh];

                ModelMat modelMat;
                modelMat.mModel = scene_transform;
                modelMat.mTransposed = glm::transpose(modelMat.mModel);
                m_modelMatrices[m_modelCount] = modelMat;

                MaterialInfo matInfo = GetMaterialInfo(ptr->materials[material]);
                auto baseTex = GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME));
                auto normalTex = GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::NORMAL_TEXTURE_NAME));
                auto emissiveTex = GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::EMISSIVE_TEXTURE_NAME));
                auto roughMetTex = GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::METALLIC_TEXTURE_NAME));
                auto occlusionTex = GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::OCCLUSION_TEXTURE_NAME));

                matInfo.useColorTex = baseTex != nullptr;
                matInfo.useEmissiveTex = emissiveTex != nullptr;
                matInfo.useNormalTex = normalTex != nullptr;
                matInfo.useOcclusionTex = occlusionTex != nullptr;
                matInfo.useMetallicRoughnessTex = roughMetTex != nullptr;

                m_modelIndexBuffer->Update(device, m_modelCount, m_modelCount);

                m_materialInstances[m_modelCount] = matInfo;
                m_modelCount++;
            }
        }
    }
}
void ModelRenderer::Render(Device& device, MAYBE_UNUSED int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<DepthStencil> depthStencil, MAYBE_UNUSED Texture** previoiusPassResults, MAYBE_UNUSED int numTextures)
{
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);
    renderTarget->Bind(device, depthStencil.get());
    renderTarget->Clear(device);
    depthStencil->Clear(device);

    m_modelMatsBuffer->Update(device, &m_modelMatrices[0], m_modelCount);
    m_materialInfoBuffer->Update(device, &m_materialInstances[0], m_modelCount);
    m_modelMatsBuffer->Bind(device, m_shader->GetShaderInput()->GetInput("model_matrix").rootIndex);
    m_materialInfoBuffer->Bind(device, m_shader->GetShaderInput()->GetInput("material_info").rootIndex);

    for (const auto& draw_entry : draw_queue)
    {

        const Mesh* mesh = GetMesh(device, draw_entry.mesh);
        auto baseTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME));
        auto normalTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::NORMAL_TEXTURE_NAME));
        auto emissiveTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::EMISSIVE_TEXTURE_NAME));
        auto roughMetTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::METALLIC_TEXTURE_NAME));
        auto occlusionTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::OCCLUSION_TEXTURE_NAME));

        if (mesh == nullptr || baseTex == nullptr)
            continue;

        using namespace MeshConstants;

        auto positions = mesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
        auto normals = mesh->GetAttribute(ATTRIBUTE_NORMALS_NAME);
        auto uvs = mesh->GetAttribute(ATTRIBUTE_TEXTURE_UVS_NAME);
        auto tangents = mesh->GetAttribute(ATTRIBUTE_TANGENTS_NAME);
        auto indices = mesh->GetAttribute(ATTRIBUTE_INDICES_NAME);
        m_modelIndexBuffer->Bind(device, m_shader->GetShaderInput()->GetInput("model_index").rootIndex, draw_entry.modelIndex);

        positions->BindAsVertexData(device, 0);
        normals->BindAsVertexData(device, 1);
        uvs->BindAsVertexData(device, 2);
        tangents->BindAsVertexData(device, 3);
        indices->BindAsIndexData(device);

        auto baseTexRoot = m_shader->GetShaderInput()->GetInput("base_tex").rootIndex;
        auto normalTexRoot = m_shader->GetShaderInput()->GetInput("normal_tex").rootIndex;
        auto emissiveTexRoot = m_shader->GetShaderInput()->GetInput("emissive_tex").rootIndex;
        auto roughMetTexRoot = m_shader->GetShaderInput()->GetInput("roughmet_tex").rootIndex;
        auto oucclusionTexRoot = m_shader->GetShaderInput()->GetInput("occlusion_tex").rootIndex;

        baseTex->Bind(device, baseTexRoot);
        normalTex->Bind(device, normalTexRoot);
        emissiveTex->Bind(device, emissiveTexRoot);
        roughMetTex->Bind(device, roughMetTexRoot);
        occlusionTex->Bind(device, oucclusionTexRoot);

        commandList->DrawIndexed(indices->GetElementCount());
    }

    // device.TrackResource(mResourceBuffers[MODEL_MAT_BUFFER]);
    m_modelCount = 0;
    draw_queue.clear();
}

const Mesh* ModelRenderer::GetMesh(const Device& device, ResourceHandle<Mesh> mesh)
{
    // Cached result
    if (auto it = mesh_cache.find(mesh); it != mesh_cache.end())
    {
        return &it->second;
    }

    // Load result
    else if (auto fileread = FileIO::OpenReadStream(mesh.path))
    {
        BinaryLoader bin { fileread.value() };
        MeshData data {};

        bin(data);

        auto [it, success] = mesh_cache.emplace(mesh, Mesh(device, data));
        return &it->second;
    }
    return nullptr;
}

const Model* ModelRenderer::GetModel(ResourceHandle<Model> model)
{
    // Cached result
    if (auto it = model_cache.find(model); it != model_cache.end())
    {
        return &it->second;
    }

    // Load result
    else if (auto fileread = FileIO::OpenReadStream(model.path))
    {
        JSONLoader json { fileread.value() };
        Model new_model {};

        json(new_model);

        auto [it, success] = model_cache.emplace(model, std::move(new_model));
        return &it->second;
    }
    return nullptr;
}

std::shared_ptr<Texture> ModelRenderer::GetTexture(const Device& device, ResourceHandle<Texture> imgPath)
{
    // Cached result
    if (auto it = tex_cache.find(imgPath); it != tex_cache.end())
    {
        return it->second;
    }

    // Load result
    else if (auto fileread = FileIO::OpenReadStream(imgPath.path))
    {
        auto imageContents = FileIO::DumpFullStream(fileread.value());

        if (auto img = LoadImageFileFromMemory(imageContents.data(), imageContents.size()))
        {
            auto new_tex = std::make_shared<Texture>(device, img.value());
            auto [it, success] = tex_cache.emplace(imgPath, std::move(new_tex));
            return it->second;
        }
    }
    return nullptr;
}

MaterialInfo ModelRenderer::GetMaterialInfo(const Material& material)
{
    MaterialInfo info {};
    info.colorFactor = material.GetParameter<glm::vec4>(MaterialConstants::BASE_COLOUR_FACTOR_NAME) ? *material.GetParameter<glm::vec4>(MaterialConstants::BASE_COLOUR_FACTOR_NAME) : MaterialConstants::BASE_COLOUR_FACTOR_DEFAULT;
    glm::vec4 NEAFactor = material.GetParameter<glm::vec4>(MaterialConstants::NEA_FACTORS_NAME) ? *material.GetParameter<glm::vec4>(MaterialConstants::NEA_FACTORS_NAME) : MaterialConstants::NEA_FACTORS_DEFAULT;
    glm::vec4 ORMFactor = material.GetParameter<glm::vec4>(MaterialConstants::ORM_FACTORS_NAME) ? *material.GetParameter<glm::vec4>(MaterialConstants::ORM_FACTORS_NAME) : MaterialConstants::ORM_FACTORS_DEFAULT;

    info.emissiveFactor = glm::vec4(NEAFactor.y, NEAFactor.y, NEAFactor.y, 1.f);
    info.normalScale = NEAFactor.x;
    info.metallicFactor = ORMFactor.z;
    info.normalScale = NEAFactor.x;
    info.roughnessFactor = ORMFactor.y;
    return info;
}
