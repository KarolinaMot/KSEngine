#pragma once
#include <resources/Material.hpp>
#include <resources/Mesh.hpp>
#include <glm/glm.hpp>

namespace KS
{

enum Subrenderers
{
    DEFERRED_RENDER,
    OCCLUDER_RENDER,
    PBR_RENDER,
    LIGHT_RENDER,
    LIGHT_SHAFT_RENDER,
    UPSCALING_RENDER,
    RT_RENDER,
    NUM_SUBRENDER
};

enum StorageBuffers
{
    MODEL_MAT_BUFFER,
    MATERIAL_INFO_BUFFER,
    DIR_LIGHT_BUFFER,
    POINT_LIGHT_BUFFER,
    NUM_SBUFFER
};

enum UniformBuffers
{
    LIGHT_INFO_BUFFER,
    FOG_INFO_BUFFER,
    MODEL_INDEX_BUFFER,
    //CAMERA_BUFFER,
    NUM_UBUFFER
};

enum VertexDataBuffers
{
    VDS_POSITIONS = 0,
    VDS_NORMALS,
    VDS_UV,
    VDS_TANGENTS
};

enum Formats
{
    R8G8B8A8_UNORM = 0,
    R16G16B16A16_FLOAT,
    R32G32B32A32_FLOAT,
    R32_FLOAT,
    D32_FLOAT,
    R16_FLOAT,
};

struct DrawEntry
{
    ResourceHandle<Mesh> mesh {};
    Material material {};
    int modelIndex;
    glm::mat4x4 modelMat;
};

struct ModelMat
{
    glm::mat4 mModel = glm::mat4x4(1.f);
    glm::mat4 mTransposed = glm::mat4x4(1.f);
};

struct DirLightInfo
{
    glm::vec4 mDir = { 0.f, 0.0f, 0.0f, 0.f };
    glm::vec4 mColorAndIntensity = { 0.f, 0.0f, 0.0f, 0.f };
};

struct PointLightInfo
{
    glm::vec4 mPosition = { 0.f, 0.0f, 0.0f, 0.f };
    glm::vec4 mColorAndIntensity = { 0.f, 0.0f, 0.0f, 0.f };
    float mRadius = 0.f;
    float padding[3];
};

struct LightInfo
{
    uint32_t numDirLights = 0;
    uint32_t numPointLights = 0;
    uint32_t padding[2];
    glm::vec4 mAmbientAndIntensity = glm::vec4(1.f);
};

struct MaterialInfo
{
    glm::vec4 colorFactor = glm::vec4(1.f);
    glm::vec4 emissiveFactor = glm::vec4(1.f);
    float metallicFactor = 1.f;
    float roughnessFactor = 0.f;
    float normalScale = 1.f;
    uint32_t useColorTex = 0;
    uint32_t useEmissiveTex = 0;
    uint32_t useMetallicRoughnessTex = 0;
    uint32_t useNormalTex = 0;
    uint32_t useOcclusionTex = 0;
};

struct FogInfo
{
    glm::vec3 fogColor;
    float fogDensity;
    int lightShaftNumberSamples;
    int sourceMipNumber;
    float exposure;
    float weight;
    float decay;
};

struct CameraMats
{
    glm::mat4x4 m_proj = glm::mat4x4(1.f);
    glm::mat4x4 m_invProj = glm::mat4x4(1.f);
    glm::mat4x4 m_view = glm::mat4x4(1.f);
    glm::mat4x4 m_invView = glm::mat4x4(1.f);
    glm::mat4x4 m_camera = glm::mat4x4(1.f);
    glm::vec4 m_cameraPos = glm::vec4(1.f);
    glm::vec4 m_cameraRight = glm::vec4(0.f);
};
};