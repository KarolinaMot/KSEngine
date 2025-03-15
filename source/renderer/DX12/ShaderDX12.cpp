#include "../Shader.hpp"
#include "../ShaderInputCollection.hpp"
#include <device/Device.hpp>
#include "Helpers/DXIncludes.hpp"
#include "Helpers/DXPipeline.hpp"
#include <renderer/DX12/Helpers/DX12Conversion.hpp>

class KS::Shader::Impl
{
public:
    ComPtr<ID3D12PipelineState> m_pipeline;
};

KS::Shader::Shader(const Device& device, ShaderType shaderType, std::shared_ptr<ShaderInputCollection> shaderInput, std::string path, Formats* rtFormats, int numFormats)
{
    m_shader_input = shaderInput;
    m_shader_type = shaderType;
    m_impl = std::make_unique<Impl>();

    if (shaderType == ShaderType::ST_MESH_RENDER)
    {
        ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(path.c_str(), "vs_5_0", "mainVS");
        ComPtr<ID3DBlob> p = DXPipelineBuilder::ShaderToBlob(path.c_str(), "ps_5_0", "mainPS");

        auto builder = DXPipelineBuilder()
                           .AddInput("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, VDS_POSITIONS)
                           .AddInput("NORMALS", DXGI_FORMAT_R32G32B32_FLOAT, VDS_NORMALS)
                           .AddInput("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, VDS_UV)
                           .AddInput("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, VDS_TANGENTS)
                           .SetVertexAndPixelShaders(v->GetBufferPointer(), v->GetBufferSize(), p->GetBufferPointer(), p->GetBufferSize());

        if (!rtFormats)
        {
            LOG(Log::Severity::WARN, "Render target format has not been passed. A default format of R8G8B8A8_UNORM will be added instead");
            builder.AddRenderTarget(Conversion::KSFormatsToDXGI(Formats::R8G8B8A8_UNORM));
        }
        else
        {
            for (int i = 0; i < numFormats; i++)
            {
                builder.AddRenderTarget(Conversion::KSFormatsToDXGI(rtFormats[i]));
            }
        }

        m_impl->m_pipeline = builder.Build(reinterpret_cast<ID3D12Device5*>(device.GetDevice()),
            reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature()),
            L"RENDER PIPELINE");
    }
    else if (shaderType == ShaderType::ST_COMPUTE)
    {
        ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(path.c_str(), "cs_5_0", "main");
        auto builder = DXPipelineBuilder().SetComputeShader(v->GetBufferPointer(), v->GetBufferSize());
        m_impl->m_pipeline = builder.Build(reinterpret_cast<ID3D12Device5*>(device.GetDevice()),
            reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature()),
            L"RENDER  COMPUTE PIPELINE");
    }
}

KS::Shader::Shader(const Device& device, ShaderType shaderType, void* shaderInput, std::string path)
{
    // m_shader_input = shaderInput;
    m_shader_type = shaderType;
    m_impl = std::make_unique<Impl>();

    ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(path.c_str(), "vs_5_0", "mainVS");
    ComPtr<ID3DBlob> p = DXPipelineBuilder::ShaderToBlob(path.c_str(), "ps_5_0", "mainPS");

    m_impl->m_pipeline = DXPipelineBuilder()
                             .AddInput("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0)
                             .AddInput("NORMALS", DXGI_FORMAT_R32G32B32_FLOAT, 1)
                             .AddInput("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, 2)
                             .AddRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM)
                             .SetVertexAndPixelShaders(v->GetBufferPointer(), v->GetBufferSize(), p->GetBufferPointer(), p->GetBufferSize())
                             .Build(reinterpret_cast<ID3D12Device5*>(device.GetDevice()),
                                 reinterpret_cast<ID3D12RootSignature*>(shaderInput),
                                 L"RENDER PIPELINE");
}

KS::Shader::~Shader()
{
}
void* KS::Shader::GetPipeline() const
{
    return m_impl->m_pipeline.Get();
}
