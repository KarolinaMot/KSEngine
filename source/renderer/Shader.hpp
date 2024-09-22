#pragma once
#include <memory>
#include <string>
#include <renderer/InfoStructs.hpp>

namespace KS
{
class Device;
class ShaderInputs;

enum class ShaderType
{
    ST_MESH_RENDER,

    ST_COMPUTE
};
class Shader
{
public:
    Shader(const Device& device, ShaderType shaderType, std::shared_ptr<ShaderInputs> shaderInput, std::string path, Formats* rtFormats = nullptr, int numFormats = 1);
    Shader(const Device& device, ShaderType shaderType, void* shaderInput, std::string path);
    ~Shader();
    std::shared_ptr<ShaderInputs> GetShaderInput() const { return m_shader_input; };
    void* GetPipeline() const;
    ShaderType GetShaderType() { return m_shader_type; }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<ShaderInputs> m_shader_input;
    ShaderType m_shader_type;
};
}