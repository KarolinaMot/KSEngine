#include "Structs.hlsli"
#include "PBR.hlsli"

ConstantBuffer<CameraMats> camera : register(b0);
ConstantBuffer<ModelMat> model_matrix : register(b1);

Texture2D base_texture : register(t0);
Texture2D normal_texture : register(t1);
Texture2D occlusion_texture : register(t2);
Texture2D metallic_texture : register(t3);
Texture2D emissive_texture : register(t4);

SamplerState main_sampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
};

struct PS_INPUT
{
    float4 screen_position : SV_Position;
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3x3 tangent_basis : TANGENT_BASIS;
};

struct PSOutput
{
    float4 colour : SV_Target0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    output.position = mul(model_matrix.mModelMat, float4(input.position, 1.f));
    output.screen_position = mul(camera.mCamera, output.position);
    output.uv = input.uv;

    float3x3 normal_transform = (float3x3)model_matrix.mInvTransposeMat;

    float3 normal = normalize(mul(normal_transform, input.normal));
    float3 bitangent = normalize(mul(normal_transform, input.tangent));
    float3 tangent = normalize(mul(normal_transform, input.bitangent));

    output.tangent_basis = float3x3(tangent, bitangent, normal);

    return output;
}

PSOutput mainPS(PS_INPUT input)
{
    //TODO: NO GAMMA CORRECTION YET

    PBRMaterial material_data;
    material_data.baseColor = base_texture.Sample(main_sampler, input.uv).rgb;
    material_data.emissiveColor = emissive_texture.Sample(main_sampler, input.uv).rgb; 
    material_data.occlusionColor = occlusion_texture.Sample(main_sampler, input.uv).rgb;

    float4 metallic_roughness = metallic_texture.Sample(main_sampler, input.uv).rgba;
    material_data.metallic = metallic_roughness.b;
    material_data.roughness = metallic_roughness.g;

    float3 normal_map = normal_texture.Sample(main_sampler, input.uv).rgb;
    material_data.normalColor = mul(input.tangent_basis, normal_map * 2.0 - 1.0);

    material_data.F0 = lerp(float3(0.04, 0.04, 0.04), material_data.baseColor, material_data.metallic);
    material_data.diffuse = lerp(material_data.baseColor, float3(0.0, 0.0, 0.0), material_data.metallic);

    material_data.roughness = material_data.roughness * material_data.roughness;

    float3 diffuse = 0.f;
    float3 specular = 0.f;

    float3 cameraDirection = mul(camera.mView, float4(0.0f, 0.0f, 1.0f, 0.0f));
    float3 viewDirection = normalize(camera.mCameraPos.xyz - input.position.xyz);
    float3 lightDirection = normalize(-float3(0.0f, 0.0f, 1.0f));
    
    GetBRDF(material_data, viewDirection, -cameraDirection, float3(1.0f, 1.0f, 1.0f), 3.0f, 1.0f, diffuse, specular);
    float3 result = (diffuse + specular) * material_data.occlusionColor + material_data.emissiveColor;
    
    PSOutput output;
    output.colour = float4(result, 1.0f);
    return output;
}
