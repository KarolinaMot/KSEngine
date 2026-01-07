#include "RTCommon.hlsl"

TextureCube skyMap : register(t4);
SamplerState mainSampler : register(s0);

[shader("miss")]
void MainMiss(inout HitInfo payload : SV_RayPayload)
{
    float3 dir = normalize(WorldRayDirection());
    payload.lightIntensityAndDistance.rgb = skyMap.SampleLevel(mainSampler, dir, 0.f).rgb*5;
    payload.lightIntensityAndDistance.a = -1.f;
}