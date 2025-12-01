#include "RTCommon.hlsl"
#include "Structs.hlsl"
#include "PBR.hlsl"
#define M_PI 3.141592653589793

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

void CreateCoordinateSystem(const float3 N, out float3 Nt, out float3 Nb);
float3 UniformSampleHemisphere(const float r1, const float r2);

uint Hash(uint x);
float Rand(inout uint seed);
uint InitSeed(uint2 pixel);

[shader("raygeneration")]
void RayGen()
{
    // Initialize the ray payload
    HitInfo payload;
    payload.lightIntensityAndDistance = float4(0.f, 0.f, 0.f, 0.f);
    payload.albedoAndRayType.a = 0;

    // Get the location within the dispatched 2D grid of work items
    // (often maps to pixels, so this could represent a pixel coordinate).
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
    
     // Horizontal pixel angular size (works well in practice)
    float fovX = 2.0 * atan(1.0 / abs(cameraMats.mProjection._11));
    float alpha0 = 2.0f * atan(tan(0.5f * fovX) / dims.x);
    payload.coneAngle = alpha0;
    
    // Define a ray, consisting of origin, direction, and the min-max distance values
    RayDesc ray;
    ray.Origin = cameraMats.mCameraPos.xyz; // simpler & correct
    float4 target = mul(cameraMats.mInvProjection, float4(d.x, -d.y, 1, 1));
    float3 dirWS = normalize(mul(cameraMats.mInvView, float4(target.xyz, 0)).xyz);
    ray.Direction = dirWS;
    ray.TMin = 0;
    ray.TMax = 100000;
    uint seed = InitSeed(launchIndex);

    
    //Direct lighting
    // Trace the ray
    TraceRay(
      // Parameter name: AccelerationStructure
      // Acceleration structure
      SceneBVH,

      // Parameter name: RayFlags
      // Flags can be used to specify the behavior upon hitting a surface
      RAY_FLAG_NONE,

      // Parameter name: InstanceInclusionMask
      // Instance inclusion mask, which can be used to mask out some geometry to this ray by
      // and-ing the mask with a geometry mask. The 0xFF flag then indicates no geometry will be
      // masked
      0xFF,

      // Parameter name: RayContributionToHitGroupIndex
      // Depending on the type of ray, a given object can have several hit groups attached
      // (ie. what to do when hitting to compute regular shading, and what to do when hitting
      // to compute shadows). Those hit groups are specified sequentially in the SBT, so the value
      // below indicates which offset (on 4 bits) to apply to the hit groups for this ray. In this
      // sample we only have one hit group per object, hence an offset of 0.
      0,

      // Parameter name: MultiplierForGeometryContributionToHitGroupIndex
      // The offsets in the SBT can be computed from the object ID, its instance ID, but also simply
      // by the order the objects have been pushed in the acceleration structure. This allows the
      // application to group shaders in the SBT in the same order as they are added in the AS, in
      // which case the value below represents the stride (4 bits representing the number of hit
      // groups) between two consecutive objects.
      0,

      // Parameter name: MissShaderIndex
      // Index of the miss shader to use in case several consecutive miss shaders are present in the
      // SBT. This allows to change the behavior of the program when no geometry have been hit, for
      // example one to return a sky color for regular rendering, and another returning a full
      // visibility value for shadow rays. This sample has only one miss shader, hence an index 0
      0,

      // Parameter name: Ray
      // Ray information to trace
      ray,

      // Parameter name: Payload
      // Payload associated to the ray, which will be used to communicate between the hit/miss
      // shaders and the raygen
      payload);
    
    float3 directLighting = payload.lightIntensityAndDistance.rgb;
    float3 indirectLighting = float3(0.f, 0.f, 0.f);

    //Global illumination
    if (payload.lightIntensityAndDistance.a > 0.f)
    {
        float3 Nt, Nb;
        CreateCoordinateSystem(payload.hitNormal, Nt, Nb);
        uint smaples = 4;
        float bias = max(1e-4f, payload.lightIntensityAndDistance.w * 1e-4f);
        for (uint n = 0; n < smaples; ++n)
        {
        //How high above the horizon of the hemisphere the line is
            float r1 = Rand(seed);
        //The spin around the axis
            float r2 = Rand(seed);
            float3 sample = UniformSampleHemisphere(r1, r2);
            float3 sampleWorld =
              sample.x * Nt
            + sample.y * Nb
            + sample.z * payload.hitNormal;
        
            RayDesc indirectRay;
            indirectRay.Origin = payload.hitPoint.xyz + sampleWorld * bias; // simpler & correct
            indirectRay.Direction = sampleWorld;
            indirectRay.TMin = 0;
            indirectRay.TMax = 100000;
        
            HitInfo indirectPayload;
            indirectPayload.albedoAndRayType.a = 1;
            TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            2,
            0,
            0,
            indirectRay,
            indirectPayload);
        
            float pdf = 1.f / (2.f * M_PI);
            indirectLighting += r1 * indirectPayload.lightIntensityAndDistance.rgb * (payload.albedoAndRayType.rgb / M_PI) / pdf;
        }
        indirectLighting /= (float) smaples;
    }
    
    float3 res = payload.lightIntensityAndDistance.a >= 0 ? (directLighting + indirectLighting) : directLighting;
    //float3 res = directLighting;
    gOutput[launchIndex] = float4(LinearToSRGB(res), 1.f);
    //gOutput[launchIndex] = float4(directLighting, 1.f);
}

float3 offset_ray(const float3 p, const float3 n)
{
    float intScale = 256.0f;
    float origin = 1.f / 32.f;
    float floatScale = 1.f / 65536.f;
    
    int3 ofI = int3(intScale * n.x, intScale * n.y, intScale * n.z);
    

    float3 pI = float3(
    float(int(p.x) + ((p.x < 0) ? -ofI.x : ofI.x)),
    float(int(p.y) + ((p.y < 0) ? -ofI.y : ofI.y)),
    float(int(p.z) + ((p.z < 0) ? -ofI.z : ofI.z)));

    return float3(abs(p.x) < origin ? p.x + floatScale * n.x : pI.x,
    abs(p.y) < origin ? p.y + floatScale * n.y : pI.y,
    abs(p.z) < origin ? p.z + floatScale * n.z : pI.z);
}

void CreateCoordinateSystem(const float3 N, out float3 Nt, out float3 Nb)
{
    if (abs(N.x) > abs(N.y))
        Nt = float3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
    else
        Nt = float3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
    Nb = cross(N, Nt);
}

float3 UniformSampleHemisphere(const float r1, const float r2)
{
    // cos(theta) = r1 = y
    // cos^2(theta) + sin^2(theta) = 1 -> sin(theta) = sqrtf(1 - cos^2(theta))
    float sinTheta = sqrt(1.f - r1 * r1);
    float phi = 2 * M_PI * r2;
    float x = sinTheta * cos(phi);
    float z = sinTheta * sin(phi);
    return float3(x, r1, z);
}

uint Hash(uint x)
{
    x ^= x >> 17;
    x *= 0xed5ad4bb;
    x ^= x >> 11;
    x *= 0xac4c1b51;
    x ^= x >> 15;
    x *= 0x31848bab;
    x ^= x >> 14;
    return x;
}

// Returns a random float in [0,1)
float Rand(inout uint seed)
{
    seed = Hash(seed);
    // Take lower 24 bits and normalize
    return (seed & 0x00FFFFFFu) / 16777216.0f; // 2^24
}

uint InitSeed(uint2 pixel)
{
    uint s = pixel.x * 1973u + pixel.y * 9277u;
    return Hash(s);
}