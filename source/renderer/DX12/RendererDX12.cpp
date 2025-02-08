#include <device/Device.hpp>
#include <glm/glm.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXConstBuffer.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DepthStencil.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputs.hpp>
#include <resources/Texture.hpp>

#include "../ComputeRenderer.hpp"
#include "../ModelRenderer.hpp"
#include "../Renderer.hpp"
#include "../SubRenderer.hpp"
#include "../UniformBuffer.hpp"
#include "Helpers/DX12Common.hpp"

KS::Renderer::Renderer(Device& device, const RendererInitParams& params)
{
    CameraMats cam{};
    m_camera_buffer = std::make_shared<UniformBuffer>(device, "CAMERA MATRIX BUFFER", cam, 1);

    for (int i = 0; i < params.shaders.size(); i++)
    {
        if (params.shaders[i]->GetShaderType() == ShaderType::ST_MESH_RENDER)
        {
            m_subrenderers.push_back(std::make_unique<ModelRenderer>(device, params.shaders[i], m_camera_buffer.get()));
        }
        else
        {
            m_subrenderers.push_back(std::make_unique<ComputeRenderer>(device, params.shaders[i]));
        }
    }

    for (int i = 0; i < 2; i++)
    {
        m_deferredRendererTex[i][0] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), KS::Formats::R32G32B32A32_FLOAT);
        m_deferredRendererTex[i][1] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), KS::Formats::R8G8B8A8_UNORM);
        m_deferredRendererTex[i][2] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), KS::Formats::R8G8B8A8_UNORM);
        m_deferredRendererTex[i][3] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), KS::Formats::R8G8B8A8_UNORM);
        m_pbrResTex[i] = std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                                   Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                                   glm::vec4(0.5f, 0.5f, 0.5f, 1.f), KS::Formats::R8G8B8A8_UNORM);
        m_raytracingResTex[i] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.5f, 0.5f, 0.5f, 1.f), KS::Formats::R8G8B8A8_UNORM, -1, RAYTRACE_RT_SLOT + i);
    }

    m_deferredRendererRT = std::make_shared<RenderTarget>();
    for (int i = 0; i < 4; i++)
    {
        m_deferredRendererRT->AddTexture(device, m_deferredRendererTex[0][i], m_deferredRendererTex[1][i],
                                         "DEFERRED RENDERER" + std::to_string(i));
    }
    m_pbrResRT = std::make_shared<RenderTarget>();
    m_pbrResRT->AddTexture(device, m_pbrResTex[0], m_pbrResTex[1], "PBR RENDER RES");

    m_raytracedRendererRT = std::make_shared<RenderTarget>();
    m_raytracedRendererRT->AddTexture(device, m_raytracingResTex[0], m_raytracingResTex[1], "RAYTRACED RENDER RES");

    m_deferredRendererDepthTex =
        std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(), Texture::TextureFlags::DEPTH_TEXTURE,
                                  glm::vec4(1.f), KS::Formats::D32_FLOAT);
    m_deferredRendererDepthStencil = std::make_shared<DepthStencil>(device, m_deferredRendererDepthTex);

    m_pointLights = std::vector<PointLightInfo>(100);
    m_directionalLights = std::vector<DirLightInfo>(100);

    mUniformBuffers[KS::LIGHT_INFO_BUFFER] = std::make_unique<UniformBuffer>(device, "LIGHT INFO BUFFER", m_lightInfo, 1);
    mStorageBuffers[KS::DIR_LIGHT_BUFFER] =
        std::make_unique<StorageBuffer>(device, "DIRECTIONAL LIGHT BUFFER", m_directionalLights, false);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER] =
        std::make_unique<StorageBuffer>(device, "POINT LIGHT BUFFER", m_pointLights, false);
}

KS::Renderer::~Renderer() {}

void KS::Renderer::QueuePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius)
{
    PointLightInfo pLight;
    pLight.mColorAndIntensity = glm::vec4(color, intensity);
    pLight.mPosition = glm::vec4(position, 0.f);
    pLight.mRadius = radius;
    m_pointLights[m_lightInfo.numPointLights] = pLight;
    m_lightInfo.numPointLights++;
}
void KS::Renderer::QueueDirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity)
{
    DirLightInfo dLight;
    dLight.mDir = glm::vec4(direction, 0.f);
    dLight.mColorAndIntensity = glm::vec4(color, intensity);
    m_directionalLights[m_lightInfo.numDirLights] = dLight;
    m_lightInfo.numDirLights++;
}

void KS::Renderer::SetAmbientLight(glm::vec3 color, float intensity)
{
    m_lightInfo.mAmbientAndIntensity = glm::vec4(color, intensity);
}

void KS::Renderer::Render(Device& device, const RendererRenderParams& params, bool raytraced)
{
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());

    CameraMats cam{};
    cam.m_proj = params.projectionMatrix;
    cam.m_invProj = glm::inverse(params.projectionMatrix);
    cam.m_view = params.viewMatrix;
    cam.m_invView = glm::inverse(params.viewMatrix);
    cam.m_camera = params.projectionMatrix * params.viewMatrix;
    cam.m_cameraPos = glm::vec4(params.cameraPos, 1.f);
    m_camera_buffer->Update(device, cam, 0);

    UpdateLights(device);

    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

    auto rootSignature = m_subrenderers[0]->GetShader()->GetShaderInput();
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()));
    mStorageBuffers[KS::DIR_LIGHT_BUFFER]->Bind(device, rootSignature->GetInput("dir_lights").rootIndex, 0);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER]->Bind(device, rootSignature->GetInput("point_lights").rootIndex, 0);
    mUniformBuffers[KS::LIGHT_INFO_BUFFER]->Bind(device, rootSignature->GetInput("light_info").rootIndex, 0);

    m_camera_buffer->Bind(device, m_subrenderers[0]->GetShader()->GetShaderInput()->GetInput("camera_matrix").rootIndex, 0);
    device.TrackResource(m_camera_buffer);
    m_subrenderers[0]->Render(device, params.cpuFrame, m_deferredRendererRT, m_deferredRendererDepthStencil);

    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), true);

    mStorageBuffers[KS::DIR_LIGHT_BUFFER]->Bind(device, rootSignature->GetInput("dir_lights").rootIndex, 0);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER]->Bind(device, rootSignature->GetInput("point_lights").rootIndex, 0);
    mUniformBuffers[KS::LIGHT_INFO_BUFFER]->Bind(device, rootSignature->GetInput("light_info").rootIndex, 0);
    m_camera_buffer->Bind(device, m_subrenderers[0]->GetShader()->GetShaderInput()->GetInput("camera_matrix").rootIndex, 0);

    Texture* textures[4] = {
        m_deferredRendererTex[device.GetCPUFrameIndex()][0].get(), m_deferredRendererTex[device.GetCPUFrameIndex()][1].get(),
        m_deferredRendererTex[device.GetCPUFrameIndex()][2].get(), m_deferredRendererTex[device.GetCPUFrameIndex()][3].get()};

    auto &boundRT = raytraced ? m_raytracedRendererRT : m_pbrResRT;

    if (!raytraced)
        m_subrenderers[1]->Render(device, params.cpuFrame, boundRT, m_deferredRendererDepthStencil, &textures[0], 4);

    device.GetRenderTarget()->CopyTo(device, boundRT, 0, 0);
}

void KS::Renderer::UpdateLights(const Device& device)
{
    m_lightInfo.padding[1] = 1;
    mUniformBuffers[LIGHT_INFO_BUFFER]->Update(device, m_lightInfo);
    mStorageBuffers[DIR_LIGHT_BUFFER]->Update(device, m_directionalLights);
    mStorageBuffers[POINT_LIGHT_BUFFER]->Update(device, m_pointLights);
    m_lightInfo.numDirLights = 0;
    m_lightInfo.numPointLights = 0;
}
