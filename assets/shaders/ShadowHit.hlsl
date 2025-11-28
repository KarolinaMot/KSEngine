#include "RTCommon.hlsl"
#include "Structs.hlsl"

[shader("closesthit")]
void ShadowClosestHit(inout ShadowPayload payload, Attributes attrib)
{
    float t = RayTCurrent();

    payload.hit = 1.f;
}

