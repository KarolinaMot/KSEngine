#include "Common.hlsl"
#include "Structs.hlsl"
#include "PBR.hlsl"

StructuredBuffer<MaterialInfo> matInfos : register(t1);
StructuredBuffer<float3> normals : register(t2);
StructuredBuffer<ModelMat> modelMats : register(t3);
StructuredBuffer<uint> indices : register(t4);
StructuredBuffer<float3> vertexPositions : register(t5);
StructuredBuffer<float2> uvs : register(t6);
StructuredBuffer<float3> tangents : register(t7);
StructuredBuffer<DirLight> dirLights : register(t8);
StructuredBuffer<PointLight> pointLights : register(t9);

Texture2D<float4> textures[] : register(t10);
SamplerState mainSampler : register(s0);

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

cbuffer LightInfoBuffer : register(b1)
{
    LightInfo lightInfo;
};

float3 NormalToColor(float3 normal);
int GetIndex(int vertId, int instance, int offset);

float3 GetNormal(int instance, int vertId, float3 barycentrics);
float3 GetPosition(int instance, int vertId, float3 barycentrics);
float2 GetUV(int instance, int vertId, float3 barycentrics);
float3 GetTangent(int instance, int vertId, float3 barycentrics);
float3x3 GetTangentBasis(float3 normal, float3 tangent, float4x4 modelMat);

float3 GetNormalInVector(int instance, int index);
float3 GetPositionInVector(int instance, int index);
float2 GetUVInVector(int instance, int index);
float3 GetTangentInVector(int instance, int index);

PBRMaterial GenerateMaterial(MaterialInfo info, float2 uv, float3 normals, float3x3 tangentBasis);

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    uint vertId = 3 * PrimitiveIndex();
    uint instance = InstanceID();

    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    float4 vertexPos = float4(GetPosition(instance, vertId, barycentrics), 1.f);
    float3 normal = GetNormal(instance, vertId, barycentrics);
    float2 uv = GetUV(instance, vertId, barycentrics);
    float3 tangent = GetTangent(instance, vertId, barycentrics);
    tangent = normalize(tangent);
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    float3 bitangent = cross(normal.xyz, tangent);

    float3x3 TBN = float3x3(tangent, bitangent, normal);

    PBRMaterial material = GenerateMaterial(matInfos[instance], uv, normal, TBN);
    
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 viewDirection = normalize(cameraMats.mCameraPos.xyz - worldOrigin);
    float3 diffuse = 0.f;
    float3 specular = 0.f;
    float3 result;

    for (uint i = 0; i < lightInfo.numDirLight; i++)
    {
        DirLight light = dirLights[i];
        GetBRDF(material, viewDirection, light.mDir.xyz * float3(1, 1, -1), light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, 1.f, diffuse, specular);
    }
    
    for (uint j = 0; j < lightInfo.numPointLight; j++)
    {
        PointLight light = pointLights[j];

        float3 lightDirection = light.mPosition.xyz - vertexPos.xyz;
        float dist = length(lightDirection);
        lightDirection /= dist;
        float att = Attenuation(dist, light.mRadius);

        GetBRDF(material, viewDirection, lightDirection, light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, att, diffuse, specular);
    }
    
    GetBRDF(material, viewDirection, viewDirection, lightInfo.ambientLightIntensity.rgb, lightInfo.ambientLightIntensity.a, 1.f, diffuse, specular);

    result = (diffuse + specular) * material.occlusionColor + material.emissiveColor;
    result = LinearToSRGB(result);

    payload.colorAndDistance = float4(result, 1.f);
}

float3 NormalToColor(float3 normal)
{
    return (normal + 1.f) * 0.5f;
}

float2 GetUV(int instance, int vertId, float3 barycentrics)
{
    const float2 A = GetUVInVector(instance, GetIndex(vertId, instance, 0));
    const float2 B = GetUVInVector(instance, GetIndex(vertId, instance, 1));
    const float2 C = GetUVInVector(instance, GetIndex(vertId, instance, 2));
    float2 uv = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
    return uv;
}

float3 GetPosition(int instance, int vertId, float3 barycentrics)
{
    const float3 A = GetPositionInVector(instance, GetIndex(vertId, instance, 0)).xyz;
    const float3 B = GetPositionInVector(instance, GetIndex(vertId, instance, 1)).xyz;
    const float3 C = GetPositionInVector(instance, GetIndex(vertId, instance, 2)).xyz;
    float3 pos = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
    return pos;
}

float3 GetNormal(int instance, int vertId, float3 barycentrics)
{
    const float3 A = GetNormalInVector(instance, GetIndex(vertId, instance, 0)).xyz;
    const float3 B = GetNormalInVector(instance, GetIndex(vertId, instance, 1)).xyz;
    const float3 C = GetNormalInVector(instance, GetIndex(vertId, instance, 2)).xyz;
    float3 normal = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
    return normal;
}

float3 GetTangent(int instance, int vertId, float3 barycentrics)
{
    const float3 A = GetTangentInVector(instance, GetIndex(vertId, instance, 0)).xyz;
    const float3 B = GetTangentInVector(instance, GetIndex(vertId, instance, 1)).xyz;
    const float3 C = GetTangentInVector(instance, GetIndex(vertId, instance, 2)).xyz;
    float3 tangent = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
    return tangent;
}

float3 GetNormalInVector(int instance, int index)
{
    int id = matInfos[instance].normalsOffset + index;
    float3 normal = normals[id].xyz;
    normal = normalize(mul(normal, (float3x3)modelMats[instance].mInvTransposeMat));
    return normal;
}

float3 GetTangentInVector(int instance, int index)
{
    int id = matInfos[instance].normalsOffset + index;
    float3 tangent = tangents[id].xyz;
    return tangent;
}

float3 GetPositionInVector(int instance, int index)
{
    int id = matInfos[instance].normalsOffset + index;
    float3 pos = vertexPositions[id].xyz;
    return pos;
}

float2 GetUVInVector(int instance, int index)
{
    int id = matInfos[instance].uvOffset + index;
    float2 uv = uvs[id];
    return uv;
}

int GetIndex(int vertId, int instance, int offset)
{
    int offs = matInfos[instance].indexOffset + vertId + offset;
    return indices[offs];
}

PBRMaterial GenerateMaterial(MaterialInfo info, float2 uv, float3 normals, float3x3 tangentBasis)
{
    PBRMaterial mat;
    mat.baseColor = pow(abs(textures[info.colorTexIndex].SampleLevel(mainSampler, uv, 0).rgb), sGamma);
    mat.baseColor *= info.colorFactor.rgb;

    mat.emissiveColor = pow(abs(textures[info.emissiveTexIndex].SampleLevel(mainSampler, uv, 0).rgb), sGamma);
    mat.emissiveColor *= info.emissiveFactor.rgb;

    float3 metallicRoughnessColor = textures[info.metallicRoughnessTexIndex].SampleLevel(mainSampler, uv, 0).rgb;
    mat.roughness = metallicRoughnessColor.g * info.metallicFactor;
    mat.metallic = metallicRoughnessColor.b * info.roughnessFactor;

    // Occlusion if it is not in matallic roughness texture
    mat.occlusionColor = textures[info.occlusionTexIndex].SampleLevel(mainSampler, uv, 0).r;
    
    mat.normalColor = textures[info.normalTexIndex].SampleLevel(mainSampler, uv, 0).rgb;
    mat.normalColor = mat.normalColor * 2.0 - 1.0;
    mat.normalColor = mul(mat.normalColor, tangentBasis);
   // mat.normalColor = (mat.normalColor + 1) * 0.5f;
    
   // mat.normalColor = normals;

    mat.F0 = float3(0.04, 0.04, 0.04);
    mat.F0 = lerp(mat.F0, mat.baseColor, mat.metallic);
    mat.diffuse = lerp(mat.baseColor, float3(0.0, 0.0, 0.0), mat.metallic);

    // To alpha roughness
    mat.roughness = mat.roughness * mat.roughness;

    return mat;
}