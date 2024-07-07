#include "../MeshRenderer.hpp"
#include "device/Device.hpp"
#include "Helpers/DXPipeline.hpp"
#include "../Buffer.hpp"
#include "../Shader.hpp"
#include "../ShaderInputs.hpp"

#include <glm/gtc/matrix_transform.hpp>

KS::MeshRenderer::MeshRenderer(const Device &device, std::shared_ptr<Shader> shader) : SubRenderer(device, shader)
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

    ModelMat mat {};

    mQuadVResource = std::make_unique<Buffer>(device, "VERTEX POS RESOURCE", positions, false);
    mQuadUVResource = std::make_unique<Buffer>(device, "VERTEX UV RESOURCE", uvs, false);
    mIndicesResource = std::make_unique<Buffer>(device, "INDICES RESOURCE", indices, false);
    mModelMatBuffer = std::make_unique<Buffer>(device, "MODEL MATRIX RESOURCE", mat, 1, false);
}

KS::MeshRenderer::~MeshRenderer()
{
}

void KS::MeshRenderer::Render(Device& device, int cpuFrameIndex)
{
    ID3D12GraphicsCommandList4 *commandList = reinterpret_cast<ID3D12GraphicsCommandList4 *>(device.GetCommandList());
    ID3D12PipelineState *pipeline = reinterpret_cast<ID3D12PipelineState *>(m_shader->GetPipeline());

    commandList->SetPipelineState(pipeline);

    ModelMat modelMat;
    modelMat.mModel = glm::translate(glm::mat4(1.f), glm::vec3(0.5f, 0.f, 0.5f));
    modelMat.mTransposed = glm::mat4x4(1.f);
    mModelMatBuffer->Update(device, modelMat, 0);

    int rootIndex = m_shader->GetShaderInput()->GetInput("model_matrix").rootIndex;
    mModelMatBuffer->BindToGraphics(device, rootIndex);

    mQuadVResource->BindAsVertexData(device, 0);
    mQuadUVResource->BindAsVertexData(device, 1);
    mIndicesResource->BindAsIndexData(device);

    device.TrackResource(mModelMatBuffer);
    device.TrackResource(mQuadVResource);
    device.TrackResource(mQuadUVResource);
    device.TrackResource(mIndicesResource);

    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
