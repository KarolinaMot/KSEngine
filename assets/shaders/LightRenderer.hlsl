#include "Structs.hlsl"

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

cbuffer LightRenderingBuffer : register(b1)
{
    float3 fogColor;
    float fogDensity;
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
    float4 clipPos = mul(worldPosition, cameraMats.mCamera);

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
    float3 finalColor = float3(0.f, 0.f, 0.f);
    float2 screenSize;
    FinalRes.GetDimensions(screenSize.x, screenSize.y);

    // Compute UV coordinates for the current pixel
    float2 uv = (float2(DTid.x, DTid.y) + 0.5) / screenSize;
    
    for (int i = 0; i < lightInfo.numPointLight; i++)
    {
        float2 lightScreenPos = GetScreenPosition(pointLights[i].mPosition, screenSize);
        float3 lightRadiusPos = pointLights[i].mPosition + cameraMats.mCameraRight * pointLights[i].mRadius;
        float screenSpaceRadius = length(lightScreenPos - GetScreenPosition(float4(lightRadiusPos*fogDensity, 1.f), screenSize));
        float pixelToLight = length(lightScreenPos - uv);
        if (pixelToLight<=screenSpaceRadius)
        {
            float3 lightColor = pointLights[i].mColorAndIntensity.rgb * pointLights[i].mColorAndIntensity.a;
            float3 pixelColor = lerp(float3(0.f, 0.f, 0.f), lightColor, screenSpaceRadius - pixelToLight);
            finalColor += pixelColor;
        }
        
    }
    
    FinalRes[DTid.xy] = float4(finalColor, 1.f);

}