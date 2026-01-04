#include "Structs.hlsl"

StructuredBuffer<BoundingBox> boundingBoxes : register(t0);
RWStructuredBuffer<int> drawIndices : register(u0);
    
cbuffer CullingBuffer : register(b0)
{
    CullingInfo cullingInfo;
}

bool FrustumTest(const Plane frustum[6], BoundingBox box);

[numthreads(8, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    if (FrustumTest(cullingInfo.cameraPlane, boundingBoxes[DTid.x]))
    {
        uint index = drawIndices.IncrementCounter();
        drawIndices[index] = DTid.x;
    }
}

bool FrustumTest(const Plane frustum[6], BoundingBox box)
{
    // Using separating axis theorem
    // From https://gist.github.com/Kinwailo/d9a07f98d8511206182e50acda4fbc9b

    float3 boxMin = box.m_center - box.m_extents;
    float3 boxMax = box.m_center + box.m_extents;

    float3 vmin = 0.0f;
    float3 vmax = 0.0f;

    for (int i = 0; i < 6; i++)
    {
        Plane plane = frustum[i];
        float3 planeNormal = plane.m_normal;

        // X axis
        if (planeNormal.x > 0)
        {
            vmin.x = boxMin.x;
            vmax.x = boxMax.x;
        }
        else
        {
            vmin.x = boxMax.x;
            vmax.x = boxMin.x;
        }
        // Y axis
        if (planeNormal.y > 0)
        {
            vmin.y = boxMin.y;
            vmax.y = boxMax.y;
        }
        else
        {
            vmin.y = boxMax.y;
            vmax.y = boxMin.y;
        }
        // Z axis
        if (planeNormal.z > 0)
        {
            vmin.z = boxMin.z;
            vmax.z = boxMax.z;
        }
        else
        {
            vmin.z = boxMax.z;
            vmax.z = boxMin.z;
        }
        float signedDistance = dot(vmax, planeNormal) - plane.m_signedOriginDistance;
        
        if (signedDistance < 0.0f)
            return false;
    }

    return true;
}