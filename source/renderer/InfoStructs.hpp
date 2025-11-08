#pragma once
#include <resources/Material.hpp>
#include <resources/Mesh.hpp>
#pragma warning(push, 0)
#include <glm/glm.hpp>
#pragma warning(pop)

#ifndef MAX_MESHES
#define MAX_MESHES 500
#endif  // !MAX_MESHES

#ifndef NUM_DRAW_THREAD
#define NUM_DRAW_THREAD 4
#endif  // !NUM_DRAW_THREAD

#define RAYTRACE_RT_SLOT 0
#define BVH_SLOT 2
#define NORMALS_SLOT 4
#define INDICES_SLOT NORMALS_SLOT + MAX_MESHES
#define VPOS_SLOT INDICES_SLOT + MAX_MESHES
#define UVS_SLOT VPOS_SLOT + MAX_MESHES
#define TAN_SLOT UVS_SLOT + MAX_MESHES
#define OTHER_RESOURCES_START TAN_SLOT + MAX_MESHES
#define NUM_OF_TEXTURES 400
#define IMGUI_START OTHER_RESOURCES_START + NUM_OF_TEXTURES

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
    CUBEMAP_GEN,
    CUBEMAP_RENDER,
    MIP_GEN,
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
    MIP_GEN_INFO,
    CAMERA_MAT_BUFFER,
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
    std::shared_ptr<Mesh> mesh{};
    Material material{};
    int modelIndex;
    glm::mat4x4 modelMat;
    uint32_t tlasHandle;
};

struct GenerateMipsInfo
{
    uint32_t SrcMipLevel;   // Texture level of source mip
    uint32_t NumMipLevels;  // Number of OutMips to write: [1-4]
    uint32_t SrcDimension;  // Width and height of the source texture are even or odd.
    uint32_t IsSRGB;        // Must apply gamma correction to sRGB textures.
    glm::vec2 TexelSize;    // 1.0 / OutMip1.Dimensions
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
    uint32_t colorTexIndex = 0;
    uint32_t emissiveTexIndex = 0;
    uint32_t metallicRoughnessTexIndex = 0;
    uint32_t normalTexIndex = 0;
    uint32_t occlusionTexIndex = 0;
    uint32_t modelIndex = 0;
    uint32_t padding[3];
};

struct CameraMats
{
    glm::mat4x4 m_proj = glm::mat4x4(1.f);
    glm::mat4x4 m_invProj = glm::mat4x4(1.f);
    glm::mat4x4 m_view = glm::mat4x4(1.f);
    glm::mat4x4 m_invView = glm::mat4x4(1.f);
    glm::mat4x4 m_camera = glm::mat4x4(1.f);
    glm::mat4x4 m_cameraNoTranslation = glm::mat4x4(1.f);
    glm::vec4 m_cameraPos = glm::vec4(1.f);
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

struct HitInfo
{
    glm::vec4 colorAndDistance = glm::vec4(1.f);
    //int32_t index = -1;
};


};