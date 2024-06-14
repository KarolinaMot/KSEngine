#include "../MeshRenderer.hpp"
#include "device/Device.hpp"
#include "Helpers/DXPipeline.hpp"
#include "Helpers/DXResource.hpp"
#include "Helpers/DXConstBuffer.hpp"
#include "../Shader.hpp"
#include "../ShaderInputs.hpp"

#include <glm/gtc/matrix_transform.hpp>

class KS::MeshRenderer::Impl
{
public:
    std::unique_ptr<DXResource> mQuadVResource;
    std::unique_ptr<DXResource> mQuadUVResource;
    std::unique_ptr<DXResource> mIndicesResource;

    std::unique_ptr<DXConstBuffer> mModelMatBuffer;
    std::unique_ptr<DXConstBuffer> mColorBuffer;

    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
    D3D12_VERTEX_BUFFER_VIEW mTexCoordBufferView;
    D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

    void InitializeQuad(ID3D12Device5 *device, ID3D12GraphicsCommandList4 *commandList);
};

KS::MeshRenderer::MeshRenderer(const Device &device, std::shared_ptr<Shader> shader) : SubRenderer(device, shader)
{
    m_impl = std::make_unique<Impl>();
    m_impl->InitializeQuad(reinterpret_cast<ID3D12Device5 *>(device.GetDevice()),
                           reinterpret_cast<ID3D12GraphicsCommandList4 *>(device.GetCommandList()));
}

KS::MeshRenderer::~MeshRenderer()
{
}

void KS::MeshRenderer::Render(const Device &device, int cpuFrameIndex)
{
    ID3D12GraphicsCommandList4 *commandList = reinterpret_cast<ID3D12GraphicsCommandList4 *>(device.GetCommandList());
    ID3D12PipelineState *pipeline = reinterpret_cast<ID3D12PipelineState *>(m_shader->GetPipeline());

    commandList->SetPipelineState(pipeline);

    ModelMat modelMat;
    modelMat.mModel = glm::translate(glm::mat4(1.f), glm::vec3(0.5f, 0.f, 0.5f));
    modelMat.mTransposed = glm::mat4x4(1.f);
    m_impl->mModelMatBuffer->Update(&modelMat, sizeof(ModelMat), 0, cpuFrameIndex);
    int rootIndex = m_shader->GetShaderInput()->GetInput("model_matrix").rootIndex;
    m_impl->mModelMatBuffer->Bind(commandList, rootIndex, 0, cpuFrameIndex);

    commandList->IASetVertexBuffers(0, 1, &m_impl->mVertexBufferView);
    commandList->IASetVertexBuffers(1, 1, &m_impl->mTexCoordBufferView);
    commandList->IASetIndexBuffer(&m_impl->mIndexBufferView);
    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void KS::MeshRenderer::Impl::InitializeQuad(ID3D12Device5 *device, ID3D12GraphicsCommandList4 *commandList)
{
    std::vector<glm::vec3> positions =
        {
            glm::vec3(-0.5f, 0.5f, 0.0f),  // Top Left
            glm::vec3(0.5f, 0.5f, 0.0f),   // Top Right
            glm::vec3(-0.5f, -0.5f, 0.0f), // Bottom Left
            glm::vec3(0.5f, -0.5f, 0.0f),  // Bottom Right
        };
    std::vector<glm::vec2> uvs =
        {
            glm::vec2(0.f),
            glm::vec2(1.f, 0.f),
            glm::vec2(0.f, 1.f),
            glm::vec2(1.f, 1.f)};
    std::vector<uint32_t> indices = {0, 1, 2, 3, 2, 1};
    int vBufferSize = sizeof(glm::vec3) * static_cast<int>(positions.size());
    int tBufferSize = sizeof(glm::vec2) * static_cast<int>(uvs.size());
    int iBufferSize = sizeof(uint32_t) * static_cast<int>(indices.size());

    mQuadVResource = std::make_unique<DXResource>(device, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), nullptr, "UI Vertex resource buffer");
    mQuadUVResource = std::make_unique<DXResource>(device, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), CD3DX12_RESOURCE_DESC::Buffer(tBufferSize), nullptr, "UI UV resource buffer");
    mIndicesResource = std::make_unique<DXResource>(device, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), CD3DX12_RESOURCE_DESC::Buffer(iBufferSize), nullptr, "UI indices resource buffer");

    mQuadVResource->CreateUploadBuffer(device, vBufferSize, 0);
    mQuadUVResource->CreateUploadBuffer(device, tBufferSize, 0);
    mIndicesResource->CreateUploadBuffer(device, iBufferSize, 0);

    D3D12_SUBRESOURCE_DATA vData = {};
    vData.pData = positions.data();
    vData.RowPitch = sizeof(float) * 3;
    vData.SlicePitch = vBufferSize;

    D3D12_SUBRESOURCE_DATA uData = {};
    uData.pData = uvs.data();
    uData.RowPitch = sizeof(float) * 2;
    uData.SlicePitch = tBufferSize;

    D3D12_SUBRESOURCE_DATA iData = {};
    iData.pData = indices.data();
    iData.RowPitch = iBufferSize;
    iData.SlicePitch = iBufferSize;

    mQuadVResource->Update(commandList, vData, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0, 1);
    mQuadUVResource->Update(commandList, uData, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0, 1);
    mIndicesResource->Update(commandList, iData, D3D12_RESOURCE_STATE_INDEX_BUFFER, 0, 1);

    mVertexBufferView.BufferLocation = mQuadVResource->GetResource()->GetGPUVirtualAddress();
    mVertexBufferView.StrideInBytes = sizeof(float) * 3;
    mVertexBufferView.SizeInBytes = vBufferSize;

    mTexCoordBufferView.BufferLocation = mQuadUVResource->GetResource()->GetGPUVirtualAddress();
    mTexCoordBufferView.StrideInBytes = sizeof(float) * 2;
    mTexCoordBufferView.SizeInBytes = tBufferSize;

    mIndexBufferView.BufferLocation = mIndicesResource->GetResource()->GetGPUVirtualAddress();
    mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    mIndexBufferView.SizeInBytes = iBufferSize;

    mModelMatBuffer = std::make_unique<DXConstBuffer>(device, sizeof(glm::mat4x4) * 2, 1, "Mesh matrix data", FRAME_BUFFER_COUNT);
}
