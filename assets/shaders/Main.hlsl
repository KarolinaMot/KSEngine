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


RWTexture2D<float4> FinalRes : register(u0);
RWTexture2D<float4> GBufferA : register(u1);
RWTexture2D<float4> GBufferB : register(u2);
RWTexture2D<float4> GBufferC : register(u3);
Texture2D<float> Depth : register(t3);

SamplerState mainSampler : register(s0);

StructuredBuffer<DirLight> dirLights : register(t0);
StructuredBuffer<PointLight> pointLights : register(t4);
Texture2D<float4> LightShafts : register(t2);

float3 GetDirFromScreen(uint2 pixel, float2 size, float4x4 invProj, float4x4 invView);

[numthreads(8, 8, 1)] void main(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    float2 screenSize;
    FinalRes.GetDimensions(screenSize.x, screenSize.y);

    float2 UV = DispatchThreadID.xy / screenSize;
    float depth = Depth.SampleLevel(mainSampler, UV, 0).r;

    PBRMaterial mat;

    float3 vertexPos = ReconstructWorldPos(DispatchThreadID.xy, screenSize, cameraMats.minvCamera, depth);
    mat.baseColor = GBufferA.Load(DispatchThreadID.xy).rgb;
    mat.normalColor = GBufferB.Load(DispatchThreadID.xy).rgb;
    mat.emissiveColor = GBufferC.Load(DispatchThreadID.xy).rgb;
    mat.metallic = GBufferA.Load(DispatchThreadID.xy).a;
    mat.roughness = GBufferB.Load(DispatchThreadID.xy).a;
    mat.occlusionColor = GBufferC.Load(DispatchThreadID.xy).a;

    float scalar = mat.normalColor.x + mat.normalColor.y + mat.normalColor.z;
    //mat.normalColor = normalize(mat.normalColor * 2.0 - 1.0);
    
    float4 result = float4(0.25f, 0.25f, 0.25f, 0.f);
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
            GetBRDF(mat, viewDirection, normalize(light.mDir.xyz * float3(1, 1, -1)), light.mColorAndIntensity.rgb, light.mColorAndIntensity.a * 0.005f, 1.f, diffuse, specular);
        }

        for (uint j = 0; j < lightInfo.numPointLight; j++)
        {
            PointLight light = pointLights[j];

            float3 lightDirection = light.mPosition.xyz - vertexPos.xyz;
            float dist = length(lightDirection);
            lightDirection /= dist;
            float att = DistanceAttenuation(lightDirection, light.mConstantAttenuation, light.mLinearAttenuation, light.mQuadraticAttenuation, 20.f);

            GetBRDF(mat, viewDirection, lightDirection, light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, att, diffuse, specular);
        }

        GetBRDF(mat, viewDirection, viewDirection, lightInfo.ambientLightIntensity.rgb, lightInfo.ambientLightIntensity.a * 0.005f, 1.f, diffuse, specular);

        result.rgb = (diffuse + specular) * mat.occlusionColor + mat.emissiveColor;
        result.a = 1.f;
        FinalRes[DispatchThreadID.xy] = result;
    }
    

}