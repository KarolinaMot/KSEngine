#include "PBR.hlsl"
#include "Structs.hlsl"

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

cbuffer LightInfoBuffer : register(b3)
{
    LightInfo lightInfo;
};

RWTexture2D<float4> GBufferA : register(u0);
RWTexture2D<float4> GBufferB : register(u1);
RWTexture2D<float4> GBufferC : register(u2);
RWTexture2D<float4> GBufferD : register(u3);
RWTexture2D<float4> FinalRes : register(u4);

SamplerState mainSampler : register(s0);

StructuredBuffer<DirLight> dirLights : register(t5);
StructuredBuffer<PointLight> pointLights : register(t6);

float3 LinearToSRGB(float3 color);
float Attenuation(float distance, float range);

[numthreads(8, 8, 1)] void main(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    PBRMaterial mat;
    float3 vertexPos = GBufferA.Load(DispatchThreadID.xy).xyz;
    mat.baseColor = GBufferB.Load(DispatchThreadID.xy).rgb;
    mat.normalColor = GBufferC.Load(DispatchThreadID.xy).rgb;
    mat.emissiveColor = GBufferD.Load(DispatchThreadID.xy).rgb;
    mat.metallic = GBufferB.Load(DispatchThreadID.xy).a;
    mat.roughness = GBufferA.Load(DispatchThreadID.xy).a;
    mat.occlusionColor = GBufferD.Load(DispatchThreadID.xy).a;

    float scalar = mat.normalColor.x + mat.normalColor.y + mat.normalColor.z;
    mat.normalColor = mat.normalColor * 2.0 - 1.0;
    if (scalar != 0)
    {
        mat.F0 = float3(0.04, 0.04, 0.04);
        mat.F0 = lerp(mat.F0, mat.baseColor, mat.metallic);
        mat.diffuse = lerp(mat.baseColor, float3(0.0, 0.0, 0.0), mat.metallic);

        float3 diffuse = 0.f;
        float3 specular = 0.f;
        float3 viewDirection = normalize(cameraMats.mCameraPos.xyz - vertexPos.xyz);

        for (uint i = 0; i < lightInfo.numDirLight; i++)
        {
            DirLight light = dirLights[i];
            GetBRDF(mat, viewDirection, light.mDir.xyz, light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, 1.f, diffuse, specular);
        }

        for (uint j = 0; j < lightInfo.numPointLight; j++)
        {
            PointLight light = pointLights[j];

            float3 lightDirection = light.mPosition.xyz - vertexPos.xyz;
            float dist = length(lightDirection);
            lightDirection /= dist;
            float att = Attenuation(dist, light.mRadius);

            GetBRDF(mat, viewDirection, lightDirection, light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, att, diffuse, specular);
        }

        GetBRDF(mat, viewDirection, viewDirection, lightInfo.ambientLightIntensity.rgb, lightInfo.ambientLightIntensity.a, 1.f, diffuse, specular);

        float3 result = (diffuse + specular) * mat.occlusionColor + mat.emissiveColor;
        result = LinearToSRGB(result);
        FinalRes[DispatchThreadID.xy] = float4(result.rgb, 1.f);
    }
    else
        FinalRes[DispatchThreadID.xy] = float4(0.25f, 0.25f, 0.25f, 1.f);

}

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 LinearToSRGB(float3 color)
{
    return pow(color, float3(sInvGamma, sInvGamma, sInvGamma));
}

float Attenuation(float distance, float range)
{
    float distance2 = distance * distance;
    return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / distance2;
}