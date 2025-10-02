#include "Common.hlsl"

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
    payload.colorAndDistance = float4(1.f, 0.f, 0.f, RayTCurrent());
}
