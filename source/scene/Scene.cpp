#include <scene/Scene.hpp>

#include <renderer/Shader.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>

#include <DXR/DXRHelper.h>
#include <DXR/nv_helpers_dx12/TopLevelASGenerator.h>
#include <DXR/nv_helpers_dx12/BottomLevelASGenerator.h>
#include <DXR/nv_helpers_dx12/RaytracingPipelineGenerator.h>
#include <DXR/nv_helpers_dx12/RootSignatureGenerator.h>
#include <DXR/nv_helpers_dx12/ShaderBindingTableGenerator.h>

#include <device/Device.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <resources/Model.hpp>
#include <resources/Texture.hpp>
#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace KS
{
struct Scene::Impl
{
public:
    Impl();
    ~Impl();
    struct ASBuffers
    {
        std::shared_ptr<DXResource> pScratch[2] = {nullptr, nullptr};
        std::shared_ptr<DXResource> pResult[2] = {nullptr, nullptr};
        std::shared_ptr<DXResource> pInstanceDesc[2] = {nullptr, nullptr};
    };

    ASBuffers m_BLBuffers[200];
    std::pair<std::shared_ptr<DXResource>, DirectX::XMMATRIX> m_instances[200];
    ASBuffers m_topLevelASBuffers;
    int m_BLCount = 0;
    DXHeapHandle m_BHVHandle[2];
    bool m_updateBVH = false;

    ComPtr<ID3D12RootSignature> m_raytracingSignature;

    nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
    nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper[2];
};
}  // namespace KS

KS::Scene::Scene(const Device& device)
{
    m_impl = std::make_unique<Impl>();
    m_pointLights = std::vector<PointLightInfo>(100);
    m_directionalLights = std::vector<DirLightInfo>(100);

    mStorageBuffers[MODEL_MAT_BUFFER] =
        std::make_unique<StorageBuffer>(device, "MODEL MATRIX RESOURCE", &m_modelMatrices[0], sizeof(ModelMat), 200, false);
    mStorageBuffers[MATERIAL_INFO_BUFFER] = std::make_unique<StorageBuffer>(
        device, "MATERIAL INFO RESOURCE", &m_materialInstances[0], sizeof(MaterialInfo), 200, false);
    mUniformBuffers[MODEL_INDEX_BUFFER] = std::make_unique<UniformBuffer>(device, "MODEL INDEX BUFFER", m_modelCount, 200);

    mUniformBuffers[KS::LIGHT_INFO_BUFFER] = std::make_unique<UniformBuffer>(device, "LIGHT INFO BUFFER", m_lightInfo, 1);
    mUniformBuffers[KS::FOG_INFO_BUFFER] = std::make_unique<UniformBuffer>(device, "FOG INFO BUFFER", m_fogInfo, 1);
    mStorageBuffers[KS::DIR_LIGHT_BUFFER] =
        std::make_unique<StorageBuffer>(device, "DIRECTIONAL LIGHT BUFFER", m_directionalLights, false);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER] =
        std::make_unique<StorageBuffer>(device, "POINT LIGHT BUFFER", m_pointLights, false);

    m_fogInfo.fogColor = glm::vec3(1.f, 1.f, 1.f);
    m_fogInfo.fogDensity = 0.6f;
    m_fogInfo.exposure = 0.15f;
    m_fogInfo.lightShaftNumberSamples = 132;
    m_fogInfo.sourceMipNumber = 2;
    m_fogInfo.weight = 0.05f;
    m_fogInfo.decay = 0.99f;
}

KS::Scene::~Scene() {}

void KS::Scene::QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform, std::string name)
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
                    LOG(Log::Severity::WARN, "Maximum number of meshes {} has been reached. Command ignored.", 200);
                    return;
                }

                draw_queue[name] = KS::DrawEntry(ptr->meshes[mesh], ptr->materials[material], m_modelCount, scene_transform);

                auto mat = ptr->materials[material];
                auto meshHandle = ptr->meshes[mesh];

                ModelMat modelMat;
                modelMat.mModel = scene_transform;
                modelMat.mTransposed = glm::transpose(modelMat.mModel);
                m_modelMatrices[m_modelCount] = modelMat;

                MaterialInfo matInfo = GetMaterialInfo(ptr->materials[material]);
                auto baseTex =
                    GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME));
                auto normalTex =
                    GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::NORMAL_TEXTURE_NAME));
                auto emissiveTex =
                    GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::EMISSIVE_TEXTURE_NAME));
                auto roughMetTex =
                    GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::METALLIC_TEXTURE_NAME));
                auto occlusionTex =
                    GetTexture(device, *mat.GetParameter<ResourceHandle<Texture>>(MaterialConstants::OCCLUSION_TEXTURE_NAME));

                matInfo.useColorTex = baseTex != nullptr;
                matInfo.useEmissiveTex = emissiveTex != nullptr;
                matInfo.useNormalTex = normalTex != nullptr;
                matInfo.useOcclusionTex = occlusionTex != nullptr;
                matInfo.useMetallicRoughnessTex = roughMetTex != nullptr;

                mUniformBuffers[MODEL_INDEX_BUFFER]->Update(device, m_modelCount, m_modelCount);

                m_materialInstances[m_modelCount] = matInfo;
                m_modelCount++;
            }
        }
    }

    mStorageBuffers[MODEL_MAT_BUFFER]->Update(device, &m_modelMatrices[0], m_modelCount);
    mStorageBuffers[MATERIAL_INFO_BUFFER]->Update(device, &m_materialInstances[0], m_modelCount);
    mUniformBuffers[LIGHT_INFO_BUFFER]->Update(device, m_lightInfo);
    mStorageBuffers[DIR_LIGHT_BUFFER]->Update(device, m_directionalLights);
    mStorageBuffers[POINT_LIGHT_BUFFER]->Update(device, m_pointLights);
}

void KS::Scene::ApplyModelTransform(Device& device, std::string name, const glm::mat4& transfrom)
{ 
    auto& entry = draw_queue[name];
    ModelMat modelMat;
    modelMat.mModel = m_modelMatrices[entry.modelIndex].mModel * transfrom;
    modelMat.mTransposed = glm::transpose(modelMat.mModel);
    m_modelMatrices[entry.modelIndex] = modelMat;
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

void KS::Scene::Tick(Device& device)
{
    if (!m_impl->m_updateBVH)
    {
        memset(m_impl->m_BLBuffers, 0, 200 * sizeof(Impl::ASBuffers));
        m_impl->m_BLCount = 0;
    }

    int i =0;
    auto cpuFrameIndex = device.GetCPUFrameIndex();
    for (const auto& draw_entry : draw_queue)
    {
        const Mesh* mesh = GetMesh(device, draw_entry.second.mesh);
        auto baseTex = GetTexture(
            device, *draw_entry.second.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME));

        if (mesh == nullptr || baseTex == nullptr) continue;

        CreateBVHBotomLevelInstance(device, draw_entry.second, m_impl->m_updateBVH, i, cpuFrameIndex);
        i++;
    }
    CreateTopLevelAS(device, m_impl->m_updateBVH, cpuFrameIndex);

    mStorageBuffers[MODEL_MAT_BUFFER]->Update(device, &m_modelMatrices[0], m_modelCount);
    mUniformBuffers[KS::FOG_INFO_BUFFER]->Update(device, m_fogInfo);
}

const KS::Mesh* KS::Scene::GetMesh(const Device& device, ResourceHandle<Mesh> mesh)
{  // Cached result
    if (auto it = mesh_cache.find(mesh); it != mesh_cache.end())
    {
        return &it->second;
    }

    // Load result
    else if (auto fileread = FileIO::OpenReadStream(mesh.path))
    {
        BinaryLoader bin{fileread.value()};
        MeshData data{};

        bin(data);

        auto [it, success] = mesh_cache.emplace(mesh, Mesh(device, data));
        return &it->second;
    }
    return nullptr;
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

        auto [it, success] = model_cache.emplace(model, std::move(new_model));
        return &it->second;
    }
    return nullptr;
}

std::shared_ptr<KS::Texture> KS::Scene::GetTexture(Device& device, ResourceHandle<Texture> imgPath)
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

KS::MeshSet KS::Scene::GetMeshSet(Device& device, int index)
{
    auto draw_entry = (*std::next(draw_queue.begin(), index)).second;

    MeshSet meshSet;
    meshSet.mesh = GetMesh(device, draw_entry.mesh);
    meshSet.baseTex = GetTexture(
        device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME));
    meshSet.normalTex = GetTexture(
        device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::NORMAL_TEXTURE_NAME));
    meshSet.emissiveTex = GetTexture(
        device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::EMISSIVE_TEXTURE_NAME));
    meshSet.roughMetTex = GetTexture(
        device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::METALLIC_TEXTURE_NAME));
    meshSet.occlusionTex = GetTexture(
        device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::OCCLUSION_TEXTURE_NAME));
    meshSet.modelIndex = draw_entry.modelIndex;

    return meshSet;
}

void KS::Scene::CreateBottomLevelAS(const Device& device, const Mesh* mesh, int cpuFrame)
{
    nv_helpers_dx12::BottomLevelASGenerator bottomLevelASGen;
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());

    using namespace MeshConstants;
    auto positions = mesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
    auto positionsResource = reinterpret_cast<DXResource*>(positions->GetRawResource());
    auto indices = mesh->GetAttribute(ATTRIBUTE_INDICES_NAME);
    auto indicesResource = reinterpret_cast<DXResource*>(indices->GetRawResource());

    commandList->ResourceBarrier(*positionsResource->Get(), positionsResource->GetState(),
                                 D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    positionsResource->ChangeState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(*indicesResource->Get(), indicesResource->GetState(),
                                 D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    indicesResource->ChangeState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    DXGI_FORMAT indexFormat;
    switch (indices->GetBufferStride())
    {
        case sizeof(unsigned char):
            indexFormat = DXGI_FORMAT_R8_UINT;
            break;
        case sizeof(unsigned short):
            indexFormat = DXGI_FORMAT_R16_UINT;
            break;
        case sizeof(unsigned int):
            indexFormat = DXGI_FORMAT_R32_UINT;
            break;
        default:
            indexFormat = DXGI_FORMAT_R16_UINT;
            break;
    }

    bottomLevelASGen.AddVertexBuffer(positionsResource->Get(), 0, static_cast<uint32_t>(positions->GetElementCount()),
                                     sizeof(glm::vec3), indicesResource->Get(), 0,
                                     static_cast<uint32_t>(indices->GetElementCount()), indexFormat, nullptr, 0, true);

    // The AS build requires some scratch space to store temporary information.
    // The amount of scratch memory is dependent on the scene complexity.
    UINT64 scratchSizeInBytes = 0;
    // The final AS also needs to be stored in addition to the existing vertex
    // buffers. It size is also dependent on the scene complexity.
    UINT64 resultSizeInBytes = 0;

    bottomLevelASGen.ComputeASBufferSizes(engineDevice, false, &scratchSizeInBytes, &resultSizeInBytes);

    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    heapProps.CreationNodeMask = 0;
    heapProps.VisibleNodeMask = 0;

    m_impl->m_BLBuffers[m_impl->m_BLCount].pScratch[cpuFrame] =
        std::make_shared<DXResource>(engineDevice, heapProps, bufDesc, nullptr, "SCRATCH  BUFFER");

    bufDesc.Width = resultSizeInBytes;
    m_impl->m_BLBuffers[m_impl->m_BLCount].pResult[cpuFrame] = std::make_shared<DXResource>(
        engineDevice, heapProps, bufDesc, nullptr, "RESULT  BUFFER", D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    bottomLevelASGen.Generate(commandList->GetCommandList().Get(),
                              m_impl->m_BLBuffers[m_impl->m_BLCount].pScratch[cpuFrame]->Get(),
                              m_impl->m_BLBuffers[m_impl->m_BLCount].pResult[cpuFrame]->Get(), false, nullptr);

    commandList->TrackResource(m_impl->m_BLBuffers[m_impl->m_BLCount].pScratch[cpuFrame]->GetResource());
    commandList->TrackResource(m_impl->m_BLBuffers[m_impl->m_BLCount].pResult[cpuFrame]->GetResource());
}

void KS::Scene::CreateBVHBotomLevelInstance(const Device& device, const DrawEntry& draw_entry, bool updateOnly, int entryIndex,
                                            int cpuFrame)
{
    if (!updateOnly)
    {
        const Mesh* mesh = GetMesh(device, draw_entry.mesh);
        if (!mesh) return;

        CreateBottomLevelAS(device, mesh, cpuFrame);

        m_impl->m_instances[m_impl->m_BLCount].first = m_impl->m_BLBuffers[m_impl->m_BLCount].pResult[cpuFrame];
        m_impl->m_instances[m_impl->m_BLCount].second = Conversion::GLMToXMMATRIX(draw_entry.modelMat);

        m_impl->m_topLevelASGenerator.AddInstance(m_impl->m_instances[m_impl->m_BLCount].first->Get(),
                                                  m_impl->m_instances[m_impl->m_BLCount].second,
                                                  static_cast<uint32_t>(m_impl->m_BLCount), static_cast<uint32_t>(0));

        m_impl->m_BLCount++;
    }
    else
    {
        m_impl->m_instances[entryIndex].second = Conversion::GLMToXMMATRIX(draw_entry.modelMat);
    }
}

void KS::Scene::CreateTopLevelAS(const Device& device, bool updateOnly, int cpuFrame)
{
    ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    if (!updateOnly)
    {
        // As for the bottom-level AS, the building the AS requires some scratch space
        // to store temporary data in addition to the actual AS. In the case of the
        // top-level AS, the instance descriptors also need to be stored in GPU
        // memory. This call outputs the memory requirements for each (scratch,
        // results, instance descriptors) so that the application can allocate the
        // corresponding memory
        UINT64 scratchSize, resultSize, instanceDescsSize;

        m_impl->m_topLevelASGenerator.ComputeASBufferSizes(engineDevice, true, &scratchSize, &resultSize, &instanceDescsSize);

        //// Create the scratch and result buffers. Since the build is all done on GPU,
        //// those can be allocated on the default heap

        auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        heapProps.CreationNodeMask = 0;
        heapProps.VisibleNodeMask = 0;

        m_impl->m_topLevelASBuffers.pScratch[cpuFrame] =
            std::make_shared<DXResource>(engineDevice, heapProps, bufDesc, nullptr, "TOP LEVEL BVH SCRATCH");
        bufDesc.Width = resultSize;
        m_impl->m_topLevelASBuffers.pResult[cpuFrame] =
            std::make_shared<DXResource>(engineDevice, heapProps, bufDesc, nullptr, "TOP LEVEL BVH RESULT",
                                         D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

        // The buffer describing the instances: ID, shader binding information,
        // matrices ... Those will be copied into the buffer by the helper through
        // mapping, so the buffer has to be allocated on the upload heap.
        bufDesc.Width = instanceDescsSize;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        heapProps.CreationNodeMask = 0;
        heapProps.VisibleNodeMask = 0;
        m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame] =
            std::make_shared<DXResource>(engineDevice, heapProps, bufDesc, nullptr, "TOP LEVEL BVH DESC");

        commandList->ResourceBarrier(*m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->Get(),
                                     m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->GetState(),
                                     D3D12_RESOURCE_STATE_GENERIC_READ);
        m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->ChangeState(D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    // After all the buffers are allocated, or if only an update is required, we
    // can build the acceleration structure. Note that in the case of the update
    // we also pass the existing AS as the 'previous' AS, so that it can be
    // refitted in place.
    m_impl->m_topLevelASGenerator.Generate(
        commandList->GetCommandList().Get(), m_impl->m_topLevelASBuffers.pScratch[cpuFrame]->Get(),
        m_impl->m_topLevelASBuffers.pResult[cpuFrame]->Get(), m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->Get(),
        updateOnly, m_impl->m_topLevelASBuffers.pResult[cpuFrame]->Get());

    if (!updateOnly)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location =
            m_impl->m_topLevelASBuffers.pResult[cpuFrame]->Get()->GetGPUVirtualAddress();

        m_impl->m_BHVHandle[cpuFrame] =
            reinterpret_cast<DXDescHeap*>(device.GetResourceHeap())
                ->AllocateResource(m_impl->m_topLevelASBuffers.pResult[cpuFrame].get(), &srvDesc, BVH_SLOT + cpuFrame);
    }

    commandList->TrackResource(m_impl->m_topLevelASBuffers.pScratch[cpuFrame]->GetResource());
    commandList->TrackResource(m_impl->m_topLevelASBuffers.pResult[cpuFrame]->GetResource());
    commandList->TrackResource(m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->GetResource());
}

KS::Scene::Impl::Impl() {}

KS::Scene::Impl::~Impl() {}
