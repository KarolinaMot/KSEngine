#include "../ModelRenderer.hpp"
#include "../Shader.hpp"
#include "../ShaderInputCollection.hpp"
#include "Helpers/DXPipeline.hpp"
#include "Helpers/DXDescHeap.hpp"
#include "Helpers/DXCommandList.hpp"
#include "Helpers/DX12Conversion.hpp"
#include "Helpers/DXHeapHandle.hpp"
#include "Helpers/DXDescHeap.hpp"
#include "Helpers/DX12Common.hpp"

#include <DXR/DXRHelper.h>
#include <DXR/nv_helpers_dx12/TopLevelASGenerator.h>
#include <DXR/nv_helpers_dx12/BottomLevelASGenerator.h>
#include <DXR/nv_helpers_dx12/RaytracingPipelineGenerator.h>
#include <DXR/nv_helpers_dx12/RootSignatureGenerator.h>
#include <DXR/nv_helpers_dx12/ShaderBindingTableGenerator.h>

#include <device/Device.hpp>
#include <fileio/FileIO.hpp>
#include <fileio/Serialization.hpp>
#include <resources/Texture.hpp>
#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <fileio\ResourceHandle.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/StorageBuffer.hpp>
#include <renderer/UniformBuffer.hpp>

class KS::ModelRenderer::Impl
{
public:
    struct ASBuffers
    {
        std::shared_ptr<DXResource> pScratch[2] = {nullptr, nullptr};
        std::shared_ptr<DXResource> pResult[2] = {nullptr, nullptr};
        std::shared_ptr<DXResource> pInstanceDesc[2] = {nullptr, nullptr};
    };
    struct HitInfo
    {
        glm::vec4 colorAndDistance;
    };

    void InitializeRaytracer(const Device& device, const UniformBuffer* cameraBuffer);

    nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
    nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper[2];

    ASBuffers m_BLBuffers[200];
    std::pair<std::shared_ptr<DXResource>, DirectX::XMMATRIX> m_instances[200];
    ASBuffers m_topLevelASBuffers;
    int m_BLCount = 0;
    DXHeapHandle m_BHVHandle[2];
    bool m_updateBVH = false;

    ComPtr<ID3D12RootSignature> m_raytracingSignature;
    ComPtr<ID3D12StateObject> m_rtPipeline;
    ComPtr<ID3D12StateObjectProperties> m_rtStateObjectProps;
    ComPtr<ID3D12Resource> m_sbtStorage[2];
    std::unique_ptr<UniformBuffer> m_frameIndex;

    
};

KS::ModelRenderer::ModelRenderer(const Device& device, std::shared_ptr<Shader> shader,
                                 const UniformBuffer* cameraBuffer)
    : SubRenderer(device, shader)
{
    m_impl = std::make_unique<Impl>();
    m_modelMatsBuffer = std::make_unique<StorageBuffer>(device, "MODEL MATRIX RESOURCE", &m_modelMatrices[0], sizeof(ModelMat), 200, false);
    m_materialInfoBuffer = std::make_unique<StorageBuffer>(device, "MATERIAL INFO RESOURCE", &m_materialInstances[0], sizeof(MaterialInfo), 200, false);
    m_modelIndexBuffer = std::make_unique<UniformBuffer>(device, "MODEL INDEX BUFFER", m_modelCount, 200);
    m_impl->InitializeRaytracer(device, cameraBuffer);
}

KS::ModelRenderer::~ModelRenderer()
{
}

void KS::ModelRenderer::QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform)
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

                draw_queue.emplace_back(
                    ptr->meshes[mesh],
                    ptr->materials[material],
                    m_modelCount,
                    scene_transform);

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

void KS::ModelRenderer::Render(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget,
                               std::shared_ptr<DepthStencil> depthStencil,
                               std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs, bool clearRenderTarget)
{
    for (int i = 0; i < inputs.size(); i++)
    {
        inputs[i].first->Bind(device, inputs[i].second);
    }

    renderTarget->Bind(device, depthStencil.get());
    if (clearRenderTarget)
    {
        renderTarget->Clear(device);
    }
    depthStencil->Clear(device);

    if (m_raytraced)
        Raytrace(device, cpuFrameIndex, renderTarget, depthStencil, inputs);
    else
        Rasterize(device, cpuFrameIndex, renderTarget, depthStencil, inputs);

    m_modelCount = 0;
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

std::shared_ptr<KS::Texture> KS::ModelRenderer::GetTexture(Device& device, ResourceHandle<Texture> imgPath)
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

KS::MaterialInfo KS::ModelRenderer::GetMaterialInfo(const KS::Material& material)
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

void KS::ModelRenderer::Raytrace(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget,
                                 std::shared_ptr<DepthStencil> depthStencil,
                                 std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs)
{
    if (!m_impl->m_updateBVH)
    {
        memset(m_impl->m_BLBuffers, 0, 200 * sizeof(Impl::ASBuffers));
        m_impl->m_BLCount = 0;
    }

    int i = 0;

    if (m_frameCount < 2)
    {
        for (const auto& draw_entry : draw_queue)
        {
            const Mesh* mesh = GetMesh(device, draw_entry.mesh);
            auto baseTex = GetTexture(
                device, *draw_entry.material.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME));

            if (mesh == nullptr || baseTex == nullptr) continue;

            CreateBVHBotomLevelInstance(device, draw_entry, m_impl->m_updateBVH, i, cpuFrameIndex);
            i++;
        }
        CreateTopLevelAS(device, m_impl->m_updateBVH, cpuFrameIndex);

    }

    m_impl->m_frameIndex->Update(device, cpuFrameIndex);

    // m_impl->m_frameIndexBuffer->Update(&cpuFrameIndex, sizeof(uint32_t), 0, cpuFrameIndex);
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    renderTarget->GetTexture(device, cpuFrameIndex)->TransitionToRW(device);

    // Setup the raytracing task
     D3D12_DISPATCH_RAYS_DESC desc = {};
    // The layout of the SBT is as follows: ray generation shader, miss
    // shaders, hit groups. As described in the CreateShaderBindingTable method,
    // all SBT entries of a given type have the same size to allow a fixed stride.

    // The ray generation shaders are always at the beginning of the SBT.
     uint32_t rayGenerationSectionSizeInBytes = m_impl->m_sbtHelper[cpuFrameIndex].GetRayGenSectionSize();
     desc.RayGenerationShaderRecord.StartAddress = m_impl->m_sbtStorage[cpuFrameIndex]->GetGPUVirtualAddress();
     desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

    // The miss shaders are in the second SBT section, right after the ray
    // generation shader. We have one miss shader for the camera rays and one
    // for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
    // also indicate the stride between the two miss shaders, which is the size
    // of a SBT entry
     uint32_t missSectionSizeInBytes = m_impl->m_sbtHelper[cpuFrameIndex].GetMissSectionSize();
     desc.MissShaderTable.StartAddress =
         m_impl->m_sbtStorage[cpuFrameIndex]->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
     desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
     desc.MissShaderTable.StrideInBytes = m_impl->m_sbtHelper[cpuFrameIndex].GetMissEntrySize();

    // The hit groups section start after the miss shaders. In this sample we
    // have one 1 hit group for the triangle
     uint32_t hitGroupsSectionSize = m_impl->m_sbtHelper[cpuFrameIndex].GetHitGroupSectionSize();
     desc.HitGroupTable.StartAddress =
         m_impl->m_sbtStorage[cpuFrameIndex]->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
     desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
     desc.HitGroupTable.StrideInBytes = m_impl->m_sbtHelper[cpuFrameIndex].GetHitGroupEntrySize();

    // Dimensions of the image to render, identical to a kernel launch dimension
     desc.Width = device.GetWidth();
     desc.Height = device.GetHeight();
     desc.Depth = 1;

    // Bind the raytracing pipeline
     commandList->GetCommandList()->SetPipelineState1(m_impl->m_rtPipeline.Get());
    // Dispatch the rays and write to the raytracing output
     commandList->GetCommandList()->DispatchRays(&desc);
     m_frameCount++;
    // renderTarget->CopyTo(device, m_impl->m_raytraceRenderTargets[cpuFrameIndex], cpuFrameIndex);
}

void KS::ModelRenderer::Rasterize(Device& device, int cpuFrameIndex, std::shared_ptr<RenderTarget> renderTarget,
                                  std::shared_ptr<DepthStencil> depthStencil,
                                  std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs)
{
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());

    commandList->BindPipeline(pipeline);

    m_modelMatsBuffer->Update(device, &m_modelMatrices[0], m_modelCount);
    m_materialInfoBuffer->Update(device, &m_materialInstances[0], m_modelCount);
    m_modelMatsBuffer->Bind(device, m_shader->GetShaderInput()->GetInput("model_matrix"));
    m_materialInfoBuffer->Bind(device, m_shader->GetShaderInput()->GetInput("material_info"));

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

        
        m_modelIndexBuffer->Bind(device, m_shader->GetShaderInput()->GetInput("model_index"), draw_entry.modelIndex);

        int shaderFlags = m_shader->GetFlags();

        if (shaderFlags & Shader::MeshInputFlags::HAS_POSITIONS) 
            positions->BindAsVertexData(device, 0);
        if (shaderFlags & Shader::MeshInputFlags::HAS_NORMALS) 
            normals->BindAsVertexData(device, 1);
        if (shaderFlags & Shader::MeshInputFlags::HAS_UVS) 
            uvs->BindAsVertexData(device, 2);
        if (shaderFlags & Shader::MeshInputFlags::HAS_TANGENTS) 
            tangents->BindAsVertexData(device, 3);

        indices->BindAsIndexData(device);

        baseTex->Bind(device, m_shader->GetShaderInput()->GetInput("base_tex"));
        normalTex->Bind(device, m_shader->GetShaderInput()->GetInput("normal_tex"));
        emissiveTex->Bind(device, m_shader->GetShaderInput()->GetInput("emissive_tex"));
        roughMetTex->Bind(device, m_shader->GetShaderInput()->GetInput("roughmet_tex"));
        occlusionTex->Bind(device, m_shader->GetShaderInput()->GetInput("occlusion_tex"));

        commandList->DrawIndexed(indices->GetElementCount());
    }
}

void KS::ModelRenderer::CreateBottomLevelAS(const Device& device, const Mesh* mesh, int cpuFrame)
{
    nv_helpers_dx12::BottomLevelASGenerator bottomLevelASGen;
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());

    using namespace MeshConstants;
    auto positions = mesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
    auto positionsResource = reinterpret_cast<DXResource*>(positions->GetRawResource());
    auto indices = mesh->GetAttribute(ATTRIBUTE_INDICES_NAME);
    auto indicesResource = reinterpret_cast<DXResource*>(indices->GetRawResource());

    commandList->ResourceBarrier(*positionsResource->Get(), positionsResource->GetState(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    positionsResource->ChangeState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(*indicesResource->Get(), indicesResource->GetState(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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

    bottomLevelASGen.AddVertexBuffer(positionsResource->Get(), 0, static_cast<uint32_t>(positions->GetElementCount()), sizeof(glm::vec3),
        indicesResource->Get(), 0, static_cast<uint32_t>(indices->GetElementCount()), indexFormat, nullptr, 0, true);

    // The AS build requires some scratch space to store temporary information.
    // The amount of scratch memory is dependent on the scene complexity.
    UINT64 scratchSizeInBytes = 0;
    // The final AS also needs to be stored in addition to the existing vertex
    // buffers. It size is also dependent on the scene complexity.
    UINT64 resultSizeInBytes = 0;

    bottomLevelASGen.ComputeASBufferSizes(engineDevice, false, &scratchSizeInBytes,
        &resultSizeInBytes);

    auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    heapProps.CreationNodeMask = 0;
    heapProps.VisibleNodeMask = 0;

    m_impl->m_BLBuffers[m_impl->m_BLCount].pScratch[cpuFrame] =
        std::make_shared<DXResource>(engineDevice, heapProps, bufDesc, nullptr, "SCRATCH  BUFFER");

    bufDesc.Width = resultSizeInBytes;
    m_impl->m_BLBuffers[m_impl->m_BLCount].pResult[cpuFrame] = std::make_shared<DXResource>(engineDevice, heapProps, bufDesc, nullptr, "RESULT  BUFFER", D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    bottomLevelASGen.Generate(commandList->GetCommandList().Get(),
                              m_impl->m_BLBuffers[m_impl->m_BLCount].pScratch[cpuFrame]->Get(),
        m_impl->m_BLBuffers[m_impl->m_BLCount].pResult[cpuFrame]->Get(), false, nullptr);

    commandList->TrackResource(m_impl->m_BLBuffers[m_impl->m_BLCount].pScratch[cpuFrame]->GetResource());
    commandList->TrackResource(m_impl->m_BLBuffers[m_impl->m_BLCount].pResult[cpuFrame]->GetResource());
}

void KS::ModelRenderer::CreateBVHBotomLevelInstance(const Device& device, const DrawEntry& draw_entry, bool updateOnly, int entryIndex, int cpuFrame)
{
    if (!updateOnly)
    {
        const Mesh* mesh = GetMesh(device, draw_entry.mesh);
        if (!mesh)
            return;

        CreateBottomLevelAS(device, mesh, cpuFrame);

        m_impl->m_instances[m_impl->m_BLCount].first = m_impl->m_BLBuffers[m_impl->m_BLCount].pResult[cpuFrame];
        m_impl->m_instances[m_impl->m_BLCount].second = Conversion::GLMToXMMATRIX(draw_entry.modelMat);

        m_impl->m_topLevelASGenerator.AddInstance(m_impl->m_instances[m_impl->m_BLCount].first->Get(),
            m_impl->m_instances[m_impl->m_BLCount].second, static_cast<uint32_t>(m_impl->m_BLCount),
            static_cast<uint32_t>(0));

        m_impl->m_BLCount++;
    }
    else
    {
        m_impl->m_instances[entryIndex].second = Conversion::GLMToXMMATRIX(draw_entry.modelMat);
    }
}

void KS::ModelRenderer::CreateTopLevelAS(const Device& device, bool updateOnly, int cpuFrame)
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

        m_impl->m_topLevelASGenerator.ComputeASBufferSizes(engineDevice, true, &scratchSize,
            &resultSize, &instanceDescsSize);

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
    m_impl->m_topLevelASGenerator.Generate(commandList->GetCommandList().Get(),
        m_impl->m_topLevelASBuffers.pScratch[cpuFrame]->Get(),
        m_impl->m_topLevelASBuffers.pResult[cpuFrame]->Get(), m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->Get(),
        updateOnly, m_impl->m_topLevelASBuffers.pResult[cpuFrame]->Get());

    if (!updateOnly)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location =
            m_impl->m_topLevelASBuffers.pResult[cpuFrame]->Get()->GetGPUVirtualAddress();

        m_impl->m_BHVHandle[cpuFrame] =
            reinterpret_cast<DXDescHeap*>(device.GetResourceHeap())
                                  ->AllocateResource(m_impl->m_topLevelASBuffers.pResult[cpuFrame].get(), &srvDesc, BVH_SLOT+cpuFrame);
    }

    commandList->TrackResource(m_impl->m_topLevelASBuffers.pScratch[cpuFrame]->GetResource());
    commandList->TrackResource(m_impl->m_topLevelASBuffers.pResult[cpuFrame]->GetResource());
    commandList->TrackResource(m_impl->m_topLevelASBuffers.pInstanceDesc[cpuFrame]->GetResource());
}

void KS::ModelRenderer::Impl::InitializeRaytracer(const Device& device, const UniformBuffer* cameraBuffer) {
    auto engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());

    int32_t frameIndex = 0;
    m_frameIndex = std::make_unique<UniformBuffer>(device, "FRAME INDEX BUFFER", frameIndex, 1);

    nv_helpers_dx12::RootSignatureGenerator rsc;
    rsc.AddHeapRangesParameter(
        {{0 /*u0*/, 1 /*1 descriptor */, 0 /*use the implicit register space 0*/,
          D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
          RAYTRACE_RT_SLOT /*heap slot where the UAV is defined*/},
         {0 /*t0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, BVH_SLOT},
        
        });
    rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
    rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 1);
    m_raytracingSignature = rsc.Generate(engineDevice, true);
    m_raytracingSignature->SetName(L"Raytracing signature");

    ComPtr<IDxcBlob> hitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"assets/shaders/Hit.hlsl");
    ComPtr<IDxcBlob> missLibrary = nv_helpers_dx12::CompileShaderLibrary(L"assets/shaders/Miss.hlsl");
    ComPtr<IDxcBlob> rayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"assets/shaders/RayGen.hlsl");

    nv_helpers_dx12::RayTracingPipelineGenerator pipeline(engineDevice);
    pipeline.AddLibrary(rayGenLibrary.Get(), {L"RayGen"});
    pipeline.AddLibrary(missLibrary.Get(), {L"Miss"});
    pipeline.AddLibrary(hitLibrary.Get(), {L"ClosestHit"});

    pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
    pipeline.AddRootSignatureAssociation(m_raytracingSignature.Get(), {L"RayGen"});
    pipeline.AddRootSignatureAssociation(m_raytracingSignature.Get(), {L"Miss"});
    pipeline.AddRootSignatureAssociation(m_raytracingSignature.Get(), {L"HitGroup"});

    pipeline.SetMaxPayloadSize(4 * sizeof(float));    // RGB + distance
    pipeline.SetMaxAttributeSize(2 * sizeof(float));  // barycentric coordinates
    pipeline.SetMaxRecursionDepth(1);

    m_rtPipeline = pipeline.Generate();
    m_rtPipeline->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps));

    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle =
        static_cast<DXDescHeap*>(device.GetResourceHeap())->Get()->GetGPUDescriptorHandleForHeapStart();
    auto heapPointer = srvUavHeapHandle.ptr;

    for (int i = 1; i > -1; i--)
    {
        // The ray generation only uses heap data
        std::vector<void*> heapPointers(3);
        heapPointers[0] = reinterpret_cast<void*>(heapPointer);
        heapPointers[1] = reinterpret_cast<void*>(m_frameIndex->GetGPUAddress(0, i));
        heapPointers[2] = reinterpret_cast<void*>(cameraBuffer->GetGPUAddress(0, i));
        m_sbtHelper[i].AddRayGenerationProgram(L"RayGen", heapPointers);

        // The miss and hit shaders do not access any external resources: instead they
        // communicate their results through the ray payload
        m_sbtHelper[i].AddMissProgram(L"Miss", heapPointers);

        // Adding the triangle hit shader
        m_sbtHelper[i].AddHitGroup(L"HitGroup", {});
        uint32_t sbtSize = m_sbtHelper[i].ComputeSBTSize();

        m_sbtStorage[i] = nv_helpers_dx12::CreateBuffer(engineDevice, sbtSize, D3D12_RESOURCE_FLAG_NONE,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
        if (!m_sbtStorage[i])
        {
            throw std::logic_error("Could not allocate the shader binding table");
        }

        m_sbtHelper[i].Generate(m_sbtStorage[i].Get(), m_rtStateObjectProps.Get());

    }
}
