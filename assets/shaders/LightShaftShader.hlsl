#include "Structs.hlsl"

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

cbuffer LightRenderingBuffer : register(b1)
{
    LightShaftInfo lightShaftInfo;
};

cbuffer LightInfoBuffer : register(b2)
{
    LightInfo lightInfo;
};

StructuredBuffer<PointLight> pointLights : register(t6);

SamplerState mainSampler : register(s0);
Texture2D<float4> SourceTex : register(t0);
RWTexture2D<float4> FinalRes : register(u0);

float2 GetScreenPosition(float4 worldPosition, float2 screenSize)
{
    // Transform to Clip Space
    float4 clipPos = mul(cameraMats.mCamera, worldPosition);

    // Perspective Divide (get Normalized Device Coordinates)
    float3 ndcPos = clipPos.xyz / clipPos.w; // ndcPos now ranges from -1 to +1

    // Convert to screen coordinates, given screen dimensions (width, height)
    float screenX = ((ndcPos.x + 1.0) * 0.5) * screenSize.x;
    float screenY = ((1.0 - ndcPos.y) * 0.5) * screenSize.y;
    
    return float2(screenX, screenY);
}


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float2 texDimentions;
    FinalRes.GetDimensions(texDimentions.x, texDimentions.y);
    float2 UV = DTid.xy / texDimentions;
    float4 result = float4(0.f, 0.f, 0.f, 1.f);

    for (int i = 0; i < lightInfo.numPointLight; i++)
    {
        float2 sampleUV = UV;
        float2 lightScreenPosition = GetScreenPosition(pointLights[i].mPosition, texDimentions) / texDimentions;
        float2 DeltaTexCoord = (UV.xy - lightScreenPosition);

        float Len = length(DeltaTexCoord);

        DeltaTexCoord *= 1.0 / lightShaftInfo.lightShaftNumberSamples * lightShaftInfo.fogDensity;

        float3 Color = SourceTex.SampleLevel(mainSampler, sampleUV, lightShaftInfo.sourceMipNumber).rgb;

        float IlluminationDecay = 1.0;

        for (int i = 0; i < lightShaftInfo.lightShaftNumberSamples; ++i)
        {

            sampleUV -= DeltaTexCoord;

            float3 Sample = SourceTex.SampleLevel(mainSampler, sampleUV, lightShaftInfo.sourceMipNumber).rgb;

            Sample *= IlluminationDecay * lightShaftInfo.weight;

            Color += Sample;

            IlluminationDecay *= lightShaftInfo.decay;

        }

        result.rgb += Color;

    }
    result.rgb *= lightShaftInfo.exposure;
    
    FinalRes[DTid.xy] = result;
}