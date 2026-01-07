#include "Structs.hlsl"

SamplerState mainSampler : register(s0);
Texture2D<float4> SourceTex : register(t0);
RWTexture2D<float4> FinalRes : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float2 texDimentions;
    FinalRes.GetDimensions(texDimentions.x, texDimentions.y);
    float2 UV = DTid.xy;
    UV /= texDimentions;
    
    // You can use a sampling function for bilinear interpolation.
    float4 color = SourceTex.SampleLevel(mainSampler, UV, 0);
    
    FinalRes[DTid.xy] = color;
}