#include <renderer/Shader.hpp>
#include <renderer/ShaderInputCollection.hpp>
#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/DX12/Helpers/DXPipeline.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>
#include <renderer/DX12/Helpers/DXRTPipeline.hpp>

#include <DXR/DXRHelper.h>
#include <DXR/nv_helpers_dx12/RaytracingPipelineGenerator.h>

class KS::Shader::Impl
{
public:
    // Union holds the two mutually-exclusive pipeline sets.
    union PipelineSet
    {
        // Normal pipeline
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipeline;

        // Raytracing pipeline
        std::shared_ptr<DXRTPipeline> m_RTPipeline;

        // Constructors/destructor. They need to be manually managed.
        PipelineSet() {}
        ~PipelineSet() {}
    } m_pipelineSet;
};

std::wstring ConvertToWideString(const char* input)
{
    if (input == nullptr)
    {
        return std::wstring();
    }

    // Determine the size (in wide characters) required to hold the converted string.
    int len = MultiByteToWideChar(CP_ACP, 0, input, -1, nullptr, 0);
    if (len == 0)
    {
        // Handle error; you can call GetLastError() to get extended error information.
        return std::wstring();
    }

    // Allocate a buffer for the wide string.
    std::wstring wideStr(len, L'\0');

    // Perform the conversion.
    int result = MultiByteToWideChar(CP_ACP, 0, input, -1, &wideStr[0], len);
    if (result == 0)
    {
        // Handle error if conversion fails.
        return std::wstring();
    }

    // The std::wstring now holds the converted string including the null terminator.
    // It is safe to get a LPCWSTR pointer by calling wideStr.c_str().
    return wideStr;
}

KS::Shader::Shader(const Device& device, ShaderType shaderType, std::shared_ptr<ShaderInputCollection> shaderInput,
                   std::initializer_list<std::string> paths, std::initializer_list<Formats> rtFormats, int flags)
{
    m_shader_input = shaderInput;
    m_shader_type = shaderType;
    m_impl = std::make_unique<Impl>();

    if (shaderType == ShaderType::ST_MESH_RENDER)
    {
        ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(paths.begin()->c_str(), "vs_5_0", "mainVS");
        ComPtr<ID3DBlob> p = DXPipelineBuilder::ShaderToBlob(paths.begin()->c_str(), "ps_5_0", "mainPS");

        auto builder = DXPipelineBuilder();

        if (flags & MeshInputFlags::HAS_POSITIONS) builder.AddInput("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, VDS_POSITIONS);
        if (flags & MeshInputFlags::HAS_NORMALS) builder.AddInput("NORMALS", DXGI_FORMAT_R32G32B32_FLOAT, VDS_NORMALS);
        if (flags & MeshInputFlags::HAS_UVS) builder.AddInput("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, VDS_UV);
        if (flags & MeshInputFlags::HAS_TANGENTS) builder.AddInput("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, VDS_TANGENTS);

        builder.SetVertexAndPixelShaders(v->GetBufferPointer(), v->GetBufferSize(), p->GetBufferPointer(), p->GetBufferSize());

        for (const auto& format : rtFormats)
        {
            builder.AddRenderTarget(Conversion::KSFormatsToDXGI(format));
        }

        m_impl->m_pipelineSet.m_pipeline =
            builder.Build(reinterpret_cast<ID3D12Device5*>(device.GetDevice()),
                          reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature()), L"RENDER PIPELINE");
    }
    else if (shaderType == ShaderType::ST_COMPUTE)
    {
        ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(paths.begin()->c_str(), "cs_5_0", "main");
        auto builder = DXPipelineBuilder().SetComputeShader(v->GetBufferPointer(), v->GetBufferSize());
        m_impl->m_pipelineSet.m_pipeline =
            builder.Build(reinterpret_cast<ID3D12Device5*>(device.GetDevice()),
                          reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature()), L"RENDER  COMPUTE PIPELINE");
    }
    else if (shaderType == ShaderType::ST_RAYTRACER)
    {
        ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());

        ComPtr<IDxcBlob> hitLibrary =
            nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString(paths.begin()->c_str()).c_str());
        ComPtr<IDxcBlob> missLibrary =
            nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString((paths.begin() + 1)->c_str()).c_str());
        ComPtr<IDxcBlob> rayGenLibrary =
            nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString((paths.begin() + 2)->c_str()).c_str());

        nv_helpers_dx12::RayTracingPipelineGenerator pipeline(engineDevice);
        pipeline.AddLibrary(rayGenLibrary.Get(), {L"RayGen"});
        pipeline.AddLibrary(missLibrary.Get(), {L"Miss"});
        pipeline.AddLibrary(hitLibrary.Get(), {L"ClosestHit"});

        pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
        pipeline.AddRootSignatureAssociation(reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature()),
                                             {L"RayGen"});
        pipeline.AddRootSignatureAssociation(reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature()), {L"Miss"});
        pipeline.AddRootSignatureAssociation(reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature()),
                                             {L"HitGroup"});

        pipeline.SetMaxPayloadSize(4 * sizeof(float));    // RGB + distance
        pipeline.SetMaxAttributeSize(2 * sizeof(float));  // barycentric coordinates
        pipeline.SetMaxRecursionDepth(1);


        m_impl->m_pipelineSet.m_RTPipeline = std::make_shared<DXRTPipeline>();
        m_impl->m_pipelineSet.m_RTPipeline->m_pipeline = pipeline.Generate();

        m_impl->m_pipelineSet.m_RTPipeline->m_pipeline->QueryInterface(
            IID_PPV_ARGS(&m_impl->m_pipelineSet.m_RTPipeline->m_stateObjectProps));
    }

    m_flags = flags;
}

KS::Shader::Shader(const Device& device, ShaderType shaderType, void* shaderInput, std::string path, int flags)
{
    // m_shader_input = shaderInput;
    m_shader_type = shaderType;
    m_impl = std::make_unique<Impl>();

    ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(path.c_str(), "vs_5_0", "mainVS");
    ComPtr<ID3DBlob> p = DXPipelineBuilder::ShaderToBlob(path.c_str(), "ps_5_0", "mainPS");

    auto builder = DXPipelineBuilder();

    if (flags & MeshInputFlags::HAS_POSITIONS) builder.AddInput("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, VDS_POSITIONS);
    if (flags & MeshInputFlags::HAS_NORMALS) builder.AddInput("NORMALS", DXGI_FORMAT_R32G32B32_FLOAT, VDS_NORMALS);
    if (flags & MeshInputFlags::HAS_UVS) builder.AddInput("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, VDS_UV);
    if (flags & MeshInputFlags::HAS_TANGENTS) builder.AddInput("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, VDS_TANGENTS);
    builder.AddRenderTarget(DXGI_FORMAT_R8G8B8A8_UNORM);
    builder.SetVertexAndPixelShaders(v->GetBufferPointer(), v->GetBufferSize(), p->GetBufferPointer(), p->GetBufferSize());
    m_impl->m_pipelineSet.m_pipeline = builder.Build(reinterpret_cast<ID3D12Device5*>(device.GetDevice()),
                                                     reinterpret_cast<ID3D12RootSignature*>(shaderInput), L"RENDER PIPELINE");

    m_flags = flags;
}

KS::Shader::~Shader() {}
void* KS::Shader::GetPipeline() const
{
    if (m_shader_type != ShaderType::ST_RAYTRACER)
    {
        return m_impl->m_pipelineSet.m_RTPipeline.get();
    }
    else
    {
        return m_impl->m_pipelineSet.m_pipeline.Get();
    }
}