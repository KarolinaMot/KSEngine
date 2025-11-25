#include "RTCommon.hlsl"

TextureCube skyMap : register(t5);
SamplerState mainSampler : register(s0);

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    float3 dir = normalize(WorldRayDirection());
    payload.lightIntensityAndDistance = float4(0, 0, 0, 0);;
    payload.lightIntensityAndDistance.a = -1.f;
}