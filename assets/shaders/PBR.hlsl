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

    //float3 F = FSchlick(mat.F0, float3(1.0, 1.0, 1.0), vDotH);
    //float3 G = V_GGX(nDotL, nDotV, mat.roughness);
    //float3 D = D_GGX(nDotH, mat.roughness);
    //float3 specularBRDF = F * G * D;

    diffuse += colorIntensity * nDotL * diffuseBRDF;
    //specular += colorIntensity * nDotL * specularBRDF;
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
#endif