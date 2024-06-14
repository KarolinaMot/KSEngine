#include "../Shader.hpp"
#include "Helpers/DXIncludes.hpp"
#include "Helpers/DXPipeline.hpp"
#include "Device/Device.hpp"
#include "../ShaderInputs.hpp"

class KS::Shader::Impl
{
public:
    ComPtr<ID3D12PipelineState> m_pipeline;
};

KS::Shader::Shader(const Device &device, ShaderType shaderType, std::shared_ptr<ShaderInputs> shaderInput, std::string path)
{
    m_shader_input = shaderInput;
    m_shader_type = shaderType;
    m_impl = std::make_unique<Impl>();

    ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(path.c_str(), "vs_5_0", "mainVS");
    ComPtr<ID3DBlob> p = DXPipelineBuilder::ShaderToBlob(path.c_str(), "ps_5_0", "mainPS");

    m_impl->m_pipeline = DXPipelineBuilder()
                             .AddInput("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0)
                             .AddInput("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 3)
                             .AddRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM)
                             .SetVertexAndPixelShaders(v->GetBufferPointer(), v->GetBufferSize(), p->GetBufferPointer(), p->GetBufferSize())
                             .Build(reinterpret_cast<ID3D12Device5 *>(device.GetDevice()),
                                    reinterpret_cast<ID3D12RootSignature *>(m_shader_input->GetSignature()),
                                    L"RENDER PIPELINE");
}

KS::Shader::~Shader()
{
}
void *KS::Shader::GetPipeline() const
{
    return m_impl->m_pipeline.Get();
}
