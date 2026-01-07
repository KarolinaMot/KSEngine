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
uint PackAlbedoMetal(float3 albedo, float metallic);
uint PackNormalOct(float3 n);
uint PackRGB9E5(float3 c);
uint PackRoughOcc(float roughness, float occlusion);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.vertexPos = mul(instanceData[meshIndex+ input.iid].modelMatrix.mModelMat, float4(input.pos, 1.f));
    output.pos = mul(cameraMats.mCamera, output.vertexPos);
    output.normals = float4(normalize(mul(input.normals.xyz, (float3x3) instanceData[meshIndex + input.iid].modelMatrix.mInvTransposeMat)), 0.f);
    output.uv = input.uv;
    output.uv.y *= -1;

    input.tangents = normalize(input.tangents);
    input.tangents = normalize(input.tangents - dot(input.tangents, input.normals) * input.normals);
    float3 bitangent = cross(output.normals.xyz, input.tangents);
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
    output.bufferA.w = PackRoughOcc(material.roughness, material.occlusionColor);
    output.bufferB = input.linearDepth;

    //output.albedo = float4(material.baseColor.rgb, material.metallic);
    //output.normals = float4(material.normalColor, material.roughness);
    //output.emissive = float4(material.emissiveColor, material.occlusionColor);
    //output.linearDepth = input.linearDepth;
    return output;
}

uint PackUNorm8(float v)
{
    return (uint) round(saturate(v) * 255.0);
}

uint PackRGB8(float3 c)
{
    uint r = PackUNorm8(c.x);
    uint g = PackUNorm8(c.y);
    uint b = PackUNorm8(c.z);
    return (r) | (g << 8) | (b << 16);
}

// Albedo RGB888 + Metallic A8  => 0xMMBBGGRR
uint PackAlbedoMetal(float3 albedo, float metallic)
{
    return PackRGB8(albedo) | (PackUNorm8(metallic) << 24);
}

// Octahedral normal encoding [-1,1] -> pack snorm16x2
float2 OctEncode(float3 n)
{
    n = normalize(n);
    n /= (abs(n.x) + abs(n.y) + abs(n.z) + 1e-8);

    float2 e = n.xy;
    if (n.z < 0.0)
        e = (1.0 - abs(e.yx)) * (float2(e.x >= 0 ? 1 : -1, e.y >= 0 ? 1 : -1));
    return e;
}

uint PackSnorm16x2(float2 v)
{
    // SNORM16: [-1,1] mapped to [-32767,32767]
    v = clamp(v, -1.0, 1.0);
    int2 s = (int2) round(v * 32767.0);

    uint lo = (uint) (s.x & 0xFFFF);
    uint hi = (uint) (s.y & 0xFFFF) << 16;
    return lo | hi;
}

uint PackNormalOct(float3 n)
{
    float2 e = OctEncode(n); // in [-1, 1]
    return PackSnorm16x2(e); // returns uint (DXC / SM6)
}

// RGB9E5 (shared exponent) 32-bit packing
uint PackRGB9E5(float3 c)
{
    c = max(c, 0.0.xxx);
    float maxc = max(c.x, max(c.y, c.z));
    if (maxc < 1.5258789e-5)
        return 0; // ~2^-16

    // shared exponent in base-2, biased by 15 (5 bits)
    int expShared = (int) floor(log2(maxc)) + 1;
    expShared = clamp(expShared, -16, 15);

    // mantissa is 9 bits => scale so that mantissa = round(c / 2^(expShared-9))
    float denom = exp2((float) expShared - 9.0);

    uint r = (uint) min(511.0, round(c.x / denom));
    uint g = (uint) min(511.0, round(c.y / denom));
    uint b = (uint) min(511.0, round(c.z / denom));
    uint e = (uint) (expShared + 15);

    // bits: [31:27]=E, [26:18]=B, [17:9]=G, [8:0]=R
    return (e << 27) | (b << 18) | (g << 9) | (r);
}

uint PackRoughOcc(float roughness, float occlusion)
{
    uint r16 = (uint) round(saturate(roughness) * 65535.0); // 16-bit
    uint o8 = PackUNorm8(occlusion); // 8-bit
    uint spare8 = 0;

    return (r16) | (o8 << 16) | (spare8 << 24);
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
