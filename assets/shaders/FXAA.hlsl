cbuffer FXAACB : register(b0)
{
    uint2 outputSize; // width, height of output (PostAA)
    float2 invOutputSize; // 1/width, 1/height
};

Texture2D<float4> gInput : register(t0);
SamplerState gLinearClamp : register(s0);
RWTexture2D<float4> gOutput : register(u0);

float Luma(float3 c)
{
    return dot(c, float3(0.299, 0.587, 0.114));
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= outputSize.x || tid.y >= outputSize.y)
        return;

    // UV at pixel center in output space
    float2 uv = (float2(tid.xy) + 0.5) * invOutputSize;

    float3 rgbM = gInput.SampleLevel(gLinearClamp, uv, 0).rgb;
    float3 rgbN = gInput.SampleLevel(gLinearClamp, uv + float2(0, -invOutputSize.y), 0).rgb;
    float3 rgbS = gInput.SampleLevel(gLinearClamp, uv + float2(0, invOutputSize.y), 0).rgb;
    float3 rgbW = gInput.SampleLevel(gLinearClamp, uv + float2(-invOutputSize.x, 0), 0).rgb;
    float3 rgbE = gInput.SampleLevel(gLinearClamp, uv + float2(invOutputSize.x, 0), 0).rgb;

    float lumaM = Luma(rgbM);
    float lumaN = Luma(rgbN);
    float lumaS = Luma(rgbS);
    float lumaW = Luma(rgbW);
    float lumaE = Luma(rgbE);

    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float lumaRange = lumaMax - lumaMin;

    // Edge threshold tweak
    if (lumaRange < 0.015)
    {
        gOutput[tid.xy] = float4(rgbM, 1);
        return;
    }

    // Direction estimate
    float2 dir;
    dir.x = -((lumaN + lumaS) - 2.0 * lumaM);
    dir.y = ((lumaW + lumaE) - 2.0 * lumaM);

    float dirReduce = max((lumaN + lumaS + lumaW + lumaE) * (0.25 * 0.5), 1e-6);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    // Clamp step to avoid overshooting
    dir = saturate(dir * rcpDirMin) * float2(invOutputSize.x, invOutputSize.y);

    float3 rgbA = 0.5 * (
        gInput.SampleLevel(gLinearClamp, uv + dir * (1.0 / 3.0 - 0.5), 0).rgb +
        gInput.SampleLevel(gLinearClamp, uv + dir * (2.0 / 3.0 - 0.5), 0).rgb);

    float3 rgbB = rgbA * 0.5 + 0.25 * (
        gInput.SampleLevel(gLinearClamp, uv + dir * (-0.5), 0).rgb +
        gInput.SampleLevel(gLinearClamp, uv + dir * (0.5), 0).rgb);

    float lumaB = Luma(rgbB);
    float3 outRgb = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;

    gOutput[tid.xy] = float4(outRgb, 1);
}