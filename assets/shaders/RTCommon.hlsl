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
    shadowRay.TMin = 1e-4f;
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

// Returns false if the point projects off-screen or behind camera.
bool ReprojectToPrevPixel(float3 worldPos,
                          float4x4 prevViewProj,
                          uint2 dims,
                          out uint2 prevPix,
                          out float prevNdcDepth)
{
    // 1) World -> previous clip space
    float4 prevClip = mul(prevViewProj, float4(worldPos, 1.0f));

    // If behind the camera or too close to w=0, reject
    if (prevClip.w <= 1e-6f)
        return false;

    // 2) Clip -> NDC (-1..1)
    float3 prevNdc = prevClip.xyz / prevClip.w;

    // If outside the NDC cube, it's off-screen (z test optional depending on convention)
    // x,y outside [-1,1] means off-screen.
    if (prevNdc.x < -1.0f || prevNdc.x > 1.0f ||
        prevNdc.y < -1.0f || prevNdc.y > 1.0f)
        return false;

    // Save NDC depth for later comparisons
    prevNdcDepth = prevNdc.z;

    // 3) NDC -> UV (0..1)
    // NDC y is +up, texture UV y is +down for typical DX conventions.
    float2 prevUv;
    prevUv.x = prevNdc.x * 0.5f + 0.5f;
    prevUv.y = -prevNdc.y * 0.5f + 0.5f;

    // 4) UV -> pixel coords
    // Use floor to get integer pixel index.
    int2 p = int2(prevUv * float2(dims));

    // Clamp/check bounds
    if (p.x < 0 || p.y < 0 || p.x >= (int) dims.x || p.y >= (int) dims.y)
        return false;

    prevPix = (uint2) p;
    return true;
}

Reservoir LoadReservoirAndOther(Texture2D<uint4> ResA, Texture2D<float4> ResB, uint2 launchIndex, out float depth, out float3 normal)
{
    uint4 aRes = ResA.Load(uint3(launchIndex, 0));
    float4 bRes = ResB.Load(uint3(launchIndex, 0));
    
    Reservoir R = LoadReservoir(ResA, ResB, launchIndex);
    depth = bRes.y; // whatever prev depth texture is
    normal = UnpackNormalOct(aRes.w);
    normal = normalize(normal);
    return R;
}


float3 ShadeChosen(StructuredBuffer<DirLight> dirLights,
                    StructuredBuffer<PointLight> pointLights,
                    RISSample s, out
                    float3 lightDir, out
                    float tmax, float3 worldPos, float3 viewDirection, PBRMaterial mat)
{
    float3 diff = 0.0f;
    float3 spec = 0.0f;
    float att = 1.f;
    float3 lightColor;
    float lightIntensity;

    if (s.lightType == 0u)
    {
        DirLight light = dirLights[s.lightIndex];
        lightDir = normalize(light.mDir.xyz);
        lightColor = light.mColorAndIntensity.rgb;
        lightIntensity = light.mColorAndIntensity.a;
        tmax = 100000;
    }
    else
    {
        PointLight light = pointLights[s.lightIndex];

        float3 toLight = light.mPosition.xyz - worldPos;
        float dist = length(toLight);

        // Normalize direction safely.
        lightDir = toLight / max(dist, 1e-6f);

        att = Attenuation(dist, light.mRadius);
        lightColor = light.mColorAndIntensity.rgb;
        lightIntensity = light.mColorAndIntensity.a;
        tmax = dist - 1e-3f;
    }

    GetBRDF(mat, viewDirection, lightDir,
            lightColor,
            lightIntensity,
            att,
            diff, spec);

    return diff + spec;
}

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float TargetAtPixel(StructuredBuffer<DirLight> dirLights,
                    StructuredBuffer<PointLight> pointLights, RISSample s, float3 worldPos, float3 viewDir, PBRMaterial mat)
{
    float3 L;
    float tmax;
    float3 c = ShadeChosen(dirLights, pointLights, s, L, tmax, worldPos, viewDir, mat);
    return Luminance(max(c, 0.0f));
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

void ReservoirUpdate(inout Reservoir R, RISSample cand, float wTotal, uint m, inout uint rng)
{
    // Increase how many candidates we represent.
    // For "normal" per-pixel candidate generation, m=1 each time.
    // For temporal/spatial merge, m can be >1 (the other reservoir's M).
    R.M += m;

    // New total weight after merging in the packet.
    float Wnew = R.W + wTotal;

    // If Wnew is zero, everything is zero contribution so skip.
    // (This happens if all candidates had target=0, e.g. surface facing away from all lights.)
    if (Wnew > 0.0f)
    {
        // Draw a random number in [0,1).
        float pick = Rand(rng);

        // With probability wTotal / Wnew, replace chosen sample.
        // This is weighted reservoir sampling: the chosen sample is distributed
        // proportionally to candidate weights without storing them all.
        if (pick < (wTotal / Wnew))
        {
            R.s = cand; // accept the new candidate (or packet representative)
        }

        // Update sum of weights.
        R.W = Wnew;
    }
}


uint InitSeed(uint2 pixel)
{
    uint s = pixel.x * 1973u + pixel.y * 9277u;
    return Hash(s);
}

void PackReservoir(Reservoir R, float3 normalColor, float depth, out uint4 A, out float4 B)
{
    A = uint4(
        R.s.lightIndex,
        R.s.lightType,
        R.M,
        PackNormalOct(normalColor));
    
    B = float4( R.W, // sum of weights
        depth, // chosen sample's target
        R.s.target, // candidate count (stored as float)
        0.0f);
}