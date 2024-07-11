struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer Camera : register(b0)
{
    float4x4 mProjection;
    float4x4 mView;
    float4x4 mCamera;
};

cbuffer ModelMatrix : register(b1)
{
    float4x4 mModelMat;
    float4x4 mInvTransposeMat;
};

SamplerState mainSampler : register(s0);
Texture2D baseColorTex : register(t0);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = mul(mModelMat, float4(input.pos, 1.f));
    output.pos = mul(mCamera, output.pos);

    output.uv = input.uv;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 baseColor = baseColorTex.SampleLevel(mainSampler, input.uv, 0);
    float4 res = float4(baseColor.rgb, 1.f);
    return res;
}