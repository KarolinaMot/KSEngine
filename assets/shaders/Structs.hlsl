#ifndef STRUCTS
#define STRUCTS

static const float sGamma = 1.8;
static const float sInvGamma = 1.0 / sGamma;
static const float sPi = 3.14159265359;

struct DirLight
{
    float4 mDir;
    float4 mColorAndIntensity;
};

struct PointLight
{
    float4 mPosition;
    float4 mColorAndIntensity;
    float mRadius;
    float3 padding;
};

struct PBRMaterial
{
    float3 baseColor;
    float3 emissiveColor;
    float metallic;
    float roughness;
    float3 normalColor;
    float occlusionColor;
    float3 F0;
    float3 diffuse;
};

struct CameraMats
{
    float4x4 mProjection;
    float4x4 mInvProjection;
    float4x4 mView;
    float4x4 mInvView;
    float4x4 mCamera;
    float4 mCameraPos;
    float4 mCameraRight;
};

struct ModelMat
{
    float4x4 mModelMat;
    float4x4 mInvTransposeMat;
};

struct MaterialInfo
{
    float4 colorFactor;
    float4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    uint useColorTex;
    uint useEmissiveTex;
    uint useMetallicRoughnessTex;
    uint useNormalTex;
    uint useOcclusionTex;
};

struct LightInfo
{
    uint numDirLight;
    uint numPointLight;
    uint2 padding1;
    float4 ambientLightIntensity;
};

struct LightShaftInfo
{
    float3 fogColor;
    float fogDensity;
    int lightShaftNumberSamples;
    int sourceMipNumber;
    float exposure;
    float weight;
    float decay;
};
#endif