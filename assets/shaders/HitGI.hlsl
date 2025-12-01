#include "RTCommon.hlsl"
#include "Structs.hlsl"
#include "PBR.hlsl"

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

StructuredBuffer<MaterialInfo> matInfos : register(t1);
StructuredBuffer<ModelMat> modelMats : register(t2);
StructuredBuffer<DirLight> dirLights : register(t3);
StructuredBuffer<PointLight> pointLights : register(t4);

StructuredBuffer<float3> normals[] : register(t0, space1);
StructuredBuffer<uint> indices[] : register(t0, space2);
StructuredBuffer<float3> vertexPositions[] : register(t0, space3);
StructuredBuffer<float2> uvs[] : register(t0, space4);
StructuredBuffer<float3> tangents[] : register(t0, space5);
Texture2D<float4> textures[] : register(t0, space6);
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

PBRMaterial GenerateMaterial(MaterialInfo info, float2 uv, float3 normals, float3x3 tangentBasis, float Lu, float Lv);

struct Vtx
{
    float3 P;
    float2 uv;
};

float ComputeTexLOD(Texture2D tex, float Lu, float Lv);

void Compute_dPdu_dPdv(Vtx v0, Vtx v1, Vtx v2,
                       out float3 dPdu, out float3 dPdv);
void ComputeUVFootprint(uint instance, uint baseIndex, float coneR, out float Lu, out float Lv);


[shader("closesthit")]
void GIClosestHit(inout HitInfo payload, Attributes attrib)
{
    uint vertId = 3 * PrimitiveIndex();
    uint instance = InstanceID();

    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    float4 vertexPos = float4(GetPosition(instance, vertId, barycentrics), 1.f);
    vertexPos = mul(modelMats[instance].mModelMat, float4(vertexPos.rgb, 1.f));
    float3 normal = GetNormal(instance, vertId, barycentrics);
    float2 uv = GetUV(instance, vertId, barycentrics);
    float3 tangent = GetTangent(instance, vertId, barycentrics);
    float3 tangentWS = normalize(mul((float3x3) modelMats[instance].mModelMat, tangent));
    tangentWS = normalize(tangentWS - dot(tangentWS, normal) * normal);
    float3 bitangentWS = normalize(cross(normal, tangentWS));
    float3x3 TBN = float3x3(tangentWS, bitangentWS, normal);
    
    float t = RayTCurrent();
    float coneRadiusWS = RayTCurrent() * tan(payload.coneAngle);
    float Lu, Lv;
    ComputeUVFootprint(instance, vertId, coneRadiusWS, Lu, Lv);
    
    
    PBRMaterial material = GenerateMaterial(matInfos[instance], uv, normal, TBN, Lu, Lv);
    
    float3 result = 0.f;
    float3 viewDirection = normalize(WorldRayDirection());

    for (uint i = 0; i < lightInfo.numDirLight; i++)
    {
        DirLight light = dirLights[i];
        float3 lightDir = normalize(light.mDir.xyz) * float3(1, 1, -1);
        float3 halfAngle = normalize(viewDirection + lightDir);
        float vDotH = clamp(dot(viewDirection, halfAngle), 0.0, 1.0);
        float nDotL = clamp(dot(material.normalColor, lightDir), 0.0, 1.0);
        result += LambertianDiffuse(material.diffuse, material.F0, float3(1.0, 1.0, 1.0), vDotH) * light.mColorAndIntensity.rgb * light.mColorAndIntensity.a * 0.005f * nDotL;
    }
    
    for (uint j = 0; j < lightInfo.numPointLight; j++)
    {
        PointLight light = pointLights[j];

        float3 lightDirection = light.mPosition.xyz - vertexPos.xyz;
        float dist = length(lightDirection);
        lightDirection /= dist;
        float att = Attenuation(dist, 3.f);
        float3 halfAngle = normalize(viewDirection + lightDirection);
        float vDotH = clamp(dot(viewDirection, halfAngle), 0.0, 1.0);
        float nDotL = clamp(dot(material.normalColor, lightDirection), 0.0, 1.0);
        result += LambertianDiffuse(material.diffuse, material.F0, float3(1.0, 1.0, 1.0), vDotH) * light.mColorAndIntensity.rgb * light.mColorAndIntensity.a * 0.005f * att * nDotL;
    }
    
    payload.lightIntensityAndDistance = float4(result, t);
    payload.hitNormal = normal;
    payload.hitPoint = vertexPos;
    payload.albedoAndRayType.rgb = material.baseColor;
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
    float3 normal = normals[instance][index].xyz;
    normal = normalize(mul(normal, (float3x3) modelMats[instance].mInvTransposeMat));
    return normal;
}

float3 GetTangentInVector(int instance, int index)
{
    float3 tangent = tangents[instance][index].xyz;
    return tangent;
}

float3 GetPositionInVector(int instance, int index)
{
    float3 pos = vertexPositions[instance][index].xyz;
    return pos;
}

float2 GetUVInVector(int instance, int index)
{
    float2 uv = uvs[instance][index];
    return uv;
}

int GetIndex(int vertId, int instance, int offset)
{
    int offs = vertId + offset;
    return indices[instance][offs];
}

float ComputeTexLOD(Texture2D tex, float Lu, float Lv)
{
    uint w, h, mips;
    tex.GetDimensions(0, w, h, mips);
    float rho = max(Lu * w, Lv * h); // texels covered along the worst axis
    return clamp(log2(max(rho, 1e-6)), 0.0, (float) (mips - 1));
}

void Compute_dPdu_dPdv(Vtx v0, Vtx v1, Vtx v2,
                       out float3 dPdu, out float3 dPdv)
{
    float3 dp1 = v1.P - v0.P;
    float3 dp2 = v2.P - v0.P;
    float2 duv1 = v1.uv - v0.uv;
    float2 duv2 = v2.uv - v0.uv;

    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    // Guard against degenerate UVs
    float invDet = (abs(det) > 1e-8f) ? 1.0f / det : 0.0f;

    dPdu = (dp1 * duv2.y - dp2 * duv1.y) * invDet; // = ∂P/∂u
    dPdv = (-dp1 * duv2.x + dp2 * duv1.x) * invDet; // = ∂P/∂v
}

void ComputeUVFootprint(uint instance, uint baseIndex, float coneR, out float Lu, out float Lv)
{
// Fetch triangle indices from your index buffer
    int i0 = GetIndex(baseIndex, instance, 0);
    int i1 = GetIndex(baseIndex, instance, 1);
    int i2 = GetIndex(baseIndex, instance, 2);

    // Vertex data (object space)
    float3 P0o = GetPositionInVector(instance, i0);
    float3 P1o = GetPositionInVector(instance, i1);
    float3 P2o = GetPositionInVector(instance, i2);
    float2 uv0 = GetUVInVector(instance, i0);
    float2 uv1 = GetUVInVector(instance, i1);
    float2 uv2 = GetUVInVector(instance, i2);

    // Transform positions to WORLD space to match coneR (world)
    float4x4 M = modelMats[instance].mModelMat;
    float3 P0 = mul(M, float4(P0o, 1)).xyz;
    float3 P1 = mul(M, float4(P1o, 1)).xyz;
    float3 P2 = mul(M, float4(P2o, 1)).xyz;

    // Build ∂P/∂u, ∂P/∂v in WORLD space
    float3 dp1 = P1 - P0;
    float3 dp2 = P2 - P0;
    float2 duv1 = uv1 - uv0;
    float2 duv2 = uv2 - uv0;

    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    float invDet = (abs(det) > 1e-8f) ? 1.0f / det : 0.0f;

    float3 dPdu = (dp1 * duv2.y - dp2 * duv1.y) * invDet;
    float3 dPdv = (-dp1 * duv2.x + dp2 * duv1.x) * invDet;

    const float eps = 1e-6;
    Lu = coneR / max(length(dPdu), eps);
    Lv = coneR / max(length(dPdv), eps);
}

PBRMaterial GenerateMaterial(MaterialInfo info, float2 uv, float3 normals, float3x3 tangentBasis, float Lu, float Lv)
{
    
    // Per-texture LODs (you can reuse or tweak per map)
    float lodColor = ComputeTexLOD(textures[info.colorTexIndex], Lu, Lv);
    float lodMR = ComputeTexLOD(textures[info.metallicRoughnessTexIndex], Lu, Lv);
    float lodOcc = ComputeTexLOD(textures[info.occlusionTexIndex], Lu, Lv);
    float lodEmit = ComputeTexLOD(textures[info.emissiveTexIndex], Lu, Lv);
    // Normal maps often benefit from a slightly finer mip to keep detail
    float lodNorm = max(lodColor - 0.5, 0.0);
    
    
    PBRMaterial mat;
    mat.baseColor = pow(abs(textures[info.colorTexIndex].SampleLevel(mainSampler, uv, lodColor).rgb), sGamma);
    mat.baseColor *= info.colorFactor.rgb;

    mat.emissiveColor = pow(abs(textures[info.emissiveTexIndex].SampleLevel(mainSampler, uv, lodEmit).rgb), sGamma);
    mat.emissiveColor *= info.emissiveFactor.rgb;

    float3 metallicRoughnessColor = textures[info.metallicRoughnessTexIndex].SampleLevel(mainSampler, uv, lodMR).rgb;
    mat.roughness = metallicRoughnessColor.g * info.metallicFactor;
    mat.metallic = metallicRoughnessColor.b * info.roughnessFactor;

    // Occlusion if it is not in matallic roughness texture
    mat.occlusionColor = textures[info.occlusionTexIndex].SampleLevel(mainSampler, uv, lodOcc).r;
    
    mat.normalColor = textures[info.normalTexIndex].SampleLevel(mainSampler, uv, lodNorm).rgb;
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