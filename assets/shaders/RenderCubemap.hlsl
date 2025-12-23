#include "structs.hlsl"

struct VS_INPUT
{
    float3 pos : POSITION;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 vertexPos : VERT_POS;
};

cbuffer Camera : register(b0)
{
    CameraMats cameraMats;
};

cbuffer ModelIndex : register(b1)
{
    int meshIndex;
};

TextureCube skyMap : register(t1);
SamplerState samplr : register(s0);


PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.vertexPos = float4(input.pos, 1.f);
    output.pos = mul(cameraMats.mCameraNoTranslation, output.vertexPos);
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return float4(clamp(skyMap.SampleLevel(samplr, input.vertexPos.xyz, 0.f).rgb, 0.f, 1.f), 1.f);
}