#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>
#include <string>

namespace KS
{
class Device;
class ShaderInputBlueprint;

enum class ShaderType
{
    ST_RT_MESH_RENDER,

    ST_MESH_RENDER,

    ST_RAYTRACER,

    ST_COMPUTE
};

class Shader
{
public:
    Shader(const Device& device, ShaderType shaderType, std::shared_ptr<ShaderInputBlueprint> shaderInputs,
           std::initializer_list<std::string> paths, std::initializer_list<Formats> rtFormats, int flags = 0);

    ~Shader();
    std::shared_ptr<ShaderInputBlueprint> GetShaderInput() const { return m_shader_input; };
    void* GetPipeline() const;
    ShaderType GetShaderType() const { return m_shader_type; }
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
        PBR_TEXTURES = 1 << 6
    };

private:

    void MeshRenderShader(const Device& device);
    void ComputeShader(const Device& device);
    void RTShader(const Device& device);

    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<ShaderInputBlueprint> m_shader_input;
    std::vector<std::string> m_paths;
    std::vector<Formats> m_formats;
    ShaderType m_shader_type;
    int m_flags;
};
}  // namespace KS