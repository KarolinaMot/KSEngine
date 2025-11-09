#include <scene/Scene.hpp>

#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>
#include <renderer/DX12/Helpers/DXShaderTable.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>

#include <device/Device.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <renderer/TLAS.hpp>
#include <resources/Model.hpp>
#include <resources/Texture.hpp>
#include <resources/Skydome.hpp>
#include <resources/Image.hpp>
#include <resources/Mesh.hpp>

class KS::Scene::Impl
{
public:
    std::shared_ptr<DXDescHeap> m_resourceHeap;
    std::unique_ptr<DXShaderTable> m_shaderTable[FRAME_BUFFER_COUNT];
};


KS::Scene::Scene() {}

KS::Scene::Scene(Device& device, std::string name, ScenesToChoose id)
{
    m_name = name;
    m_identifyingIndex = id;

    m_impl = std::make_unique<Impl>();

    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    std::string heapName = m_name + " resource heap";

    m_impl->m_resourceHeap =
        DXDescHeap::Construct(engineDevice, RESOURCE_HEAP_SIZE, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                              Conversion::utf8_to_wide(heapName).c_str(),
                              OTHER_RESOURCES_START, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    m_pointLights = std::vector<PointLightInfo>(100);
    m_directionalLights = std::vector<DirLightInfo>(100);

    SetSkydome(device, *commandList, ResourceHandle<Texture>("assets/textures/cubemap.hdr"));
    m_skyDomeMesh.second = ResourceHandle<Mesh>("assets\\models\\Cube\\meshes\\Cube.bin");
    m_skyDomeMesh.first = GetMesh(device, commandList.get(), m_skyDomeMesh.second);

    CameraMats cam{};

    mStorageBuffers[MODEL_MAT_BUFFER] = std::make_unique<StorageBuffer>(device, m_impl->m_resourceHeap.get(), * commandList, "MODEL MATRIX RESOURCE",
                                        &m_modelMatrices[0], static_cast<uint32_t>(sizeof(ModelMat)), MAX_MESHES, false);
    mStorageBuffers[MATERIAL_INFO_BUFFER] = std::make_unique<StorageBuffer>(
        device, m_impl->m_resourceHeap.get(), *commandList, "MATERIAL INFO RESOURCE", &m_materialInstances[0], static_cast<uint32_t>(sizeof(MaterialInfo)), MAX_MESHES, false);
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
    mStorageBuffers[KS::DIR_LIGHT_BUFFER] = std::make_unique<StorageBuffer>(
        device, m_impl->m_resourceHeap.get(), *commandList, "DIRECTIONAL LIGHT BUFFER", m_directionalLights, false);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER] = std::make_unique<StorageBuffer>(
        device, m_impl->m_resourceHeap.get(), *commandList, "POINT LIGHT BUFFER", m_pointLights, false);

    std::shared_ptr<Texture> deferredRendererTex[2][4];
    std::shared_ptr<Texture> deferredRendererDepthTex;
    std::shared_ptr<Texture> compute_resTex[2];
    std::shared_ptr<Texture> raytracingResTex[2];
    std::shared_ptr<Texture> lightRenderingTex[2];
    std::shared_ptr<Texture> lightShaftTex[2];
    std::shared_ptr<Texture> upscaledLightShaftTex[2];

    for (int i = 0; i < 2; i++)
    {
        deferredRendererTex[i][0] =
            std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.0f, 0.f, 0.f, 1.f),
            Formats::R8G8B8A8_UNORM, "deferredRendererRTTexA " + std::to_string(i));
        deferredRendererTex[i][1] =
            std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.0f, 0.f, 0.f, 1.f),
            Formats::R32G32B32A32_FLOAT, "deferredRendererRTTexB " + std::to_string(i));
        deferredRendererTex[i][2] =
            std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.0f, 0.f, 0.f, 1.f),
            Formats::R8G8B8A8_UNORM, "deferredRendererRTTexC " + std::to_string(i));
        deferredRendererTex[i][3] =
            std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), Formats::R8G8B8A8_UNORM, "deferredRendererRTTexD " + std::to_string(i));

        compute_resTex[i] = std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                                      glm::vec4(0.5f, 0.5f, 0.5f, 1.f), Formats::R8G8B8A8_UNORM,
                                                      "PBRRTTexC " + std::to_string(i));
        raytracingResTex[i] =
            std::make_shared<Texture>(device, m_impl->m_resourceHeap.get(), device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.5f, 0.5f, 0.5f, 1.f),
            Formats::R8G8B8A8_UNORM, "RTX_RT " + std::to_string(i), -1, RAYTRACE_RT_SLOT + i);

        lightRenderingTex[i] = std::make_shared<Texture>(
            device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
            Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.f, 0.f, 0.f, 0.f),
            Formats::R32G32B32A32_FLOAT, "lightRenderingTex " + std::to_string(i), static_cast<std::uint16_t>(4));

        lightShaftTex[i] = std::make_shared<Texture>(device, device.GetSwapchainWidth() / 4, device.GetSwapchainHeight() / 4,
                                                     Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                                     glm::vec4(0.f, 0.f, 0.f, 0.f), Formats::R32G32B32A32_FLOAT,
                                                     "lightShaftTex " + std::to_string(i));

        upscaledLightShaftTex[i] =
            std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.f, 0.f, 0.f, 0.f),
            Formats::R8G8B8A8_UNORM, "upscaledLightShaftTex " + std::to_string(i));
    }


    deferredRendererDepthTex =
        std::make_shared<Texture>(device, device.GetSwapchainWidth(), device.GetSwapchainHeight(), Texture::TextureFlags::DEPTH_TEXTURE, glm::vec4(1.f),
        Formats::D32_FLOAT, "deferredRendererDepthTex");

    m_deferredRendererDepthStencil = std::make_shared<DepthStencil>(device, deferredRendererDepthTex);

    m_renderTargets[DEFERRED_RENDER] = std::make_shared<RenderTarget>();
    for (int i = 0; i < 4; i++)
    {
        m_renderTargets[DEFERRED_RENDER]->AddTexture(device, deferredRendererTex[0][i], deferredRendererTex[1][i],
                                                     "DEFERRED RENDERER" + std::to_string(i) + " ");
    }

    m_renderTargets[PBR_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[PBR_RENDER]->AddTexture(device, compute_resTex[0], compute_resTex[1], "PBR RENDER RES");

    m_renderTargets[RT_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[RT_RENDER]->AddTexture(device, raytracingResTex[0], raytracingResTex[1], "RAYTRACED RENDER RES");

    m_renderTargets[LIGHT_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[LIGHT_RENDER]->AddTexture(device, lightRenderingTex[0], lightRenderingTex[1], "LIGHT RENDER RES");

    m_renderTargets[LIGHT_SHAFT_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[LIGHT_SHAFT_RENDER]->AddTexture(device, lightShaftTex[0], lightShaftTex[1], "LIGHT SHAFT RENDER RES");

    m_renderTargets[UPSCALING_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[UPSCALING_RENDER]->AddTexture(device, upscaledLightShaftTex[0], upscaledLightShaftTex[1],
                                                  "UPSCALED LIGHT SHAFT RENDER RES");

    m_BVH = std::make_unique<TLAS>();
    InitializeShaderTable();



    commandContext.Close();
}

KS::Scene::~Scene() {}

void KS::Scene::QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform, std::string name)
{
    auto commandContext = device.GetCommandContext();
    auto commandList = commandContext.m_commandList.get();

    auto* ptr = GetModel(model);
    if (ptr)
    {
        for (auto node : ptr->nodes)
        {
            auto scene_transform = transform * node.transform;

            for (auto [mesh, material] : node.mesh_material_indices)
            {
                if (m_modelCount >= MAX_MESHES)
                {
                    LOG(Log::Severity::WARN, "Maximum number of meshes {} has been reached. Command ignored.", MAX_MESHES);
                    break;
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
                if (!baseTex)
                    LOG(Log::Severity::WARN, "Empty texture warning.");

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

                mUniformBuffers[MODEL_INDEX_BUFFER]->Update(device, m_modelCount, m_modelCount);

                m_materialInstances[m_modelCount] = matInfo;
                m_modelCount++;
                m_BVH->AddInstance(&draw_queue[key], modelMat.mModel);
            }
        }

        //for (int i = 0; i < ptr->pointLights.size(); i++)
        //{
        //    auto light = ptr->pointLights[i];
        //    glm::vec4 hp = transform * light.mPosition; 
        //    light.mPosition = hp;
        //    QueuePointLight(light);
        //}

        for (int i = 0; i < ptr->dirLights.size(); i++)
        {
            QueueDirectionalLight(ptr->dirLights[i]);
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
}

void KS::Scene::QueuePointLight(PointLightInfo info)
{
    m_pointLights[m_lightInfo.numPointLights] = info;
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

void KS::Scene::QueueDirectionalLight(DirLightInfo info)
{
    m_directionalLights[m_lightInfo.numDirLights] = info;
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
    m_BVH->Build(device, *this, *commandList);

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

        auto meshPtr = std::make_shared<Mesh>(device, m_impl->m_resourceHeap.get(), *commandList, data,
                                              mesh_name_extracted.c_str(),
                                              static_cast<uint32_t>(mesh_cache.size()));

        auto [obj, success] = mesh_cache.emplace(meshHandle, std::move(meshPtr));

        return obj->second;
    }
    else
    {
        LOG(Log::Severity::WARN, "Model path ( {} ) was not found.", meshHandle.path);
        return nullptr;
    }
}

void KS::Scene::InitializeShaderTable()
{
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        m_impl->m_shaderTable[i] = std::make_unique<DXShaderTable>();

        std::vector<void*> heapPointers;
        heapPointers.reserve(15);

        D3D12_GPU_DESCRIPTOR_HANDLE outputHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        outputHandle.ptr += static_cast<uint64_t>((RAYTRACE_RT_SLOT + i) * m_impl->m_resourceHeap->GetDescriptorSize());

        D3D12_GPU_DESCRIPTOR_HANDLE tlasHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        tlasHandle.ptr += static_cast<uint64_t>(BVH_SLOT+i) * m_impl->m_resourceHeap->GetDescriptorSize();

        D3D12_GPU_DESCRIPTOR_HANDLE materialHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        materialHandle.ptr += static_cast<uint64_t>(GetStorageBuffer(MATERIAL_INFO_BUFFER)->GetHandle(true)) *
                                m_impl->m_resourceHeap->GetDescriptorSize();

        D3D12_GPU_DESCRIPTOR_HANDLE normalsHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        normalsHandle.ptr += static_cast<uint64_t>(NORMALS_SLOT) * m_impl->m_resourceHeap->GetDescriptorSize();

        D3D12_GPU_DESCRIPTOR_HANDLE modelMatHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        modelMatHandle.ptr +=
            static_cast<uint64_t>(GetStorageBuffer(MODEL_MAT_BUFFER)->GetHandle(true)) * m_impl->m_resourceHeap->GetDescriptorSize();

        D3D12_GPU_DESCRIPTOR_HANDLE dirLights = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        dirLights.ptr +=
            static_cast<uint64_t>(GetStorageBuffer(DIR_LIGHT_BUFFER)->GetHandle(true)) * m_impl->m_resourceHeap->GetDescriptorSize();

        D3D12_GPU_DESCRIPTOR_HANDLE pointLights = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        pointLights.ptr +=
            static_cast<uint64_t>(GetStorageBuffer(POINT_LIGHT_BUFFER)->GetHandle(true)) * m_impl->m_resourceHeap->GetDescriptorSize();

        D3D12_GPU_DESCRIPTOR_HANDLE textures = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();

        D3D12_GPU_DESCRIPTOR_HANDLE indexHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        indexHandle.ptr += static_cast<uint64_t>(INDICES_SLOT) * m_impl->m_resourceHeap->GetDescriptorSize();

        D3D12_GPU_DESCRIPTOR_HANDLE vPosHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        vPosHandle.ptr += static_cast<uint64_t>(VPOS_SLOT) * m_impl->m_resourceHeap->GetDescriptorSize();

        D3D12_GPU_DESCRIPTOR_HANDLE uvHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        uvHandle.ptr += static_cast<uint64_t>(UVS_SLOT) * m_impl->m_resourceHeap->GetDescriptorSize();

        D3D12_GPU_DESCRIPTOR_HANDLE tanHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        tanHandle.ptr += static_cast<uint64_t>(TAN_SLOT) * m_impl->m_resourceHeap->GetDescriptorSize();

        auto skyboxHandleID = GetSkydome().first->GetHandleIndex(true);
        D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle = m_impl->m_resourceHeap->Get()->GetGPUDescriptorHandleForHeapStart();
        skyboxHandle.ptr += static_cast<uint64_t>(skyboxHandleID) * m_impl->m_resourceHeap->GetDescriptorSize();

        heapPointers.push_back(reinterpret_cast<void*>(outputHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(tlasHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(GetUniformBuffer(CAMERA_MAT_BUFFER)->GetGPUAddress(0, i)));
        heapPointers.push_back(reinterpret_cast<void*>(materialHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(modelMatHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(dirLights.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(pointLights.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(skyboxHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(normalsHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(indexHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(vPosHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(uvHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(tanHandle.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(textures.ptr));
        heapPointers.push_back(reinterpret_cast<void*>(GetUniformBuffer(LIGHT_INFO_BUFFER)->GetGPUAddress(0, i)));

        m_impl->m_shaderTable[i]->AddRayGen(L"RayGen", heapPointers.data(),
                                            static_cast<UINT>(sizeof(void*) * heapPointers.size()));

        m_impl->m_shaderTable[i]->AddHitGroup(L"HitGroup", heapPointers.data(),
                                              static_cast<UINT>(sizeof(void*) * heapPointers.size()));

        m_impl->m_shaderTable[i]->AddMiss(L"Miss", heapPointers.data(), static_cast<UINT>(sizeof(void*) * heapPointers.size()));
    }
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

        std::filesystem::path p(imgPath.path);
        auto fileName = p.stem().string();
        if (auto img = LoadImageFileFromMemory(imageContents.data(), imageContents.size(), fileName))
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

DXShaderTable* KS::Scene::GetShaderTable(int index) const { return m_impl->m_shaderTable[index].get(); }

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