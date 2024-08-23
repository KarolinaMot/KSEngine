#include <renderer/ModelRenderer.hpp>
#include <renderer/Shader.hpp>
#include <renderer/UniformBuffer.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/ShaderInputs.hpp>
#include <renderer/DX12/Helpers/DXPipeline.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <device/Device.hpp>

#include <fileio/FileIO.hpp>
#include <fileio/Serialization.hpp>
#include <resources/Texture.hpp>
#include <resources/Image.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <renderer/InfoStructs.hpp>

KS::ModelRenderer::ModelRenderer(const Device& device, std::shared_ptr<Shader> shader)
    : SubRenderer(device, shader)
{
    ModelMat mat {};
    MaterialInfo matInfo {};

    m_pointLights = std::vector<PointLightInfo>(100);
    m_directionalLights = std::vector<DirLightInfo>(100);

    mUniformBuffers[KS::MODEL_MAT_BUFFER] = std::make_unique<UniformBuffer>(device, "MODEL MATRIX RESOURCE", mat, 200);
    mUniformBuffers[KS::MATERIAL_INFO_BUFFER] = std::make_unique<UniformBuffer>(device, "MATERIAL INFO RESOURCE", matInfo, 200);
    mUniformBuffers[KS::LIGHT_INFO_BUFFER] = std::make_unique<UniformBuffer>(device, "LIGHT INFO BUFFER", m_lightInfo, 1);
    mStorageBuffers[KS::DIR_LIGHT_BUFFER] = std::make_unique<StorageBuffer>(device, "DIRECTIONAL LIGHT INFO BUFFER", m_directionalLights, false);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER] = std::make_unique<StorageBuffer>(device, "DIRECTIONAL LIGHT INFO BUFFER", m_pointLights, false);
}

KS::ModelRenderer::~ModelRenderer()
{
}

void KS::ModelRenderer::QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius)
{
    PointLightInfo pLight;
    pLight.mColorAndIntensity = glm::vec4(color, intensity);
    pLight.mPosition = glm::vec4(position, 0.f);
    pLight.mRadius = radius;
    m_pointLights[m_lightInfo.numPointLights] = pLight;
    m_lightInfo.numPointLights++;
}
void KS::ModelRenderer::QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity)
{
    DirLightInfo dLight;
    dLight.mDir = glm::vec4(direction, 0.f);
    dLight.mColorAndIntensity = glm::vec4(color, intensity);
    m_directionalLights[m_lightInfo.numDirLights] = dLight;
    m_lightInfo.numDirLights++;
}

void KS::ModelRenderer::SetAmbientLight(glm::vec3 color, float intensity)
{
    m_lightInfo.mAmbientAndIntensity = glm::vec4(color, intensity);
}

void KS::ModelRenderer::QueueModel(ResourceHandle<Model> model, const glm::mat4& transform)
{
    if (auto* ptr = GetModel(model))
    {
        for (auto node : ptr->nodes)
        {
            auto scene_transform = transform * node.transform;

            for (auto [mesh, material] : node.mesh_material_indices)
            {
                draw_queue.emplace_back(
                    scene_transform,
                    ptr->meshes[mesh],
                    ptr->materials[material]);
            }
        }
    }
}
void KS::ModelRenderer::Render(Device& device, int cpuFrameIndex)
{
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    commandList->BindPipeline(pipeline);
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

    UpdateLights(device);
    mStorageBuffers[KS::DIR_LIGHT_BUFFER]->Bind(device, m_shader->GetShaderInput()->GetInput("dir_lights").rootIndex);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER]->Bind(device, m_shader->GetShaderInput()->GetInput("point_lights").rootIndex);
    mUniformBuffers[KS::LIGHT_INFO_BUFFER]->Bind(device, m_shader->GetShaderInput()->GetInput("light_info").rootIndex, 0);

    int drawQueueCount = 0;
    for (const auto& draw_entry : draw_queue)
    {

        MaterialInfo matInfo = GetMaterialInfo(draw_entry);
        const Mesh* mesh = GetMesh(device, draw_entry.mesh);
        auto baseTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME));
        auto normalTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::NORMAL_TEXTURE_NAME));
        auto emissiveTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::EMISSIVE_TEXTURE_NAME));
        auto roughMetTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::METALLIC_TEXTURE_NAME));
        auto occlusionTex = GetTexture(device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::OCCLUSION_TEXTURE_NAME));
        matInfo.useColorTex = baseTex != nullptr;
        matInfo.useEmissiveTex = emissiveTex != nullptr;
        matInfo.useNormalTex = normalTex != nullptr;
        matInfo.useOcclusionTex = occlusionTex != nullptr;
        matInfo.useMetallicRoughnessTex = roughMetTex != nullptr;

        if (mesh == nullptr || baseTex == nullptr)
            continue;

        ModelMat modelMat;
        modelMat.mModel = draw_entry.transform;
        modelMat.mTransposed = glm::transpose(modelMat.mModel);
        mUniformBuffers[KS::MODEL_MAT_BUFFER]->Update(device, modelMat, drawQueueCount);
        mUniformBuffers[KS::MODEL_MAT_BUFFER]->Bind(device, m_shader->GetShaderInput()->GetInput("model_matrix").rootIndex, drawQueueCount);

        mUniformBuffers[KS::MATERIAL_INFO_BUFFER]->Update(device, matInfo, drawQueueCount);
        mUniformBuffers[KS::MATERIAL_INFO_BUFFER]->Bind(device, m_shader->GetShaderInput()->GetInput("material_info").rootIndex, drawQueueCount);

        using namespace MeshConstants;

        auto positions = mesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
        auto normals = mesh->GetAttribute(ATTRIBUTE_NORMALS_NAME);
        auto uvs = mesh->GetAttribute(ATTRIBUTE_TEXTURE_UVS_NAME);
        auto tangents = mesh->GetAttribute(ATTRIBUTE_TANGENTS_NAME);
        auto indices = mesh->GetAttribute(ATTRIBUTE_INDICES_NAME);

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
        drawQueueCount++;
    }
    draw_queue.clear();
}

const KS::Mesh* KS::ModelRenderer::GetMesh(const Device& device, ResourceHandle<Mesh> mesh)
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

const KS::Model* KS::ModelRenderer::GetModel(ResourceHandle<Model> model)
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

std::shared_ptr<KS::Texture> KS::ModelRenderer::GetTexture(const Device& device, ResourceHandle<Texture> imgPath)
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

void KS::ModelRenderer::UpdateLights(const Device& device)
{
    m_lightInfo.padding[1] = 1;
    mUniformBuffers[LIGHT_INFO_BUFFER]->Update(device, m_lightInfo);
    mStorageBuffers[DIR_LIGHT_BUFFER]->Update(device, m_directionalLights);
    mStorageBuffers[POINT_LIGHT_BUFFER]->Update(device, m_pointLights);
    m_lightInfo.numDirLights = 0;
    m_lightInfo.numPointLights = 0;
}

KS::MaterialInfo KS::ModelRenderer::GetMaterialInfo(const DrawEntry& drawEntry)
{
    MaterialInfo info {};
    info.colorFactor = drawEntry.material.GetParameter<glm::vec4>(MaterialConstants::BASE_COLOUR_FACTOR_NAME) ? *drawEntry.material.GetParameter<glm::vec4>(MaterialConstants::BASE_COLOUR_FACTOR_NAME) : MaterialConstants::BASE_COLOUR_FACTOR_DEFAULT;
    glm::vec4 NEAFactor = drawEntry.material.GetParameter<glm::vec4>(MaterialConstants::NEA_FACTORS_NAME) ? *drawEntry.material.GetParameter<glm::vec4>(MaterialConstants::NEA_FACTORS_NAME) : MaterialConstants::NEA_FACTORS_DEFAULT;
    glm::vec4 ORMFactor = drawEntry.material.GetParameter<glm::vec4>(MaterialConstants::ORM_FACTORS_NAME) ? *drawEntry.material.GetParameter<glm::vec4>(MaterialConstants::ORM_FACTORS_NAME) : MaterialConstants::ORM_FACTORS_DEFAULT;

    info.emissiveFactor = glm::vec4(NEAFactor.y, NEAFactor.y, NEAFactor.y, 1.f);
    info.normalScale = NEAFactor.x;
    info.metallicFactor = ORMFactor.z;
    info.normalScale = NEAFactor.x;
    info.roughnessFactor = ORMFactor.y;
    return info;
}
