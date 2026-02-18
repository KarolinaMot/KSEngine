#include "RTCommon.hlsl"
#include "Structs.hlsl"
#include "PBR.hlsl"
#define M_PI 3.141592653589793

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> giHistory : register(u1);
RWTexture2D<float4> diHistory : register(u2);
Texture2D<uint4> GBufferA : register(t6);
Texture2D<float> GBufferB : register(t7);
Texture2D<float4> RenderedSkymap : register(t8);
Texture2D<uint4> DIReservoirA : register(t9);
Texture2D<float4> DIReservoirB : register(t10);
Texture2D<uint4> DIPRevReservoirA : register(t11);
Texture2D<float4> DIPrevReservoirB : register(t12);

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
void SpatialReuse(
    uint2 p, uint2 dims,
    float currDepth, float3 currN,
    float3 worldPos, float3 viewDir, PBRMaterial mat,
    Texture2D<uint4> A0, Texture2D<float4> B0,
    inout Reservoir R,
    inout uint rng);

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

    bool emptyPixel = (depthValue >= 0.9999f);
    PBRMaterial mat = LoadMaterialFromGBuffer(GBufferA, launchIndex);
    
    float fovX = 2.0 * atan(1.0 / abs(cameraMats.mProjection._11));
    float alpha0 = 2.0f * atan(tan(0.5f * fovX) / dims.x);
    bool valid = false;
    float3 directLighting = 0.f;
    float3 indirectLighting = 0.f;
    float3 res = 0.f;
    uint seed = InitSeed(launchIndex) ^ Hash(pathTracingData.frameIndex * 9781u);
    float3 outDI = directLighting;
    float outW = 1.0f;

    if (!emptyPixel && !mat.baseColor.a)
    {
        MaterialPayload matPayload = (MaterialPayload) 0;
        matPayload.bounceCount = 31;
        matPayload.coneAngle = alpha0;
        float4 target = mul(cameraMats.mInvProjection, float4(d.x, -d.y, 1, 1));
        float3 dirWS = normalize(mul(cameraMats.mInvView, float4(target.xyz, 0)).xyz);

        matPayload = ShootMaterialRay(dirWS, cameraMats.mCameraPos.xyz, SceneBVH, matPayload);

        UnpackAlbedoMetal(matPayload.bufferA.x, mat.baseColor.rgb, mat.metallic);
        mat.normalColor = UnpackNormalOct(matPayload.bufferA.y);
        mat.emissiveColor = UnpackEmissive(matPayload.bufferA.z);
        UnpackRoughOcc(matPayload.bufferA.w, mat.roughness, mat.occlusionColor, mat.baseColor.a);
        worldPos = matPayload.position;
        viewDirection = normalize(cameraMats.mCameraPos.xyz - worldPos.xyz);
        t = length(cameraMats.mCameraPos.xyz - worldPos.xyz);
        bias = max(1e-4f, t * 1e-4f);
            
        emptyPixel = matPayload.bufferA.a == 0;
        
    }
    
    if(!emptyPixel)
    {        
        Reservoir currentR = LoadReservoir(DIReservoirA, DIReservoirB, launchIndex);
        SpatialReuse(launchIndex, dims, depthValue, mat.normalColor, worldPos, viewDirection, mat, DIReservoirA, DIReservoirB, currentR, seed);

        directLighting = DirectLighting(currentR, mat, launchIndex, worldPos, viewDirection, seed, bias);
        
        float4 h = diHistory.Load(loadLocation);
        float3 histDI = h.rgb;
        float histW = h.a;
        uint4 prebReservA = DIPRevReservoirA.Load(uint3(launchIndex, 0));
        float4 prebReservB = DIPrevReservoirB.Load(uint3(launchIndex, 0));
        float3 prevNormal = UnpackNormalOct(prebReservA.w);
        prevNormal = normalize(prevNormal);
        float prevDepth = prebReservB.y;
        
        if ((histW > 0.f))
        {
            float alpha = 1.0f / (histW + 1.0f); // unbiased running average
            alpha = max(alpha, 1.0f / 64.0f); // don’t get *too* sluggish
            outDI = lerp(histDI, directLighting, alpha);
            outW = min(histW + 1.0f, 64.0f);
        }
        else
        {
           // reset history if invalid
            outDI = directLighting;
            outW = 1.0f;
        }
        
        diHistory[launchIndex] = float4(outDI, outW);
        
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
        outDI = RenderedSkymap.Load(loadLocation);
    }

    float4 historyValue = giHistory.Load(loadLocation).rgba;
    float3 historySum = historyValue.rgb;
    float sampleCount = historyValue.a;
    float3 newGISum = historySum + indirectLighting;
    float newSampleCount = sampleCount + pathTracingData.GIsampleNumber;
    float3 superSampledGI = newGISum / newSampleCount;
    
    giHistory[launchIndex] = float4(newGISum, newSampleCount);

    res = outDI.rgb + superSampledGI * lightInfo.GIBounceStrength;
    
    res.rgb *= lightInfo.exposure; // e.g. exposure = 1.0 .. 2.0 (or make it a slider)
    res.rgb = ToneMapReinhard(res.rgb);

    gOutput[launchIndex] = float4(LinearToSRGB(res.rgb), 1.f);

}

uint2 ClampPixel(int2 p, uint2 dims)
{
    p.x = clamp(p.x, 0, (int) dims.x - 1);
    p.y = clamp(p.y, 0, (int) dims.y - 1);
    return (uint2) p;
}

bool InBounds(int2 p, uint2 dims)
{
    return (p.x >= 0 && p.y >= 0 && p.x < (int) dims.x && p.y < (int) dims.y);
}

void SpatialReuse(
    uint2 p, uint2 dims,
    float currDepth, float3 currN,
    float3 worldPos, float3 viewDir, PBRMaterial mat,
    Texture2D<uint4> A0, Texture2D<float4> B0,
    inout Reservoir R,
    inout uint rng)
{
    static const int2 kOffsets8[8] =
    {
        int2(-1, 0), int2(1, 0),
                int2(0, -1), int2(0, 1),
                int2(-1, -1), int2(1, -1),
                int2(-1, 1), int2(1, 1),
    };
    
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        int2 pn_i = (int2) p + kOffsets8[i];
        
        if (!InBounds(pn_i, dims))
            continue;
        uint2 pn = (uint2) pn_i;

        float neighDepth;
        float3 neighN;

        Reservoir Rn = LoadReservoirAndOther(A0, B0, pn, neighDepth, neighN);
        
        if (Rn.M == 0u || Rn.W <= 0.0f || Rn.s.target <= 1e-8f)
            continue;

        // Target ratio correction (THIS is important)
        float t_prev = Rn.s.target;
        float t_here = TargetAtPixel(dirLights, pointLights, Rn.s, worldPos, viewDir, mat);
        if (t_here <= 1e-8f)
            continue;

        RISSample cand = Rn.s;
        cand.target = t_here;

        float ratio = t_here / max(t_prev, 1e-4f);
        ratio = clamp(ratio, 0.25f, 4.0f); // start conservative
        float wTotal_here = Rn.W * ratio;
        
        ReservoirUpdate(R, cand, wTotal_here, Rn.M, rng);
    }
    
    const uint M_CAP = 32;
    if (R.M > M_CAP)
    {
        R.W *= (float) M_CAP / (float) R.M;
        R.M = M_CAP;
    }
}

bool DepthCompatible(float currLinearDepth, float prevLinearDepth)
{
    return abs(currLinearDepth - prevLinearDepth) < max(0.002f, 0.002f * currLinearDepth);
}

bool NormalCompatible(float3 currN, float3 prevN)
{
    return dot(currN, prevN) > 0.9f; // tune
}

float2 ConcentricSampleDisk(float2 u)
{
    float2 a = 2.0f * u - 1.0f;
    if (a.x == 0 && a.y == 0)
        return 0;

    float r, phi;
    if (abs(a.x) > abs(a.y))
    {
        r = a.x;
        phi = (M_PI / 4.0f) * (a.y / a.x);
    }
    else
    {
        r = a.y;
        phi = (M_PI / 2.0f) - (M_PI / 4.0f) * (a.x / a.y);
    }

    return r * float2(cos(phi), sin(phi));
}

void MakeOrthonormalBasis(float3 n, out float3 t, out float3 b)
{
    float3 up = (abs(n.z) < 0.999f) ? float3(0, 0, 1) : float3(0, 1, 0);
    t = normalize(cross(up, n));
    b = cross(n, t);
}

float3 SampleConeDirection(float3 axis, float cosMax, float2 u)
{
    float cosTheta = lerp(cosMax, 1.0f, u.x);
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = 2.0f * M_PI * u.y;

    float3 t, b;
    MakeOrthonormalBasis(axis, t, b);

    return normalize(t * (cos(phi) * sinTheta) + b * (sin(phi) * sinTheta) + axis * cosTheta);
}

float3 ShadeChosenSoft(StructuredBuffer<DirLight> dirLights,
                       StructuredBuffer<PointLight> pointLights,
                       RISSample s,
                       inout uint seed,
                       out float3 lightDir,
                       out float tmax,
                       float3 worldPos,
                       float3 viewDirection,
                       PBRMaterial mat)
{
    float3 diff = 0.0f;
    float3 spec = 0.0f;
    float att = 1.0f;

    float3 lightColor;
    float lightIntensity;
    float2 u = float2(Rand(seed), Rand(seed));

    if (s.lightType == 0u)
    {
        DirLight light = dirLights[s.lightIndex];

        // Your code used lightDir = normalize(light.mDir.xyz).
        // For sun softness: sample around that direction.
        float3 axis = normalize(light.mDir.xyz);
        float cosMax = cos(max(light.mAngularRadius, 1e-6f));

        lightDir = SampleConeDirection(axis, cosMax, u);

        lightColor = light.mColorAndIntensity.rgb;
        lightIntensity = light.mColorAndIntensity.a;

        att = 1.0f;
        tmax = 1e30f; // "infinite"
    }
    else
    {
        PointLight light = pointLights[s.lightIndex];

        // Sample a disk on the light, oriented to face the shading point.
        float3 toC = light.mPosition.xyz - worldPos;
        float distC = length(toC);
        float3 z = toC / max(distC, 1e-6f);

        float3 t, b;
        MakeOrthonormalBasis(z, t, b);

        float2 d = ConcentricSampleDisk(u) * light.mRadius;

        float3 lightSamplePos = light.mPosition.xyz + t * d.x + b * d.y;

        float3 toP = lightSamplePos - worldPos;
        float distP = length(toP);

        lightDir = toP / max(distP, 1e-6f);

        // IMPORTANT: attenuation must use dist to sampled point (distP), not distC.
        att = Attenuation(distP, /*range*/5.f);

        lightColor = light.mColorAndIntensity.rgb;
        lightIntensity = light.mColorAndIntensity.a;

        tmax = distP - 1e-3f;
    }

    GetBRDF(mat, viewDirection, lightDir,
            lightColor, lightIntensity, att,
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

        float3 c = ShadeChosenSoft(dirLights, pointLights, R.s, seed,
                           lightDir, tmax, worldPos, viewDirection, mat);

        ShadowPayload shadowPayload = ShootShadowRay(lightDir, worldPos + mat.normalColor * bias, SceneBVH, tmax);
        bool visible = (shadowPayload.hit == 0) ? 1.0f : 0.0f;

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

