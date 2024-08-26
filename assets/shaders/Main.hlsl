static const float sGamma = 1.8;
static const float sInvGamma = 1.0 / sGamma;
static const float sPi = 3.14159265359;

#include "DataStructs.hlsl"

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normals : NORMALS;
    float2 uv : TEXCOORD;
    float3 tangents : TANGENT;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 vertexPos : VERT_POS;
    float4 normals : NORMALS;
    float2 uv : TEXCOORD;
    float3x3 tangentBasis : TANGENT_BASIS;
};

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

cbuffer ModelMatrix : register(b1)
{
    ModelMat modelMats;
};

cbuffer MaterialInfo : register(b2)
{
    MaterialInfo materialInfo;
}

cbuffer LightInfoBuffer : register(b3)
{
    LightInfo lightInfo;
};

SamplerState mainSampler : register(s0);
Texture2D baseColorTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D emissiveTex : register(t2);
Texture2D metallicRoughnessTex : register(t3);
Texture2D occlusionTex : register(t4);
StructuredBuffer<DirLight> dirLights : register(t5);
StructuredBuffer<PointLight> pointLights : register(t6);

float3 LinearToSRGB(float3 color);
float3 LambertianDiffuse(float3 albedo, float3 f0, float3 f90, float vDotH);
float3 FSchlick(float3 F0, float3 F90, float vDotH);
float D_GGX(in float NdotH, in float alphaRoughness);
float V_GGX(in float NdotL, in float NoV, in float alphaRoughness);
void GetBRDF(
    PBRMaterial mat,
    float3 viewDir,
    float3 lightDir,
    float3 lightColor,
    float lightIntensity,
    float attenuation,
    inout float3 diffuse,
    inout float3 specular);
float Attenuation(float distance, float range);
PBRMaterial GenerateMaterial(PS_INPUT input);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.vertexPos = mul(modelMats.mModelMat, float4(input.pos, 1.f));
    output.pos = mul(cameraMats.mCamera, output.vertexPos);
    output.normals = float4(normalize(mul(input.normals.xyz, (float3x3)modelMats.mInvTransposeMat)), 0.f);
    output.uv = input.uv;

    input.tangents = normalize(input.tangents);
    input.tangents = normalize(input.tangents - dot(input.tangents, input.normals) * input.normals);
    float3 bitangent = cross(output.normals.xyz, input.tangents);

    float3x3 TBN = float3x3(input.tangents, bitangent, output.normals.xyz);
    output.tangentBasis = TBN;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    PBRMaterial material = GenerateMaterial(input);
    float3 diffuse = 0.f;
    float3 specular = 0.f;
    float3 viewDirection = normalize(cameraMats.mCameraPos.xyz - input.vertexPos.xyz);

    for (int i = 0; i < lightInfo.numDirLight; i++)
    {
        DirLight light = dirLights[i];
        GetBRDF(material, viewDirection, light.mDir.xyz, light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, 1.f, diffuse, specular);
    }

    for (int j = 0; j < lightInfo.numPointLight; j++)
    {
        PointLight light = pointLights[j];

        float3 lightDirection = light.mPosition.xyz - input.vertexPos.xyz;
        float dist = length(lightDirection);
        lightDirection /= dist;
        float att = Attenuation(dist, light.mRadius);

        GetBRDF(material, viewDirection, lightDirection, light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, att, diffuse, specular);
    }

    GetBRDF(material, viewDirection, viewDirection, lightInfo.ambientLightIntensity.rgb, lightInfo.ambientLightIntensity.a, 1.f, diffuse, specular);

    float3 result = (diffuse + specular) * material.occlusionColor + material.emissiveColor;
    result = LinearToSRGB(result);
    return float4(result, 1.f);
}

PBRMaterial GenerateMaterial(PS_INPUT input)
{
    PBRMaterial mat;
    if (materialInfo.useColorTex)
    {
        mat.baseColor = pow(abs(baseColorTex.Sample(mainSampler, input.uv).rgb), sGamma);
        mat.baseColor *= materialInfo.colorFactor.rgb;
    }
    else
    {
        mat.baseColor = float3(1.0, 1.0, 1.0);
        mat.baseColor *= materialInfo.colorFactor.rgb;
    }

    if (materialInfo.useEmissiveTex)
    {
        mat.emissiveColor = pow(abs(emissiveTex.Sample(mainSampler, input.uv).rgb), sGamma);
        mat.emissiveColor *= materialInfo.emissiveFactor.rgb;
    }
    else
    {
        mat.emissiveColor = float3(0.0, 0.0, 0.0);
    }

    if (materialInfo.useMetallicRoughnessTex)
    {
        float3 metallicRoughnessColor = metallicRoughnessTex.Sample(mainSampler, input.uv).rgb;
        mat.roughness = metallicRoughnessColor.g;
        mat.metallic = metallicRoughnessColor.b;
    }
    else
    {
        mat.occlusionColor = 1.0;
        mat.metallic = materialInfo.metallicFactor;
        mat.roughness = materialInfo.roughnessFactor;
    }

    // Occlusion if it is not in matallic roughness texture
    if (materialInfo.useOcclusionTex)
    {
        mat.occlusionColor = occlusionTex.Sample(mainSampler, input.uv).r;
    }

    if (materialInfo.useNormalTex)
    {
        mat.normalColor = normalTex.Sample(mainSampler, input.uv).rgb;
        mat.normalColor = mat.normalColor * 2.0 - 1.0;
        mat.normalColor = mul(mat.normalColor, input.tangentBasis);
    }
     else
     {
         mat.normalColor = input.normals.xyz;
     }

    mat.F0 = float3(0.04, 0.04, 0.04);
    mat.F0 = lerp(mat.F0, mat.baseColor, mat.metallic);
    mat.diffuse = lerp(mat.baseColor, float3(0.0, 0.0, 0.0), mat.metallic);

    // To alpha roughness
    mat.roughness = mat.roughness * mat.roughness;

    return mat;
}

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 LinearToSRGB(float3 color)
{
    return pow(color, float3(sInvGamma, sInvGamma, sInvGamma));
}

float3 LambertianDiffuse(float3 albedo, float3 f0, float3 f90, float vDotH)
{
    return (1.0 - FSchlick(f0, f90, vDotH)) * (albedo / sPi);
}

float3 FSchlick(float3 F0, float3 F90, float vDotH)
{
    return F0 + (F90 - F0) * pow(clamp(1.0 - vDotH, 0.0, 1.0), 5.0);
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

float Attenuation(float distance, float range)
{
    float distance2 = distance * distance;
    return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / distance2;
}