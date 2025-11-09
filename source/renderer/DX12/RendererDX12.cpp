#include "../Renderer.hpp"

#include <device/Device.hpp>

#pragma warning(push, 0)
#include <glm/glm.hpp>
#pragma warning(pop)

#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <renderer/DX12/Helpers/DXCommandContextPool.hpp>
#include <renderer/DX12/Helpers/DXResource.hpp>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
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
#include <renderer/InfoStructs.hpp>

#include <resources/Texture.hpp>
#include <resources/Skydome.hpp>
#include <resources/Image.hpp>
#include <scene/Scene.hpp>

KS::Renderer::Renderer(Device& device)
{
    SamplerDesc clampSampler;
    clampSampler.addressMode = SamplerAddressMode::SAM_CLAMP;

    m_mainInputs = ShaderInputBlueprintBuilder()
                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"camera_matrix"})
                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"model_index", "fog_info"})
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, RESOURCE_HEAP_SIZE, {"textures"}, ShaderInputMod::READ_ONLY,
                              1)
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"compute_res"}, KS::ShaderInputMod::READ_WRITE)
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"GBuffer1"}, KS::ShaderInputMod::READ_WRITE)
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"GBuffer2"}, KS::ShaderInputMod::READ_WRITE)
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"GBuffer3"}, KS::ShaderInputMod::READ_WRITE)
                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"GBuffer4"}, KS::ShaderInputMod::READ_WRITE)
                       .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1,
                                         {"dir_lights", "cubemap_src", "light_render_res"})
                       .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"point_lights", "cubemap_tex"})
                       .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"model_matrix", "light_shaft_res"})
                       .AddStorageBuffer(KS::ShaderInputVisibility::PIXEL, 1, {"material_info"})
                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"light_info"})
                       .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, KS::SamplerDesc{})
                       .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, clampSampler)
                       .Build(device, "MAIN SIGNATURE");

    m_rtInputs =
        KS::ShaderInputBlueprintBuilder()
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"output_tex"}, ShaderInputMod::READ_WRITE)
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"BVH"})
            .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"camera_buffer"})
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"material_info"})
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"modelMats"})
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"dir_lights"})
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"point_lights"})
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"cubemap_tex"})
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"normals"}, ShaderInputMod::READ_ONLY, 1)
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"indices"}, ShaderInputMod::READ_ONLY, 2)
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"vertexPos"}, ShaderInputMod::READ_ONLY, 3)
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"uvs"}, ShaderInputMod::READ_ONLY, 4)
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"tangents"}, ShaderInputMod::READ_ONLY, 5)
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, RESOURCE_HEAP_SIZE, {"textures"}, ShaderInputMod::READ_ONLY,
                              6)
            .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, KS::SamplerDesc{})
            .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"light_info"})
            .SetLocal()
            .Build(device, "RT SIGNATURE");


    auto commandContext = device.GetCommandContext();

    int fullInputFlags =
        Shader::HAS_POSITIONS | Shader::HAS_NORMALS | Shader::HAS_UVS | Shader::HAS_TANGENTS | Shader::PBR_TEXTURES;
    int positionsInputFlags = Shader::HAS_POSITIONS;
    int skyboxInputFlags = Shader::HAS_POSITIONS | Shader::DEPTH_DISABLED | Shader::NO_CULLING;

    std::shared_ptr<Shader> mainShader = ShaderBuilder()
                                             .SetType(ShaderType::ST_MESH_RENDER)
                                             .AddShaderPath("assets/shaders/Deferred.hlsl", L"main")
                                             .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                             .AddRenderTarget(Formats::R32G32B32A32_FLOAT)
                                             .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                             .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                             .SetFlags(fullInputFlags)
                                             .SetGlobalSignature(m_mainInputs)
                                             .Build(device);

    std::shared_ptr<Shader> lightOccluderShader = ShaderBuilder()
                                                      .SetType(ShaderType::ST_MESH_RENDER)
                                                      .AddShaderPath("assets/shaders/OccluderShader.hlsl", L"main")
                                                      .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                                      .SetFlags(positionsInputFlags)
                                                      .SetGlobalSignature(m_mainInputs)
                                                      .Build(device);

    std::shared_ptr<Shader> skyboxRenderShader = ShaderBuilder()
                                                     .SetType(ShaderType::ST_MESH_RENDER)
                                                     .AddShaderPath("assets/shaders/RenderCubemap.hlsl", L"main")
                                                     .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                                     .SetFlags(skyboxInputFlags)
                                                     .SetGlobalSignature(m_mainInputs)
                                                     .Build(device);

    std::shared_ptr<Shader> computePBRShader = ShaderBuilder()
                                                   .SetType(ShaderType::ST_COMPUTE)
                                                   .AddShaderPath("assets/shaders/Main.hlsl", L"main")
                                                   .SetGlobalSignature(m_mainInputs)
                                                   .Build(device);
        

    std::shared_ptr<Shader> lightRendererShader = ShaderBuilder()
                                                      .SetType(ShaderType::ST_COMPUTE)
                                                      .AddShaderPath("assets/shaders/LightRenderer.hlsl", L"main")
                                                      .SetGlobalSignature(m_mainInputs)
                                                      .Build(device);

    std::shared_ptr<Shader> lightShaftShader = ShaderBuilder()
                                                   .SetType(ShaderType::ST_COMPUTE)
                                                   .AddShaderPath("assets/shaders/LightShaftShader.hlsl", L"main")
                                                   .SetGlobalSignature(m_mainInputs)
                                                   .Build(device);

    std::shared_ptr<Shader> upscalingShader = ShaderBuilder()
                                                  .SetType(ShaderType::ST_COMPUTE)
                                                  .AddShaderPath("assets/shaders/Upscaling.hlsl", L"main")
                                                  .SetGlobalSignature(m_mainInputs)
                                                  .Build(device);

    std::shared_ptr<Shader> rtShader = ShaderBuilder()
                                           .SetType(ShaderType::ST_RAYTRACER)
                                           .AddShaderPath("assets/shaders/Hit.hlsl", L"ClosestHit")
                                           .AddShaderPath("assets/shaders/Miss.hlsl", L"Miss")
                                           .AddShaderPath("assets/shaders/RayGen.hlsl", L"RayGen")
                                           .AddLocalShaderInputLink(m_rtInputs, std::initializer_list<LPCWSTR>{L"ClosestHit", L"Miss", L"RayGen"})
                                           //.SetGlobalSignature(m_rtInputs)     
                                           .Build(device);


    std::shared_ptr<Shader> skyboxShader = ShaderBuilder()
                                               .SetType(ShaderType::ST_COMPUTE)
                                               .AddShaderPath("assets/shaders/SkyboxGen.hlsl", L"main")
                                               .SetGlobalSignature(m_mainInputs)
                                               .Build(device);

    m_subrenderers[DEFERRED_RENDER] = std::make_unique<ModelRenderer>(device, mainShader);
    m_subrenderers[CUBEMAP_RENDER] = std::make_unique<ModelRenderer>(device, skyboxRenderShader, true);
    m_subrenderers[PBR_RENDER] = std::make_unique<ComputeRenderer>(device, computePBRShader);
    m_subrenderers[OCCLUDER_RENDER] = std::make_unique<ModelRenderer>(device, lightOccluderShader);
    m_subrenderers[LIGHT_RENDER] = std::make_unique<ComputeRenderer>(device, lightRendererShader);
    m_subrenderers[LIGHT_SHAFT_RENDER] = std::make_unique<ComputeRenderer>(device, lightShaftShader);
    m_subrenderers[UPSCALING_RENDER] = std::make_unique<ComputeRenderer>(device, upscalingShader);
    m_subrenderers[RT_RENDER] = std::make_unique<RTRenderer>(device, rtShader);
    m_subrenderers[CUBEMAP_GEN] = std::make_unique<ComputeRenderer>(device, skyboxShader);

    m_inputs[DEFERRED_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(6);
    m_inputs[OCCLUDER_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(2);
    m_inputs[PBR_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(9);
    m_inputs[LIGHT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(4);
    m_inputs[LIGHT_SHAFT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(5);
    m_inputs[UPSCALING_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(1);
    m_inputs[RT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(8);
    m_inputs[MIP_GEN] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(5);
    m_inputs[CUBEMAP_GEN] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(2);
    m_inputs[CUBEMAP_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(3);

    KS::SamplerDesc desc{};
    desc.addressMode = KS::SamplerAddressMode::SAM_CLAMP;
    desc.borderColor = SamplerBorderColor::SBC_TRANSPARENT_BLACK;
    desc.filter = SamplerFilter::SF_LINEAR;

    m_mipMapShaderInputs = KS::ShaderInputBlueprintBuilder()
                               .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"mipmap_info"})
                               .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"mip_1"}, KS::ShaderInputMod::READ_WRITE)
                               .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"mip_2"}, KS::ShaderInputMod::READ_WRITE)
                               .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"mip_3"}, KS::ShaderInputMod::READ_WRITE)
                               .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"mip_0"}, KS::ShaderInputMod::READ_ONLY)
                               .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, desc)
                               .Build(device, "MIPMAP SIGNATURE");

    std::shared_ptr<Shader> mipMapShader = ShaderBuilder()
                                               .SetType(ShaderType::ST_COMPUTE)
                                               .AddShaderPath("assets/shaders/MipGen.hlsl", L"main")
                                               .SetGlobalSignature(m_mipMapShaderInputs)
                                               .Build(device);
        
    m_subrenderers[MIP_GEN] = std::make_unique<ComputeRenderer>(device, mipMapShader);

    commandContext.Close();
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
    cam.m_cameraNoTranslation = params.projectionMatrix * glm::mat4(glm::mat3(params.viewMatrix));
    cam.m_cameraPos = glm::vec4(params.cameraPos, 1.f);
    scene.GetUniformBuffer(CAMERA_MAT_BUFFER)->Update(device, cam, 0);

    if (recompileShaders)
    {
        for (int i = 0; i < NUM_SUBRENDER; i++)
        {
            if (m_subrenderers[i]) m_subrenderers[i]->Recompile(device);
        }
    }

    GodRays(device, scene);
    GenCubemap(device, scene);

    GenerateMipmaps(device, scene);

    if (raytraced)
        Raytrace(device, scene);
    else
    {
        RenderCubemap(device, scene);
        Main(device, scene);
    }

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

    RenderParameters defPar{};
    defPar.clearRt = true;
    defPar.ds = scene.GetDepthStencil();
    defPar.rt = scene.GetRenderTarget(LIGHT_RENDER);
    defPar.scene = &scene;
    defPar.inputs = &m_inputs[LIGHT_RENDER];
    m_subrenderers[LIGHT_RENDER]->Render(device, &commandContext, defPar);

    // RENDERING OCCLUDER MESHES
    m_inputs[OCCLUDER_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER),
                                                                            rootSignature->GetInput("camera_matrix"));
    m_inputs[OCCLUDER_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(MODEL_MAT_BUFFER),
                                                                            rootSignature->GetInput("model_matrix"));

    defPar.clearRt = false;
    defPar.ds = scene.GetDepthStencil();
    defPar.rt = scene.GetRenderTarget(LIGHT_RENDER);
    defPar.scene = &scene;
    defPar.inputs = &m_inputs[OCCLUDER_RENDER];
    m_subrenderers[OCCLUDER_RENDER]->Render(device, &commandContext, defPar);

    commandContext = device.GetCommandContext();
    commandList = commandContext.m_commandList;

    // GENERATE LIGHT SCATTERING RENDER TARGET MIPS
    auto lightRenderTex = scene.GetRenderTarget(LIGHT_RENDER)->GetTexture(frameIndex, 0);
    scene.AddToMipmapQueue(lightRenderTex);

    // LIGHT SHAFT RENDER
    m_inputs[LIGHT_SHAFT_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER),
                                                                               rootSignature->GetInput("point_lights"));
    m_inputs[LIGHT_SHAFT_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER),
                                                                               rootSignature->GetInput("light_info"));
    m_inputs[LIGHT_SHAFT_RENDER][2] =
        std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(FOG_INFO_BUFFER), rootSignature->GetInput("fog_info"));

    m_inputs[LIGHT_SHAFT_RENDER][3] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER),
                                                                               rootSignature->GetInput("camera_matrix"));
    m_inputs[LIGHT_SHAFT_RENDER][4] =
        std::pair<ShaderInput*, ShaderInputDesc>(lightRenderTex.get(), rootSignature->GetInput("light_render_res"));

    defPar.clearRt = true;
    defPar.ds = scene.GetDepthStencil();
    defPar.rt = scene.GetRenderTarget(LIGHT_SHAFT_RENDER);
    defPar.scene = &scene;
    defPar.inputs = &m_inputs[LIGHT_SHAFT_RENDER];

    m_subrenderers[LIGHT_SHAFT_RENDER]->Render(device, &commandContext, defPar);

    // LIGHT SHAFT UPSCALE
    auto lightShaftTex = scene.GetRenderTarget(LIGHT_SHAFT_RENDER)->GetTexture(frameIndex, 0);
    m_inputs[UPSCALING_RENDER][0] =
        std::pair<ShaderInput*, ShaderInputDesc>(lightShaftTex.get(), rootSignature->GetInput("base_tex"));

    defPar.clearRt = true;
    defPar.ds = scene.GetDepthStencil();
    defPar.rt = scene.GetRenderTarget(UPSCALING_RENDER);
    defPar.scene = &scene;
    defPar.inputs = &m_inputs[UPSCALING_RENDER];

    m_subrenderers[UPSCALING_RENDER]->Render(device, &commandContext, defPar);

    commandContext.Close();
}

void KS::Renderer::Main(Device& device, Scene& scene)
{
    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto& rootSignature = m_mainInputs;
    auto frameIndex = device.GetCPUFrameIndex();

    // DEFERRED RENDERER
    m_inputs[DEFERRED_RENDER][0] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(DIR_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("dir_lights")));
    m_inputs[DEFERRED_RENDER][1] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(POINT_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("point_lights")));
    m_inputs[DEFERRED_RENDER][2] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetUniformBuffer(LIGHT_INFO_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("light_info")));
    m_inputs[DEFERRED_RENDER][3] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetUniformBuffer(CAMERA_MAT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("camera_matrix")));
    m_inputs[DEFERRED_RENDER][4] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(MODEL_MAT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("model_matrix")));
    m_inputs[DEFERRED_RENDER][5] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(MATERIAL_INFO_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("material_info")));

    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), false);

    RenderParameters defPar{};
    defPar.clearRt = true;
    defPar.ds = scene.GetDepthStencil();
    defPar.rt = scene.GetRenderTarget(DEFERRED_RENDER);
    defPar.scene = &scene;
    defPar.inputs = &m_inputs[DEFERRED_RENDER];
    m_subrenderers[DEFERRED_RENDER]->Render(device, &commandContext, defPar);

    commandContext = device.GetCommandContext();
    commandList = commandContext.m_commandList;

    // PBR RENDER
    for (int i = 0; i < 4; i++)
    {
        auto texture = scene.GetRenderTarget(DEFERRED_RENDER)->GetTexture(frameIndex, i);
        m_inputs[PBR_RENDER][i] =
            std::pair<ShaderInput*, ShaderInputDesc>(texture.get(), m_mainInputs->GetInput("GBuffer" + std::to_string(i + 1)));
    }

    auto upscaledTex = scene.GetRenderTarget(UPSCALING_RENDER)->GetTexture(frameIndex, 0);
    m_inputs[PBR_RENDER][4] = std::pair<ShaderInput*, ShaderInputDesc>(upscaledTex.get(), rootSignature->GetInput("light_shaft_res"));
    m_inputs[PBR_RENDER][5] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(POINT_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("point_lights")));
    m_inputs[PBR_RENDER][6] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetUniformBuffer(LIGHT_INFO_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("light_info")));
    m_inputs[PBR_RENDER][7] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(DIR_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("dir_lights")));
    m_inputs[PBR_RENDER][8] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetUniformBuffer(CAMERA_MAT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("camera_matrix")));

    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), true);

    RenderParameters pbrPar{};
    pbrPar.clearRt = false;
    pbrPar.ds = scene.GetDepthStencil();
    pbrPar.rt = scene.GetRenderTarget(PBR_RENDER);
    pbrPar.scene = &scene;
    pbrPar.inputs = &m_inputs[PBR_RENDER];

    m_subrenderers[PBR_RENDER]->Render(device, &commandContext, pbrPar);

    device.GetRenderTarget()->CopyTo(*commandList, frameIndex, scene.GetRenderTarget(PBR_RENDER), 0, 0);

    commandContext.Close();
}

void KS::Renderer::GenerateMipmaps(Device& device, Scene& scene)
{
    auto commandContext = device.GetCommandContext();
    auto commandList = commandContext.m_commandList;
    auto texCount = scene.GetTexWithoutMipmapCount();

    for (int i = 0; i < texCount; i++)
    {
        auto texture = scene.GetTextureForMipmapGen(i);
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
        m_inputs[MIP_GEN][4] =
            std::pair<ShaderInput*, ShaderInputBindDesc>(texture, ShaderInputBindDesc(m_mipMapShaderInputs->GetInput("mip_0")));

        reinterpret_cast<ComputeRenderer*>(m_subrenderers[MIP_GEN].get())
            ->SetDispatchSize(static_cast<uint32_t>(1.f / mipDesc.TexelSize.x),
                              static_cast<uint32_t>(1.f / mipDesc.TexelSize.y));

        RenderParameters defPar{};
        defPar.clearRt = false;
        defPar.scene = &scene;
        defPar.inputs = &m_inputs[MIP_GEN];

        m_subrenderers[MIP_GEN]->Render(device, &commandContext, defPar);
    }

    scene.ClearMipmapQueue();
    commandContext.Close();
}

void KS::Renderer::Raytrace(Device& device, Scene& scene)
{
    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto rootSignature = m_subrenderers[RT_RENDER]->GetShader()->GetShaderInput();
    auto frameIndex = device.GetCPUFrameIndex();

    RenderParameters defPar{};
    defPar.clearRt = true;
    defPar.scene = &scene;
    defPar.rt = scene.GetRenderTarget(RT_RENDER);
    defPar.inputs = &m_inputs[RT_RENDER];

    m_subrenderers[RT_RENDER]->Render(device, &commandContext, defPar);

    auto boundRT = scene.GetRenderTarget(RT_RENDER);
    device.GetRenderTarget()->CopyTo(*commandList, frameIndex, boundRT, 0, 0);

    commandContext.Close();
}

void KS::Renderer::GenCubemap(Device& device, Scene& scene)
{
    auto skydome = scene.GetSkydome();
    if (skydome.first->GetIsReady()) return;

    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto rootSignature = m_subrenderers[CUBEMAP_GEN]->GetShader()->GetShaderInput();

    m_inputs[CUBEMAP_GEN][0] = std::pair<ShaderInput*, ShaderInputDesc>(
        reinterpret_cast<ShaderInput*>(scene.GetTexture(device, commandList.get(), skydome.second).get()),
        rootSignature->GetInput("cubemap_src"));
    m_inputs[CUBEMAP_GEN][1] = std::pair<ShaderInput*, ShaderInputDesc>(reinterpret_cast<ShaderInput*>(skydome.first.get()),
                                                                        rootSignature->GetInput("compute_res"));

    reinterpret_cast<ComputeRenderer*>(m_subrenderers[CUBEMAP_GEN].get())
        ->SetDispatchSize(skydome.first->GetWidth(), skydome.first->GetHeight(), 6);

    RenderParameters defPar{};
    defPar.clearRt = true;
    defPar.scene = &scene;
    defPar.inputs = &m_inputs[CUBEMAP_GEN];

    m_subrenderers[CUBEMAP_GEN]->Render(device, &commandContext, defPar);

    commandContext.Close();
}

void KS::Renderer::RenderCubemap(Device& device, Scene& scene)
{
    auto skydome = scene.GetSkydome();
    if (!skydome.first->GetIsReady()) return;

    auto commandContext = device.GetCommandContext();
    auto rootSignature = m_subrenderers[CUBEMAP_GEN]->GetShader()->GetShaderInput();

    m_inputs[CUBEMAP_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER),
                                                                           rootSignature->GetInput("camera_matrix"));
    m_inputs[CUBEMAP_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(MODEL_MAT_BUFFER),
                                                                           rootSignature->GetInput("model_matrix"));
    m_inputs[CUBEMAP_RENDER][2] = std::pair<ShaderInput*, ShaderInputDesc>(reinterpret_cast<ShaderInput*>(skydome.first.get()),
                                                                           rootSignature->GetInput("cubemap_tex"));

    RenderParameters par{};
    par.clearRt = true;
    par.ds = scene.GetDepthStencil();
    par.rt = scene.GetRenderTarget(PBR_RENDER);
    par.scene = &scene;
    par.inputs = &m_inputs[CUBEMAP_RENDER];

    m_subrenderers[CUBEMAP_RENDER]->Render(device, &commandContext, par);

    commandContext.Close();
}
