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
    float3 position;
    uint padding;
    
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

ShadowPayload ShootShadowRay(float3 direction, float3 position, RaytracingAccelerationStructure SceneBVH)
{
    RayDesc shadowRay;
    shadowRay.Origin = position; // simpler & correct
    shadowRay.Direction = direction;
    shadowRay.TMin = 0;
    shadowRay.TMax = 100000;

        
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