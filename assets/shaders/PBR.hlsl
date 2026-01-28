#ifndef PBR
#define PBR

#include "Structs.hlsl"

float3 FSchlick(float3 F0, float3 F90, float vDotH)
{
    return F0 + (F90 - F0) * pow(clamp(1.0 - vDotH, 0.0, 1.0), 5.0);
}

float3 LambertianDiffuse(float3 albedo, float3 f0, float3 f90, float vDotH)
{
    return (1.0 - FSchlick(f0, f90, vDotH)) * (albedo / sPi);
}

float D_GGX(in float NdotH, in float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;

    return alphaRoughnessSq / (sPi * f * f);
}

float V_GGX(in float NdotL, in float NoV, in float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NoV * NoV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NoV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }

    return 0.0;
}

void GetBRDF(
    PBRMaterial mat,
    float3 viewDir,
    float3 lightDir,
    float3 lightColor,
    float lightIntensity,
    float attenuation,
    inout float3 diffuse,
    inout float3 specular)
{
    float3 halfAngle = normalize(viewDir + lightDir);
    float nDotL = clamp(dot(mat.normalColor, lightDir), 0.0, 1.0);
    float nDotH = clamp(dot(mat.normalColor, halfAngle), 0.0, 1.0);
    float nDotV = clamp(dot(mat.normalColor, viewDir), 0.0, 1.0);
    float vDotH = clamp(dot(viewDir, halfAngle), 0.0, 1.0);

    float3 colorIntensity = lightColor * lightIntensity;
    colorIntensity *= attenuation;

    float3 diffuseBRDF = LambertianDiffuse(mat.diffuse, mat.F0, float3(1.0, 1.0, 1.0), vDotH);

    float3 F = FSchlick(mat.F0, float3(1.0, 1.0, 1.0), vDotH);
    float3 G = V_GGX(nDotL, nDotV, mat.roughness);
    float3 D = D_GGX(nDotH, mat.roughness);
    float3 specularBRDF = F * G * D;

    diffuse += colorIntensity * nDotL * diffuseBRDF;
    specular += colorIntensity * nDotL * specularBRDF;
}

float3 ReconstructWorldPos(uint2 pixelCoord,
                           uint2 screenSize,
                           float4x4 invViewProj,
                           float depth)
{
    float2 uv = (pixelCoord + 0.5) / screenSize;

    // D3D: depth already in [0,1] NDC
    float4 ndc;
    ndc.x = uv.x * 2.0f - 1.0f;
    ndc.y = (1.0f - uv.y) * 2.0f - 1.0f; // flip Y
    ndc.z = depth;
    ndc.w = 1.0f;

    // NDC -> world
    float4 worldPos = mul(invViewProj, ndc); // vector * matrix (see CPU side below)
    worldPos /= worldPos.w;
    return worldPos;

}

float3 ReconstructViewPosFromViewZ(float2 uv, float viewZ, float4x4 invProj)
{
    // uv in [0..1]
    float2 ndc;
    ndc.x = uv.x * 2.0f - 1.0f;
    ndc.y = (1.0f - uv.y) * 2.0f - 1.0f; // flip Y if your UV origin is top-left

    // Unproject a point on the far plane (z=1 in D3D clip space)
    float4 clip = float4(ndc, 1.0f, 1.0f);
    float4 viewH = mul(invProj, clip);
    float3 viewFar = viewH.xyz / viewH.w; // view-space point on far plane

    float3 dirVS = normalize(viewFar); // ray dir from camera in view space

    // We stored viewZ = -posVS.z  => posVS.z = -viewZ
    // Scale so that the resulting position has z = -viewZ
    float t = (-viewZ) / dirVS.z;
    return dirVS * t;
}

float Attenuation(float distance, float range)
{
    float distance2 = distance * distance;
    return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / distance2;
}

float DistanceAttenuation(float3 Lvec, float kc, float kl, float kq, float radius)
{
    // Lvec = lightPos - P
    float d2 = dot(Lvec, Lvec);
    float d = sqrt(d2);

    // Assimp model: I(d) = I0 / (kc + kl*d + kq*d^2)
    float denom = kc + kl * d + kq * d2;

    // Guard against divide-by-zero; if everything is 0 treat as no falloff.
    float att = (kc == 0 && kl == 0 && kq == 0) ? 1.0 : rcp(max(denom, 1e-6));

    // Optional cutoff by radius (hard or smooth)
    if (radius > 0)
    {
        // Hard cutoff:
        att *= step(d, radius);

        // Or smooth edge (comment hard cutoff and use this instead):
        // float t = saturate(1.0 - d / radius);
        // att *= t * t; // or t^4 for softer edge
    }

    return att;
}

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 LinearToSRGB(float3 color)
{
    return pow(clamp(color, 0.f, 1.f), float3(sInvGamma, sInvGamma, sInvGamma));
}

float3 DirToFaceUV(float3 dir)
{
    dir = normalize(dir);
    float ax = abs(dir.x), ay = abs(dir.y), az = abs(dir.z);
    int face;
    float2 uv;
    
    float2 p; // in [-1, 1]
    float inv; // 1 / dominant component magnitude

    if (ax >= ay && ax >= az)
    {
        // ±X is dominant
        if (dir.x > 0.0f)
        {
            face = 0;
            inv = 1.0f / ax;
            p = float2(-dir.z, -dir.y) * inv;
        } // +X
        else
        {
            face = 1;
            inv = 1.0f / ax;
            p = float2(dir.z, -dir.y) * inv;
        } // -X
    }
    else if (ay >= ax && ay >= az)
    {
        // ±Y is dominant
        if (dir.y > 0.0f)
        {
            face = 2;
            inv = 1.0f / ay;
            p = float2(dir.x, dir.z) * inv;
        } // +Y
        else
        {
            face = 3;
            inv = 1.0f / ay;
            p = float2(dir.x, -dir.z) * inv;
        } // -Y
    }
    else
    {
        // ±Z is dominant
        if (dir.z > 0.0f)
        {
            face = 4;
            inv = 1.0f / az;
            p = float2(dir.x, -dir.y) * inv;
        } // +Z
        else
        {
            face = 5;
            inv = 1.0f / az;
            p = float2(-dir.x, -dir.y) * inv;
        } // -Z
    }

    // Map from [-1,1] to [0,1]; nudge to avoid sampling exactly on 1.0
    const float eps = 1e-7f;
    uv = saturate(p * 0.5f + 0.5f);
    uv = clamp(uv, eps, 1.0f - eps);
    return float3(uv, face);
}

float3 UnpackEmissive(uint p)
{
    uint r = (p) & 0x1FF;
    uint g = (p >> 9) & 0x1FF;
    uint b = (p >> 18) & 0x1FF;
    uint e = (p >> 27) & 0x1F;

    if (e == 0 && r == 0 && g == 0 && b == 0)
        return 0.0.xxx;

    // exponent is biased by 15
    int expShared = (int) e - 15;

    // Our pack used: mantissa ≈ round(c / 2^(expShared-9))
    // => c ≈ mantissa * 2^(expShared-9)
    float scale = exp2((float) expShared - 9.0);

    return float3((float) r, (float) g, (float) b) * scale;
}

float UnpackUNorm16(uint v16)
{
    return (float) (v16 & 0xFFFF) / 65535.0;
}

void UnpackRoughOcc(uint packed, out float roughness, out float occlusion, out float alpha)
{
    uint r16 = packed & 0xFFFF;
    uint o8 = (packed >> 16) & 0xFF;

    roughness = UnpackUNorm16(r16);
    occlusion = (float) o8 / 255.0;
    alpha = (packed >> 24) & 0xFF;
}

float2 UnpackSnorm16x2(uint p)
{
    int sx = (int) (p << 16) >> 16; // sign-extend low 16
    int sy = (int) p >> 16; // sign-extend high 16
    return float2(sx, sy) / 32767.0;
}

float3 OctDecode(float2 e)
{
    float3 n = float3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0)
    {
        float2 t = (1.0 - abs(n.yx)) * (float2(n.x >= 0 ? 1 : -1, n.y >= 0 ? 1 : -1));
        n.x = t.x;
        n.y = t.y;
    }
    return normalize(n);
}

float3 UnpackNormalOct(uint packed)
{
    float2 e = UnpackSnorm16x2(packed);
    return OctDecode(e);
}

float3 UnpackRGB8(uint packed)
{
    float r = (float) (packed & 0xFF) / 255.0;
    float g = (float) ((packed >> 8) & 0xFF) / 255.0;
    float b = (float) ((packed >> 16) & 0xFF) / 255.0;
    return float3(r, g, b);
}

float UnpackUNorm8(uint packed8)
{
    return (float) (packed8 & 0xFF) / 255.0;
}

void UnpackAlbedoMetal(uint packed, out float3 albedo, out float metallic)
{
    albedo = UnpackRGB8(packed);
    metallic = UnpackUNorm8(packed >> 24);
}

uint PackUNorm8(float v)
{
    return (uint) round(saturate(v) * 255.0);
}

uint PackRGB8(float3 c)
{
    uint r = PackUNorm8(c.x);
    uint g = PackUNorm8(c.y);
    uint b = PackUNorm8(c.z);
    return (r) | (g << 8) | (b << 16);
}

// Albedo RGB888 + Metallic A8  => 0xMMBBGGRR
uint PackAlbedoMetal(float3 albedo, float metallic)
{
    return PackRGB8(albedo) | (PackUNorm8(metallic) << 24);
}

// Octahedral normal encoding [-1,1] -> pack snorm16x2
float2 OctEncode(float3 n)
{
    n = normalize(n);
    n /= (abs(n.x) + abs(n.y) + abs(n.z) + 1e-8);

    float2 e = n.xy;
    if (n.z < 0.0)
        e = (1.0 - abs(e.yx)) * (float2(e.x >= 0 ? 1 : -1, e.y >= 0 ? 1 : -1));
    return e;
}

uint PackSnorm16x2(float2 v)
{
    // SNORM16: [-1,1] mapped to [-32767,32767]
    v = clamp(v, -1.0, 1.0);
    int2 s = (int2) round(v * 32767.0);

    uint lo = (uint) (s.x & 0xFFFF);
    uint hi = (uint) (s.y & 0xFFFF) << 16;
    return lo | hi;
}

uint PackNormalOct(float3 n)
{
    float2 e = OctEncode(n); // in [-1, 1]
    return PackSnorm16x2(e); // returns uint (DXC / SM6)
}

// RGB9E5 (shared exponent) 32-bit packing
uint PackRGB9E5(float3 c)
{
    c = max(c, 0.0.xxx);
    float maxc = max(c.x, max(c.y, c.z));
    if (maxc < 1.5258789e-5)
        return 0; // ~2^-16

    // shared exponent in base-2, biased by 15 (5 bits)
    int expShared = (int) floor(log2(maxc)) + 1;
    expShared = clamp(expShared, -16, 15);

    // mantissa is 9 bits => scale so that mantissa = round(c / 2^(expShared-9))
    float denom = exp2((float) expShared - 9.0);

    uint r = (uint) min(511.0, round(c.x / denom));
    uint g = (uint) min(511.0, round(c.y / denom));
    uint b = (uint) min(511.0, round(c.z / denom));
    uint e = (uint) (expShared + 15);

    // bits: [31:27]=E, [26:18]=B, [17:9]=G, [8:0]=R
    return (e << 27) | (b << 18) | (g << 9) | (r);
}

uint PackRoughOcc(float roughness, float occlusion, uint albedoAlpha)
{
    uint r16 = (uint) round(saturate(roughness) * 65535.0); // 16-bit
    uint o8 = PackUNorm8(occlusion); // 8-bit

    return (r16) | (o8 << 16) | (albedoAlpha << 24);
}

PBRMaterial LoadMaterialFromGBuffer(Texture2D<uint4> GBufferA, uint2 loadLocation, out bool emptyPixel)
{
    uint4 bufferAValue = GBufferA.Load(uint3(loadLocation, 0));

    PBRMaterial mat = (PBRMaterial) 0;
    UnpackAlbedoMetal(bufferAValue.x, mat.baseColor.rgb, mat.metallic);
    mat.normalColor = UnpackNormalOct(bufferAValue.y);
    mat.emissiveColor = UnpackEmissive(bufferAValue.z);
    UnpackRoughOcc(bufferAValue.w, mat.roughness, mat.occlusionColor, mat.baseColor.a);
    float3 normalColor = (mat.normalColor + 1.f) * 0.5f;
    mat.baseColor.rgb *= mat.baseColor.a;
    mat.F0 = float3(0.04, 0.04, 0.04);
    mat.F0 = lerp(mat.F0, mat.baseColor.rgb, mat.metallic);
    mat.diffuse = lerp(mat.baseColor.rgb, float3(0.0, 0.0, 0.0), mat.metallic) * mat.baseColor.a;
    emptyPixel = (normalColor.x == 0.f && normalColor.y == 0.f && normalColor.z == 1.f);
    return mat;
}


#endif