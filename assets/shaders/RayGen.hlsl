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
//Texture2D<float4> GBufferC : register(t7);
//Texture2D<float> GBufferD : register(t8);

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

uint Hash(uint x);
float Rand(inout uint seed);
uint InitSeed(uint2 pixel);

[shader("raygeneration")]
void RayGen( /*uint3 dispatchThreadID : SV_DispatchThreadID*/)
{  
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;
    float2 uv = (launchIndex + 0.5) / dims;

    float3 directLighting = 0.f;
    float3 indirectLighting = 0.f;

    float3 loadLocation = float3(launchIndex, 0.f);
    uint4 bufferAValue = GBufferA.Load(loadLocation);
    float bufferBValue = GBufferB.Load(loadLocation);
    
    float3 viewPos = ReconstructViewPosFromViewZ(uv, bufferBValue, cameraMats.mInvProjection);
    float3 worldPos = mul(cameraMats.mInvView, float4(viewPos, 1.0f)).xyz;
    float3 viewDirection = normalize(cameraMats.mCameraPos.xyz - worldPos.xyz);
    float3 diffuse = 0.f;
    float3 specular = 0.f;

    PBRMaterial mat;
    UnpackAlbedoMetal(bufferAValue.x, mat.baseColor, mat.metallic);
    mat.normalColor = UnpackNormalOct(bufferAValue.y);
    mat.emissiveColor = UnpackEmissive(bufferAValue.z);
    UnpackRoughOcc(bufferAValue.w, mat.roughness, mat.occlusionColor);
    float3 normalColor = mat.normalColor;
    mat.normalColor = normalize(mat.normalColor * 2.0 - 1.0);

    
    if (!(normalColor.x == 0.f && normalColor.y == 0.f &&
        normalColor.z == 1.f))
    {
        //Shadows
        float3 t = length(cameraMats.mCameraPos.xyz - worldPos.xyz);
        float bias = max(1e-4f, t * 1e-4f);
        uint seed = InitSeed(launchIndex) ^ Hash(pathTracingData.frameIndex * 9781u);
        uint shadowSeed = InitSeed(launchIndex);
        
        mat.F0 = float3(0.04, 0.04, 0.04);
        mat.F0 = lerp(mat.F0, mat.baseColor, mat.metallic);
        mat.diffuse = lerp(mat.baseColor, float3(0.0, 0.0, 0.0), mat.metallic);
        
        for (uint i = 0; i < lightInfo.numDirLight; i++)
        {
            DirLight light = dirLights[i];
            float3 lightDir = normalize(light.mDir.xyz);
            float angularRadius = 0.0047f;
            float coneScale = tan(angularRadius);
        
            float3 T, B;
            CreateCoordinateSystem(lightDir, T, B);
    
            uint samples = pathTracingData.shadowSampleNumber;
            float visible = 0.f;
            for (uint i = 0; i < samples; i++)
            {
                float u1 = Rand(shadowSeed);
                float u2 = Rand(shadowSeed);
                float2 d = UniformSampleHemisphere(u1, u2) * coneScale;
                float3 dir = normalize(lightDir + T * d.x + B * d.y);
            
                RayDesc shadowRay;
                shadowRay.Origin = worldPos.xyz + mat.normalColor * bias; // simpler & correct
                shadowRay.Direction = dir;
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
            
            float3 dirDiffuse = 0.f;
            float3 dirSpecular = 0.f;
            GetBRDF(mat, viewDirection, lightDir, light.mColorAndIntensity.rgb, light.mColorAndIntensity.a * 0.005f, 1.f, dirDiffuse, dirSpecular);
            
            diffuse += dirDiffuse * visible;
            specular += dirSpecular * visible;
        }
        
        for (uint j = 0; j < lightInfo.numPointLight; j++)
        {
            PointLight light = pointLights[j];

            float3 toL = light.mPosition.xyz - worldPos.xyz;
            float dist = length(toL);
            float3 L = toL / max(dist, 1e-6);

            float radius = 4.f; // store per-light if possible
            float att = Attenuation(dist, /*range*/5.f);

            float3 T, B;
            CreateCoordinateSystem(L, T, B);

            uint samples = pathTracingData.shadowSampleNumber;
            float visible = 0.f;

            for (uint i = 0; i < samples; ++i)
            {
    // uniform disk sample
                float u1 = Rand(shadowSeed);
                float u2 = Rand(shadowSeed);
                float r = sqrt(u1);
                float phi = 2.0 * M_PI * u2;
                float2 disk = r * float2(cos(phi), sin(phi));

                float3 lightSamplePos = light.mPosition.xyz + (T * disk.x + B * disk.y) * radius;

                float3 dir = lightSamplePos - worldPos.xyz;
                float distS = length(dir);
                dir /= max(distS, 1e-6);

                RayDesc shadowRay;
                shadowRay.Origin = worldPos.xyz + mat.normalColor * bias;
                shadowRay.Direction = dir;
                shadowRay.TMin = 0;
                shadowRay.TMax = distS - 1e-3f;

                ShadowPayload shadowPayload;
                shadowPayload.hit = 0;

                TraceRay(
                SceneBVH,
                RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                0xFF,
                1, 0, 1,
                shadowRay,
                shadowPayload);

                visible += (shadowPayload.hit == 0) ? 1.0f : 0.0f;
            }

            visible *= (1.0f / samples);

            // shade using center dir (fast approximation) OR per-sample dir (more correct)
            float3 pointDiffuse = 0, pointSpecular = 0;
            GetBRDF(mat, viewDirection, L, light.mColorAndIntensity.rgb,
            light.mColorAndIntensity.a * 0.003f, att, pointDiffuse, pointSpecular);

            diffuse += pointDiffuse * visible;
            specular += pointSpecular * visible;
        }
    
    //Global illumination
        float3 Nt, Nb;
        CreateCoordinateSystem(mat.normalColor, Nt, Nb);
        uint smaples = pathTracingData.GIsampleNumber;

        for (uint n = 0; n < smaples; ++n)
        {
        //How high above the horizon of the hemisphere the line is
            float r1 = Rand(seed);
        //The spin around the axis
            float r2 = Rand(seed);
            float3 s = CosineSampleHemisphere(r1, r2);
            float3 sampleWorld = s.x * Nt + s.y * Nb + s.z * mat.normalColor;
            //float3 sampleWorld = sample.x * Nt + sample.z * Nb + sample.y * mat.normalColor;
        
            RayDesc indirectRay;
            indirectRay.Direction = normalize(sampleWorld);
            indirectRay.Origin = worldPos + mat.normalColor * bias;
            indirectRay.TMin = 0;
            indirectRay.TMax = 100000;
        
            HitInfo indirectPayload;
            indirectPayload.albedoAndRayType.a = 1;
            TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            0,
            0,
            indirectRay,
            indirectPayload);
        
            float nDotWi = saturate(dot(mat.normalColor, sampleWorld));
            float3 Li = indirectPayload.lightIntensityAndDistance.rgb;

            // luminance clamp (example)
            float lum = dot(Li, float3(0.2126, 0.7152, 0.0722));
            float maxLum = 10.0; // tune
            Li *= min(1.0, maxLum / max(lum, 1e-6));

            indirectLighting += Li * mat.baseColor;
        }
        directLighting = (diffuse + specular) * mat.occlusionColor + mat.emissiveColor;
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
    
    
    float3 res = directLighting.rgb + superSampledGI;
    gOutput[launchIndex] = float4(LinearToSRGB(res), 1.f);
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