#include "Structs.hlsl"
#include "PBR.hlsl"

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
    float linearDepth : LINEAR_DEPTH;
    uint iid : SV_InstanceID;
};

struct PSOutput
{
    uint4 bufferA : SV_Target0;
    float bufferB : SV_Target1;
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

StructuredBuffer<InstanceData> instanceData : register(t1);
StructuredBuffer<MaterialInfo> materials : register(t0);
Texture2D<float4> textures[65536] : register(t0, space1);

PBRMaterial GenerateMaterial(PS_INPUT input, uint iid);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.vertexPos = mul(instanceData[meshIndex+ input.iid].modelMatrix.mModelMat, float4(input.pos, 1.f));
    output.pos = mul(cameraMats.mCamera, output.vertexPos);
    output.normals = float4(normalize(mul(input.normals.xyz, (float3x3) instanceData[meshIndex + input.iid].modelMatrix.mInvTransposeMat)), 0.f);
    output.uv = input.uv;
    output.uv.y *= -1;

    input.tangents = normalize(mul(input.tangents, (float3x3) instanceData[meshIndex + input.iid].modelMatrix.mModelMat)); // or use invTranspose if non-uniform scale issues
    input.tangents = normalize(input.tangents - dot(input.tangents, output.normals.xyz) * output.normals.xyz);
    
    float3 bitangent = normalize(cross(output.normals.xyz, input.tangents));
    output.iid = input.iid;

    float3x3 TBN = float3x3(input.tangents, bitangent, output.normals.xyz);
    output.tangentBasis = TBN;
   
    output.linearDepth = -output.pos.z; // D3D convention: camera looks down -Z

    return output;
}

PSOutput mainPS(PS_INPUT input)
    : SV_TARGET
{
    PBRMaterial material = GenerateMaterial(input, input.iid);

    float metallic, roughness;
    PSOutput output;
    
    output.bufferA.x = PackAlbedoMetal(material.baseColor.rgb, material.metallic);
    output.bufferA.y = PackNormalOct(material.normalColor);
    output.bufferA.z = PackRGB9E5(material.emissiveColor);
    output.bufferA.w = PackRoughOcc(material.roughness, material.occlusionColor, material.baseColor.a);
    output.bufferB = input.linearDepth;

    return output;
}


PBRMaterial GenerateMaterial(PS_INPUT input, uint iid)
{
    PBRMaterial mat;
    uint materialIndex = instanceData[meshIndex + iid].materialIndex;
    const MaterialInfo matInfo = materials[materialIndex];
    Texture2D baseColorTex = textures[matInfo.colorTexIndex];
    Texture2D emissiveTex = textures[matInfo.emissiveTexIndex];
    Texture2D metallicRoughnessTex = textures[matInfo.metallicRoughnessTexIndex];
    Texture2D occlusionTex = textures[matInfo.occlusionTexIndex];
    Texture2D normalTex = textures[matInfo.normalTexIndex];

    mat.baseColor = baseColorTex.Sample(mainSampler, input.uv);
    mat.baseColor.rgb *= matInfo.colorFactor.rgb;

    mat.emissiveColor = emissiveTex.Sample(mainSampler, input.uv).rgb;
    mat.emissiveColor *= matInfo.emissiveFactor.rgb;

    float3 metallicRoughnessColor = metallicRoughnessTex.Sample(mainSampler, input.uv).rgb;
    mat.roughness = metallicRoughnessColor.g * matInfo.metallicFactor;
    mat.metallic = metallicRoughnessColor.b * matInfo.roughnessFactor;

    // Occlusion if it is not in matallic roughness texture
    mat.occlusionColor = occlusionTex.Sample(mainSampler, input.uv).r;

    mat.normalColor = input.normals;

    mat.F0 = float3(0.04, 0.04, 0.04);
    mat.F0 = lerp(mat.F0, mat.baseColor.rgb, mat.metallic);
    mat.diffuse = lerp(mat.baseColor.rgb, float3(0.0, 0.0, 0.0), mat.metallic) * mat.baseColor.a;

    // To alpha roughness
    mat.roughness = mat.roughness * mat.roughness;

    return mat;
}
