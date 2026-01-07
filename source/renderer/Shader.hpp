#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>
#include <string>

namespace KS
{
class Device;
class ShaderInputBlueprint;

enum class PipelineType
{
    ST_RT_MESH_RENDER,

    ST_MESH_RENDER,

    ST_RAYTRACER,

    ST_COMPUTE
};

enum class ShaderType
{
    VERTEX_SHADER,

    PIXEL_SHADER,

    COMBINED_PS_VS,

    COMPUTE_SHADER,

    RAY_GEN_SHADER,

    MISS_SHADER,

    HIT_GROUP_SHADER,
};

struct ShaderLibrary
{
    ShaderType type{};
    std::string path;
    std::wstring name;
};

struct ShaderHitGroup
{
    std::wstring hitGroupName;
    std::wstring closestHitExport;
    std::wstring anyHitExport;
};

struct ShaderInputLink
{
    std::shared_ptr<ShaderInputBlueprint> m_input;
    std::vector<const wchar_t*> shaderNames;
};

class Shader
{
public:
    Shader(const Device& device, PipelineType shaderType, std::vector<ShaderLibrary>&& shaders,
           std::vector<ShaderInputLink>&& links, std::shared_ptr<ShaderInputBlueprint>& globalSignature,
           std::vector<ShaderHitGroup>&& hitGroups, std::vector<Formats>&& rtFormats, int flags = 0);
    
    ~Shader();
    std::shared_ptr<ShaderInputBlueprint> GetShaderInput() const {return m_globalRoot; };
    void* GetPipeline() const;
    PipelineType GetShaderType() const { return m_shader_type; }
    int GetFlags() const { return m_flags; }
    void Compile(const Device& device);

    enum MeshInputFlags
    {
        HAS_POSITIONS = 1 << 0,
        HAS_NORMALS = 1 << 1,
        HAS_UVS = 1 << 2,
        HAS_TANGENTS = 1 << 3,
        DEPTH_DISABLED = 1 << 4,
        NO_CULLING = 1 << 5,
        PBR_TEXTURES = 1 << 6,
        DEPTH_EQUAL = 1 << 7,
        DEPTH_WRITE_DISABLED = 1 << 8
    };

private:

    void MeshRenderShader(const Device& device);
    void ComputeShader(const Device& device);
    void RTShader(const Device& device);

    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::vector<ShaderLibrary> m_shaders;
    std::vector<ShaderInputLink> m_links;
    std::shared_ptr<ShaderInputBlueprint> m_globalRoot;
    std::vector<ShaderHitGroup> m_hitGroups{};
    std::vector<Formats> m_formats;
    PipelineType m_shader_type;
    int m_flags;
};

class ShaderBuilder
{
public:
    ShaderBuilder(){};
    ~ShaderBuilder(){};

    ShaderBuilder& AddShaderPath(ShaderType type, std::string path, std::wstring shaderName)
    {
        m_shaderLibraries.push_back({type, path, shaderName});
        return *this;
    };

    ShaderBuilder& AddLocalShaderInputLink(std::shared_ptr<ShaderInputBlueprint> localInput,
                                           std::initializer_list<const wchar_t*> names)
    {
        m_inputLinks.push_back({localInput, names});return *this;
    }

    ShaderBuilder& SetType(PipelineType type) { m_type = type; return *this; };
    ShaderBuilder& AddRenderTarget(Formats format) { m_rtFormats.push_back(format);return *this; };
    ShaderBuilder& SetGlobalSignature(std::shared_ptr<ShaderInputBlueprint>& signature) 
    {
        m_globalRootSignature = signature; 
        return *this;
    };

    ShaderBuilder& AddHitGroup(std::wstring hitGroupName, std::wstring closestHitName, std::wstring anyHitName)
    {
        m_hitGroups.push_back({hitGroupName, closestHitName, anyHitName});
        return *this;
    };

    ShaderBuilder& SetFlags(int flags) { m_flags = flags; return *this;};
    std::shared_ptr<Shader> Build(const Device& device)
    {
        return std::make_shared<Shader>(device, m_type, std::move(m_shaderLibraries), 
            std::move(m_inputLinks), m_globalRootSignature, std::move(m_hitGroups), 
            std::move(m_rtFormats), m_flags);
    };

private:
    std::vector<ShaderLibrary> m_shaderLibraries{};
    std::vector<ShaderHitGroup> m_hitGroups{};
    std::vector<ShaderInputLink> m_inputLinks{};
    std::vector<Formats> m_rtFormats{};
    std::shared_ptr<ShaderInputBlueprint> m_globalRootSignature;
    int m_flags = 0;
    PipelineType m_type = PipelineType::ST_RT_MESH_RENDER;
};
}  // namespace KS