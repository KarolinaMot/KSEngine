#include "Common.hlsl"
#include "Structs.hlsl"

StructuredBuffer<MaterialInfo> matInfos : register(t1);
StructuredBuffer<float3> normals : register(t2);
StructuredBuffer<ModelMat> modelMats : register(t3);
StructuredBuffer<uint> indices : register(t4);

float3 NormalToColor(float3 normal);
float3 GetNormal(int instance, int vertId, float3 barycentrics);
int GetIndex(int vertId, int instance, int offset);
float3 GetNormalInVector(int instance, int index);

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    uint vertId = 3 * PrimitiveIndex();
    uint instance = InstanceID();

    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    float3 normal = GetNormal(instance, vertId, barycentrics);
    payload.colorAndDistance = float4(NormalToColor(normal), 1.f);
    //matInfos[InstanceID()].colorFactor;
}

float3 NormalToColor(float3 normal)
{
    return (normal + 1.f) * 0.5f;
}

float3 GetNormal(int instance, int vertId, float3 barycentrics)
{
    const float3 A = GetNormalInVector(instance, GetIndex(vertId, instance, 0)).xyz;
    const float3 B = GetNormalInVector(instance, GetIndex(vertId, instance, 1)).xyz;
    const float3 C = GetNormalInVector(instance, GetIndex(vertId, instance, 2)).xyz;
    float3 normal = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
    return normal;
}

float3 GetNormalInVector(int instance, int index)
{
    int id = matInfos[instance].normalsOffset + index;
    float3 normal = normals[id].xyz;
    normal = normalize(mul(normal, (float3x3)modelMats[instance].mInvTransposeMat));
    return normal;
}

int GetIndex(int vertId, int instance, int offset)
{
    int offs = matInfos[instance].indexOffset + vertId + offset;
    return indices[offs];
}
