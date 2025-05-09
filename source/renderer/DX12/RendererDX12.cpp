#include "../Renderer.hpp"

#include <device/Device.hpp>
#include <glm/glm.hpp>

#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DepthStencil.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/Shader.hpp>
#include <renderer/Subrenderer.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <renderer/ShaderInputCollectionBuilder.hpp>
#include <renderer/ShaderInput.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ComputeRenderer.hpp>
#include <renderer/ModelRenderer.hpp>
#include <renderer/RTRenderer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <renderer/DX12/Helpers/DX12Common.hpp>

#include <resources/Texture.hpp>
#include <scene/Scene.hpp>

KS::Renderer::Renderer(Device& device)
{
    SamplerDesc clampSampler;
    clampSampler.addressMode = SamplerAddressMode::SAM_CLAMP;

    m_mainInputs = ShaderInputCollectionBuilder()
                     .AddUniform(ShaderInputVisibility::COMPUTE, {"camera_matrix"})
                     .AddUniform(ShaderInputVisibility::COMPUTE, {"model_index", "fog_info"})
                     .AddTexture(ShaderInputVisibility::COMPUTE, "base_tex")
                     .AddTexture(ShaderInputVisibility::PIXEL, "normal_tex")
                     .AddTexture(ShaderInputVisibility::PIXEL, "emissive_tex")
                     .AddTexture(ShaderInputVisibility::PIXEL, "roughmet_tex")
                     .AddTexture(ShaderInputVisibility::PIXEL, "occlusion_tex")
                     .AddTexture(ShaderInputVisibility::COMPUTE, "PBRRes", ShaderInputMod::READ_WRITE)
                     .AddTexture(ShaderInputVisibility::COMPUTE, "GBuffer1", ShaderInputMod::READ_WRITE)
                     .AddTexture(ShaderInputVisibility::COMPUTE, "GBuffer2", ShaderInputMod::READ_WRITE)
                     .AddTexture(ShaderInputVisibility::COMPUTE, "GBuffer3", ShaderInputMod::READ_WRITE)
                     .AddTexture(ShaderInputVisibility::COMPUTE, "GBuffer4", ShaderInputMod::READ_WRITE)
                     .AddStorageBuffer(ShaderInputVisibility::COMPUTE, 100, "dir_lights")
                     .AddStorageBuffer(ShaderInputVisibility::COMPUTE, 100, "point_lights")
                     .AddStorageBuffer(ShaderInputVisibility::VERTEX, 200, "model_matrix")
                     .AddStorageBuffer(ShaderInputVisibility::PIXEL, 200, "material_info")
                     .AddUniform(ShaderInputVisibility::COMPUTE, {"light_info"})
                     .AddStaticSampler(ShaderInputVisibility::COMPUTE, SamplerDesc{})
                     .AddStaticSampler(ShaderInputVisibility::COMPUTE, clampSampler)
                     .Build(device, "MAIN SIGNATURE");

    m_rtInputs = ShaderInputCollectionBuilder()
                     .AddRanges(ShaderInputVisibility::COMPUTE,
                                {{ShaderInputMod::READ_WRITE, 1}, {ShaderInputMod::READ_ONLY, 1}}, "heap_range")
                     .AddUniform(ShaderInputVisibility::COMPUTE, {"frame_index"})
                     .AddUniform(ShaderInputVisibility::COMPUTE, {"camera_matrix"})
                     .Build(device, "RAYTRACE SIGNATURE", true);

    std::shared_ptr<Texture> deferredRendererTex[2][4];
    std::shared_ptr<Texture> deferredRendererDepthTex;
    std::shared_ptr<Texture> pbrResTex[2];
    std::shared_ptr<Texture> raytracingResTex[2];
    std::shared_ptr<Texture> lightRenderingTex[2];
    std::shared_ptr<Texture> lightShaftTex[2];
    std::shared_ptr<Texture> upscaledLightShaftTex[2];

    for (int i = 0; i < 2; i++)
    {
        deferredRendererTex[i][0] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), Formats::R32G32B32A32_FLOAT);
        deferredRendererTex[i][1] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), Formats::R8G8B8A8_UNORM);
        deferredRendererTex[i][2] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), Formats::R8G8B8A8_UNORM);
        deferredRendererTex[i][3] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), Formats::R8G8B8A8_UNORM);

        pbrResTex[i] = std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                                 Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                                 glm::vec4(0.5f, 0.5f, 0.5f, 1.f), Formats::R8G8B8A8_UNORM);
        raytracingResTex[i] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.5f, 0.5f, 0.5f, 1.f), Formats::R8G8B8A8_UNORM, 1, -1, RAYTRACE_RT_SLOT+i);
        
        lightRenderingTex[i] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.f, 0.f, 0.f, 0.f), Formats::R32G32B32A32_FLOAT, 4);

        lightShaftTex[i] = std::make_shared<Texture>(device, device.GetWidth() / 4, device.GetHeight() / 4,
                                                     Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                                     glm::vec4(0.f, 0.f, 0.f, 0.f), Formats::R32G32B32A32_FLOAT);

        upscaledLightShaftTex[i] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.f, 0.f, 0.f, 0.f), Formats::R8G8B8A8_UNORM);
    }

    deferredRendererDepthTex = std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(), Texture::TextureFlags::DEPTH_TEXTURE,
                                glm::vec4(1.f), Formats::D32_FLOAT);
    m_deferredRendererDepthStencil = std::make_shared<DepthStencil>(device, deferredRendererDepthTex);
    CameraMats cam{};
    m_camera_buffer = std::make_shared<UniformBuffer>(device, "CAMERA MATRIX BUFFER", cam, 1);


    int fullInputFlags = Shader::HAS_POSITIONS | Shader::HAS_NORMALS | Shader::HAS_UVS | Shader::HAS_TANGENTS;
    int positionsInputFlags = Shader::HAS_POSITIONS;

    std::shared_ptr<Shader> mainShader = std::make_shared<Shader>(
        device, ShaderType::ST_MESH_RENDER, m_mainInputs, std::initializer_list<std::string>{"assets/shaders/Deferred.hlsl"},
                                 std::initializer_list<Formats>{Formats::R32G32B32A32_FLOAT, Formats::R8G8B8A8_UNORM,
                                                                Formats::R8G8B8A8_UNORM,
                                      Formats::R8G8B8A8_UNORM},
                                     fullInputFlags);

    
    std::shared_ptr<Shader> lightOccluderShader =
        std::make_shared<Shader>(device, ShaderType::ST_MESH_RENDER, m_mainInputs,
                                 std::initializer_list<std::string>{"assets/shaders/OccluderShader.hlsl"},
                                 std::initializer_list<Formats>{Formats::R32G32B32A32_FLOAT}, positionsInputFlags);

    std::shared_ptr<Shader> computePBRShader =
        std::make_shared<Shader>(device, ShaderType::ST_COMPUTE, m_mainInputs, std::initializer_list<std::string>{"assets/shaders/Main.hlsl"},
        std::initializer_list<Formats>{});

    std::shared_ptr<Shader> lightRendererShader = std::make_shared<Shader>(
        device, ShaderType::ST_COMPUTE, m_mainInputs, std::initializer_list<std::string>{"assets/shaders/LightRenderer.hlsl"},
                                 std::initializer_list<Formats>{});

    std::shared_ptr<Shader> lightShaftShader = std::make_shared<Shader>(
        device, ShaderType::ST_COMPUTE, m_mainInputs,
        std::initializer_list<std::string>{"assets/shaders/LightShaftShader.hlsl"},
                                 std::initializer_list<Formats>{});

    std::shared_ptr<Shader> upscalingShader = std::make_shared<Shader>(
        device, ShaderType::ST_COMPUTE, m_mainInputs, std::initializer_list<std::string>{"assets/shaders/Upscaling.hlsl"},
                                 std::initializer_list<Formats>{});

    std::shared_ptr<Shader> rtShader = std::make_shared<Shader>(
        device, ShaderType::ST_RAYTRACER, m_rtInputs,
        std::initializer_list<std::string>{"assets/shaders/Hit.hlsl", "assets/shaders/Miss.hlsl", "assets/shaders/RayGen.hlsl"},
        std::initializer_list<Formats>{});


    m_renderTargets[DEFERRED_RENDER] = std::make_shared<RenderTarget>();
    for (int i = 0; i < 4; i++)
    {
        m_renderTargets[DEFERRED_RENDER]->AddTexture(device, deferredRendererTex[0][i], deferredRendererTex[1][i],
                                         "DEFERRED RENDERER" + std::to_string(i));
    }

    m_renderTargets[PBR_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[PBR_RENDER]->AddTexture(device, pbrResTex[0], pbrResTex[1], "PBR RENDER RES");

    m_renderTargets[RT_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[RT_RENDER]->AddTexture(device, raytracingResTex[0], raytracingResTex[1], "RAYTRACED RENDER RES");

    m_renderTargets[LIGHT_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[LIGHT_RENDER]->AddTexture(device, lightRenderingTex[0], lightRenderingTex[1], "LIGHT RENDER RES");

    m_renderTargets[LIGHT_SHAFT_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[LIGHT_SHAFT_RENDER]->AddTexture(device, lightShaftTex[0], lightShaftTex[1], "LIGHT SHAFT RENDER RES");
    
    m_renderTargets[UPSCALING_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[UPSCALING_RENDER]->AddTexture(device, upscaledLightShaftTex[0], upscaledLightShaftTex[1],
                                       "UPSCALED LIGHT SHAFT RENDER RES");

    SubRendererDesc defferedDesc;
    defferedDesc.shader = mainShader;
    defferedDesc.renderTarget = m_renderTargets[DEFERRED_RENDER];
    defferedDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[DEFERRED_RENDER] = std::make_unique<ModelRenderer>(device, defferedDesc);

    SubRendererDesc occluderDesc;
    occluderDesc.shader = lightOccluderShader;
    occluderDesc.renderTarget = m_renderTargets[LIGHT_RENDER];
    occluderDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[OCCLUDER_RENDER] = std::make_unique<ModelRenderer>(device, occluderDesc);

    SubRendererDesc pbrDesc;
    pbrDesc.shader = computePBRShader;
    pbrDesc.renderTarget = m_renderTargets[PBR_RENDER];
    pbrDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[PBR_RENDER] = std::make_unique<ComputeRenderer>(device, pbrDesc);

    SubRendererDesc lightDesc;
    lightDesc.shader = lightRendererShader;
    lightDesc.renderTarget = m_renderTargets[LIGHT_RENDER];
    lightDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[LIGHT_RENDER] = std::make_unique<ComputeRenderer>(device, lightDesc);

    SubRendererDesc lightShaftDesc;
    lightShaftDesc.shader = lightShaftShader;
    lightShaftDesc.renderTarget = m_renderTargets[LIGHT_SHAFT_RENDER];
    lightShaftDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[LIGHT_SHAFT_RENDER] = std::make_unique<ComputeRenderer>(device, lightShaftDesc);

    SubRendererDesc upscalingDesc;
    upscalingDesc.shader = upscalingShader;
    upscalingDesc.renderTarget = m_renderTargets[UPSCALING_RENDER];
    upscalingDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[UPSCALING_RENDER] = std::make_unique<ComputeRenderer>(device, upscalingDesc);

    SubRendererDesc rtDesc;
    rtDesc.shader = rtShader;
    rtDesc.renderTarget = m_renderTargets[RT_RENDER];
    rtDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[RT_RENDER] = std::make_unique<RTRenderer>(device, rtDesc, m_camera_buffer.get());

    m_inputs[DEFERRED_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputDesc>>(6);
    m_inputs[OCCLUDER_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputDesc>>(2);
    m_inputs[PBR_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputDesc>>(9);
    m_inputs[LIGHT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputDesc>>(4);
    m_inputs[LIGHT_SHAFT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputDesc>>(5);
    m_inputs[UPSCALING_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputDesc>>(1);
}

KS::Renderer::~Renderer() {}


void KS::Renderer::Render(Device& device, Scene& scene, const RenderTickParams& params, bool raytraced)
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


    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

    auto rootSignature = m_subrenderers[0]->GetShader()->GetShaderInput();
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()));
    device.TrackResource(m_camera_buffer);

    //DEFERRED RENDERER
    m_inputs[DEFERRED_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(DIR_LIGHT_BUFFER), rootSignature->GetInput("dir_lights"));
    m_inputs[DEFERRED_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER), rootSignature->GetInput("point_lights"));
    m_inputs[DEFERRED_RENDER][2] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER),rootSignature->GetInput("light_info"));
    m_inputs[DEFERRED_RENDER][3] = std::pair<ShaderInput*, ShaderInputDesc>(m_camera_buffer.get(), rootSignature->GetInput("camera_matrix"));
    m_inputs[DEFERRED_RENDER][4] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(MODEL_MAT_BUFFER), rootSignature->GetInput("model_matrix"));
    m_inputs[DEFERRED_RENDER][5] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(MATERIAL_INFO_BUFFER), rootSignature->GetInput("material_info"));
    m_subrenderers[DEFERRED_RENDER]->Render(device, scene, m_inputs[DEFERRED_RENDER], true);

    //RENDERING LIGHTS
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), true);
    m_inputs[LIGHT_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER), rootSignature->GetInput("point_lights"));
    m_inputs[LIGHT_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER),  rootSignature->GetInput("light_info"));
    m_inputs[LIGHT_RENDER][2] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(FOG_INFO_BUFFER),  rootSignature->GetInput("fog_info"));
    m_inputs[LIGHT_RENDER][3] = std::pair<ShaderInput*, ShaderInputDesc>(m_camera_buffer.get(),rootSignature->GetInput("camera_matrix"));
    m_subrenderers[LIGHT_RENDER]->Render(device, scene, m_inputs[LIGHT_RENDER], true);

    //RENDERING OCCLUDER MESHES
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), false);
    m_inputs[OCCLUDER_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(m_camera_buffer.get(), rootSignature->GetInput("camera_matrix"));
    m_inputs[OCCLUDER_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(MODEL_MAT_BUFFER), rootSignature->GetInput("model_matrix"));
    m_subrenderers[OCCLUDER_RENDER]->Render(device, scene, m_inputs[OCCLUDER_RENDER], false);

    //GENERATE LIGHT SCATTERING RENDER TARGET MIPS
    auto lightRenderTex = m_renderTargets[LIGHT_RENDER]->GetTexture(device, 0);
    lightRenderTex->GenerateMipmaps(device);

    //LIGHT SHAFT RENDER
    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), true);
    m_inputs[LIGHT_SHAFT_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER), rootSignature->GetInput("point_lights"));
    m_inputs[LIGHT_SHAFT_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER),  rootSignature->GetInput("light_info"));
    m_inputs[LIGHT_SHAFT_RENDER][2] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(FOG_INFO_BUFFER),  rootSignature->GetInput("fog_info"));
    m_inputs[LIGHT_SHAFT_RENDER][3] = std::pair<ShaderInput*, ShaderInputDesc>(m_camera_buffer.get(),rootSignature->GetInput("camera_matrix"));
    m_inputs[LIGHT_SHAFT_RENDER][4] = std::pair<ShaderInput*, ShaderInputDesc>(lightRenderTex.get(), rootSignature->GetInput("base_tex"));
    m_subrenderers[LIGHT_SHAFT_RENDER]->Render(device, scene, m_inputs[LIGHT_SHAFT_RENDER], true);

    //LIGHT SHAFT UPSCALE
    auto lightShaftTex = m_renderTargets[LIGHT_SHAFT_RENDER]->GetTexture(device, 0);
    m_inputs[UPSCALING_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(lightShaftTex.get(), rootSignature->GetInput("base_tex"));
    m_subrenderers[UPSCALING_RENDER]->Render(device, scene, m_inputs[UPSCALING_RENDER], true);

    //PBR RENDER
    auto upscaledTex = m_renderTargets[UPSCALING_RENDER]->GetTexture(device, 0);
    for (int i = 0; i < 4; i++)
    {
    auto texture = m_renderTargets[DEFERRED_RENDER]->GetTexture(device, i);
    m_inputs[PBR_RENDER][i] = std::pair<ShaderInput*, ShaderInputDesc>(texture.get(), m_mainInputs->GetInput("GBuffer" + std::to_string(i + 1)));
    }
    m_inputs[PBR_RENDER][4] = std::pair<ShaderInput*, ShaderInputDesc>(upscaledTex.get(), rootSignature->GetInput("base_tex"));
    m_inputs[PBR_RENDER][5] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER), rootSignature->GetInput("point_lights"));
    m_inputs[PBR_RENDER][6] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER),  rootSignature->GetInput("light_info"));
    m_inputs[PBR_RENDER][7] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(DIR_LIGHT_BUFFER), rootSignature->GetInput("dir_lights"));
    m_inputs[PBR_RENDER][8] = std::pair<ShaderInput*, ShaderInputDesc>(m_camera_buffer.get(),rootSignature->GetInput("camera_matrix"));
    m_subrenderers[PBR_RENDER]->Render(device, scene, m_inputs[PBR_RENDER], true);

    if (raytraced)
    {
        commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(m_rtInputs->GetSignature()), false);
        m_subrenderers[RT_RENDER]->Render(device, scene, m_inputs[RT_RENDER], true);
    }

    auto& boundRT = raytraced ? m_renderTargets[RT_RENDER] : m_renderTargets[PBR_RENDER];
    device.GetRenderTarget()->CopyTo(device, boundRT, 0, 0);
    device.GetRenderTarget()->Bind(device, device.GetDepthStencil().get());
}
