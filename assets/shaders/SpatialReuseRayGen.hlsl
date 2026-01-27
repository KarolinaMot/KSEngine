#include "RTCommon.hlsl"
#include "Structs.hlsl"
#include "PBR.hlsl"
#define M_PI 3.141592653589793

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> giHistory : register(u1);
Texture2D<uint4> GBufferA : register(t6);
Texture2D<float> GBufferB : register(t7);
Texture2D<float4> RenderedSkymap : register(t8);
Texture2D<uint4> DIReservoirA : register(t9);
Texture2D<float4> DIReservoirB : register(t10);

StructuredBuffer<DirLight> dirLights : register(t2);
StructuredBuffer<PointLight> pointLights : register(t3);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

bool ReprojectToPrevPixel(float3 worldPos,
                          float4x4 prevViewProj,
                          uint2 dims,
                          out uint2 prevPix,
                          out float prevNdcDepth);


SamplerState mainSampler : register(s0);

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};
cbuffer LightInfoBuffer : register(b1)
{
    LightInfo lightInfo;
};
cbuffer RTX : register(b2)
{
    PathTracingData pathTracingData;
};

void CreateCoordinateSystem(const float3 N, out float3 Nt, out float3 Nb);
float3 UniformSampleHemisphere(const float r1, const float r2);
float3 CosineSampleHemisphere(float u1, float u2);
float3 DirectLighting(Reservoir R, PBRMaterial mat, float2 launchIndex, float3 worldPos, float3 viewDirection, float seed, float bias);
bool DepthCompatible(float currLinearDepth, float prevLinearDepth);
bool NormalCompatible(float3 currN, float3 prevN);
void ReservoirUpdate(inout Reservoir R, RISSample cand, float wTotal, uint m, inout uint rng);

uint Hash(uint x);
float Rand(inout uint seed);
uint InitSeed(uint2 pixel);

[shader("raygeneration")]void RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;
    float2 uv = (launchIndex + 0.5) / dims;
    float3 loadLocation = float3(launchIndex, 0.f);
    
    float depthValue = GBufferB.Load(loadLocation);

    float3 viewPos = ReconstructViewPosFromViewZ(uv, depthValue, cameraMats.mInvProjection);
    float3 worldPos = mul(cameraMats.mInvView, float4(viewPos, 1.0f)).xyz;
    float3 viewDirection = normalize(cameraMats.mCameraPos.xyz - worldPos.xyz);
    float3 t = length(cameraMats.mCameraPos.xyz - worldPos.xyz);
    float bias = max(1e-4f, t * 1e-4f);
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);

    bool emptyPixel;
    PBRMaterial mat = LoadMaterialFromGBuffer(GBufferA, launchIndex, emptyPixel);
    
    float fovX = 2.0 * atan(1.0 / abs(cameraMats.mProjection._11));
    float alpha0 = 2.0f * atan(tan(0.5f * fovX) / dims.x);
    bool valid = false;
    float3 directLighting = 0.f;
    float3 indirectLighting = 0.f;
    float3 res = 0.f;

    if (!emptyPixel)
    {
        Reservoir currentR = LoadReservoir(DIReservoirA, DIReservoirB, launchIndex);
        
        uint seed = InitSeed(launchIndex) ^ Hash(pathTracingData.frameIndex * 9781u);

        directLighting = DirectLighting(currentR, mat, launchIndex, worldPos, viewDirection, seed, bias);

        // Global illumination
        float3 Nt, Nb;
        CreateCoordinateSystem(mat.normalColor, Nt, Nb);
        uint smaples = pathTracingData.GIsampleNumber;

        for (uint n = 0; n < smaples; ++n)
        {
            // How high above the horizon of the hemisphere the line is
            float r1 = Rand(seed);
            // The spin around the axis
            float r2 = Rand(seed);
            float3 s = CosineSampleHemisphere(r1, r2);
            float3 sampleWorld = s.x * Nt + s.y * Nb + s.z * mat.normalColor;

            HitInfo indirectPayload = (HitInfo) 0;
            indirectPayload.bounceCount = 35;
            indirectPayload.coneAngle = alpha0;

            indirectPayload = ShootBRDFRay(normalize(sampleWorld), worldPos + mat.normalColor * bias, SceneBVH, indirectPayload);

            float nDotWi = saturate(dot(mat.normalColor, sampleWorld));
            float3 Li = indirectPayload.lightIntensityAndDistance.rgb;

            // luminance clamp (example)
            float lum = dot(Li, float3(0.2126, 0.7152, 0.0722));
            float maxLum = 10.0; // tune
            Li *= min(1.0, maxLum / max(lum, 1e-6));

            indirectLighting += Li * mat.baseColor.rgb;
        }
    }
    else
    {
        directLighting = RenderedSkymap.Load(loadLocation);
    }

    float4 historyValue = giHistory.Load(loadLocation).rgba;
    float3 historySum = historyValue.rgb;
    float sampleCount = historyValue.a;
    float3 newGISum = historySum + indirectLighting;
    float newSampleCount = sampleCount + pathTracingData.GIsampleNumber;
    float3 superSampledGI = newGISum / newSampleCount;
    
    giHistory[launchIndex] = float4(newGISum, newSampleCount);

    res = directLighting.rgb + superSampledGI;
    gOutput[launchIndex] = float4(LinearToSRGB(res), 1.f);

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

bool DepthCompatible(float currLinearDepth, float prevLinearDepth)
{
    return abs(currLinearDepth - prevLinearDepth) < max(0.002f, 0.002f * currLinearDepth);
}

bool NormalCompatible(float3 currN, float3 prevN)
{
    return dot(currN, prevN) > 0.9f; // tune
}

float3 ShadeChosen(RISSample s, out float3 lightDir, out float tmax, float3 worldPos, float3 viewDirection, PBRMaterial mat)
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

        att = Attenuation(dist, /*range*/5.f);
        lightColor = light.mColorAndIntensity.rgb;
        lightIntensity = light.mColorAndIntensity.a;
        tmax = dist - 1e-3f;
    }

    GetBRDF(mat, viewDirection, lightDir,
            lightColor,
            lightIntensity * 0.005,
            att,
            diff, spec);

    return diff + spec;
}




float3 DirectLighting(Reservoir R, PBRMaterial mat, float2 launchIndex, float3 worldPos, float3 viewDirection, float seed, float bias)
{
    float3 direct = 0.0f;

    if (R.M > 0u && R.W > 0.0f && R.s.target > 1e-8f)
    {
        float3 diffuse = 0.f;
        float3 specular = 0.f;

        // Shadows
        uint shadowSeed = InitSeed(launchIndex);
        float3 lightDir;
        float tmax;
        float3 c = ShadeChosen(R.s, lightDir, tmax, worldPos, viewDirection, mat);

        // Harsh shadows for now
        ShadowPayload shadowPayload = ShootShadowRay(lightDir, worldPos + mat.normalColor * bias, SceneBVH, tmax);
        float visible = (shadowPayload.hit == 0) ? 1.0f : 0.0f;

        // -----------------------------
        // Reservoir normalization
        // -----------------------------
        // Intuition:
        // - R.W approximates sum(target/pdf) over candidates.
        // - We selected one sample proportionally to its weight.
        // - norm turns the chosen sample into an unbiased estimator of the sum.
        //
        // IMPORTANT:
        // Use R.s.target from *the chosen sample*.
        // If target is tiny, norm can blow up -> fireflies.
        // Clamp if needed (or clamp c/target).
        float norm = R.W / (max(1u, R.M) * max(R.s.target, 1e-8f));
        direct = c * visible * norm;
    }

    return direct * mat.occlusionColor + mat.emissiveColor;
}


float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
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

float3 CosineSampleHemisphere(float u1, float u2)
{
    float r = sqrt(u1);
    float phi = 2.0 * M_PI * u2;
    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = sqrt(1.0 - u1);
    return float3(x, y, z);
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