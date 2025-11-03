#include <scene/Scene.hpp>

#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>

#include <device/Device.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <renderer/MeshPool.hpp>
#include <renderer/TLAS.hpp>
#include <resources/Model.hpp>
#include <resources/Texture.hpp>
#include <resources/Skydome.hpp>
#include <resources/Image.hpp>
#include <resources/Mesh.hpp>


KS::Scene::Scene(Device& device)
{
    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;

    m_pointLights = std::vector<PointLightInfo>(100);
    m_directionalLights = std::vector<DirLightInfo>(100);
    m_meshPool = std::make_unique<MeshPool>(device, *commandList, MAX_MESHES, 30000, 30000);

    SetSkydome(device, *commandList, ResourceHandle<Texture>("assets/textures/cubemap.hdr"));
    m_skyDomeMesh.second = ResourceHandle<Mesh>("assets\\models\\Cube\\meshes\\Cube.bin");
    m_skyDomeMesh.first = GetMesh(device, commandList.get(), m_skyDomeMesh.second);

    CameraMats cam{};

    mStorageBuffers[MODEL_MAT_BUFFER] = std::make_unique<StorageBuffer>(
        device, *commandList, "MODEL MATRIX RESOURCE", &m_modelMatrices[0], static_cast<uint32_t>(sizeof(ModelMat)), MAX_MESHES, false);
    mStorageBuffers[MATERIAL_INFO_BUFFER] = std::make_unique<StorageBuffer>(
        device, *commandList, "MATERIAL INFO RESOURCE", &m_materialInstances[0], static_cast<uint32_t>(sizeof(MaterialInfo)), MAX_MESHES, false);
    mUniformBuffers[MODEL_INDEX_BUFFER] =
        std::make_unique<UniformBuffer>(device, "MODEL INDEX BUFFER", m_modelCount, MAX_MESHES, false);
    mUniformBuffers[CAMERA_MAT_BUFFER] = std::make_shared<UniformBuffer>(device, "CAMERA MATRIX BUFFER", cam, 1);

    GenerateMipsInfo mipInfo;
    mUniformBuffers[MIP_GEN_INFO] = std::make_unique<UniformBuffer>(device, "MIP GEN INFO", mipInfo, NUM_OF_TEXTURES);

    m_fogInfo.fogColor = glm::vec3(1.f, 1.f, 1.f);
    m_fogInfo.fogDensity = 0.6f;
    m_fogInfo.exposure = 0.15f;
    m_fogInfo.lightShaftNumberSamples = 132;
    m_fogInfo.sourceMipNumber = 2;
    m_fogInfo.weight = 0.05f;
    m_fogInfo.decay = 0.99f;

    mUniformBuffers[KS::LIGHT_INFO_BUFFER] =
        std::make_unique<UniformBuffer>(device, "LIGHT INFO BUFFER", m_lightInfo, 1);
    mUniformBuffers[KS::FOG_INFO_BUFFER] = std::make_unique<UniformBuffer>(device, "FOG INFO BUFFER", m_fogInfo, 1, false);
    mStorageBuffers[KS::DIR_LIGHT_BUFFER] =
        std::make_unique<StorageBuffer>(device, *commandList, "DIRECTIONAL LIGHT BUFFER", m_directionalLights, false);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER] =
        std::make_unique<StorageBuffer>(device, *commandList, "POINT LIGHT BUFFER", m_pointLights, false);

    m_BVH = std::make_unique<TLAS>();
    commandContext.Close();
}

KS::Scene::~Scene() {}

void KS::Scene::QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform, std::string name)
{
    auto commandContext = device.GetCommandContext();
    auto commandList = commandContext.m_commandList.get();

    if (auto* ptr = GetModel(model))
    {
        for (auto node : ptr->nodes)
        {
            auto scene_transform = transform * node.transform;

            for (auto [mesh, material] : node.mesh_material_indices)
            {
                if (m_modelCount >= MAX_MESHES)
                {
                    LOG(Log::Severity::WARN, "Maximum number of meshes {} has been reached. Command ignored.", MAX_MESHES);
                    return;
                }

                auto meshHandle = ptr->meshes[mesh];
                auto mat = ptr->materials[material];

                std::shared_ptr<Mesh> meshPtr = GetMesh(device, commandList, meshHandle);
                std::string key = name + std::to_string(m_modelCount);

                draw_queue[key] = KS::DrawEntry(meshPtr, ptr->materials[material], m_modelCount, scene_transform);

                ModelMat modelMat;
                modelMat.mModel = scene_transform;
                modelMat.mTransposed = glm::transpose(modelMat.mModel);
                m_modelMatrices[m_modelCount] = modelMat;

                auto baseTexHandle = mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME);
                auto normalTexHandle = mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::NORMAL_TEXTURE_NAME);
                auto emissiveTexHandle = mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::EMISSIVE_TEXTURE_NAME);
                auto roughMetHandle = mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::METALLIC_TEXTURE_NAME);
                auto occlusionHandle = mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::OCCLUSION_TEXTURE_NAME);

                MaterialInfo matInfo = GetMaterialInfo(ptr->materials[material]);
                auto baseTex = GetTexture(device, commandList, *baseTexHandle);
                auto normalTex = GetTexture(device, commandList, *normalTexHandle);
                auto emissiveTex = GetTexture(device, commandList, *emissiveTexHandle);
                auto roughMetTex = GetTexture(device, commandList, *roughMetHandle);
                auto occlusionTex = GetTexture(device, commandList, *occlusionHandle);

                matInfo.colorTexIndex = baseTex->GetHandleIndex(true);
                matInfo.emissiveTexIndex = emissiveTex->GetHandleIndex(true);
                matInfo.normalTexIndex = normalTex->GetHandleIndex(true);
                matInfo.occlusionTexIndex = occlusionTex->GetHandleIndex(true);
                matInfo.metallicRoughnessTexIndex = roughMetTex->GetHandleIndex(true);

                matInfo.modelIndex = draw_queue[key].mesh->GetMeshIndex();
                matInfo.vOffset = draw_queue[key].mesh->GetVDataOffset();
                matInfo.indexOffset = draw_queue[key].mesh->GetIDataOffset();

                mUniformBuffers[MODEL_INDEX_BUFFER]->Update(device, m_modelCount, m_modelCount);

                m_materialInstances[m_modelCount] = matInfo;
                m_modelCount++;
                m_BVH->AddInstance(device, *commandList, &draw_queue[key], modelMat.mModel);
            }
        }
    }

    mStorageBuffers[MODEL_MAT_BUFFER]->Update(device, *commandList, &m_modelMatrices[0], m_modelCount);
    mStorageBuffers[MATERIAL_INFO_BUFFER]->Update(device, *commandList, &m_materialInstances[0], m_modelCount);
    mStorageBuffers[DIR_LIGHT_BUFFER]->Update(device, *commandList, m_directionalLights);
    mStorageBuffers[POINT_LIGHT_BUFFER]->Update(device, *commandList, m_pointLights);
    commandContext.Close();
}

void KS::Scene::ApplyModelTransform(std::string name, const glm::mat4& transfrom)
{
    auto& entry = draw_queue[name];
    ModelMat modelMat;
    modelMat.mModel = m_modelMatrices[entry.modelIndex].mModel * transfrom;
    modelMat.mTransposed = glm::transpose(modelMat.mModel);
    m_modelMatrices[entry.modelIndex] = modelMat;

    m_BVH->UpdateTransform(entry.tlasHandle, modelMat.mModel);
}

void KS::Scene::QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius)
{
    PointLightInfo pLight;
    pLight.mColorAndIntensity = glm::vec4(color, intensity);
    pLight.mPosition = glm::vec4(position, 0.f);
    pLight.mRadius = radius;
    m_pointLights[m_lightInfo.numPointLights] = pLight;
    m_lightInfo.numPointLights++;
}
void KS::Scene::QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity)
{
    DirLightInfo dLight;
    dLight.mDir = glm::vec4(direction, 0.f);
    dLight.mColorAndIntensity = glm::vec4(color, intensity);
    m_directionalLights[m_lightInfo.numDirLights] = dLight;
    m_lightInfo.numDirLights++;
}

void KS::Scene::SetAmbientLight(glm::vec3 color, float intensity)
{
    m_lightInfo.mAmbientAndIntensity = glm::vec4(color, intensity);
}

void KS::Scene::SetFogValues(Device& device, const FogInfo& newFogInfo)
{
    m_fogInfo = newFogInfo;
    mUniformBuffers[KS::FOG_INFO_BUFFER]->Update(device, m_fogInfo);
}

void KS::Scene::Tick(Device& device)
{

    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;

    mStorageBuffers[MODEL_MAT_BUFFER]->Update(device, *commandList, &m_modelMatrices[0], m_modelCount);
    mUniformBuffers[LIGHT_INFO_BUFFER]->Update(device, m_lightInfo);
    m_BVH->Build(device, *commandList);

    commandContext.Close();
}

void KS::Scene::SetSkydome(Device& device, DXCommandList& commandList, ResourceHandle<Texture> skydomeTexture)
{ 
    if (skydomeTexture.path == "")
    {
        LOG(Log::Severity::WARN, "Skydome texture handle was empty, so it won't be set.");
    }
    else
    {
        auto skydomeTex = GetTexture(device, &commandList, skydomeTexture);
        auto pair = std::pair<std::shared_ptr<Skydome>, ResourceHandle<Texture>>();
        pair.first = std::make_shared<Skydome>(device, commandList, *skydomeTex.get());
        pair.second = skydomeTexture;
        m_skyDome = pair;
    }
}

const KS::Model* KS::Scene::GetModel(ResourceHandle<Model> model)
{
    // Cached result
    if (auto it = model_cache.find(model); it != model_cache.end())
    {
        return &it->second;
    }

    // Load result
    else if (auto fileread = FileIO::OpenReadStream(model.path))
    {
        JSONLoader json{fileread.value()};
        Model new_model{};

        json(new_model);

        auto [obj, success] = model_cache.emplace(model, std::move(new_model));
        return &obj->second;
    }
    return nullptr;
}

const std::shared_ptr<KS::Mesh> KS::Scene::GetMesh(Device& device, DXCommandList* commandList, ResourceHandle<Mesh> meshHandle)
{
    // Cached result
    if (auto it = mesh_cache.find(meshHandle); it != mesh_cache.end())
    {
        return it->second;
    }
    else if (auto fileRead = FileIO::OpenReadStream(meshHandle.path))
    {
        BinaryLoader bin{fileRead.value()};
        MeshData data{};
        bin(data);

        std::filesystem::path p(meshHandle.path);
        std::string mesh_name_extracted = p.stem().string();

        //auto meshPtr = std::make_shared<Mesh>(device, *commandList, data, mesh_name_extracted.c_str(), mesh_cache.size());
        auto meshPtr = m_meshPool->AllocateMesh(device, *commandList, data, mesh_name_extracted.c_str());
        auto [obj, success] = mesh_cache.emplace(meshHandle, std::move(meshPtr));
        //obj->second->SetVDataOffset(m_vDataOffset);
        //obj->second->SetIndexDataOffset(m_indexDataOffset);

        auto view = data.GetAttribute(MeshConstants::ATTRIBUTE_NORMALS_NAME)->GetView<uint8_t>();
        size_t size = view.count();
        size_t stride = MeshConstants::ATTRIBUTE_STRIDES.find(MeshConstants::ATTRIBUTE_NORMALS_NAME)->second;
        m_vDataOffset += size / stride;

        view = data.GetAttribute(MeshConstants::ATTRIBUTE_INDICES_NAME)->GetView<uint8_t>();
        size = view.count();
        stride = MeshConstants::ATTRIBUTE_STRIDES.find(MeshConstants::ATTRIBUTE_INDICES_NAME)->second;
        m_indexDataOffset += size / stride;

        return obj->second;
    }
    else
    {
        LOG(Log::Severity::WARN, "Model path ( {} ) was not found.", meshHandle.path);
        return nullptr;
    }

    return nullptr;
}

std::shared_ptr<KS::Texture> KS::Scene::GetTexture(Device& device, DXCommandList* commandList, ResourceHandle<Texture> imgPath)
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
            auto new_tex = std::make_shared<Texture>(device, *commandList, img.value());
            device.AddToMipmapQueue(new_tex);
            auto [obj, success] = tex_cache.emplace(imgPath, std::move(new_tex));
            return obj->second;
        }
    }
    return nullptr;
}

KS::MaterialInfo KS::Scene::GetMaterialInfo(const Material& material) const
{
    MaterialInfo info{};
    info.colorFactor = material.GetParameter<glm::vec4>(MaterialConstants::BASE_COLOUR_FACTOR_NAME)
                           ? *material.GetParameter<glm::vec4>(MaterialConstants::BASE_COLOUR_FACTOR_NAME)
                           : MaterialConstants::BASE_COLOUR_FACTOR_DEFAULT;
    glm::vec4 NEAFactor = material.GetParameter<glm::vec4>(MaterialConstants::NEA_FACTORS_NAME)
                              ? *material.GetParameter<glm::vec4>(MaterialConstants::NEA_FACTORS_NAME)
                              : MaterialConstants::NEA_FACTORS_DEFAULT;
    glm::vec4 ORMFactor = material.GetParameter<glm::vec4>(MaterialConstants::ORM_FACTORS_NAME)
                              ? *material.GetParameter<glm::vec4>(MaterialConstants::ORM_FACTORS_NAME)
                              : MaterialConstants::ORM_FACTORS_DEFAULT;

    info.emissiveFactor = glm::vec4(NEAFactor.y, NEAFactor.y, NEAFactor.y, 1.f);
    info.normalScale = NEAFactor.x;
    info.metallicFactor = ORMFactor.z;
    info.normalScale = NEAFactor.x;
    info.roughnessFactor = ORMFactor.y;
    return info;
}

KS::MeshSet KS::Scene::GetMeshSet(Device& device, DXCommandList* commandList, int index)
{
    auto draw_entry = (*std::next(draw_queue.begin(), index)).second;

    MeshSet meshSet;
    meshSet.mesh = draw_entry.mesh.get();
    meshSet.baseTex =
        GetTexture(device, commandList,
                   *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME));
    meshSet.normalTex =
        GetTexture(device, commandList,
                   *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::NORMAL_TEXTURE_NAME));
    meshSet.emissiveTex = GetTexture(device, commandList,
                   *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::EMISSIVE_TEXTURE_NAME));
    meshSet.roughMetTex = GetTexture(device, commandList,
                   *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::METALLIC_TEXTURE_NAME));
    meshSet.occlusionTex = GetTexture(device, commandList,
                   *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::OCCLUSION_TEXTURE_NAME));
    meshSet.modelIndex = draw_entry.modelIndex;
    meshSet.transform = draw_entry.modelMat;

    return meshSet;
}