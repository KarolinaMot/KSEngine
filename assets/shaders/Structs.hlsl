#ifndef STRUCTS
#define STRUCTS

static const float sGamma = 1.8;
static const float sInvGamma = 1.0 / sGamma;
static const float sPi = 3.14159265359;

struct DirLight
{
    float3 mDir;
    float mAngularRadius;
    float4 mColorAndIntensity;
};

struct PointLight
{
    float4 mPosition;
    float4 mColorAndIntensity;
    float mLinearAttenuation;
    float mQuadraticAttenuation;
    float mConstantAttenuation;
    float mRadius;
};

struct FXAAInfo
{
    uint2 outputSize; // width, height of output (PostAA)
    float2 invOutputSize; // 1/width, 1/height
};

struct PBRMaterial
{
    float4 baseColor;
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
    float4x4 minvCamera;
    float4x4 mCameraNoTranslation;
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
    uint colorTexIndex;
    uint emissiveTexIndex;
    uint metallicRoughnessTexIndex;
    uint normalTexIndex;
    uint occlusionTexIndex;
};

struct InstanceData
{
    ModelMat modelMatrix;
    uint materialIndex;
    uint padsing[3];
 };

struct LightInfo
{
    uint numDirLight;
    uint numPointLight;
    float exposure;
    uint padding;
    float4 ambientLightIntensity;
};

struct PathTracingData
{
    uint frameIndex;
    uint shadowSampleNumber;
    uint GIsampleNumber;
    uint padding;
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

struct BoundingBox
{
    float4 m_center;
    float4 m_extents; 
};

struct Plane
{
    float3 m_normal; //(A, B, C)
    float m_signedOriginDistance;
};

struct CullingInfo
{
    Plane cameraPlane[6];
    uint boundingBoxCount;
    uint3 padding;
};

#endif