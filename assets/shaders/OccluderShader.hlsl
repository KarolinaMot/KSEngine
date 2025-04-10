#include "Structs.hlsl"

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

PBRMaterial GenerateMaterial(PS_INPUT input);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.vertexPos = mul(modelMats[meshIndex].mModelMat, float4(input.pos, 1.f));
    output.pos = mul(cameraMats.mCamera, output.vertexPos);
    return output;
}

float4 mainPS(PS_INPUT input)
    : SV_TARGET
{

    return float4(0.f, 0.f, 0.f, 0.f);
}

