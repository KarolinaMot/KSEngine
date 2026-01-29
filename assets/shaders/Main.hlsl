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
Texture2D<uint4> GBufferA : register(t0);
Texture2D<float> GBufferB : register(t1);

SamplerState mainSampler : register(s0);

StructuredBuffer<DirLight> dirLights : register(t2);
StructuredBuffer<PointLight> pointLights : register(t3);

[numthreads(8, 8, 1)] void main(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    float2 screenSize;
    FinalRes.GetDimensions(screenSize.x, screenSize.y);

    float2 UV = DispatchThreadID.xy / screenSize;

    float3 loadLocation = float3(DispatchThreadID.xy, 0.f);
    uint4 bufferAValue = GBufferA.Load(loadLocation);
    float bufferBValue = GBufferB.Load(loadLocation);
    
    float3 viewPos = ReconstructViewPosFromViewZ(UV, bufferBValue, cameraMats.mInvProjection);
    float3 worldPos = mul(cameraMats.mInvView, float4(viewPos, 1.0f)).xyz;
    
    float4 result = float4(0.25f, 0.25f, 0.25f, 0.f);
    float3 diffuse = 0.f;
    float3 specular = 0.f;
    float3 viewDirection = normalize(cameraMats.mCameraPos.xyz - worldPos.xyz);
    
    bool emptyPixel = (bufferBValue >= 0.9999f);
    PBRMaterial mat = LoadMaterialFromGBuffer(GBufferA, DispatchThreadID.xy);

    if (!emptyPixel)
    {

        for (uint i = 0; i < lightInfo.numDirLight; i++)
        {
            DirLight light = dirLights[i];
            GetBRDF(mat, viewDirection, normalize(light.mDir.xyz * float3(1, 1, -1)), light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, 1.f, diffuse, specular);
        }

        for (uint j = 0; j < lightInfo.numPointLight; j++)
        {
            PointLight light = pointLights[j];

            float3 lightDirection = light.mPosition.xyz - worldPos.xyz;
            float dist = length(lightDirection);
            lightDirection /= dist;
            float att = Attenuation(dist, 5.f);

            GetBRDF(mat, viewDirection, lightDirection, light.mColorAndIntensity.rgb, light.mColorAndIntensity.a, att, diffuse, specular);
        }

        GetBRDF(mat, viewDirection, viewDirection, lightInfo.ambientLightIntensity.rgb, lightInfo.ambientLightIntensity.a, 1.f, diffuse, specular);

        result.rgb = (diffuse + specular) * mat.occlusionColor + mat.emissiveColor;
        
        result.rgb *= lightInfo.exposure; // e.g. exposure = 1.0 .. 2.0 (or make it a slider)
        result.rgb = ToneMapReinhard(result.rgb);
        
        result.rgb = LinearToSRGB(result.rgb);
        result.a = 1.f;
        FinalRes[DispatchThreadID.xy] = result;
    }
    

}

