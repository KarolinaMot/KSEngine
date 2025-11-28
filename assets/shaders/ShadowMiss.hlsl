#include "RTCommon.hlsl"

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload : SV_RayPayload)
{
    payload.hit = 0.f;
}