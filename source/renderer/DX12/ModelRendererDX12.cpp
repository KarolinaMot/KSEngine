#include "../Buffer.hpp"
#include "../ModelRenderer.hpp"
#include "../Shader.hpp"
#include "../ShaderInputs.hpp"
#include "Helpers/DXPipeline.hpp"
#include "Helpers/DXDescHeap.hpp"
#include <device/Device.hpp>

#include <fileio/FileIO.hpp>
#include <fileio/Serialization.hpp>
#include <resources/Texture.hpp>
#include <resources/Image.hpp>
#include <glm/gtc/matrix_transform.hpp>

KS::ModelRenderer::ModelRenderer(const Device& device, std::shared_ptr<Shader> shader)
    : SubRenderer(device, shader)
{
    ModelMat mat {};
    mModelMatBuffer = std::make_unique<Buffer>(device, "MODEL MATRIX RESOURCE", mat, 1, false);
}

KS::ModelRenderer::~ModelRenderer()
{
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
    ID3D12GraphicsCommandList4* commandList = reinterpret_cast<ID3D12GraphicsCommandList4*>(device.GetCommandList());
    ID3D12PipelineState* pipeline = reinterpret_cast<ID3D12PipelineState*>(m_shader->GetPipeline());
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    commandList->SetPipelineState(pipeline);
    ID3D12DescriptorHeap* descriptorHeaps[] = { resourceHeap->Get() };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    for (const auto& draw_entry : draw_queue)
    {

        const Mesh* mesh = GetMesh(device, draw_entry.mesh);
        auto tex = GetTexture(device, *draw_entry.baseTex.GetParameter<ResourceHandle<Texture>>(MaterialConstants::BASE_TEXTURE_NAME));

        if (mesh == nullptr || tex == nullptr)
            continue;

        ModelMat modelMat;
        modelMat.mModel = draw_entry.transform;
        modelMat.mTransposed = glm::transpose(modelMat.mModel);
        mModelMatBuffer->Update(device, modelMat, 0);
        mModelMatBuffer->BindToGraphics(device, m_shader->GetShaderInput()->GetInput("model_matrix").rootIndex, 0);

        using namespace MeshConstants;

        auto positions = mesh->GetAttribute(ATTRIBUTE_POSITIONS_NAME);
        auto uvs = mesh->GetAttribute(ATTRIBUTE_TEXTURE_UVS_NAME);
        auto indices = mesh->GetAttribute(ATTRIBUTE_INDICES_NAME);

        positions->BindAsVertexData(device, 0);
        uvs->BindAsVertexData(device, 1);
        indices->BindAsIndexData(device);
        tex->BindToGraphics(device, m_shader->GetShaderInput()->GetInput("base_tex").rootIndex);

        device.TrackResource(positions);
        device.TrackResource(uvs);
        device.TrackResource(indices);
        device.TrackResource(tex);

        commandList->DrawIndexedInstanced(indices->GetElementCount(), 1, 0, 0, 0);
    }

    device.TrackResource(mModelMatBuffer);
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
