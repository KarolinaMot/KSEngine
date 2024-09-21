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
#include <renderer/InfoStructs.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/DepthStencil.hpp>
#include <resources/Texture.hpp>

KS::Renderer::Renderer(Device& device, const RendererInitParams& params)
{
    for (int i = 0; i < params.shaders.size(); i++)
    {
        m_subrenderers.push_back(std::make_unique<ModelRenderer>(device, params.shaders[i]));
    }

    CameraMats cam {};
    m_camera_buffer = std::make_shared<UniformBuffer>(device, "CAMERA MATRIX BUFFER", cam, 1);

    for (int i = 0; i < 2; i++)
    {
        m_deferredRendererTex[i][0] = std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(), Texture::TextureFlags::RENDER_TARGET, glm::vec4(0.5f, 0.5f, 0.5f, 1.f), KS::Formats::R32G32B32A32_FLOAT);
        m_deferredRendererTex[i][1] = std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(), Texture::TextureFlags::RENDER_TARGET, glm::vec4(0.5f, 0.5f, 0.5f, 1.f), KS::Formats::R8G8B8A8_UNORM);
        m_deferredRendererTex[i][2] = std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(), Texture::TextureFlags::RENDER_TARGET, glm::vec4(0.5f, 0.5f, 0.5f, 1.f), KS::Formats::R8G8B8A8_UNORM);
        m_deferredRendererTex[i][3] = std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(), Texture::TextureFlags::RENDER_TARGET, glm::vec4(0.5f, 0.5f, 0.5f, 1.f), KS::Formats::R8G8B8A8_UNORM);
    }

    m_deferredRendererRT = std::make_shared<RenderTarget>();
    for (int i = 0; i < 4; i++)
    {
        m_deferredRendererRT->AddTexture(device, m_deferredRendererTex[0][i], m_deferredRendererTex[1][i], "DEFERRED RENDERER" + std::to_string(i));
    }

    m_deferredRendererDepthTex = std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(), Texture::TextureFlags::DEPTH_TEXTURE, glm::vec4(1.f), KS::Formats::D32_FLOAT);
    m_deferredRendererDepthStencil = std::make_shared<DepthStencil>(device, m_deferredRendererDepthTex);
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
        m_subrenderers[i]->Render(device, params.cpuFrame, m_deferredRendererRT, m_deferredRendererDepthStencil);
    }

    device.GetRenderTarget()->CopyTo(device, m_deferredRendererRT, 1, 0);
}
