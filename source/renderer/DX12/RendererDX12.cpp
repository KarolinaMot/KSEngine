#include "../UniformBuffer.hpp"
#include "../ModelRenderer.hpp"
#include "../Renderer.hpp"
#include "../SubRenderer.hpp"
#include <device/Device.hpp>
#include <glm/glm.hpp>
#include <renderer/DX12/Helpers/DXConstBuffer.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputs.hpp>

KS::Renderer::Renderer(const Device& device, const RendererInitParams& params)
{
    for (int i = 0; i < params.shaders.size(); i++)
    {
        m_subrenderers.push_back(std::make_unique<ModelRenderer>(device, params.shaders[i]));
    }

    CameraMats cam {};
    m_camera_buffer = std::make_shared<UniformBuffer>(device, "CAMERA MATRIX BUFFER", cam, 1);
}

KS::Renderer::~Renderer()
{
}

void KS::Renderer::Render(Device& device, const RendererRenderParams& params)
{
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    CameraMats cam {};
    cam.m_proj = params.projectionMatrix;
    cam.m_view = params.viewMatrix;
    cam.m_camera = params.projectionMatrix * params.viewMatrix;
    cam.m_cameraPos = glm::vec4(params.cameraPos, 1.f);
    m_camera_buffer->Update(device, cam, 0);

    for (int i = 0; i < m_subrenderers.size(); i++)
    {
        commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(m_subrenderers[i]->GetShader()->GetShaderInput()->GetSignature()));
        m_camera_buffer->Bind(device,
            m_subrenderers[i]->GetShader()->GetShaderInput()->GetInput("camera_matrix").rootIndex,
            0);
        device.TrackResource(m_camera_buffer);
        m_subrenderers[i]->Render(device, params.cpuFrame);
    }
}
