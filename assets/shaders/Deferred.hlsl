#include "Structs.hlsl"

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normals : NORMALS;
    float2 uv : TEXCOORD;
    float3 tangents : TANGENT;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 vertexPos : VERT_POS;
    float4 normals : NORMALS;
    float2 uv : TEXCOORD;
    float3x3 tangentBasis : TANGENT_BASIS;
};

struct PSOutput
{
    float4 vertexPos : SV_Target1;
    float4 albedo : SV_Target0;
    float4 normals : SV_Target2;
    float4 emissive : SV_Target2;
};

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

cbuffer ModelMatrix : register(b1)
{
    ModelMat modelMats;
};

cbuffer MaterialInfos : register(b2)
{
    MaterialInfo matInfo;
};

SamplerState mainSampler : register(s0);
Texture2D baseColorTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D emissiveTex : register(t2);
Texture2D metallicRoughnessTex : register(t3);
Texture2D occlusionTex : register(t4);
PBRMaterial GenerateMaterial(PS_INPUT input);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.vertexPos = mul(modelMats.mModelMat, float4(input.pos, 1.f));
    output.pos = mul(cameraMats.mCamera, output.vertexPos);
    output.normals = float4(normalize(mul(input.normals.xyz, (float3x3)modelMats.mInvTransposeMat)), 0.f);
    output.uv = input.uv;

    input.tangents = normalize(input.tangents);
    input.tangents = normalize(input.tangents - dot(input.tangents, input.normals) * input.normals);
    float3 bitangent = cross(output.normals.xyz, input.tangents);

    float3x3 TBN = float3x3(input.tangents, bitangent, output.normals.xyz);
    output.tangentBasis = TBN;

    return output;
}

PSOutput mainPS(PS_INPUT input)
    : SV_TARGET
{
    PBRMaterial material = GenerateMaterial(input);

    float metallic, roughness;
    PSOutput output;
    output.albedo = float4(material.baseColor.rgb, material.metallic);
    output.normals = float4(material.normalColor, 1.f);
    output.vertexPos = float4(input.vertexPos.xyz, material.roughness);
    output.emissive = float4(material.emissiveColor, material.occlusionColor);

    return output;
}

PBRMaterial GenerateMaterial(PS_INPUT input)
{
    PBRMaterial mat;
    if (matInfo.useColorTex)
    {
        mat.baseColor = pow(abs(baseColorTex.Sample(mainSampler, input.uv).rgb), sGamma);
        mat.baseColor *= matInfo.colorFactor.rgb;
    }
    else
    {
        mat.baseColor = float3(1.0, 1.0, 1.0);
        mat.baseColor *= matInfo.colorFactor.rgb;
    }

    if (matInfo.useEmissiveTex)
    {
        mat.emissiveColor = pow(abs(emissiveTex.Sample(mainSampler, input.uv).rgb), sGamma);
        mat.emissiveColor *= matInfo.emissiveFactor.rgb;
    }
    else
    {
        mat.emissiveColor = float3(0.0, 0.0, 0.0);
    }

    if (matInfo.useMetallicRoughnessTex)
    {
        float3 metallicRoughnessColor = metallicRoughnessTex.Sample(mainSampler, input.uv).rgb;
        mat.roughness = metallicRoughnessColor.g;
        mat.metallic = metallicRoughnessColor.b;
    }
    else
    {
        mat.occlusionColor = 1.0;
        mat.metallic = matInfo.metallicFactor;
        mat.roughness = matInfo.roughnessFactor;
    }

    // Occlusion if it is not in matallic roughness texture
    if (matInfo.useOcclusionTex)
    {
        mat.occlusionColor = occlusionTex.Sample(mainSampler, input.uv).r;
    }

    if (matInfo.useNormalTex)
    {
        mat.normalColor = normalTex.Sample(mainSampler, input.uv).rgb;
        mat.normalColor = mat.normalColor * 2.0 - 1.0;
        mat.normalColor = mul(mat.normalColor, input.tangentBasis);
        mat.normalColor = (mat.normalColor + 1) * 0.5f;
    }
    else
    {
        mat.normalColor = input.normals.xyz;
    }

    mat.F0 = float3(0.04, 0.04, 0.04);
    mat.F0 = lerp(mat.F0, mat.baseColor, mat.metallic);
    mat.diffuse = lerp(mat.baseColor, float3(0.0, 0.0, 0.0), mat.metallic);

    // To alpha roughness
    mat.roughness = mat.roughness * mat.roughness;

    return mat;
}
