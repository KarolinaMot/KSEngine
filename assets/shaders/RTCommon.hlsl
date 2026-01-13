// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
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
    uint2 padding;
    
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
  float2 bary;
};

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