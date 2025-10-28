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

StructuredBuffer<ModelMat> modelMats : register(t7);

TextureCube skyMap : register(t5);
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
    return float4(skyMap.SampleLevel(samplr, input.vertexPos.xyz, 0.f).rgb, 1.f);
}