
Texture2D<float4> OriginalTex : register(t0);
RWTexture2DArray<float4> FinalRes : register(u0);
SamplerState mainSampler : register(s0);

float2 SampleSphericalMap(float3 v)
{
    const float2 invAtan = float2(0.1591, 0.3183);
    float3x3 rot = float3x3(
    -1, 0, 0,
    0, -1, 0,
    0, 0, 1);
    
    v = mul(v, rot);
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID)
{
    
    uint3 dimensions;
    FinalRes.GetDimensions(dimensions.x, dimensions.y, dimensions.z);
    
    uint2 pix = DTid.xy;
    if (pix.x >= dimensions.x || pix.y >= dimensions.y)
        return;

    // map pixel to NDC in [-1,1]
    float2 uv = (float2(pix) + 0.5) / dimensions.xy * 2.0 - 1.0;
    uv.y *= -1;

    // get face index via root constants / dispatch loop
    uint face = DTid.z;

    // build direction per cube face
    float3 dir;
    if (face == 0)
        dir = normalize(float3(1, uv.y, -uv.x)); // +X
    if (face == 1)
        dir = normalize(float3(-1, uv.y, uv.x)); // -X
    if (face == 2)
        dir = normalize(float3(uv.x, 1, -uv.y)); // +Y
    if (face == 3)
        dir = normalize(float3(uv.x, -1, uv.y)); // -Y
    if (face == 4)
        dir = normalize(float3(uv.x, uv.y, 1)); // +Z
    if (face == 5)
        dir = normalize(float3(-uv.x, uv.y, -1)); // -Z
    
    float2 sampleUv = SampleSphericalMap(dir);
    
    float3 res = OriginalTex.SampleLevel(mainSampler, sampleUv, 0);
    FinalRes[DTid] = float4(res, 1.f);
}