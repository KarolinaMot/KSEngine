#include <assimp/GltfMaterial.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>
#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXShaderTable.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/TLAS.hpp>
#include <renderer/UniformBuffer.hpp>
#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <resources/Model.hpp>
#include <resources/Skydome.hpp>
#include <resources/Texture.hpp>
#include <scene/Scene.hpp>

class KS::Scene::Impl
{
public:
    std::shared_ptr<DXDescHeap> m_resourceHeap;
};

KS::Scene::Scene() {}

KS::Scene::Scene(Device& device, std::string name, ScenesToChoose id)
{
    m_name = name;
    m_identifyingIndex = id;
    draw_queue.resize(MAX_MESHES);
    m_impl = std::make_unique<Impl>();

    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    std::string heapName = m_name + " resource heap";

    m_impl->m_resourceHeap = DXDescHeap::Construct(engineDevice, RESOURCE_HEAP_SIZE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                                   Conversion::utf8_to_wide(heapName).c_str(), OTHER_RESOURCES_START,
                                                   D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    material_cache.resize(MAX_MATERIALS);
    m_pointLights.resize(20);
    m_directionalLights.resize(20);


    CameraMats cam{};

    mStorageBuffers[CULLED_INSTANCE_DATA_BUFFER] =
        std::make_unique<StorageBuffer>(device, m_impl->m_resourceHeap.get(), *commandList, "CULLED MODEL INSTANCE DATA",
                                        &m_instanceData[0], static_cast<uint32_t>(sizeof(InstanceData)), MAX_MESHES, false);
    mStorageBuffers[INSTANCE_DATA_BUFFER] =
        std::make_unique<StorageBuffer>(device, m_impl->m_resourceHeap.get(), *commandList, "MODEL INSTANCE DATA",
                                        &m_instanceData[0], static_cast<uint32_t>(sizeof(InstanceData)), MAX_MESHES, false);

    mUniformBuffers[MODEL_INDEX_BUFFER] =
        std::make_unique<UniformBuffer>(device, "MODEL INDEX BUFFER", m_drawCallCount, MAX_MESHES, false);

    mUniformBuffers[CAMERA_MAT_BUFFER] = std::make_shared<UniformBuffer>(device, "CAMERA MATRIX BUFFER", cam, 1);

    GenerateMipsInfo mipInfo;
    mUniformBuffers[MIP_GEN_INFO] = std::make_unique<UniformBuffer>(device, "MIP GEN INFO", mipInfo, NUM_OF_TEXTURES);

    CullingInfo cullingInfo;
    mUniformBuffers[CULLING_INFO] = std::make_unique<UniformBuffer>(device, "CULLING INFO", cullingInfo, 1);
    
    mUniformBuffers[PATH_TRACING_BUFFER] = std::make_unique<UniformBuffer>(device, "PATH TRACING INFO", m_pathTracingInfo, 1);

    m_fogInfo.fogColor = glm::vec3(1.f, 1.f, 1.f);
    m_fogInfo.fogDensity = 0.6f;
    m_fogInfo.exposure = 0.15f;
    m_fogInfo.lightShaftNumberSamples = 132;
    m_fogInfo.sourceMipNumber = 2;
    m_fogInfo.weight = 0.05f;
    m_fogInfo.decay = 0.99f;

    mUniformBuffers[KS::LIGHT_INFO_BUFFER] = std::make_unique<UniformBuffer>(device, "LIGHT INFO BUFFER", m_lightInfo, 1);
    mStorageBuffers[KS::DIR_LIGHT_BUFFER] = std::make_unique<StorageBuffer>(
        device, m_impl->m_resourceHeap.get(), *commandList, "DIRECTIONAL LIGHT BUFFER", m_directionalLights, false);
    mStorageBuffers[KS::MATERIAL_INFO_BUFFER] = std::make_unique<StorageBuffer>(
        device, m_impl->m_resourceHeap.get(), *commandList, "MATERIAL INFO BUFFER", material_cache, false);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER] = std::make_unique<StorageBuffer>(
        device, m_impl->m_resourceHeap.get(), *commandList, "POINT LIGHT BUFFER", m_pointLights, false);
    mStorageBuffers[KS::BOUNDING_BOX_BUFFER] = std::make_unique<StorageBuffer>(
        device, m_impl->m_resourceHeap.get(), *commandList, "BOUNDING BOX INFO", m_boundingBoxes, false);
    mStorageBuffers[KS::DRAW_INDICES] =
        std::make_unique<StorageBuffer>(device, m_impl->m_resourceHeap.get(), *commandList, "DRAW INDICES", m_drawIndices, true,
                                        StorageBuffer::COUNTER_RESOURCE | StorageBuffer::READBACK_RESOURCE);

    SetSkydome(device, *commandList, ResourceHandle<Texture>("assets/textures/cubemap.hdr"));
    GetModel(device, *commandList, ResourceHandle<Model>("assets/models/Cube.glb"));
    m_skyDomeMesh.second = ResourceHandle<Mesh>("assets\\models\\Cube");

    std::shared_ptr<Texture> deferredRendererTex[2][2];
    std::shared_ptr<Texture> deferredRendererDepthTex;
    std::shared_ptr<Texture> compute_resTex[2];
    std::shared_ptr<Texture> raytracingResTex[2];
    std::shared_ptr<Texture> finalRT[2];
    std::shared_ptr<Texture> superSampledGI[2];
    std::shared_ptr<Texture> diHistory[2];

    for (int i = 0; i < 2; i++)
    {
        deferredRendererTex[i][0] = std::make_shared<Texture>(
            device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
            Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.0f, 0.f, 0.f, 0.f),
            Formats::R32G32B32A32_UINT, "deferredRendererRTTexA " + std::to_string(i));
        deferredRendererTex[i][1] = std::make_shared<Texture>(
            device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
            Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(1.0f, 1.f, 1.f, 1.f),
            Formats::R32_FLOAT, "deferredRendererRTTexB " + std::to_string(i));

        compute_resTex[i] = std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                                      glm::vec4(0.0f, 0.0f, 0.0f, 1.f), Formats::R8G8B8A8_UNORM,
                                                      "PBRRTTexC " + std::to_string(i));
        raytracingResTex[i] = std::make_shared<Texture>(
            device, m_impl->m_resourceHeap.get(), device.GetSwapchainWidth(), device.GetSwapchainHeight(),
            Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.5f, 0.5f, 0.5f, 1.f),
            Formats::R8G8B8A8_UNORM, "RTX_RT " + std::to_string(i), -1, RAYTRACE_RT_SLOT + i);

        superSampledGI[i] = std::make_shared<Texture>(
            device, m_impl->m_resourceHeap.get(), device.GetSwapchainWidth(), device.GetSwapchainHeight(),
            Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.0f, 0.0f, 0.0f, 1.f),
            Formats::R32G32B32A32_FLOAT, "superSampledGI " + std::to_string(i));

        finalRT[i] = std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                               Texture::TextureFlags::RENDER_TARGET, glm::vec4(0.f, 0.f, 0.f, 0.f),
                                               Formats::R8G8B8A8_UNORM, "finalRT " + std::to_string(i));

        m_DIReservoirs[i] = std::make_shared<Texture>(
            device, m_impl->m_resourceHeap.get(), device.GetSwapchainWidth(), device.GetSwapchainHeight(),
            Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.0f, 0.0f, 0.0f, 1.f),
            Formats::R32G32B32A32_UINT, "DIReservoirA " + std::to_string(i));

        m_DIReservoirs[2 + i] = std::make_shared<Texture>(
            device, m_impl->m_resourceHeap.get(), device.GetSwapchainWidth(), device.GetSwapchainHeight(),
            Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.0f, 0.0f, 0.0f, 1.f),
            Formats::R32G32B32A32_FLOAT, "DIReservoirB " + std::to_string(i));

        diHistory[i] = std::make_shared<Texture>(
            device, m_impl->m_resourceHeap.get(), device.GetSwapchainWidth(), device.GetSwapchainHeight(),
            Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.0f, 0.0f, 0.0f, 1.f),
            Formats::R32G32B32A32_FLOAT, "diHistory " + std::to_string(i));

    }

    deferredRendererDepthTex = std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                                         Texture::TextureFlags::DEPTH_TEXTURE, glm::vec4(1.f),
                                                         Formats::R32_TYPELESS, "deferredRendererDepthTex");

    m_deferredRendererDepthStencil = std::make_shared<DepthStencil>(device, deferredRendererDepthTex);

    m_renderTargets[DEFERRED_RENDER] = std::make_shared<RenderTarget>();
    for (int i = 0; i < 2; i++)
    {
        m_renderTargets[DEFERRED_RENDER]->AddTexture(device, deferredRendererTex[0][i], deferredRendererTex[1][i],
                                                     "DEFERRED RENDERER" + std::to_string(i) + " ");
    }

    m_renderTargets[PBR_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[PBR_RENDER]->AddTexture(device, compute_resTex[0], compute_resTex[1], "PBR RENDER RES");

    m_renderTargets[RT_TEMPORAL_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[RT_TEMPORAL_RENDER]->AddTexture(device, raytracingResTex[0], raytracingResTex[1], "RAYTRACED RENDER RES");

    m_renderTargets[SUPER_SAMPLED_GI] = std::make_shared<RenderTarget>();
    m_renderTargets[SUPER_SAMPLED_GI]->AddTexture(device, superSampledGI[0], superSampledGI[1], "SUPERSAMPLED GI");

    m_renderTargets[SUPER_SAMPLED_DI] = std::make_shared<RenderTarget>();
    m_renderTargets[SUPER_SAMPLED_DI]->AddTexture(device, diHistory[0], diHistory[1], "SUPERSAMPLED DI");

    m_finalRT = std::make_shared<RenderTarget>();
    m_finalRT->AddTexture(device, finalRT[0], finalRT[1], "FINAL RENDER TARGET");
    m_BVH = std::make_unique<TLAS>();

    commandContext.Close();
}

KS::Scene::~Scene() {}

uint32_t KS::Scene::QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform, std::string name)
{
    auto commandContext = device.GetCommandContext();
    auto commandList = commandContext.m_commandList.get();

    auto* ptr = GetModel(device, *commandList, model);
    if (ptr)
    {
        for (auto node : ptr->nodes)
        {
            auto scene_transform = transform * node.transform;

            for (auto [mesh, material] : node.mesh_material_indices)
            {
                if (m_drawCallCount >= MAX_MESHES)
                {
                    LOG(Log::Severity::WARN, "Maximum number of draw calls {} has been reached. Command ignored.", MAX_MESHES);
                    break;
                }

                auto meshHandle = ptr->meshes[mesh];
                auto mat = ptr->materialIndices[material];

                std::shared_ptr<Mesh> meshPtr = GetMesh(meshHandle);
                std::string key = name + std::to_string(m_drawCallCount);

                auto AABB = meshPtr->GetLocalBounds();
                AABB = AABB.ApplyTransform(scene_transform);

                draw_queue[m_drawCallCount] = KS::DrawEntry(meshHandle, scene_transform, 0, mat, m_drawCallCount);
                m_boundingBoxes[m_drawCallCount] = AABB;
                mUniformBuffers[MODEL_INDEX_BUFFER]->Update(device, m_drawCallCount, m_drawCallCount);

                m_instanceData[m_drawCallCount].materialIndex = mat;
                ModelMat modelMat;
                modelMat.mModel = scene_transform;
                modelMat.mTransposed = glm::transpose(modelMat.mModel);
                m_instanceData[m_drawCallCount].modelMatrix = modelMat;
                m_BVH->AddInstance(&draw_queue[m_drawCallCount], scene_transform);
                m_drawCallCount++;
            }
        }

        for (int i = 0; i < ptr->pointLights.size(); i++)
        {
            auto light = ptr->pointLights[i];
            glm::vec4 hp = transform * light.mPosition;
            light.mPosition = hp;
            QueuePointLight(light);
        }

        for (int i = 0; i < ptr->dirLights.size(); i++)
        {
            QueueDirectionalLight(ptr->dirLights[i]);
        }
    }

    mStorageBuffers[INSTANCE_DATA_BUFFER]->Update(device, *commandList, m_impl->m_resourceHeap.get(), &m_instanceData[0],
                                                  m_drawCallCount);
    mStorageBuffers[DIR_LIGHT_BUFFER]->Update(device, *commandList, m_impl->m_resourceHeap.get(), m_directionalLights.data(),
                                              m_lightInfo.numDirLights);
    mStorageBuffers[POINT_LIGHT_BUFFER]->Update(device, *commandList, m_impl->m_resourceHeap.get(), m_pointLights.data(),
                                                m_lightInfo.numPointLights);
    mStorageBuffers[BOUNDING_BOX_BUFFER]->Update(device, *commandList, m_impl->m_resourceHeap.get(), m_boundingBoxes);

    commandContext.Close();
    m_updateScene = true;
    return m_drawCallCount;
}

void KS::Scene::ApplyModelTransform(uint32_t meshId, const glm::mat4& transfrom)
{
    auto& entry = draw_queue[meshId];
    auto& modelMatrix = m_instanceData[entry.modelIndex].modelMatrix;
    ModelMat modelMat;
    modelMat.mModel = modelMatrix.mModel * transfrom;
    modelMat.mTransposed = glm::transpose(modelMat.mModel);
    modelMatrix = modelMat;

    auto mesh = GetMesh(entry.meshHandle);
    auto AABB = mesh->GetLocalBounds();
    m_boundingBoxes[m_drawCallCount] = AABB;

    m_BVH->UpdateTransform(entry.tlasHandle, modelMat.mModel);
}

void KS::Scene::QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float att)
{
    PointLightInfo pLight;
    pLight.mColorAndIntensity = glm::vec4(color, intensity);
    pLight.mPosition = glm::vec4(position, 0.f);
    pLight.mLinearAttenuation = att;
    pLight.mQuadraticAttenuation = att;
    pLight.mConstantAttenuation = att;
    m_pointLights[m_lightInfo.numPointLights] = pLight;
    m_lightInfo.numPointLights++;
    m_updatePointLights = true;
    SetUpdateSuperSampler();
}

void KS::Scene::QueuePointLight(PointLightInfo info)
{
    m_pointLights[m_lightInfo.numPointLights] = info;
    m_lightInfo.numPointLights++;
    m_updatePointLights = true;
    SetUpdateSuperSampler();
}

void KS::Scene::QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity)
{
    DirLightInfo dLight;
    dLight.mDir = glm::vec4(direction, 0.f);
    dLight.mColorAndIntensity = glm::vec4(color, intensity);
    m_directionalLights[m_lightInfo.numDirLights] = dLight;
    m_lightInfo.numDirLights++;
    m_updateDirLights = true;
    SetUpdateSuperSampler();
}

void KS::Scene::QueueDirectionalLight(DirLightInfo info)
{
    m_directionalLights[m_lightInfo.numDirLights] = info;
    m_lightInfo.numDirLights++;
    m_updateDirLights = true;
    SetUpdateSuperSampler();
}

void KS::Scene::SetAmbientLight(glm::vec3 color, float intensity)
{
    m_lightInfo.mAmbientAndIntensity = glm::vec4(color, intensity);
}

void KS::Scene::Tick(Device& device)
{
    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto frameIndex = device.GetFrameIndex();

    mUniformBuffers[LIGHT_INFO_BUFFER]->Update(device, m_lightInfo);

    if (m_updateDirLights)
        mStorageBuffers[DIR_LIGHT_BUFFER]->Update(device, *commandList, m_impl->m_resourceHeap.get(), m_directionalLights);
    if (m_updatePointLights)
        mStorageBuffers[POINT_LIGHT_BUFFER]->Update(device, *commandList, m_impl->m_resourceHeap.get(), m_pointLights);

    m_pathTracingInfo.frameIndex++;

    if (m_updateSupersampled)
    {
        m_renderTargets[SUPER_SAMPLED_GI]->Bind(*commandList, frameIndex, m_deferredRendererDepthStencil.get());
        m_renderTargets[SUPER_SAMPLED_GI]->Clear(*commandList, frameIndex);
        m_renderTargets[SUPER_SAMPLED_DI]->Bind(*commandList, frameIndex, m_deferredRendererDepthStencil.get());
        m_renderTargets[SUPER_SAMPLED_DI]->Clear(*commandList, frameIndex);

        m_pathTracingInfo.frameIndex = 0;
        m_updateSupersampled--;
    }

    mUniformBuffers[PATH_TRACING_BUFFER]->Update(device, m_pathTracingInfo);

    m_updateDirLights = m_updatePointLights = false;
    m_cameraUpdated = false;

    m_BVH->Build(device, *this, *commandList);

    m_cullInfo.boundingBoxCount = m_drawCallCount;

    commandContext.Close();
}

void KS::Scene::GetFinalRTInfo(Device& device, DXDescHeap* heap, uint32_t frameIndex, uint64_t& gpuPtr, uint32_t& width,
                               uint32_t& height)
{
    auto commandContext = device.GetCommandContext();
    auto commandList = commandContext.m_commandList.get();

    auto tex = m_finalRT->GetTexture(frameIndex, 0);
    tex->TransitionToRO(heap, *commandList);

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += static_cast<uint64_t>(tex->GetHandleIndex(true)) * m_impl->m_resourceHeap->GetDescriptorSize();

    gpuPtr = gpuHandle.ptr;
    width = m_finalRT->GetWidth();
    height = m_finalRT->GetHeight();
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
        pair.first = std::make_shared<Skydome>(device, m_impl->m_resourceHeap.get(), *skydomeTex.get());
        pair.second = skydomeTexture;
        m_skyDome = pair;
    }
}

const KS::Model* KS::Scene::GetModel(Device& device, DXCommandList& commandList, ResourceHandle<Model> model)
{
    // Cached result
    if (auto it = model_cache.find(model); it != model_cache.end())
    {
        return &it->second;
    }

    // Load result
    else if (auto fileread = FileIO::OpenReadStream(model.path))
    {
        Assimp::Importer importer;
        const aiScene* scene = nullptr;

        scene = importer.ReadFile(model.path, aiProcess_FindInstances | aiProcess_CalcTangentSpace);
        if (!scene)
        {
            LOG(Log::Severity::WARN, "Could not import cached model: {} ({})", model.path, importer.GetErrorString());
            return nullptr;
        }

        std::vector<ResourceHandle<Mesh>> mesh_paths;
        mesh_paths.reserve(scene->mNumMeshes);
        auto totalObjects = scene->mNumMeshes + scene->mNumTextures + scene->mNumMaterials;
        // Process all meshes
        {
            for (size_t i = 0; i < scene->mNumMeshes; i++)
            {
                auto mesh = Model::ProcessMesh(scene->mMeshes[i]);
                std::string mesh_name{};

                if (scene->mMeshes[i]->mName.length == 0)
                {
                    mesh_name = "mesh" + std::to_string(i);
                }
                else
                {
                    mesh_name = scene->mMeshes[i]->mName.C_Str();
                }
                auto output_path = (FileIO::Path(model.path).make_preferred().parent_path() / mesh_name);

                auto handle = ResourceHandle<Mesh>(output_path.string());
                std::shared_ptr<Mesh> meshPtr = GetMesh(handle);
                if (!meshPtr)
                {
                    meshPtr = std::make_shared<Mesh>(device, m_impl->m_resourceHeap.get(), commandList, mesh, mesh_name.c_str(),
                                                     static_cast<uint32_t>(mesh_cache.size()));
                    auto [obj, success] = mesh_cache.emplace(output_path.string(), std::move(meshPtr));
                }

                mesh_paths.emplace_back(output_path.string());

                LOG(Log::Severity::INFO, "Processed mesh {}/{}, object {}/{}", i + 1, scene->mNumMeshes, i + 1, totalObjects);
            }
        }

        std::vector<std::string> image_paths;
        auto out_dir = FileIO::Path(model.path).make_preferred().parent_path();
        image_paths.reserve(scene->mNumTextures);

        // Process all Images
        {
            auto images_out = out_dir / "textures";
            FileIO::MakeDirectory(images_out);

            for (size_t i = 0; i < scene->mNumTextures; i++)
            {
                auto ai_image = scene->mTextures[i];
                auto image = Model::ProcessImage(ai_image);

                std::string image_name{};

                if (scene->mTextures[i]->mFilename.length == 0)
                {
                    image_name = "texture" + std::to_string(i);
                }
                else
                {
                    image_name = scene->mTextures[i]->mFilename.C_Str();
                }

                auto output_path = (images_out / (image_name + ".png")).string();
                image_paths.emplace_back(output_path);

                if (std::filesystem::exists(output_path))
                {
                    continue;
                }

                auto output_file = FileIO::OpenWriteStream(output_path);
                auto compressed_data = SaveImageToPNG(image);

                if (output_file && compressed_data)
                {
                    auto ptr = compressed_data.value().GetView<char>().begin();
                    auto size = compressed_data.value().GetView<char>().count();

                    output_file.value().write(ptr, size);
                }
                else
                {
                    LOG(Log::Severity::WARN, "Failed to write output texture file {}", output_path);
                }

                LOG(Log::Severity::INFO, "Processed image {}/{}, object {}/{}", i + 1, scene->mNumTextures,
                    i + 1 + scene->mNumMeshes, totalObjects);
            }
        }

        std::vector<uint32_t> materialIndices;
        materialIndices.reserve(scene->mNumMaterials);

        // Process Materials
        {
            for (size_t i = 0; i < scene->mNumMaterials; i++)
            {
                auto m = scene->mMaterials[i];
                auto material = Model::ProcessMaterial(image_paths, m);

                materialIndices.emplace_back(static_cast<uint32_t>(m_materialCounter));
                auto baseTexHandle = material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME);
                auto normalTexHandle = material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::NORMAL_TEXTURE_NAME);
                auto emissiveTexHandle = material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::EMISSIVE_TEXTURE_NAME);
                auto roughMetHandle = material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::METALLIC_TEXTURE_NAME);
                auto occlusionHandle = material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::OCCLUSION_TEXTURE_NAME);

                MaterialInfo matInfo = GetMaterialInfo(material);
                auto baseTex = GetTexture(device, &commandList, *baseTexHandle, true);
                auto normalTex = GetTexture(device, &commandList, *normalTexHandle);
                auto emissiveTex = GetTexture(device, &commandList, *emissiveTexHandle, true);
                auto roughMetTex = GetTexture(device, &commandList, *roughMetHandle);
                auto occlusionTex = GetTexture(device, &commandList, *occlusionHandle);

                matInfo.colorTexIndex = baseTex->GetHandleIndex(true);
                matInfo.emissiveTexIndex = emissiveTex->GetHandleIndex(true);
                matInfo.normalTexIndex = normalTex->GetHandleIndex(true);
                matInfo.occlusionTexIndex = occlusionTex->GetHandleIndex(true);
                matInfo.metallicRoughnessTexIndex = roughMetTex->GetHandleIndex(true);

                material_cache[m_materialCounter] = matInfo;
                m_materialCounter++;

                LOG(Log::Severity::INFO, "Processed material {}/{}, object {}/{}", i + 1, scene->mNumMaterials,
                    i + 1 + scene->mNumMeshes + scene->mNumTextures, totalObjects);
            }
        }

        std::vector<PointLightInfo> pointLights;
        std::vector<DirLightInfo> dirLights;
        std::vector<Model::Node> nodes;
        std::vector<uint32_t> meshInstances = std::vector<uint32_t>(scene->mNumMeshes, 0);

        // Process Nodes
        {
            Model::ProcessNodesRecursive(nodes, dirLights, meshInstances, pointLights, scene, scene->mRootNode, glm::identity<glm::mat4>());
        }

        Model new_model{.nodes = std::move(nodes),
                        .meshes = std::move(mesh_paths),
                        .materialIndices = std::move(materialIndices),
                        .pointLights = std::move(pointLights),
                        .dirLights = std::move(dirLights)};

        auto [obj, success] = model_cache.emplace(model, std::move(new_model));

        mStorageBuffers[MATERIAL_INFO_BUFFER]->Update(device, commandList, m_impl->m_resourceHeap.get(), material_cache.data(),
                                                      m_materialCounter);

        return &obj->second;
    }

    return nullptr;
}

const std::shared_ptr<KS::Mesh> KS::Scene::GetMesh(ResourceHandle<Mesh> meshHandle) const
{
    if (auto it = mesh_cache.find(meshHandle); it != mesh_cache.end())
    {
        return it->second;
    }
    else
    {
        return nullptr;
    }
}

void KS::Scene::CreateBatches(Device& device, DXCommandList& list)
{
    // Sorting
    //   Sort only the valid prefix if m_drawIndices is larger than m_drawIndicesCount
    auto begin = m_drawIndices.begin();
    auto end = begin + m_culledIndicesCount;

    struct Key
    {
        uint32_t meshIndex;
        uint32_t materialIndex;
        uint32_t drawIndex;  // the original ia (index into draw_queue)
    };

    std::vector<Key> keys;
    keys.resize(m_culledIndicesCount);

    for (uint32_t i = 0; i < m_culledIndicesCount; ++i)
    {
        uint32_t ia = m_drawIndices[i];
        const DrawEntry& a = draw_queue[ia];
        keys[i] = {.meshIndex = GetMesh(a.meshHandle)->GetMeshIndex(), .materialIndex = a.materialIndex, .drawIndex = ia};
    }

    std::sort(keys.begin(), keys.end(),
              [](const Key& a, const Key& b)
              {
                  if (a.meshIndex != b.meshIndex) return a.meshIndex < b.meshIndex;
                  if (a.materialIndex != b.materialIndex) return a.materialIndex < b.materialIndex;
                  return a.drawIndex < b.drawIndex;
              });

    // write back sorted indices
    for (uint32_t i = 0; i < m_culledIndicesCount; ++i) m_drawIndices[i] = keys[i].drawIndex;

    // Batching
    batch_queue.clear();
    batch_queue.reserve(m_culledIndicesCount);

    for (uint32_t i = 0; i < m_culledIndicesCount; i++)
    {
        auto drawCallIndex = m_drawIndices[i];
        auto& drawCall = draw_queue[drawCallIndex];
        m_instanceData[i].modelMatrix.mModel = drawCall.modelMat;
        m_instanceData[i].modelMatrix.mTransposed = glm::transpose(drawCall.modelMat);
        m_instanceData[i].materialIndex = drawCall.materialIndex;
        drawCall.modelIndex = i;
    }

    mStorageBuffers[CULLED_INSTANCE_DATA_BUFFER]->Update(device, list, GetResourceHeap()->Get(), m_instanceData.data(),
                                                  m_culledIndicesCount);

    auto sameBatch = [&](DrawEntry& a, DrawEntry& b)
    {
        auto meshA = GetMesh(a.meshHandle);
        auto meshB = GetMesh(b.meshHandle);
        auto meshAIndex = meshA->GetMeshIndex();
        auto materialAIndex = a.materialIndex;
        auto meshBIndex = meshB->GetMeshIndex();
        auto materialBIndex = b.materialIndex;

        return meshAIndex == meshBIndex && materialAIndex == materialBIndex;
    };

    for (uint32_t i = 0; i < m_culledIndicesCount;)
    {
        uint32_t j = i + 1;
        while (j < m_culledIndicesCount && sameBatch(draw_queue[m_drawIndices[i]], draw_queue[m_drawIndices[j]]))
        {
            ++j;
        }

        auto drawCallIndex = m_drawIndices[i];
        auto mesh = GetMesh(draw_queue[drawCallIndex].meshHandle);
        batch_queue.push_back(
            BatchRange{.mesh = mesh, .materialIndex = draw_queue[drawCallIndex].materialIndex, .first = i, .count = j - i});

        i = j;
    }
}

std::shared_ptr<KS::Texture> KS::Scene::GetTexture(Device& device, DXCommandList* commandList, ResourceHandle<Texture> imgPath,
                                                   bool isSrgb)
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

        std::filesystem::path p(imgPath.path);
        auto fileName = p.stem().string();

        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Treat common HDR formats as HDR
        bool isHdr = (ext == ".hdr" || ext == ".exr");

        Formats format = isSrgb ? Formats::R8G8B8A8_UNORM_SRGB : Formats::R8G8B8A8_UNORM;
        format = isHdr ? Formats::R32G32B32A32_FLOAT : format;

        if (auto img = LoadImageFileFromMemory(imageContents.data(), imageContents.size(), fileName, format))
        {
            auto new_tex = std::make_shared<Texture>(device, m_impl->m_resourceHeap.get(), *commandList, img.value());
            AddToMipmapQueue(new_tex);
            auto [obj, success] = tex_cache.emplace(imgPath, std::move(new_tex));
            return obj->second;
        }
    }
    return nullptr;
}

DXDescHeap* KS::Scene::GetResourceHeap() const { return m_impl->m_resourceHeap.get(); }

void KS::Scene::UpdateDirLights(int index, DirLightInfo info)
{
    m_directionalLights[index] = info;
    m_updateDirLights = true;
    SetUpdateSuperSampler();
}

void KS::Scene::UpdatePointLights(int index, PointLightInfo info)
{
    m_pointLights[index] = info;
    m_updatePointLights = true;
    SetUpdateSuperSampler();
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