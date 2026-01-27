// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
#include "PBR.hlsl"

struct HitInfo
{
    float4 lightIntensityAndDistance;
    float coneAngle;
    float3 hitNormal;    
    float4 hitPoint;    
    float3 albedo;  
    uint bounceCount;
};

struct ShadowPayload
{
    uint hit;
};

struct MaterialPayload
{
    float4 bufferA;
    float coneAngle;
    uint bounceCount;
    float3 position;
    uint padding;
    
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
  float2 bary;
};

struct RISSample
{
    uint lightType; // 0=directional, 1=point
    uint lightIndex; // index in light buffers
    float target; // scalar target value for this sample at this pixel (e.g. luminance of unshadowed contribution)
};

struct Reservoir
{
    RISSample s; // the selected sample (light choice)
    float W; // sum of weights across all candidates (weight = target/pdf)
    uint M; // number of candidates represented
};

void ReservoirInit(out Reservoir R)
{
    R.W = 0.0f;
    R.M = 0u;
    R.s.lightType = 0u;
    R.s.lightIndex = 0u;
    R.s.target = 0.0f;
}

HitInfo ShootBRDFRay(float3 direction, float3 position, RaytracingAccelerationStructure SceneBVH, HitInfo indirectPayload)
{
    indirectPayload.bounceCount--; // decrement BEFORE tracing
    if (indirectPayload.bounceCount == 0)
        return indirectPayload;

    RayDesc indirectRay;
    indirectRay.Direction = direction;
    indirectRay.Origin = position;
    indirectRay.TMin = 0;
    indirectRay.TMax = 100000;
        
    TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            3,
            0,
            indirectRay,
            indirectPayload);
    
    return indirectPayload;
}

MaterialPayload ShootMaterialRay(float3 direction, float3 position, RaytracingAccelerationStructure SceneBVH, MaterialPayload materialPayload)
{
    materialPayload.bounceCount--; // decrement BEFORE tracing
    if (materialPayload.bounceCount == 0)
        return materialPayload;
    
    RayDesc materialRay;
    materialRay.Direction = direction;
    materialRay.Origin = position + direction * 1e-3f;
    materialRay.TMin = 1e-4f;
    materialRay.TMax = 100000;
        
    TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            2,
            3,
            0,
            materialRay,
            materialPayload);
    
    return materialPayload;
}

ShadowPayload ShootShadowRay(float3 direction, float3 position, RaytracingAccelerationStructure SceneBVH, float tmax)
{
    RayDesc shadowRay;
    shadowRay.Origin = position; // simpler & correct
    shadowRay.Direction = direction;
    shadowRay.TMin = 0;
    shadowRay.TMax = tmax;

    ShadowPayload shadowPayload;
    shadowPayload.hit = 0;
    // Trace the ray
    TraceRay(
        // Acceleration structure
        SceneBVH,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        0xFF,
        // Hit group
        1,
        3,
        // Index of the miss shader
        1,
        // Ray information to trace
        shadowRay,
        // Payload associated to the ray, which will be used to communicate
        // between the hit/miss shaders and the raygen
        shadowPayload);

    return shadowPayload;
}

Reservoir LoadReservoir(Texture2D<uint4> ResA, Texture2D<float4> ResB, uint2 launchIndex)
{
    uint4 aRes = ResA.Load(uint3(launchIndex, 0));
    float4 bRes = ResB.Load(uint3(launchIndex, 0));
    
    Reservoir R;
    R.s.lightIndex = aRes.x;
    R.s.lightType = aRes.y;
    R.s.target = bRes.z;
            
    R.W = bRes.x;
    R.M = (uint) (aRes.z + 0.5f);

    return R;
}

Reservoir LoadReservoirAndOther(Texture2D<uint4> ResA, Texture2D<float4> ResB, uint2 launchIndex, out float depth, out float3 normal)
{
    uint4 aRes = ResA.Load(uint3(launchIndex, 0));
    float4 bRes = ResB.Load(uint3(launchIndex, 0));
    
    Reservoir R = LoadReservoir(ResA, ResB, launchIndex);
    depth = bRes.y; // whatever prev depth texture is
    normal = UnpackNormalOct(aRes.w);
    normal = normalize(normal * 2.0 - 1.0);
    
    return R;
}

PBRMaterial LoadMaterialFromGBuffer(Texture2D<uint4> GBufferA, uint2 loadLocation, out bool emptyPixel)
{
    uint4 bufferAValue = GBufferA.Load(uint3(loadLocation, 0));

    PBRMaterial mat = (PBRMaterial) 0;
    UnpackAlbedoMetal(bufferAValue.x, mat.baseColor.rgb, mat.metallic);
    mat.normalColor = UnpackNormalOct(bufferAValue.y);
    mat.emissiveColor = UnpackEmissive(bufferAValue.z);
    UnpackRoughOcc(bufferAValue.w, mat.roughness, mat.occlusionColor, mat.baseColor.a);
    float3 normalColor = mat.normalColor;
    mat.normalColor = normalize(mat.normalColor * 2.0 - 1.0);
    mat.baseColor.rgb *= mat.baseColor.a;
    mat.F0 = float3(0.04, 0.04, 0.04);
    mat.F0 = lerp(mat.F0, mat.baseColor.rgb, mat.metallic);
    mat.diffuse = lerp(mat.baseColor.rgb, float3(0.0, 0.0, 0.0), mat.metallic) * mat.baseColor.a;
    emptyPixel = (normalColor.x == 0.f && normalColor.y == 0.f &&  normalColor.z == 1.f);
    return mat;
}