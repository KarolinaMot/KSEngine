#include "RTCommon.hlsl"
#include "Structs.hlsl"

[shader("anyhit")]
void ShadowAnyHit(inout ShadowPayload payload, Attributes attrib)
{
    float t = RayTCurrent();

    payload.hit = 1.f;
    AcceptHitAndEndSearch();
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowPayload payload, Attributes attrib)
{
    float t = RayTCurrent();

    payload.hit = 1.f;

}
