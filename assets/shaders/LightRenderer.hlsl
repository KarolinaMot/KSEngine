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
void main( uint3 DTid : SV_DispatchThreadID )
{
    float4 finalColor = float4(0.f, 0.f, 0.f, 0.f);
    float2 screenSize;
    FinalRes.GetDimensions(screenSize.x, screenSize.y);

   
    for (int i = 0; i < lightInfo.numPointLight; i++)
    {
        float2 lightScreenPos = GetScreenPosition(float4(pointLights[i].mPosition.xyz, 1.f), screenSize);
        float3 lightRadiusPos = pointLights[i].mPosition + normalize(cameraMats.mCameraRight) * pointLights[i].mRadius;
        float2 lightRadiusScreenPos = GetScreenPosition(float4(lightRadiusPos, 1.f), screenSize);
        float screenSpaceRadius = length(lightRadiusScreenPos - lightScreenPos);
        //float screenSpaceRadius = screenSize.x*0.25f;
        float pixelToLightDistance = length(lightScreenPos - float2(DTid.x, DTid.y));
        if (pixelToLightDistance <= screenSpaceRadius)
        {
             // Smooth falloff for the light's edge.
            float alpha = saturate(1.0 - (pixelToLightDistance / screenSpaceRadius)) * pointLights[i].mColorAndIntensity.a;

            // Blend the edge based on global fog density.
            // Here, we are reducing the light's intensity based on the fog.
            float fogFactor = saturate(1.0 - lightShaftInfo.fogDensity);
            // If you had a per-pixel fog density, you could sample it like this:
            // float fogFactor = saturate(1.0 - FogTexture.Load(int3(pixelCoord, 0)));
            alpha *= fogFactor;
            
            float3 lightColor = pointLights[i].mColorAndIntensity.rgb;
            float3 pixelColor = lightColor * alpha;
            finalColor.rgb += pixelColor;
            finalColor.a = 1.f;

        }
        
        //finalColor = saturate(float3(lightScreenPos, 0.f));

    }
    
    FinalRes[DTid.xy] = finalColor;

}