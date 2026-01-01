#pragma once
#include <resources/Material.hpp>
#include <resources/Mesh.hpp>
#pragma warning(push, 0)
#include <glm/glm.hpp>
#pragma warning(pop)

#ifndef MAX_MESHES
#define MAX_MESHES 2048
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
#define NUM_OF_TEXTURES 8192
#define RESOURCE_HEAP_SIZE 65536

namespace KS
{

enum SceneObjectTypes
{
    // SAN_MIGUEL,
    MESH,
    POINT_LIGHT,
    DIR_LIGHT,
    AMBIENT_LIGHT
};

enum ScenesToChoose
{
    //SAN_MIGUEL,
    TEST_SCENE,
    COUNT
};

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
    MESH_CULLING,
    NUM_SUBRENDER
};

enum StorageBuffers
{
    INSTANCE_DATA_BUFFER,
    DIR_LIGHT_BUFFER,
    POINT_LIGHT_BUFFER,
    BOUNDING_BOX_BUFFER,
    DRAW_INDICES,
    NUM_SBUFFER
};

enum UniformBuffers
{
    LIGHT_INFO_BUFFER,
    FOG_INFO_BUFFER,
    MODEL_INDEX_BUFFER,
    MIP_GEN_INFO,
    CAMERA_MAT_BUFFER,
    CULLING_INFO,
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
    R8G8B8A8_UNORM_SRGB,
    R16G16B16A16_FLOAT,
    R32G32B32A32_FLOAT,
    R32_FLOAT,
    D32_FLOAT,
    R32_TYPELESS,
    R16_FLOAT,
};

struct DrawEntry
{
    ResourceHandle<Mesh> meshHandle;
    glm::mat4x4 modelMat{};
    uint32_t tlasHandle{};
    Material material;
    int modelIndex{};
};

struct BatchRange
{
    std::shared_ptr<Mesh> mesh;
    Material* material;
    uint32_t first;  // index into draw_queue (or into your instance buffer)
    uint32_t count;
};

struct GenerateMipsInfo
{
    uint32_t SrcMipLevel = 0;   // Texture level of source mip
    uint32_t NumMipLevels=0;  // Number of OutMips to write: [1-4]
    uint32_t SrcDimension = 0;  // Width and height of the source texture are even or odd.
    uint32_t IsSRGB = 0;        // Must apply gamma correction to sRGB textures.
    glm::vec2 TexelSize{};      // 1.0 / OutMip1.Dimensions
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
    float mLinearAttenuation = 0.f;
    float mQuadraticAttenuation = 0.f;
    float mConstantAttenuation = 0.f;
    float padding;
};

struct LightInfo
{
    uint32_t numDirLights = 0;
    uint32_t numPointLights = 0;
    uint32_t padding[2];
    glm::vec4 mAmbientAndIntensity = glm::vec4(1.f);
};

template <typename A>
inline void serialize(A& ar, PointLightInfo& v)
{
    ar(cereal::make_nvp("Position", v.mPosition));
    ar(cereal::make_nvp("ConstantAttenuation", v.mConstantAttenuation));
    ar(cereal::make_nvp("LinearAttenuation", v.mLinearAttenuation));
    ar(cereal::make_nvp("QuadraticAttenuation", v.mQuadraticAttenuation));
    ar(cereal::make_nvp("ColorAndIntensity", v.mColorAndIntensity));
}

template <typename A>
inline void serialize(A& ar, DirLightInfo& v)
{
    ar(cereal::make_nvp("ColorAndIntensity", v.mColorAndIntensity));
    ar(cereal::make_nvp("Direction", v.mDir));
}

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
};

struct InstanceData
{
    ModelMat modelMatrix;
    MaterialInfo materialInfo;
};

struct CameraMats
{
    glm::mat4x4 m_proj = glm::mat4x4(1.f);
    glm::mat4x4 m_invProj = glm::mat4x4(1.f);
    glm::mat4x4 m_view = glm::mat4x4(1.f);
    glm::mat4x4 m_invView = glm::mat4x4(1.f);
    glm::mat4x4 m_camera = glm::mat4x4(1.f);
    glm::mat4x4 m_invCamera = glm::mat4x4(1.f);
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
    uint32_t variables[16];
};

struct CullingInfo
{
    Plane cameraPlane[6];
    uint32_t boundingBoxCount;
    uint32_t padding[3];
};

};