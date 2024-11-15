#include "Structs.hlsl"

ConstantBuffer<CameraMats> camera : register(b0);
ConstantBuffer<ModelMat> model_matrix : register(b1);

struct VS_INPUT
{
    float3 pos : POSITION;
};

struct PS_INPUT
{
    float4 pos : SV_Position;
};

struct PSOutput
{
    float4 albedo : SV_Target0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    float4 world_space = mul(model_matrix.mModelMat, float4(input.pos, 1.f));
    output.pos = mul(camera.mCamera, world_space);

    return output;
}

PSOutput mainPS(PS_INPUT input)
{
    PSOutput output;
    output.albedo = float4(1.0f, 1.0f, 1.0f, 1.0f);
    return output;
}