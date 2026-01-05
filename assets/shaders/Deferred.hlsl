#include "Structs.hlsl"

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normals : NORMALS;
    float2 uv : TEXCOORD;
    float3 tangents : TANGENT;
    uint iid : SV_InstanceID;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 vertexPos : VERT_POS;
    float4 normals : NORMALS;
    float2 uv : TEXCOORD;
    float3x3 tangentBasis : TANGENT_BASIS;
    uint iid : SV_InstanceID;
};

struct PSOutput
{
    float4 albedo : SV_Target0;
    float4 normals : SV_Target1;
    float4 emissive : SV_Target2;
};

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

cbuffer ModelIndex : register(b1)
{
    int meshIndex;
};

SamplerState mainSampler : register(s0);

StructuredBuffer<InstanceData> instanceData : register(t2);
StructuredBuffer<MaterialInfo> materialData : register(t3);
Texture2D<float4> textures[65536] : register(t0, space1);

PBRMaterial GenerateMaterial(PS_INPUT input, uint iid);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.vertexPos = mul(instanceData[meshIndex+ input.iid].modelMatrix.mModelMat, float4(input.pos, 1.f));
    output.pos = mul(cameraMats.mCamera, output.vertexPos);
    output.normals = float4(normalize(mul(input.normals.xyz, (float3x3) instanceData[meshIndex + input.iid].modelMatrix.mInvTransposeMat)), 0.f);
    output.uv = input.uv;

    input.tangents = normalize(input.tangents);
    input.tangents = normalize(input.tangents - dot(input.tangents, input.normals) * input.normals);
    float3 bitangent = cross(output.normals.xyz, input.tangents);
    output.iid = input.iid;

    float3x3 TBN = float3x3(input.tangents, bitangent, output.normals.xyz);
    output.tangentBasis = TBN;

    return output;
}

PSOutput mainPS(PS_INPUT input)
    : SV_TARGET
{
    PBRMaterial material = GenerateMaterial(input, input.iid);

    float metallic, roughness;
    PSOutput output;
    output.albedo = float4(material.baseColor.rgb, material.metallic);
    output.normals = float4(material.normalColor, material.roughness);
    output.emissive = float4(material.emissiveColor, material.occlusionColor);

    return output;
}

PBRMaterial GenerateMaterial(PS_INPUT input, uint iid)
{
    PBRMaterial mat;
    const MaterialInfo matInfo = materialData[instanceData[meshIndex + iid].materialIndex];
    Texture2D baseColorTex = textures[matInfo.colorTexIndex];
    Texture2D emissiveTex = textures[matInfo.emissiveTexIndex];
    Texture2D metallicRoughnessTex = textures[matInfo.metallicRoughnessTexIndex];
    Texture2D occlusionTex = textures[matInfo.occlusionTexIndex];
    Texture2D normalTex = textures[matInfo.normalTexIndex];

    mat.baseColor = baseColorTex.Sample(mainSampler, input.uv).rgb;
    mat.baseColor *= matInfo.colorFactor.rgb;

    mat.emissiveColor = emissiveTex.Sample(mainSampler, input.uv).rgb;
    mat.emissiveColor *= matInfo.emissiveFactor.rgb;

    float3 metallicRoughnessColor = metallicRoughnessTex.Sample(mainSampler, input.uv).rgb;
    mat.roughness = metallicRoughnessColor.g * matInfo.metallicFactor;
    mat.metallic = metallicRoughnessColor.b * matInfo.roughnessFactor;

    // Occlusion if it is not in matallic roughness texture
    mat.occlusionColor = occlusionTex.Sample(mainSampler, input.uv).r;

    mat.normalColor = normalTex.Sample(mainSampler, input.uv).rgb;
    mat.normalColor = mat.normalColor * 2.0 - 1.0;
    mat.normalColor = mul(mat.normalColor, input.tangentBasis);
    mat.normalColor = (mat.normalColor + 1) * 0.5f;

    mat.F0 = float3(0.04, 0.04, 0.04);
    mat.F0 = lerp(mat.F0, mat.baseColor, mat.metallic);
    mat.diffuse = lerp(mat.baseColor, float3(0.0, 0.0, 0.0), mat.metallic);

    // To alpha roughness
    mat.roughness = mat.roughness * mat.roughness;

    return mat;
}
