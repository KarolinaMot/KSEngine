#pragma once
#include <memory>
#include <string>

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
    Shader(const Device& device, ShaderType shaderType, std::shared_ptr<ShaderInputs> shaderInput, std::string path);
    ~Shader();
    std::shared_ptr<ShaderInputs> GetShaderInput() const { return m_shader_input; };
    void* GetPipeline() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<ShaderInputs> m_shader_input;
    ShaderType m_shader_type;
};
}