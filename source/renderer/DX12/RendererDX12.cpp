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
    .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"camera_matrix", "culling_info"})                                     //b0
    .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"model_index", "fog_info"})                                           //b1
    .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, RESOURCE_HEAP_SIZE, {"textures"}, ShaderInputMod::READ_ONLY, 1)  // t0, s1
    .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"compute_res", "draw_indices"}, KS::ShaderInputMod::READ_WRITE)       //u0
    .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"GBuffer1"}, KS::ShaderInputMod::READ_WRITE)                          //u1
    .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"GBuffer2"}, KS::ShaderInputMod::READ_WRITE)                          //u2
    .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"GBuffer3"}, KS::ShaderInputMod::READ_WRITE)                          //u3
    .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"dir_lights", "cubemap_src", "light_render_res", "bounding_boxes"}) //t0
    .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"cubemap_tex"})                                              //t1
    .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"instance_data", "light_shaft_res"})                         //t2
    .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"Depth", "material_info"})            // t3
    .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"point_lights"})                                             //t4
    .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"light_info"})                                                        //b2
    .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, KS::SamplerDesc{})                                               //s0
    .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, clampSampler)                                                    //s1
    .Build(device, "MAIN SIGNATURE");

    m_rtInputs =
        KS::ShaderInputBlueprintBuilder()
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"output_tex"}, ShaderInputMod::READ_WRITE) //u0
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"GBufferA"}, ShaderInputMod::READ_WRITE)   //u1
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"GBufferB"}, ShaderInputMod::READ_WRITE)   // u2
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"directLighting"}, ShaderInputMod::READ_WRITE) //u3
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"BVH"}) //t0
            .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"camera_buffer"}) //b0
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"instance_data"}) //t1
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"dir_lights"}) //t2
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 1, {"point_lights"}) //t3
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"cubemap_tex"}) //t4
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"normals"}, ShaderInputMod::READ_ONLY, 1) //t0, space1
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"indices"}, ShaderInputMod::READ_ONLY, 2)//t0, space2
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"vertexPos"}, ShaderInputMod::READ_ONLY, 3)//t0, space3
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"uvs"}, ShaderInputMod::READ_ONLY, 4)//t0, space4
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, MAX_MESHES, {"tangents"}, ShaderInputMod::READ_ONLY, 5)//t0, space5
            .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, RESOURCE_HEAP_SIZE, {"textures"}, ShaderInputMod::READ_ONLY,
                              6)//t0, space6
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"Depth"}, ShaderInputMod::READ_ONLY) //t5
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, {"material_info"}, ShaderInputMod::READ_ONLY) //t6
            .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, KS::SamplerDesc{}) //s0
            .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"light_info"})//s1
            .Build(device, "RT SIGNATURE");


    auto commandContext = device.GetCommandContext();

    int fullInputFlags = Shader::HAS_POSITIONS | Shader::HAS_NORMALS | Shader::HAS_UVS | Shader::HAS_TANGENTS |
                         Shader::PBR_TEXTURES | Shader::NO_CULLING;
    int positionsInputFlags = Shader::HAS_POSITIONS;
    int skyboxInputFlags = Shader::HAS_POSITIONS | Shader::DEPTH_DISABLED | Shader::NO_CULLING;

    std::shared_ptr<Shader> mainShader = ShaderBuilder()
                                             .SetType(PipelineType::ST_MESH_RENDER)
                                             .AddShaderPath(ShaderType::COMBINED_PS_VS, "assets/shaders/Deferred.hlsl", L"main")
                                             .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                             .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                             .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                             .SetFlags(fullInputFlags)
                                             .SetGlobalSignature(m_mainInputs)
                                             .Build(device);

    std::shared_ptr<Shader> lightOccluderShader = ShaderBuilder()
                                                      .SetType(PipelineType::ST_MESH_RENDER)
                                                      .AddShaderPath(ShaderType::COMBINED_PS_VS,"assets/shaders/OccluderShader.hlsl", L"main")
                                                      .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                                      .SetFlags(positionsInputFlags)
                                                      .SetGlobalSignature(m_mainInputs)
                                                      .Build(device);

    std::shared_ptr<Shader> skyboxRenderShader = ShaderBuilder()
                                                     .SetType(PipelineType::ST_MESH_RENDER)
                                                     .AddShaderPath(ShaderType::COMBINED_PS_VS,"assets/shaders/RenderCubemap.hlsl", L"main")
                                                     .AddRenderTarget(Formats::R8G8B8A8_UNORM)
                                                     .SetFlags(skyboxInputFlags)
                                                     .SetGlobalSignature(m_mainInputs)
                                                     .Build(device);

    std::shared_ptr<Shader> computePBRShader = ShaderBuilder()
                                                   .SetType(PipelineType::ST_COMPUTE)
                                                   .AddShaderPath(ShaderType::COMPUTE_SHADER, "assets/shaders/Main.hlsl", L"main")
                                                   .SetGlobalSignature(m_mainInputs)
                                                   .Build(device);
        

    std::shared_ptr<Shader> lightRendererShader = ShaderBuilder()
                                                      .SetType(PipelineType::ST_COMPUTE)
                                                      .AddShaderPath(ShaderType::COMPUTE_SHADER,"assets/shaders/LightRenderer.hlsl", L"main")
                                                      .SetGlobalSignature(m_mainInputs)
                                                      .Build(device);

    std::shared_ptr<Shader> lightShaftShader = ShaderBuilder()
                                                   .SetType(PipelineType::ST_COMPUTE)
                                                   .AddShaderPath(ShaderType::COMPUTE_SHADER,"assets/shaders/LightShaftShader.hlsl", L"main")
                                                   .SetGlobalSignature(m_mainInputs)
                                                   .Build(device);

    std::shared_ptr<Shader> upscalingShader = ShaderBuilder()
                                                  .SetType(PipelineType::ST_COMPUTE)
                                                  .AddShaderPath(ShaderType::COMPUTE_SHADER,"assets/shaders/Upscaling.hlsl", L"main")
                                                  .SetGlobalSignature(m_mainInputs)
                                                  .Build(device);

    std::shared_ptr<Shader> rtShader = ShaderBuilder()
                                           .SetType(PipelineType::ST_RAYTRACER)
                                           .AddShaderPath(ShaderType::CLOSEST_HIT_SHADER, "assets/shaders/Hit.hlsl", L"MainClosestHit")
                                           .AddShaderPath(ShaderType::CLOSEST_HIT_SHADER, "assets/shaders/ShadowHit.hlsl", L"ShadowClosestHit")
                                           .AddShaderPath(ShaderType::CLOSEST_HIT_SHADER, "assets/shaders/HitGI.hlsl", L"GIClosestHit")
                                           .AddShaderPath(ShaderType::MISS_SHADER, "assets/shaders/Miss.hlsl", L"MainMiss")
                                           .AddShaderPath(ShaderType::MISS_SHADER, "assets/shaders/ShadowMiss.hlsl", L"ShadowMiss")
                                           .AddShaderPath(ShaderType::RAY_GEN_SHADER, "assets/shaders/RayGen.hlsl", L"RayGen")
                                           .AddHitGroup(L"MainHitGroup", L"MainClosestHit")
                                           .AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit")
                                           .AddHitGroup(L"GIHitGroup", L"GIClosestHit")
                                           .SetGlobalSignature(m_rtInputs)
                                           .Build(device);


    std::shared_ptr<Shader> skyboxShader = ShaderBuilder()
                                               .SetType(PipelineType::ST_COMPUTE)
                                               .AddShaderPath(ShaderType::COMPUTE_SHADER, "assets/shaders/SkyboxGen.hlsl", L"main")
                                               .SetGlobalSignature(m_mainInputs)
                                               .Build(device);

    std::shared_ptr<Shader> cullingShader =
        ShaderBuilder()
            .SetType(PipelineType::ST_COMPUTE)
            .AddShaderPath(ShaderType::COMPUTE_SHADER, "assets/shaders/CullingShader.hlsl", L"main")
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
    m_subrenderers[MESH_CULLING] = std::make_unique<ComputeRenderer>(device, cullingShader);

    m_inputs[DEFERRED_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(3);
    m_inputs[OCCLUDER_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(2);
    m_inputs[PBR_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(9);
    m_inputs[LIGHT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(4);
    m_inputs[LIGHT_SHAFT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(5);
    m_inputs[UPSCALING_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(1);
    m_inputs[RT_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(13);
    m_inputs[MIP_GEN] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(5);
    m_inputs[CUBEMAP_GEN] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(2);
    m_inputs[CUBEMAP_RENDER] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(2);
    m_inputs[MESH_CULLING] = std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>(3);

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
                                               .SetType(PipelineType::ST_COMPUTE)
                                               .AddShaderPath(ShaderType::COMPUTE_SHADER, "assets/shaders/MipGen.hlsl", L"main")
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
    cam.m_invCamera = glm::inverse(cam.m_camera);
    cam.m_cameraNoTranslation = params.projectionMatrix * glm::mat4(glm::mat3(params.viewMatrix));
    cam.m_cameraPos = glm::vec4(params.cameraPos, 1.f);

    auto cullingInfo = scene.GetCullingInfo();
    std::copy(params.frustum.begin(), params.frustum.end(), &cullingInfo.cameraPlane[0]);

    scene.GetUniformBuffer(CAMERA_MAT_BUFFER)->Update(device, cam, 0);
    scene.GetUniformBuffer(CULLING_INFO)->Update(device, cullingInfo, 0);

    if (recompileShaders)
    {
        for (int i = 0; i < NUM_SUBRENDER; i++)
        {
            if (m_subrenderers[i]) m_subrenderers[i]->Recompile(device);
        }
    }

    //GodRays(device, scene);
    GenCubemap(device, scene);

    GenerateMipmaps(device, scene);

    RenderCubemap(device, scene);
    Culling(device, scene);
    Main(device, scene, params.frustum, raytraced);

    if (raytraced)
        Raytrace(device, scene);
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
    m_inputs[OCCLUDER_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(INSTANCE_DATA_BUFFER),
                                                                            rootSignature->GetInput("instance_data"));

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

void KS::Renderer::Main(Device& device, Scene& scene, const std::array<Plane, 6>& plane, bool raytraced)
{
    auto commandContext = device.GetCommandContext();
    auto& commandList = commandContext.m_commandList;
    auto rootSignature = m_subrenderers[DEFERRED_RENDER]->GetShader()->GetShaderInput();
    auto frameIndex = device.GetCPUFrameIndex();

    // DEFERRED RENDERER
    m_inputs[DEFERRED_RENDER][0] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetUniformBuffer(CAMERA_MAT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("camera_matrix")));
    m_inputs[DEFERRED_RENDER][1] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(INSTANCE_DATA_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("instance_data")));
    m_inputs[DEFERRED_RENDER][2] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(MATERIAL_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("material_info")));

    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), false);

    RenderParameters defPar{};
    defPar.clearRt = true;
    defPar.ds = scene.GetDepthStencil();
    defPar.rt = scene.GetRenderTarget(DEFERRED_RENDER);
    defPar.scene = &scene;
    defPar.inputs = &m_inputs[DEFERRED_RENDER];
    defPar.cameraFrustum = plane;
    m_subrenderers[DEFERRED_RENDER]->Render(device, &commandContext, defPar);

    commandContext = device.GetCommandContext();
    commandList = commandContext.m_commandList;
    rootSignature = m_subrenderers[PBR_RENDER]->GetShader()->GetShaderInput();

    // PBR RENDER
    for (int i = 0; i < 3; i++)
    {
        auto texture = scene.GetRenderTarget(DEFERRED_RENDER)->GetTexture(frameIndex, i);
        m_inputs[PBR_RENDER][i] = std::pair<ShaderInput*, ShaderInputDesc>(texture.get(), m_mainInputs->GetInput("GBuffer" + std::to_string(i + 1)));
    }

    auto depthTex = scene.GetDepthStencil()->GetTexture();
    auto upscaledTex = scene.GetRenderTarget(UPSCALING_RENDER)->GetTexture(frameIndex, 0);
    m_inputs[PBR_RENDER][3] =
        std::pair<ShaderInput*, ShaderInputBindDesc>(upscaledTex.get(), rootSignature->GetInput("light_shaft_res"));
    m_inputs[PBR_RENDER][4] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(POINT_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("point_lights")));
    m_inputs[PBR_RENDER][5] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetUniformBuffer(LIGHT_INFO_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("light_info")));
    m_inputs[PBR_RENDER][6] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetStorageBuffer(DIR_LIGHT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("dir_lights")));
    m_inputs[PBR_RENDER][7] = std::pair<ShaderInput*, ShaderInputBindDesc>(
        scene.GetUniformBuffer(CAMERA_MAT_BUFFER), ShaderInputBindDesc(rootSignature->GetInput("camera_matrix")));
    m_inputs[PBR_RENDER][8] = std::pair<ShaderInput*, ShaderInputBindDesc>(depthTex, ShaderInputBindDesc(rootSignature->GetInput("Depth")));

    commandList->BindRootSignature(reinterpret_cast<ID3D12RootSignature*>(rootSignature->GetSignature()), true);

    RenderParameters pbrPar{};
    pbrPar.clearRt = false;
    pbrPar.ds = scene.GetDepthStencil();
    pbrPar.rt = scene.GetRenderTarget(PBR_RENDER);
    pbrPar.scene = &scene;
    pbrPar.inputs = &m_inputs[PBR_RENDER];

    m_subrenderers[PBR_RENDER]->Render(device, &commandContext, pbrPar);

    if (!raytraced)
    {
        auto boundRT = scene.GetRenderTarget(PBR_RENDER);
        scene.GetFinalRT()->CopyTo(*commandList, frameIndex, boundRT, 0, 0);
    }

    commandContext.Close();
}

void KS::Renderer::GenerateMipmaps(Device& device, Scene& scene)
{
    auto commandContext = device.GetCommandContext();
    auto commandList = commandContext.m_commandList;
    auto texCount = scene.GetTexWithoutMipmapCount();
    uint32_t mipCount = 0;
    for (int i = 0; i < texCount; i++)
    {
        auto texture = scene.GetTextureForMipmapGen(i);
        if (!texture) continue;

        uint32_t j = 0;
        auto mipLevel = texture->GetMipLevel();
        LOG(Log::Severity::INFO, "Creating mip chain for texture {} / {}", i, texCount);
        GenerateMipsInfo mipDesc{};

        while (true)
        {
            mipDesc = texture->GetMipmapInfo(j);
            if (mipDesc.NumMipLevels == 0) 
                break;
            scene.GetUniformBuffer(MIP_GEN_INFO)->Update(device, mipDesc, mipCount);

            m_inputs[MIP_GEN][0] = std::pair<ShaderInput*, ShaderInputBindDesc>(
                scene.GetUniformBuffer(MIP_GEN_INFO),
                ShaderInputBindDesc(mipCount, m_mipMapShaderInputs->GetInput("mipmap_info")));

            if (j + 1 < mipLevel)
                m_inputs[MIP_GEN][1] = std::pair<ShaderInput*, ShaderInputBindDesc>(
                texture, ShaderInputBindDesc(j + 1, m_mipMapShaderInputs->GetInput("mip_1")));
            if (j + 2 < mipLevel)
                m_inputs[MIP_GEN][2] = std::pair<ShaderInput*, ShaderInputBindDesc>(
                texture, ShaderInputBindDesc(j + 2, m_mipMapShaderInputs->GetInput("mip_2")));
            if (j + 3 < mipLevel)
                m_inputs[MIP_GEN][3] = std::pair<ShaderInput*, ShaderInputBindDesc>(
                texture, ShaderInputBindDesc(j + 3, m_mipMapShaderInputs->GetInput("mip_3")));

            m_inputs[MIP_GEN][4] = std::pair<ShaderInput*, ShaderInputBindDesc>(
                texture, ShaderInputBindDesc(m_mipMapShaderInputs->GetInput("mip_0")));

            reinterpret_cast<ComputeRenderer*>(m_subrenderers[MIP_GEN].get())
                ->SetDispatchSize(static_cast<uint32_t>(1.f / mipDesc.TexelSize.x),
                                  static_cast<uint32_t>(1.f / mipDesc.TexelSize.y));

            RenderParameters defPar{};
            defPar.clearRt = false;
            defPar.scene = &scene;
            defPar.inputs = &m_inputs[MIP_GEN];

            m_subrenderers[MIP_GEN]->Render(device, &commandContext, defPar);
            auto resource = reinterpret_cast<DXResource*>(texture->GetResource());
            commandList->ResourceBarrier(*resource, D3D12_RESOURCE_BARRIER_TYPE_UAV);
            LOG(Log::Severity::INFO, "Creating mips for texture {}-{}/ {}", mipDesc.SrcMipLevel, j + mipDesc.NumMipLevels, mipLevel-1);
            if (j + mipDesc.NumMipLevels == mipLevel - 1) break;
            j += mipDesc.NumMipLevels;
            mipCount++;
        }
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

    auto rtTexture = scene.GetRenderTarget(RT_RENDER)->GetTexture(frameIndex, 0).get();
    auto cubemapTexture = scene.GetSkydome().first.get();
    auto gbudfferA = scene.GetRenderTarget(DEFERRED_RENDER)->GetTexture(frameIndex, 0);
    auto gbudfferB = scene.GetRenderTarget(DEFERRED_RENDER)->GetTexture(frameIndex, 1);
    auto pbrTex = scene.GetRenderTarget(PBR_RENDER)->GetTexture(frameIndex, 0);
    auto depthTex = scene.GetDepthStencil()->GetTexture();

    m_inputs[RT_RENDER][0] = std::pair<ShaderInput*, ShaderInputDesc>(reinterpret_cast<ShaderInput*>(rtTexture),
                                                                      rootSignature->GetInput("output_tex"));
    m_inputs[RT_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(reinterpret_cast<ShaderInput*>(scene.GetBVH()),
                                                                      rootSignature->GetInput("BVH"));
    m_inputs[RT_RENDER][2] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(CAMERA_MAT_BUFFER),
                                                                      rootSignature->GetInput("camera_buffer"));
    m_inputs[RT_RENDER][3] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(INSTANCE_DATA_BUFFER),
                                                                      rootSignature->GetInput("instance_data"));
    m_inputs[RT_RENDER][4] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(DIR_LIGHT_BUFFER),
                                                                      rootSignature->GetInput("dir_lights"));
    m_inputs[RT_RENDER][5] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(POINT_LIGHT_BUFFER),
                                                                      rootSignature->GetInput("point_lights"));
    m_inputs[RT_RENDER][6] = std::pair<ShaderInput*, ShaderInputDesc>(cubemapTexture,
                                                                      rootSignature->GetInput("cubemap_tex"));
    m_inputs[RT_RENDER][7] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(LIGHT_INFO_BUFFER),
                                                                      rootSignature->GetInput("light_info"));
    m_inputs[RT_RENDER][8] = std::pair<ShaderInput*, ShaderInputDesc>(gbudfferA.get(), rootSignature->GetInput("GBufferA"));
    m_inputs[RT_RENDER][9] = std::pair<ShaderInput*, ShaderInputDesc>(gbudfferB.get(), rootSignature->GetInput("GBufferB"));
    m_inputs[RT_RENDER][10] = std::pair<ShaderInput*, ShaderInputDesc>(pbrTex.get(), rootSignature->GetInput("directLighting"));
    m_inputs[RT_RENDER][11] = std::pair<ShaderInput*, ShaderInputDesc>(depthTex, rootSignature->GetInput("Depth"));
    m_inputs[RT_RENDER][12] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(MATERIAL_BUFFER),
                                                                       rootSignature->GetInput("material_info"));

    RenderParameters defPar{};
    defPar.clearRt = true;
    defPar.scene = &scene;
    defPar.rt = scene.GetRenderTarget(RT_RENDER);
    defPar.inputs = &m_inputs[RT_RENDER];

    m_subrenderers[RT_RENDER]->Render(device, &commandContext, defPar);

    auto boundRT = scene.GetRenderTarget(RT_RENDER);
    scene.GetFinalRT()->CopyTo(*commandList, frameIndex, boundRT, 0, 0);

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
        reinterpret_cast<ShaderInput*>(scene.GetTexture(device, *commandList, skydome.second).get()),
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
    m_inputs[CUBEMAP_RENDER][1] = std::pair<ShaderInput*, ShaderInputDesc>(reinterpret_cast<ShaderInput*>(skydome.first.get()),
                                                                           rootSignature->GetInput("cubemap_tex"));

    RenderParameters par{};
    par.clearRt = false;
    par.ds = scene.GetDepthStencil();
    par.rt = scene.GetRenderTarget(PBR_RENDER);
    par.scene = &scene;
    par.inputs = &m_inputs[CUBEMAP_RENDER];

    m_subrenderers[CUBEMAP_RENDER]->Render(device, &commandContext, par);

    commandContext.Close();
}

void KS::Renderer::Culling(Device& device, Scene& scene)
{
     auto commandContext = device.GetCommandContext();
     auto& commandList = commandContext.m_commandList;
     auto rootSignature = m_subrenderers[MESH_CULLING]->GetShader()->GetShaderInput();
     auto heap = scene.GetResourceHeap();

     auto indicesStorageBuffer = scene.GetStorageBuffer(DRAW_INDICES);
     auto counterResource = reinterpret_cast<DXResource*>(indicesStorageBuffer->GetRawCounterResource());
     auto counterRBResource = reinterpret_cast<DXResource*>(indicesStorageBuffer->GetRawRBCounterResource());
     auto resource = reinterpret_cast<DXResource*>(indicesStorageBuffer->GetRawResource());
     auto resourceRB = reinterpret_cast<DXResource*>(indicesStorageBuffer->GetRawRBResource());

     m_inputs[MESH_CULLING][0] = std::pair<ShaderInput*, ShaderInputDesc>(indicesStorageBuffer,
                                                                       rootSignature->GetInput("draw_indices"));
     m_inputs[MESH_CULLING][1] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetUniformBuffer(CULLING_INFO),
                                                                       rootSignature->GetInput("culling_info"));
     m_inputs[MESH_CULLING][2] = std::pair<ShaderInput*, ShaderInputDesc>(scene.GetStorageBuffer(BOUNDING_BOX_BUFFER),
                                                                       rootSignature->GetInput("bounding_boxes"));
     RenderParameters par{};
     par.scene = &scene;
     par.inputs = &m_inputs[MESH_CULLING];

     UINT clear[4] = {0, 0, 0, 0};
     auto counterHandle = indicesStorageBuffer->GetCounterHandle(false);

     auto counterUavGpuHandle = heap->Get()->GetGPUDescriptorHandleForHeapStart();
     counterUavGpuHandle.ptr += counterHandle * heap->GetDescriptorSize();

     auto counterUavCpuHandle = heap->Get()->GetCPUDescriptorHandleForHeapStart();
     counterUavCpuHandle.ptr += counterHandle * heap->GetDescriptorSize();

     indicesStorageBuffer->ClearCounterBuffer(device, *commandList);

     auto& drawIndices = scene.GetDrawIndices();
     memset(drawIndices.data(), 0, MAX_MESHES * sizeof(uint32_t));
     indicesStorageBuffer->Update(device, *commandList, heap, drawIndices);

     reinterpret_cast<ComputeRenderer*>(m_subrenderers[MESH_CULLING].get())
         ->SetDispatchSize(static_cast<uint32_t>(std::ceil(scene.GetDrawQueueSize())), 1, 1);

     m_subrenderers[MESH_CULLING]->Render(device, &commandContext, par);
    
     commandList->ResourceBarrier(*resource, D3D12_RESOURCE_BARRIER_TYPE_UAV);
     commandList->ResourceBarrier(*counterResource, D3D12_RESOURCE_BARRIER_TYPE_UAV);

     commandList->TransitionResource(*resource, D3D12_RESOURCE_STATE_COPY_SOURCE);
     commandList->TransitionResource(*resourceRB, D3D12_RESOURCE_STATE_COPY_DEST);
     commandList->CopyResource(*resource, *resourceRB);

     commandList->TransitionResource(*counterResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
     commandList->TransitionResource(*counterRBResource, D3D12_RESOURCE_STATE_COPY_DEST);
     commandList->CopyResource(*counterResource, *counterRBResource);
     commandContext.Close();

     device.FlushAndWait();
     commandContext = device.GetCommandContext();
     commandList = commandContext.m_commandList;

     uint32_t* countPtr = nullptr;
     counterRBResource->Get()->Map(0, nullptr, (void**)&countPtr);
     uint32_t count = *countPtr;
     counterRBResource->Get()->Unmap(0, nullptr);

    // Read data
     void* mapped = nullptr;
     resourceRB->Get()->Map(0, nullptr, &mapped);
     uint32_t* src = static_cast<uint32_t*>(mapped);

     memcpy(drawIndices.data(), src, size_t(count) * sizeof(uint32_t));

     resourceRB->Get()->Unmap(0, nullptr);

     scene.SetDrawIndicesCount(count);
     scene.CreateBatches(device, *commandList);
     commandContext.Close();
}
