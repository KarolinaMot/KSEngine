#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DX12Conversion.hpp>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/DX12/Helpers/DXPipeline.hpp>
#include <renderer/DX12/Helpers/DXRTPipeline.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputBlueprint.hpp>
#include <renderer/InfoStructs.hpp>
#pragma warning(push, 0)
#include <DXR/DXRHelper.h>
#include <DXR/nv_helpers_dx12/RaytracingPipelineGenerator.h>
#pragma warning(pop)

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

    struct DXRLibrary
    {
        D3D12_DXIL_LIBRARY_DESC libDesc{};
        D3D12_EXPORT_DESC exports;  // keep storage alive
        D3D12_STATE_SUBOBJECT subobject{};
    };

    DXRLibrary MakeLibrarySO(IDxcBlob* dxil, const wchar_t* exportName, const wchar_t* toRename);
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

KS::Shader::Shader(const Device& device, ShaderType shaderType, std::shared_ptr<ShaderInputBlueprint> ShaderInputs,
                   std::initializer_list<std::string> paths, std::initializer_list<Formats> rtFormats, int flags)
{
    m_shader_input = ShaderInputs;
    m_shader_type = shaderType;
    m_impl = std::make_unique<Impl>();
    m_paths = paths;
    m_formats = rtFormats;
    m_flags = flags;

    Compile(device);
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

void KS::Shader::Compile(const Device& device)
{
    if (m_shader_type == ShaderType::ST_MESH_RENDER)
    {
        MeshRenderShader(device);
    }
    else if (m_shader_type == ShaderType::ST_COMPUTE)
    {
        ComputeShader(device);
    }
    else if (m_shader_type == ShaderType::ST_RAYTRACER)
    {
        RTShader(device);
    }
}

void KS::Shader::MeshRenderShader(const Device& device)
{
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto signature = reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature());

    ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(m_paths.begin()->c_str(), "vs_5_0", "mainVS");
    ComPtr<ID3DBlob> p = DXPipelineBuilder::ShaderToBlob(m_paths.begin()->c_str(), "ps_5_0", "mainPS");

    if (!v || !p)
    {
        std::exit(EXIT_FAILURE);
    }

    auto builder = DXPipelineBuilder();

    if (m_flags & MeshInputFlags::HAS_POSITIONS) builder.AddInput("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, VDS_POSITIONS);
    if (m_flags & MeshInputFlags::HAS_NORMALS) builder.AddInput("NORMALS", DXGI_FORMAT_R32G32B32_FLOAT, VDS_NORMALS);
    if (m_flags & MeshInputFlags::HAS_UVS) builder.AddInput("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, VDS_UV);
    if (m_flags & MeshInputFlags::HAS_TANGENTS) builder.AddInput("TANGENT", DXGI_FORMAT_R32G32B32_FLOAT, VDS_TANGENTS);

    if (m_flags & MeshInputFlags::DEPTH_DISABLED)
    {
        CD3DX12_DEPTH_STENCIL_DESC depth = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        depth.DepthEnable = TRUE;  // <- no depth testing
        depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depth.StencilEnable = FALSE;
        depth.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        builder.SetDepthState(depth);
    }

    if (m_flags & MeshInputFlags::NO_CULLING)
    {
        CD3DX12_RASTERIZER_DESC rast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        rast.CullMode = D3D12_CULL_MODE_FRONT;
        builder.SetRasterizer(rast);
    }

    builder.SetVertexAndPixelShaders(v->GetBufferPointer(), v->GetBufferSize(), p->GetBufferPointer(), p->GetBufferSize());

    for (const auto& format : m_formats)
    {
        builder.AddRenderTarget(Conversion::KSFormatsToDXGI(format));
    }

    auto testPipeline = builder.Build(engineDevice, signature, L"RENDER  COMPUTE PIPELINE");
    if (testPipeline) m_impl->m_pipelineSet.m_pipeline = testPipeline;
}

void KS::Shader::ComputeShader(const Device& device) 
{
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto signature = reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature());

    ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(m_paths.begin()->c_str(), "cs_5_0", "main");
    
    if (!v)
    {
        std::exit(EXIT_FAILURE);
    }

    auto builder = DXPipelineBuilder().SetComputeShader(v->GetBufferPointer(), v->GetBufferSize());

    auto testPipeline = builder.Build(engineDevice, signature, L"RENDER  COMPUTE PIPELINE");

    if (testPipeline) 
        m_impl->m_pipelineSet.m_pipeline = testPipeline;
}

void KS::Shader::RTShader(const Device& device)
{
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto signature = reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature());

    auto& rtPipeline = m_impl->m_pipelineSet.m_RTPipeline;
    rtPipeline = std::make_shared<DXRTPipeline>();

    ComPtr<IDxcBlob> hitBlob = nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString(m_paths.begin()->c_str()).c_str());
    ComPtr<IDxcBlob> missBlob =
        nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString((m_paths.begin() + 1)->c_str()).c_str());
    ComPtr<IDxcBlob> rayGenBlob =
        nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString((m_paths.begin() + 2)->c_str()).c_str());

    if (!hitBlob || !missBlob || !rayGenBlob)
    {
        std::exit(EXIT_FAILURE);
    }

    Impl::DXRLibrary missLibrary = m_impl->MakeLibrarySO(missBlob.Get(), L"Miss", nullptr);
    Impl::DXRLibrary rayGenLibrary = m_impl->MakeLibrarySO(rayGenBlob.Get(), L"RayGen", nullptr);
    Impl::DXRLibrary hitLibrary = m_impl->MakeLibrarySO(hitBlob.Get(), L"ClosestHit", nullptr);

    D3D12_HIT_GROUP_DESC hitGroup = {
        .HitGroupExport = L"HitGroup", .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES, .ClosestHitShaderImport = L"ClosestHit"};

    D3D12_RAYTRACING_SHADER_CONFIG shaderCfg = {
        .MaxPayloadSizeInBytes = sizeof(HitInfo),
        .MaxAttributeSizeInBytes = 8,
    };

    bool localSignature = !m_shader_input->GetIsGlobal();

    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineCfg = {.MaxTraceRecursionDepth = 1};

    std::vector<D3D12_STATE_SUBOBJECT> subs;
    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &missLibrary.libDesc});
    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &hitLibrary.libDesc});
    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &rayGenLibrary.libDesc});
    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroup});
    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderCfg});

    if (localSignature)
    {
        D3D12_LOCAL_ROOT_SIGNATURE localSig = {signature};
        subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &localSig});
    }
    else
    {
        D3D12_GLOBAL_ROOT_SIGNATURE globalSig = {signature};
        subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &globalSig});
    }

    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineCfg});

    // Create a list of shader entry point names that use the payload.
    const WCHAR* shaderPayloadExports[] = {L"RayGen", L"HitGroup", L"Miss"};

    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION assocShaderCfg = {};
    assocShaderCfg.NumExports = _countof(shaderPayloadExports);
    assocShaderCfg.pExports = shaderPayloadExports;
    assocShaderCfg.pSubobjectToAssociate = &subs[4];  // shaderCfg subobject
    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &assocShaderCfg});

    if (localSignature)
    {
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION assocLocalRS = {};
        assocLocalRS.NumExports = _countof(shaderPayloadExports);
        assocLocalRS.pExports = shaderPayloadExports;
        assocLocalRS.pSubobjectToAssociate = &subs[5];  // local RS subobject
        subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &assocLocalRS});
    }

    D3D12_STATE_OBJECT_DESC desc = {.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
                                    .NumSubobjects = static_cast<UINT>(subs.size()),
                                    .pSubobjects = subs.data()};

    ComPtr<ID3D12StateObject> testPipeline;
    HRESULT hr = engineDevice->CreateStateObject(&desc, IID_PPV_ARGS(&testPipeline));

    if (FAILED(hr))
    {
        MessageBox(NULL, L"Failed to create pipeline", L"FATAL ERROR!", MB_ICONERROR | MB_OK);
        ASSERT(false && "Failed to create pipeline");
        std::exit(EXIT_FAILURE);
    }
    rtPipeline->m_pipeline = testPipeline;

    ComPtr<ID3D12StateObjectProperties> testProp;
    hr = rtPipeline->m_pipeline->QueryInterface(IID_PPV_ARGS(&testProp));

    if (FAILED(hr))
    {
        MessageBox(NULL, L"Failed to create pipeline", L"FATAL ERROR!", MB_ICONERROR | MB_OK);
        ASSERT(false && "Failed to create pipeline");
        std::exit(EXIT_FAILURE);
    }

    rtPipeline->m_stateObjectProps = testProp;
}

KS::Shader::Impl::DXRLibrary KS::Shader::Impl::MakeLibrarySO(IDxcBlob* dxil, const wchar_t* exportName, const wchar_t* toRename)
{
    DXRLibrary out{};
    out.libDesc.DXILLibrary.pShaderBytecode = dxil->GetBufferPointer();
    out.libDesc.DXILLibrary.BytecodeLength = dxil->GetBufferSize();
    out.exports.Name = exportName;  // exported name
    out.exports.ExportToRename = toRename;
    out.exports.Flags = D3D12_EXPORT_FLAG_NONE;

    out.libDesc.NumExports = 1;
    out.libDesc.pExports = &out.exports;

    out.subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    out.subobject.pDesc = &out.libDesc;
    return out;
}
