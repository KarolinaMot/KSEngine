#include <device/Device.hpp>
#include <glm/glm.hpp>

#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DepthStencil.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <renderer/ShaderInput.hpp>
#include <resources/Texture.hpp>

#include "../ComputeRenderer.hpp"
#include "../ModelRenderer.hpp"
#include "../Renderer.hpp"
#include "../SubRenderer.hpp"
#include "../UniformBuffer.hpp"
#include "Helpers/DX12Common.hpp"

KS::Renderer::Renderer(Device& device, RendererInitParams& params)
{
    CameraMats cam{};
    m_camera_buffer = std::make_shared<UniformBuffer>(device, "CAMERA MATRIX BUFFER", cam, 1);

    for (int i = 0; i < params.subRenderers.size(); i++)
    {
        if (params.subRenderers[i].shader->GetShaderType() == ShaderType::ST_MESH_RENDER)
        {
            m_subrenderers.push_back(std::make_unique<ModelRenderer>(device, params.subRenderers[i].shader,
                                                                     m_camera_buffer.get()));
        }
        else
        {
            m_subrenderers.push_back(
                std::make_unique<ComputeRenderer>(device, params.subRenderers[i].shader));
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

        m_lightRenderingTex[i] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.f, 0.f, 0.f, 0.f), KS::Formats::R32G32B32A32_FLOAT, 4);

       m_lightShaftTex[i] =
            std::make_shared<Texture>(device, device.GetWidth()/4, device.GetHeight()/4,
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                                       glm::vec4(0.f, 0.f, 0.f, 0.f), KS::Formats::R32G32B32A32_FLOAT);

        m_upscaledLightShaftTex[i] = std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                                Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                     glm::vec4(0.f, 0.f, 0.f, 0.f), KS::Formats::R8G8B8A8_UNORM);


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

    m_lightRenderRT = std::make_shared<RenderTarget>();
    m_lightRenderRT->AddTexture(device, m_lightRenderingTex[0], m_lightRenderingTex[1], "LIGHT RENDER RES");

    m_lightShaftRT = std::make_shared<RenderTarget>();
    m_lightShaftRT->AddTexture(device, m_lightShaftTex[0], m_lightShaftTex[1], "LIGHT SHAFT RENDER RES");
    
    m_upscaledLightShaftRT = std::make_shared<RenderTarget>();
    m_upscaledLightShaftRT->AddTexture(device, m_upscaledLightShaftTex[0], m_upscaledLightShaftTex[1],
                                       "UPSCALED LIGHT SHAFT RENDER RES");

    m_deferredRendererDepthTex =
        std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(), Texture::TextureFlags::DEPTH_TEXTURE,
                                  glm::vec4(1.f), KS::Formats::D32_FLOAT);

    m_deferredRendererDepthStencil = std::make_shared<DepthStencil>(device, m_deferredRendererDepthTex);

    m_pointLights = std::vector<PointLightInfo>(100);
    m_directionalLights = std::vector<DirLightInfo>(100);

    m_fogInfo.fogColor = glm::vec3(1.f, 1.f, 1.f);
    m_fogInfo.fogDensity = 0.6f;
    m_fogInfo.exposure = 0.15f;
    m_fogInfo.lightShaftNumberSamples = 132;
    m_fogInfo.sourceMipNumber = 2;
    m_fogInfo.weight = 0.05f;
    m_fogInfo.decay = 0.99f;

    mUniformBuffers[KS::LIGHT_INFO_BUFFER] = std::make_unique<UniformBuffer>(device, "LIGHT INFO BUFFER", m_lightInfo, 1);
    mUniformBuffers[KS::FOG_INFO_BUFFER] = std::make_unique<UniformBuffer>(device, "FOG INFO BUFFER", m_fogInfo, 1);
    mStorageBuffers[KS::DIR_LIGHT_BUFFER] =
        std::make_unique<StorageBuffer>(device, "DIRECTIONAL LIGHT BUFFER", m_directionalLights, false);
    mStorageBuffers[KS::POINT_LIGHT_BUFFER] =
        std::make_unique<StorageBuffer>(device, "POINT LIGHT BUFFER", m_pointLights, false);

    auto rootSignature = m_subrenderers[1]->GetShader()->GetShaderInput();

    m_mainComputeShaderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(mStorageBuffers[KS::DIR_LIGHT_BUFFER].get(),
                                                                               rootSignature->GetInput("dir_lights")));
    m_mainComputeShaderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(mStorageBuffers[KS::POINT_LIGHT_BUFFER].get(),
                                                                               rootSignature->GetInput("point_lights")));
    m_mainComputeShaderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(mUniformBuffers[KS::LIGHT_INFO_BUFFER].get(),
                                                                               rootSignature->GetInput("light_info")));
    m_mainComputeShaderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(m_camera_buffer.get(),
                                                                               rootSignature->GetInput("camera_matrix")));
    for (int i = 0; i < 4; i++)
    {
        m_mainComputeShaderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(m_deferredRendererTex[device.GetCPUFrameIndex()][i].get(),
                                                                                  rootSignature->GetInput("GBuffer" + std::to_string(i + 1))));
    }

    m_mainComputeShaderInputs.push_back(
    std::pair<ShaderInput*, ShaderInputDesc>(m_upscaledLightShaftTex[0].get(), rootSignature->GetInput("base_tex")));


    rootSignature = m_subrenderers[0]->GetShader()->GetShaderInput();

    m_deferredShaderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(mStorageBuffers[KS::DIR_LIGHT_BUFFER].get(),
                                                                                 rootSignature->GetInput("dir_lights")));
    m_deferredShaderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(mStorageBuffers[KS::POINT_LIGHT_BUFFER].get(),
                                                                                 rootSignature->GetInput("point_lights")));
    m_deferredShaderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(mUniformBuffers[KS::LIGHT_INFO_BUFFER].get(),
                                                                                 rootSignature->GetInput("light_info")));
    m_deferredShaderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(m_camera_buffer.get(), rootSignature->GetInput("camera_matrix")));



    m_lightRenderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(mStorageBuffers[KS::POINT_LIGHT_BUFFER].get(),
                                                                              rootSignature->GetInput("point_lights")));
    m_lightRenderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(mUniformBuffers[KS::LIGHT_INFO_BUFFER].get(),
                                                                              rootSignature->GetInput("light_info")));
    m_lightRenderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(mUniformBuffers[KS::FOG_INFO_BUFFER].get(),
                                                                              rootSignature->GetInput("fog_info")));
    m_lightRenderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(m_camera_buffer.get(), rootSignature->GetInput("camera_matrix")));

    m_lightOccluderInputs.push_back(std::pair<ShaderInput*, ShaderInputDesc>(m_camera_buffer.get(), rootSignature->GetInput("camera_matrix")));

    m_lightShaftInputs = m_lightRenderInputs;
    m_lightShaftInputs.push_back(
        std::pair<ShaderInput*, ShaderInputDesc>(m_lightRenderingTex[0].get(), rootSignature->GetInput("base_tex")));

    m_upscaleLightShaftInputs.push_back(
        std::pair<ShaderInput*, ShaderInputDesc>(m_lightShaftTex[0].get(), rootSignature->GetInput("base_tex")));
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

void KS::Renderer::QueueModel(Device& device, ResourceHandle<Model> model, const glm::mat4& transform)
{ 
    for (int i = 0; i < m_subrenderers.size(); i++)
    {
        if (m_subrenderers[i]->GetShader()->GetShaderType() == ShaderType::ST_MESH_RENDER)
        {
            reinterpret_cast<ModelRenderer*>(m_subrenderers[i].get())->QueueModel(device, model, transform);
        }
    }
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
    cam.m_cameraRight = glm::vec4(params.cameraRight, 1.f);
    m_camera_buffer->Update(device, cam, 0);
    mUniformBuffers[KS::FOG_INFO_BUFFER]->Update(device, m_fogInfo);

    UpdateLights(device);

    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

    auto rootSignature = m_subrenderers[0]->GetShader()->GetShaderInput();
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()));
    device.TrackResource(m_camera_buffer);

    //DEFERRED RENDERER
    m_subrenderers[0]->Render(device, params.cpuFrame, m_deferredRendererRT, m_deferredRendererDepthStencil, m_deferredShaderInputs);

    //RENDERING LIGHTS
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), true);
    m_subrenderers[2]->Render(device, params.cpuFrame, m_lightRenderRT, m_deferredRendererDepthStencil, m_lightRenderInputs);

    //RENDERING OCCLUDER MESHES
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), false);
    m_subrenderers[3]->Render(device, params.cpuFrame, m_lightRenderRT, m_deferredRendererDepthStencil, m_lightOccluderInputs, false);

    //GENERATE LIGHT SCATTERING RENDER TARGET MIPS
    m_lightRenderingTex[device.GetCPUFrameIndex()]->GenerateMipmaps(device);

    //LIGHT SHAFT RENDER
    m_lightShaftInputs[m_lightShaftInputs.size() - 1] =  std::pair<ShaderInput*, ShaderInputDesc>(m_lightRenderingTex[device.GetCPUFrameIndex()].get(), rootSignature->GetInput("base_tex"));
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), true);
    m_subrenderers[4]->Render(device, params.cpuFrame, m_lightShaftRT, m_deferredRendererDepthStencil, m_lightShaftInputs,
                              true);

    //LIGHT SHAFT UPSCALE
    m_upscaleLightShaftInputs[m_upscaleLightShaftInputs.size() - 1] =  std::pair<ShaderInput*, ShaderInputDesc>(m_lightShaftTex[device.GetCPUFrameIndex()].get(), rootSignature->GetInput("base_tex"));
    m_subrenderers[5]->Render(device, params.cpuFrame, m_upscaledLightShaftRT, m_deferredRendererDepthStencil,
                              m_upscaleLightShaftInputs,
                              true);
    //PBR RENDER
    m_mainComputeShaderInputs[m_mainComputeShaderInputs.size() - 1] = std::pair<ShaderInput*, ShaderInputDesc>(
        m_upscaledLightShaftTex[device.GetCPUFrameIndex()].get(), rootSignature->GetInput("base_tex"));
    auto& boundRT = raytraced ? m_raytracedRendererRT : m_pbrResRT;
    if (!raytraced)
        m_subrenderers[1]->Render(device, params.cpuFrame, boundRT, m_deferredRendererDepthStencil, m_mainComputeShaderInputs);

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
