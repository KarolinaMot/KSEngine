#include "Common.hlsl"
#include "Structs.hlsl"

StructuredBuffer<MaterialInfo> matInfos : register(t1);
StructuredBuffer<float3> normals : register(t2);
StructuredBuffer<ModelMat> modelMats : register(t3);
StructuredBuffer<uint> indices : register(t4);
StructuredBuffer<float3> vertexPositions : register(t5);
StructuredBuffer<float2> uvs : register(t6);
StructuredBuffer<float3> tangents : register(t7);

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

    payload.colorAndDistance = float4(NormalToColor(normal), 1.f);
    //matInfos[InstanceID()].colorFactor;
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
