#include "../Renderer.hpp"
#include "../SubRenderer.hpp"
#include "../MeshRenderer.hpp"
#include "device/Device.hpp"
#include "device/Device.hpp"
#include "Renderer/DX12/Helpers/DXResource.hpp"
#include "Renderer/DX12/Helpers/DXConstBuffer.hpp"
#include "Renderer/Shader.hpp"
#include "Renderer/ShaderInputs.hpp"
#include <glm/glm.hpp>

class KS::Renderer::Impl
{
public:
    std::unique_ptr<DXConstBuffer> m_camera_buffer;
};

KS::Renderer::Renderer(const Device &device, const RendererInitParams &params)
{
    for (int i = 0; i < params.shaders.size(); i++)
    {
        m_subrenderers.push_back(std::make_unique<MeshRenderer>(device, params.shaders[i]));
    }

    m_impl = std::make_unique<Impl>();
    m_impl->m_camera_buffer = std::make_unique<DXConstBuffer>(reinterpret_cast<ID3D12Device5 *>(device.GetDevice()),
                                                              sizeof(glm::mat4x4) * 3,
                                                              1,
                                                              "CAMERA MATRIX BUFFER",
                                                              FRAME_BUFFER_COUNT);
}

KS::Renderer::~Renderer()
{
}

void KS::Renderer::Render(const Device &device, const RendererRenderParams &params)
{
    ID3D12GraphicsCommandList4 *commandList = reinterpret_cast<ID3D12GraphicsCommandList4 *>(device.GetCommandList());

    glm::mat4x4 cameraMatrices[3];
    cameraMatrices[0] = params.projectionMatrix;
    cameraMatrices[1] = params.viewMatrix;
    cameraMatrices[2] = params.projectionMatrix * params.viewMatrix;
    m_impl->m_camera_buffer->Update(&cameraMatrices, sizeof(glm::mat4x4) * 3, 0, params.cpuFrame);

    for (int i = 0; i < m_subrenderers.size(); i++)
    {
        commandList->SetGraphicsRootSignature(reinterpret_cast<ID3D12RootSignature *>(m_subrenderers[i]->GetShader()->GetShaderInput()->GetSignature()));
        m_impl->m_camera_buffer->Bind(commandList, 0, 0, params.cpuFrame);
        m_subrenderers[i]->Render(device, params.cpuFrame);
    }
}
