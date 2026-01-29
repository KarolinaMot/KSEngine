#include "RTCommon.hlsl"
#include "Structs.hlsl"
#include "PBR.hlsl"
#define M_PI 3.141592653589793

// Raytracing output texture, accessed as a UAV
Texture2D<uint4> GBufferA : register(t6);
Texture2D<float> GBufferB : register(t7);
Texture2D<float4> RenderedSkymap : register(t8);
RWTexture2D<uint4> DIReservoirA : register(u2);
RWTexture2D<float4> DIReservoirB : register(u3);

Texture2D<uint4> DIPRevReservoirA : register(t9);
Texture2D<float4> DIPrevReservoirB : register(t10);

StructuredBuffer<DirLight> dirLights : register(t2);
StructuredBuffer<PointLight> pointLights : register(t3);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

SamplerState mainSampler : register(s0);

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};
cbuffer prevCamera : register(b3)
{
    CameraMats prevCameraMats;
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
Reservoir BuildReservoir(inout uint seed, PBRMaterial mat, float3 viewDir, float3 worldPos);
bool DepthCompatible(float currLinearDepth, float prevLinearDepth);
bool NormalCompatible(float3 currN, float3 prevN);

[shader("raygeneration")]void RayGen( /*uint3 dispatchThreadID : SV_DispatchThreadID*/)
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;
    float2 uv = (launchIndex + 0.5) / dims;

    float3 directLighting = 0.f;
    float3 indirectLighting = 0.f;

    float depthValue = GBufferB.Load(uint3(launchIndex, 0));
    float3 viewPos = ReconstructViewPosFromViewZ(uv, depthValue, cameraMats.mInvProjection);
    float3 worldPos = mul(cameraMats.mInvView, float4(viewPos, 1.0f)).xyz;
    float3 viewDirection = normalize(cameraMats.mCameraPos.xyz - worldPos.xyz);
    
    float3 t = length(cameraMats.mCameraPos.xyz - worldPos.xyz);
    float bias = max(1e-4f, t * 1e-4f);
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
    float3 res = 0.f;
    bool ok = false;
    float prevDepth = 0.f; // whatever prev depth texture is
    float3 prevNormal = 0.f; // whatever prev depth texture is

    bool emptyPixel = (depthValue >= 0.9999f);
    PBRMaterial mat = LoadMaterialFromGBuffer(GBufferA, launchIndex);
    
    float fovX = 2.0 * atan(1.0 / abs(cameraMats.mProjection._11));
    float alpha0 = 2.0f * atan(tan(0.5f * fovX) / dims.x);
    bool valid = true;
    if (!emptyPixel)
    {
        if (!mat.baseColor.a)
        {
            MaterialPayload matPayload = (MaterialPayload) 0;
            matPayload.bounceCount = 31;
            matPayload.coneAngle = alpha0;
            matPayload = ShootMaterialRay(-viewDirection, worldPos, SceneBVH, matPayload);

            UnpackAlbedoMetal(matPayload.bufferA.x, mat.baseColor.rgb, mat.metallic);
            mat.normalColor = UnpackNormalOct(matPayload.bufferA.y);
            mat.emissiveColor = UnpackEmissive(matPayload.bufferA.z);
            UnpackRoughOcc(matPayload.bufferA.w, mat.roughness, mat.occlusionColor, mat.baseColor.a);
            float3 normalColor = mat.normalColor;
            mat.normalColor = normalize(mat.normalColor * 2.0 - 1.0);
            worldPos = matPayload.position;
            viewDirection = normalize(cameraMats.mCameraPos.xyz - worldPos.xyz);
            t = length(cameraMats.mCameraPos.xyz - worldPos.xyz);
            bias = max(1e-4f, t * 1e-4f);
        }
        
        uint seed = InitSeed(launchIndex) ^ Hash(pathTracingData.frameIndex * 9781u);

        Reservoir currentR = BuildReservoir(seed, mat, viewDirection, worldPos);
        
        uint2 prevPix;
        float prevNdcZ;
        ok = ReprojectToPrevPixel(worldPos, prevCameraMats.mCamera, dims, prevPix, prevNdcZ);

        if (ok)
        {
            Reservoir Rprev = LoadReservoirAndOther(DIPRevReservoirA, DIPrevReservoirB, prevPix, prevDepth, prevNormal);

            if (valid)
            {
                float t_prev = Rprev.s.target; // target at previous pixel
                float t_here = TargetAtPixel(dirLights, pointLights, Rprev.s, worldPos, viewDirection, mat); // target at this pixel
                RISSample cand = Rprev.s;
                cand.target = t_here; // IMPORTANT: store target for THIS pixel

                if (t_prev > 1e-4f && t_here > 1e-4f)   // use bigger epsilon than 1e-8
                {
                    float ratio = t_here / t_prev;

                    // Clamp ratio to prevent extreme rescaling from tiny target changes
                    ratio = clamp(ratio, 0.25f, 4.0f); // start conservative, loosen later

                    float wTotal_here = Rprev.W * ratio;
                    ReservoirUpdate(currentR, cand, wTotal_here, Rprev.M, seed);
                }
            }
        }
        
        const uint M_CAP = 32;
        if (currentR.M > M_CAP)
        {
            float scale = (float) M_CAP / (float) currentR.M;
            currentR.W *= scale;
            currentR.M = M_CAP;
        }
        
        uint4 A;
        float4 B;
        PackReservoir(currentR, mat.normalColor, depthValue, A, B);
        
        DIReservoirA[launchIndex] = A;      
        DIReservoirB[launchIndex] = B;
    }
}



bool DepthCompatible(float currLinearDepth, float prevLinearDepth)
{
    return abs(currLinearDepth - prevLinearDepth) < max(0.002f, 0.002f * currLinearDepth);
}

bool NormalCompatible(float3 currN, float3 prevN)
{
    return dot(currN, prevN) > 0.f;
}

// Build a per-pixel reservoir from K unshadowed candidates.
// We only evaluate BRDF and light attenuation (math), not visibility.
Reservoir BuildReservoir(inout uint seed, PBRMaterial mat, float3 viewDir, float3 worldPos)
{
    Reservoir R;
    ReservoirInit(R);

    uint numPL = lightInfo.numPointLight;
    uint numDL = lightInfo.numDirLight;
    uint numLights = numPL + numDL;

    uint candidatesPerPixel = 4;
    float3 c = 0.0f; // "unshadowed contribution estimate" for candidate

    for (int i = 0; i < candidatesPerPixel; i++)
    {
        // Randomly choose a light uniformly (later I could replace it with CDF).
        uint li = (uint) (Rand(seed) * numLights);
        li = min(li, numLights - 1);

        RISSample cand;

        if (li < numDL)
        {
            cand.lightType = 0u;
            cand.lightIndex = li;
        }
        else
        {
            cand.lightType = 1u;
            cand.lightIndex = li - numDL;
        }
        float3 lightDir;
        float tmax;
        c = ShadeChosen(dirLights, pointLights, cand, lightDir, tmax, worldPos, viewDir, mat);

        cand.target = Luminance(max(c, 0.0f));

        // Uniform over lights => pdf = 1 / numLights.
        float pdf = 1.0f / max(1.0f, (float) numLights);

        // Candidate weight is target/pdf (RIS weight).
        // This is the quantity summed into reservoir.W.
        float w = cand.target / max(pdf, 1e-8f);

        // Update reservoir with this single candidate (m=1).
        ReservoirUpdate(R, cand, w, 1u, seed);
    }
    
    
    return R;
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

