#include "RTCommon.hlsl"
#include "Structs.hlsl"
#include "PBR.hlsl"
#define M_PI 3.141592653589793

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> GBufferA : register(u1);
RWTexture2D<float4> GBufferB : register(u2);
RWTexture2D<float4> DirectLighting : register(u3);
Texture2D<float> Depth : register(t5);

StructuredBuffer<DirLight> dirLights : register(t2);
StructuredBuffer<PointLight> pointLights : register(t3);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);
SamplerState mainSampler : register(s0);

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};
cbuffer LightInfoBuffer : register(b1)
{
    LightInfo lightInfo;
};

void CreateCoordinateSystem(const float3 N, out float3 Nt, out float3 Nb);
float3 UniformSampleHemisphere(const float r1, const float r2);

uint Hash(uint x);
float Rand(inout uint seed);
uint InitSeed(uint2 pixel);

[shader("raygeneration")]
void RayGen(/*uint3 dispatchThreadID : SV_DispatchThreadID*/)
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;
    float2 uv = (launchIndex + 0.5) / dims;
    
    float3 normal = GBufferB.Load(launchIndex).rgb;
    normal = normalize(normal * 2.0 - 1.0);

    float3 directLighting = DirectLighting.Load(launchIndex).rgb;
    float4 albedo = GBufferA.Load(launchIndex).rgba;
    float depth = Depth.SampleLevel(mainSampler, uv, 0).r;
    float3 vertexPos = ReconstructWorldPos(launchIndex, dims, cameraMats.minvCamera, depth);

    //Shadows
    float3 t = length(cameraMats.mCameraPos.xyz - vertexPos);
    float bias = max(1e-4f, t * 1e-4f);
    uint seed = InitSeed(launchIndex);

    for (uint i = 0; i < lightInfo.numDirLight; i++)
    {
        DirLight light = dirLights[i];
        float3 lightDir = normalize(light.mDir.xyz);
        float angularRadius = 0.0047f;
        float coneScale = tan(angularRadius);
        
        float3 T, B;
        CreateCoordinateSystem(lightDir, T, B);
        
        uint samples = 4;
        float visible = 0.f;
        for (uint i = 0; i < samples; i++)
        {
            float u1 = Rand(seed);
            float u2 = Rand(seed);
            float2 d = UniformSampleHemisphere(u1, u2) * coneScale;
            float3 dir = normalize(lightDir + T * d.x + B * d.y);
            
            RayDesc shadowRay;
            shadowRay.Origin = vertexPos.xyz + normal * bias; // simpler & correct
            shadowRay.Direction = dir;
            shadowRay.TMin = 0;
            shadowRay.TMax = 100000;
        
            ShadowPayload shadowPayload;
            // Trace the ray
            TraceRay(
              // Acceleration structure
              SceneBVH,
              RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
              0xFF,
              // Hit group
              1,
              0,
              // Index of the miss shader
              1,
              // Ray information to trace
              shadowRay,
              // Payload associated to the ray, which will be used to communicate
              // between the hit/miss shaders and the raygen
              shadowPayload);
            
            visible += !shadowPayload.hit ? 1.0f : 0.0f;
        }
        
        visible = visible / samples;
        directLighting *= visible;
    }
    
    //Global illumination
    float3 indirectLighting = float3(0.f, 0.f, 0.f);
    float3 Nt, Nb;
    CreateCoordinateSystem(normal, Nt, Nb);
    uint smaples = 4;
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
        + sample.z * normal;
        
        RayDesc indirectRay;
        indirectRay.Direction = normalize(sampleWorld);
        indirectRay.Origin = vertexPos + indirectRay.Direction * bias;
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
        indirectLighting += r1 * indirectPayload.lightIntensityAndDistance.rgb * (albedo.rgb / M_PI) / pdf;
    }
    indirectLighting /= (float) smaples;
    
    float3 res = directLighting.rgb + indirectLighting;
    gOutput[launchIndex] = float4(LinearToSRGB(res), 1.f);
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