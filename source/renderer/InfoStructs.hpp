#pragma once
#include <resources/Material.hpp>
#include <resources/Mesh.hpp>
#include <glm/glm.hpp>

namespace KS
{
enum ResourceBuffers
{
    MODEL_MAT_BUFFER,
    MATERIAL_INFO_BUFFER,
    DIR_LIGHT_BUFFER,
    POINT_LIGHT_BUFFER,
    LIGHT_INFO_BUFFER,
    NUM_BUFFERS
};

enum VertexDataBuffers
{
    VDS_POSITIONS = 0,
    VDS_NORMALS,
    VDS_UV,
    VDS_TANGENTS
};

struct DrawEntry
{
    glm::mat4 transform {};
    ResourceHandle<Mesh> mesh {};
    Material material {};
};

struct ModelMat
{
    glm::mat4 mModel;
    glm::mat4 mTransposed;
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
    glm::vec4 colorFactor;
    glm::vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    uint32_t useColorTex;
    uint32_t useEmissiveTex;
    uint32_t useMetallicRoughnessTex;
    uint32_t useNormalTex;
    uint32_t useOcclusionTex;
};
};