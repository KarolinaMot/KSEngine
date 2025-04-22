#pragma once
#include <memory>
#include <string>
#include <renderer/InfoStructs.hpp>

namespace KS
{
class Device;
class ShaderInputCollection;

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
    Shader(const Device& device, ShaderType shaderType, std::shared_ptr<ShaderInputCollection> shaderInput,
           std::initializer_list<std::string> paths, std::initializer_list<Formats> rtFormats, int flags = 0);
    Shader(const Device& device, ShaderType shaderType, void* shaderInput, std::string path, int flags = 0);
    ~Shader();
    std::shared_ptr<ShaderInputCollection> GetShaderInput() const { return m_shader_input; };
    void* GetPipeline() const;
    ShaderType GetShaderType() const { return m_shader_type; }
    int GetFlags() const { return m_flags; }
    enum MeshInputFlags
    {
        HAS_POSITIONS = 1 << 0,
        HAS_NORMALS = 1 << 1,
        HAS_UVS = 1 << 2,
        HAS_TANGENTS = 1 << 3,
    };

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<ShaderInputCollection> m_shader_input;
    ShaderType m_shader_type;
    int m_flags;
};
}