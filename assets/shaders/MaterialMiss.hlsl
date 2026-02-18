#include "RTCommon.hlsl"

[shader("miss")]
void MaterialMiss(inout MaterialPayload payload : SV_RayPayload)
{
    payload.bufferA = uint4(0,0,0,0);
}