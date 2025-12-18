#include "RTCommon.hlsl"

TextureCube skyMap : register(t5);
SamplerState mainSampler : register(s0);

[shader("miss")]
void MainMiss(inout HitInfo payload : SV_RayPayload)
{
    float3 dir = normalize(WorldRayDirection());
    payload.lightIntensityAndDistance.rgb = skyMap.SampleLevel(mainSampler, dir, 0.f).rgb*2;
   // payload.lightIntensityAndDistance.rgb = 10.f;
    //payload.lightIntensityAndDistance.rgb = 1.f;
    payload.lightIntensityAndDistance.a = -1.f;
}