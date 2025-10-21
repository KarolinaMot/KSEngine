#include "../Renderer.hpp"

#include <device/Device.hpp>

#pragma warning (push, 0)
#include <glm/glm.hpp>
#pragma warning (pop)

#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DepthStencil.hpp>
#include <renderer/InfoStructs.hpp>
#include <renderer/RenderTarget.hpp>
#include <renderer/Shader.hpp>
#include <renderer/Subrenderer.hpp>
#include <renderer/ShaderInputBlueprintBuilder.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ComputeRenderer.hpp>
#include <renderer/ModelRenderer.hpp>
#include <renderer/RTRenderer.hpp>
#include <renderer/UniformBuffer.hpp>
#include <renderer/DX12/Helpers/DX12Common.hpp>

#include <resources/Texture.hpp>
#include <resources/Image.hpp>
#include <scene/Scene.hpp>
KS::Renderer::Renderer(Device& device, Scene& scene)
{
    SamplerDesc clampSampler;
    clampSampler.addressMode = SamplerAddressMode::SAM_CLAMP;

    m_mainInputs = ShaderInputBlueprintBuilder()
                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"camera_matrix"})
                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"model_index", "fog_info"})
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "base_tex")
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "normal_tex")
                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "emissive_tex")
                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "roughmet_tex")
                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "occlusion_tex")
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "PBRRes", KS::ShaderInputMod::READ_WRITE)
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer1", KS::ShaderInputMod::READ_WRITE)
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer2", KS::ShaderInputMod::READ_WRITE)
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer3", KS::ShaderInputMod::READ_WRITE)
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer4", KS::ShaderInputMod::READ_WRITE)
                       .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 100, "dir_lights")
                       .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 100, "point_lights")
                       .AddStorageBuffer(KS::ShaderInputVisibility::VERTEX, 200, "model_matrix")
                       .AddStorageBuffer(KS::ShaderInputVisibility::PIXEL, 200, "material_info")
                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"light_info"})
                       .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, KS::SamplerDesc{})
                       .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, clampSampler)
                       .Build(device, "MAIN SIGNATURE");

    m_rtInputs = KS::ShaderInputBlueprintBuilder()
                    .AddTexture(KS::ShaderInputVisibility::COMPUTE, "output_tex", ShaderInputMod::READ_WRITE)
                    .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, "BVH")
                    .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"camera_buffer"})
                    .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 200, "material_info")
                    .SetLocal()
                    .Build(device, "RT SIGNATURE");

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
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), Formats::R8G8B8A8_UNORM);
        deferredRendererTex[i][1] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.0f, 0.f, 0.f, 1.f), Formats::R32G32B32A32_FLOAT);
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
                                      glm::vec4(0.5f, 0.5f, 0.5f, 1.f), Formats::R8G8B8A8_UNORM, -1, RAYTRACE_RT_SLOT+i);

        lightRenderingTex[i] = std::make_shared<Texture>(
            device, device.GetWidth(), device.GetHeight(),
            Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE, glm::vec4(0.f, 0.f, 0.f, 0.f),
            Formats::R32G32B32A32_FLOAT, static_cast<std::uint16_t>(4));

        lightShaftTex[i] = std::make_shared<Texture>(device, device.GetWidth() / 4, device.GetHeight() / 4,
                                                     Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                                     glm::vec4(0.f, 0.f, 0.f, 0.f), Formats::R32G32B32A32_FLOAT);

        upscaledLightShaftTex[i] =
            std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(),
                                      Texture::TextureFlags::RENDER_TARGET | Texture::TextureFlags::RW_TEXTURE,
                                      glm::vec4(0.f, 0.f, 0.f, 0.f), Formats::R8G8B8A8_UNORM);
    }

    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;

    deferredRendererDepthTex =
        std::make_shared<Texture>(device, device.GetWidth(), device.GetHeight(), Texture::TextureFlags::DEPTH_TEXTURE,
                                  glm::vec4(1.f), Formats::D32_FLOAT);
    m_deferredRendererDepthStencil = std::make_shared<DepthStencil>(device, deferredRendererDepthTex);

    int fullInputFlags = Shader::HAS_POSITIONS | Shader::HAS_NORMALS | Shader::HAS_UVS | Shader::HAS_TANGENTS;
    int positionsInputFlags = Shader::HAS_POSITIONS;

    std::shared_ptr<Shader> mainShader = std::make_shared<Shader>(
        device, ShaderType::ST_MESH_RENDER, m_mainInputs, std::initializer_list<std::string>{"assets/shaders/Deferred.hlsl"},
        std::initializer_list<Formats>{Formats::R8G8B8A8_UNORM, Formats::R32G32B32A32_FLOAT, Formats::R8G8B8A8_UNORM,
                                       Formats::R8G8B8A8_UNORM},
        fullInputFlags);

    std::shared_ptr<Shader> lightOccluderShader =
        std::make_shared<Shader>(device, ShaderType::ST_MESH_RENDER, m_mainInputs,
                                 std::initializer_list<std::string>{"assets/shaders/OccluderShader.hlsl"},
                                 std::initializer_list<Formats>{Formats::R32G32B32A32_FLOAT}, positionsInputFlags);

    std::shared_ptr<Shader> computePBRShader = std::make_shared<Shader>(
        device, ShaderType::ST_COMPUTE, m_mainInputs, std::initializer_list<std::string>{"assets/shaders/Main.hlsl"},
        std::initializer_list<Formats>{});

    std::shared_ptr<Shader> lightRendererShader = std::make_shared<Shader>(
        device, ShaderType::ST_COMPUTE, m_mainInputs, std::initializer_list<std::string>{"assets/shaders/LightRenderer.hlsl"},
        std::initializer_list<Formats>{});

    std::shared_ptr<Shader> lightShaftShader = std::make_shared<Shader>(
        device, ShaderType::ST_COMPUTE, m_mainInputs,
        std::initializer_list<std::string>{"assets/shaders/LightShaftShader.hlsl"}, std::initializer_list<Formats>{});

    std::shared_ptr<Shader> upscalingShader = std::make_shared<Shader>(
        device, ShaderType::ST_COMPUTE, m_mainInputs, std::initializer_list<std::string>{"assets/shaders/Upscaling.hlsl"},
        std::initializer_list<Formats>{});

    std::shared_ptr<Shader> rtShader = std::make_shared<Shader>(
         device, ShaderType::ST_RAYTRACER, m_rtInputs,
         std::initializer_list<std::string>{"assets/shaders/Hit.hlsl", "assets/shaders/Miss.hlsl",
         "assets/shaders/RayGen.hlsl"}, std::initializer_list<Formats>{});

    m_renderTargets[DEFERRED_RENDER] = std::make_shared<RenderTarget>();
    for (int i = 0; i < 4; i++)
    {
        m_renderTargets[DEFERRED_RENDER]->AddTexture(device, deferredRendererTex[0][i], deferredRendererTex[1][i],
                                                     "DEFERRED RENDERER" + std::to_string(i));
    }

    m_renderTargets[PBR_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[PBR_RENDER]->AddTexture(device, pbrResTex[0], pbrResTex[1], "PBR RENDER RES");

    m_renderTargets[RT_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[RT_RENDER]->AddTexture(device, raytracingResTex[0], raytracingResTex[1],
                                           "RAYTRACED RENDER RES");

    m_renderTargets[LIGHT_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[LIGHT_RENDER]->AddTexture(device, lightRenderingTex[0], lightRenderingTex[1],
                                              "LIGHT RENDER RES");

    m_renderTargets[LIGHT_SHAFT_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[LIGHT_SHAFT_RENDER]->AddTexture(device, lightShaftTex[0], lightShaftTex[1],
                                                    "LIGHT SHAFT RENDER RES");

    m_renderTargets[UPSCALING_RENDER] = std::make_shared<RenderTarget>();
    m_renderTargets[UPSCALING_RENDER]->AddTexture(device, upscaledLightShaftTex[0], upscaledLightShaftTex[1],
                                                  "UPSCALED LIGHT SHAFT RENDER RES");

    SubRendererDesc defferedDesc;
    defferedDesc.shader = mainShader;
    defferedDesc.renderTarget = m_renderTargets[DEFERRED_RENDER];
    defferedDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[DEFERRED_RENDER] = std::make_unique<ModelRenderer>(device, defferedDesc);

    SubRendererDesc pbrDesc;
    pbrDesc.shader = computePBRShader;
    pbrDesc.renderTarget = m_renderTargets[PBR_RENDER];
    pbrDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[PBR_RENDER] = std::make_unique<ComputeRenderer>(device, pbrDesc);

    SubRendererDesc occluderDesc;
    occluderDesc.shader = lightOccluderShader;
    occluderDesc.renderTarget = m_renderTargets[LIGHT_RENDER];
    occluderDesc.depthStencil = m_deferredRendererDepthStencil;
    m_subrenderers[OCCLUDER_RENDER] = std::make_unique<ModelRenderer>(device, occluderDesc);

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
    m_subrenderers[RT_RENDER] = std::make_unique<RTRenderer>(device, scene, rtDesc);

    m_inputs[DEFERRED_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(6);
    m_inputs[OCCLUDER_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(2);
    m_inputs[PBR_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(10);
    m_inputs[LIGHT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(4);
    m_inputs[LIGHT_SHAFT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(5);
    m_inputs[UPSCALING_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(1);
    m_inputs[RT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(0);
    m_inputs[MIP_GEN] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(5);

     KS::SamplerDesc desc{};
     desc.addressMode = KS::SamplerAddressMode::SAM_CLAMP;
     desc.borderColor = SamplerBorderColor::SBC_TRANSPARENT_BLACK;
     desc.filter = SamplerFilter::SF_LINEAR;

     m_mipMapShaderInputs = KS::ShaderInputBlueprintBuilder()
                                .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"mipmap_info"})
                                .AddTexture(KS::ShaderInputVisibility::COMPUTE, "mip_1", KS::ShaderInputMod::READ_WRITE)
                                .AddTexture(KS::ShaderInputVisibility::COMPUTE, "mip_2", KS::ShaderInputMod::READ_WRITE)
                                .AddTexture(KS::ShaderInputVisibility::COMPUTE, "mip_3", KS::ShaderInputMod::READ_WRITE)
                                .AddTexture(KS::ShaderInputVisibility::COMPUTE, "mip_0", KS::ShaderInputMod::READ_ONLY)
                                .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, desc)
                                .Build(device, "MIPMAP SIGNATURE");

    std::shared_ptr<Shader> mipMapShader = std::make_shared<Shader>(device, ShaderType::ST_COMPUTE, m_mipMapShaderInputs,
                                               std::initializer_list<std::string>{"assets/shaders/MipGen.hlsl"},
                                               std::initializer_list<Formats>{});

    SubRendererDesc mipGenDesc{};
    mipGenDesc.shader = mipMapShader;
    m_subrenderers[MIP_GEN] = std::make_unique<ComputeRenderer>(device, mipGenDesc);


    device.CloseCommandContext(std::move(commandContext));
}

KS::Renderer::~Renderer() {}

void KS::Renderer::Render(Device& device, Scene& scene, const RenderTickParams& params, bool raytraced, bool recompileShaders)
{
    CameraMats cam{};
    cam.m_proj = params.projectionMatrix;
    cam.m_invProj = glm::inverse(params.projectionMatrix);
    cam.m_view = params.viewMatrix;
    cam.m_invView = glm::inverse(params.viewMatrix);
    cam.m_camera = params.projectionMatrix * params.viewMatrix;
    cam.m_cameraPos = glm::vec4(params.cameraPos, 1.f);
    scene.GetUniformBuffer(CAMERA_MAT_BUFFER)->Update(device, cam, 0);

    if (recompileShaders)
    {
        for (int i = 0; i < NUM_SUBRENDER; i++)
        {
            m_subrenderers[i]->Recompile(device);
        }
    }

    GodRays(device, scene);

    GenerateMipmaps(device, scene);

    if (raytraced) 
        Raytrace(device, scene);
    else
        Main(device, scene);
}

void KS::Renderer::GodRays(Device& device, Scene& scene)
{
    if (scene.GetLightInfo().numPointLights == 0) return;

    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto rootSignature = m_subrenderers[0]->GetShader()->GetShaderInput();
    auto frameIndex = device.GetCPUFrameIndex();

    // RENDERING LIGHTS
    m_inputs[LIGHT_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER),
                                                                         rootSignature->GetInput("point_lights"));
    m_inputs[LIGHT_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER),
                                                                         rootSignature->GetInput("light_info"));
    m_inputs[LIGHT_RENDER][2] =
        std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(FOG_INFO_BUFFER), rootSignature->GetInput("fog_info"));
    m_inputs[LIGHT_RENDER][3] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER),
                                                                         rootSignature->GetInput("camera_matrix"));
    m_subrenderers[LIGHT_RENDER]->Render(device, &commandContext, scene, m_inputs[LIGHT_RENDER], true);

    // RENDERING OCCLUDER MESHES
    m_inputs[OCCLUDER_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER),
                                                                            rootSignature->GetInput("camera_matrix"));
    m_inputs[OCCLUDER_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(MODEL_MAT_BUFFER),
                                                                            rootSignature->GetInput("model_matrix"));
    m_subrenderers[OCCLUDER_RENDER]->Render(device, &commandContext, scene, m_inputs[OCCLUDER_RENDER], false);

    commandContext = device.GetCommandContext();
    commandList = commandContext.m_commandList;

    // GENERATE LIGHT SCATTERING RENDER TARGET MIPS
    auto lightRenderTex = m_renderTargets[LIGHT_RENDER]->GetTexture(frameIndex, 0);
    device.AddToMipmapQueue(lightRenderTex);

    // LIGHT SHAFT RENDER
    m_inputs[LIGHT_SHAFT_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER),
                                                                               rootSignature->GetInput("point_lights"));
    m_inputs[LIGHT_SHAFT_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER),
                                                                               rootSignature->GetInput("light_info"));
    m_inputs[LIGHT_SHAFT_RENDER][2] =
        std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(FOG_INFO_BUFFER), 
                                                rootSignature->GetInput("fog_info"));

    m_inputs[LIGHT_SHAFT_RENDER][3] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER),
                                                                               rootSignature->GetInput("camera_matrix"));
    m_inputs[LIGHT_SHAFT_RENDER][4] =
        std::pair<ShaderInput*, ShaderInputDesc>(lightRenderTex.get(), rootSignature->GetInput("base_tex"));
    m_subrenderers[LIGHT_SHAFT_RENDER]->Render(device, &commandContext, scene, m_inputs[LIGHT_SHAFT_RENDER], true);

    // LIGHT SHAFT UPSCALE
    auto lightShaftTex = m_renderTargets[LIGHT_SHAFT_RENDER]->GetTexture(frameIndex, 0);
    m_inputs[UPSCALING_RENDER][0] =
        std::pair<ShaderInput*, ShaderInputDesc>(lightShaftTex.get(), rootSignature->GetInput("base_tex"));
    m_subrenderers[UPSCALING_RENDER]->Render(device, &commandContext, scene, m_inputs[UPSCALING_RENDER], true);

    m_subrenderers[UPSCALING_RENDER]->Render(device, &commandContext, scene, m_inputs[UPSCALING_RENDER], true);

    device.CloseCommandContext(std::move(commandContext));
}

void KS::Renderer::Main(Device& device, Scene& scene)
{
    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto& rootSignature = m_mainInputs;
    auto frameIndex = device.GetCPUFrameIndex();
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    // DEFERRED RENDERER
    m_inputs[DEFERRED_RENDER][0] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetStorageBuffer(DIR_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("dir_lights")));
    m_inputs[DEFERRED_RENDER][1] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("point_lights")));
    m_inputs[DEFERRED_RENDER][2] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("light_info")));
    m_inputs[DEFERRED_RENDER][3] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("camera_matrix")));
    m_inputs[DEFERRED_RENDER][4] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetStorageBuffer(MODEL_MAT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("model_matrix")));
    m_inputs[DEFERRED_RENDER][5] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetStorageBuffer(MATERIAL_INFO_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("material_info")));

    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), false);
    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);

    m_subrenderers[DEFERRED_RENDER]->Render(device, &commandContext, scene, m_inputs[DEFERRED_RENDER], true);

    commandContext = device.GetCommandContext();
    commandList = commandContext.m_commandList;

    // PBR RENDER
    for (int i = 0; i < 4; i++)
    {
        auto texture = m_renderTargets[DEFERRED_RENDER]->GetTexture(frameIndex, i);
        m_inputs[PBR_RENDER][i] = std::pair<ShaderInput*, ShaderInputDesc>(
            texture.get(), m_mainInputs->GetInput("GBuffer" + std::to_string(i + 1)));
    }

    auto upscaledTex = m_renderTargets[UPSCALING_RENDER]->GetTexture(frameIndex, 0);
    m_inputs[PBR_RENDER][4] = std::pair<ShaderInput*, ShaderInputDesc>(upscaledTex.get(),
    rootSignature->GetInput("base_tex"));
    m_inputs[PBR_RENDER][5] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("point_lights")));
    m_inputs[PBR_RENDER][6] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("light_info")));
    m_inputs[PBR_RENDER][7] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetStorageBuffer(DIR_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("dir_lights")));
    m_inputs[PBR_RENDER][8] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("camera_matrix")));
    m_inputs[PBR_RENDER][9] = std::pair<ShaderInput*, ShaderInputBindDesc>(scene.GetSkydomeTex(device, *commandList).get(), ShaderInputBindDesc(rootSignature->GetInput("normal_tex")));

    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), true);

    m_subrenderers[PBR_RENDER]->Render(device, &commandContext, scene, m_inputs[PBR_RENDER], true);

    device.GetRenderTarget()->CopyTo(*commandList, frameIndex, m_renderTargets[PBR_RENDER], 0, 0);

    device.CloseCommandContext(std::move(commandContext));
}

void KS::Renderer::GenerateMipmaps(Device& device, Scene& scene)
{
    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto texCount = device.GetTexWithoutMipmapCount();
    
    for (int i = 0; i < texCount; i++)
    {
        auto texture = device.GetTextureForMipmapGen(i);
        if (!texture) continue;

        auto mipDesc = texture->GetMipmapInfo();
        scene.GetUniformBuffer(MIP_GEN_INFO)->Update(device, mipDesc, i);

        m_inputs[MIP_GEN][0] = std::pair<ShaderInput*, ShaderInputBindDesc>(
            scene.GetUniformBuffer(MIP_GEN_INFO), ShaderInputBindDesc(i, m_mipMapShaderInputs->GetInput("mipmap_info")));
        m_inputs[MIP_GEN][1] = std::pair<ShaderInput*, ShaderInputBindDesc>(
            texture, ShaderInputBindDesc(1, m_mipMapShaderInputs->GetInput("mip_1")));
        m_inputs[MIP_GEN][2] = std::pair<ShaderInput*, ShaderInputBindDesc>(
            texture, ShaderInputBindDesc(2, m_mipMapShaderInputs->GetInput("mip_2")));
        m_inputs[MIP_GEN][3] = std::pair<ShaderInput*, ShaderInputBindDesc>(
            texture, ShaderInputBindDesc(3, m_mipMapShaderInputs->GetInput("mip_3")));
        m_inputs[MIP_GEN][4] = std::pair<ShaderInput*, ShaderInputBindDesc>(
            texture, ShaderInputBindDesc(m_mipMapShaderInputs->GetInput("mip_0")));

       reinterpret_cast<ComputeRenderer*>(m_subrenderers[MIP_GEN].get())
            ->SetDispatchSize(static_cast<uint32_t>(1.f / mipDesc.TexelSize.x), static_cast<uint32_t>(1.f / mipDesc.TexelSize.y));

       m_subrenderers[MIP_GEN]->Render(device, &commandContext, scene, m_inputs[MIP_GEN], false);
    }

    device.ClearMipmapQueue();
    device.CloseCommandContext(std::move(commandContext));
}

void KS::Renderer::Raytrace(Device& device, Scene& scene)
{
    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto rootSignature = m_subrenderers[RT_RENDER]->GetShader()->GetShaderInput();
    auto frameIndex = device.GetCPUFrameIndex();
    auto resourceHeap = reinterpret_cast<DXDescHeap*>(device.GetResourceHeap());

    commandList->BindDescriptorHeaps(resourceHeap, nullptr, nullptr);
    m_subrenderers[RT_RENDER]->Render(device, &commandContext, scene, m_inputs[RT_RENDER], true);

    auto& boundRT = m_renderTargets[RT_RENDER];
    device.GetRenderTarget()->CopyTo(*commandList, frameIndex, boundRT, 0, 0);

    device.CloseCommandContext(std::move(commandContext));
}