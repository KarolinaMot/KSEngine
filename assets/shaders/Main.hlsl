#include "PBR.hlsl"
#include "Structs.hlsl"

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

cbuffer LightInfoBuffer : register(b2)
{
    LightInfo lightInfo;
};

float2 SampleSphericalMap(float3 v)
{
    const float2 invAtan = float2(0.1591, 0.3183);
    float3x3 rot = float3x3(
    -1, 0, 0,
    0, -1, 0,
    0, 0, 1);
    
    v = mul(v, rot);
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}


Texture2D<float4> LightShafts : register(t0);
RWTexture2D<float4> FinalRes : register(u0);
RWTexture2D<float4> GBufferA : register(u1);
RWTexture2D<float4> GBufferB : register(u2);
RWTexture2D<float4> GBufferC : register(u3);
RWTexture2D<float4> GBufferD : register(u4);
Texture2D<float4> Skydome : register(t1);

SamplerState mainSampler : register(s0);

StructuredBuffer<DirLight> dirLights : register(t5);
StructuredBuffer<PointLight> pointLights : register(t6);

float3 LinearToSRGB(float3 color);
float Attenuation(float distance, float range);
float3 ReconstructDirWS(uint2 pix, float2 size);

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
    //mat.normalColor = normalize(mat.normalColor * 2.0 - 1.0);
    
    float2 screenSize;
    FinalRes.GetDimensions(screenSize.x, screenSize.y);
    float2 UV = DispatchThreadID.xy / screenSize;
    float3 result = float4(0.25f, 0.25f, 0.25f, 1.f);
    float3 diffuse = 0.f;
    float3 specular = 0.f;
    float3 viewDirection = normalize(cameraMats.mCameraPos.xyz - vertexPos.xyz);

    
    if (scalar != 0)
    {
        mat.F0 = float3(0.04, 0.04, 0.04);
        mat.F0 = lerp(mat.F0, mat.baseColor, mat.metallic);
        mat.diffuse = lerp(mat.baseColor, float3(0.0, 0.0, 0.0), mat.metallic);

        for (uint i = 0; i < lightInfo.numDirLight; i++)
        {
            DirLight light = dirLights[i];
            GetBRDF(mat, viewDirection, light.mDir.xyz * float3(1, 1, -1), light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, 1.f, diffuse, specular);
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

        //result = (diffuse + specular) * mat.occlusionColor + mat.emissiveColor;
        //result = LinearToSRGB(result);
        result = diffuse;
    }
    //else
    //{
    //    float3 dirWS = ReconstructDirWS(DispatchThreadID.xy, screenSize); // camera → world
    //    float2 sampleUv = SampleSphericalMap(-viewDirection);
    //    result = Skydome.SampleLevel(mainSampler, sampleUv, 0);

    //}
    
    float4 lightShaftColor = LightShafts.SampleLevel(mainSampler, UV, 0);
    result += lightShaftColor.rgb;
    FinalRes[DispatchThreadID.xy] = float4(result, 1.f);

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

float3 ReconstructDirWS(uint2 pix, float2 size)
{
    // Pixel center -> NDC
    float2 ndc = ((pix + 0.5f) / size) * 2.0f - 1.0f;
    // Note: D3D has y-down in screen space; flip to NDC y-up:
    ndc.y = -ndc.y;

    // View-space ray from inverse projection: (x,y,1) -> unproject -> view dir
    float4 pVS = mul(cameraMats.mInvProjection, float4(ndc.x, ndc.y, 1.0f, 1.0f));
    float3 dirVS = normalize(pVS.xyz / pVS.w);

    // To world (ignore translation)
    float3 dirWS = normalize(mul((float3x3) cameraMats.mInvView, dirVS));
    return dirWS;
}