#include "Common.hlsl"

TextureCube skyMap : register(t11);
SamplerState mainSampler : register(s0);

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    float3 dir = normalize(WorldRayDirection());
    payload.colorAndDistance = skyMap.SampleLevel(mainSampler, dir, 0.f);
}