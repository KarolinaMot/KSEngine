#include "../Renderer.hpp"
#include "../SubRenderer.hpp"
#include "../MeshRenderer.hpp"
#include "../Buffer.hpp"
#include <device/Device.hpp>
#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXConstBuffer.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputs.hpp>
#include <glm/glm.hpp>

KS::Renderer::Renderer(const Device &device, const RendererInitParams &params)
{
    for (int i = 0; i < params.shaders.size(); i++)
    {
        m_subrenderers.push_back(std::make_unique<MeshRenderer>(device, params.shaders[i]));
    }

    CameraMats cam {};
    m_camera_buffer = std::make_shared<Buffer>(device, "CAMERA MATRIX BUFFER", cam, 1, false);
}

KS::Renderer::~Renderer()
{
}

void KS::Renderer::Render(Device& device, const RendererRenderParams& params)
{
    ID3D12GraphicsCommandList4 *commandList = reinterpret_cast<ID3D12GraphicsCommandList4 *>(device.GetCommandList());

    CameraMats cam {};
    cam.m_proj = params.projectionMatrix;
    cam.m_view = params.viewMatrix;
    cam.m_camera = params.projectionMatrix * params.viewMatrix;
    m_camera_buffer->Update(device, cam, 0);

    for (int i = 0; i < m_subrenderers.size(); i++)
    {
        commandList->SetGraphicsRootSignature(reinterpret_cast<ID3D12RootSignature *>(m_subrenderers[i]->GetShader()->GetShaderInput()->GetSignature()));
        m_camera_buffer->BindToGraphics(device, 0, 0, params.cpuFrame);
        device.TrackResource(m_camera_buffer);
        m_subrenderers[i]->Render(device, params.cpuFrame);
    }
}
