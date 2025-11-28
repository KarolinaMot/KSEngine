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
        ComPtr<IDxcBlob> dxil;    

        D3D12_DXIL_LIBRARY_DESC libDesc{};
        D3D12_EXPORT_DESC exports;  // keep storage alive
        D3D12_STATE_SUBOBJECT subobject{};

        std::wstring exportName;  // owns the strings
    };
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

KS::Shader::Shader(const Device& device, PipelineType shaderType, std::vector<ShaderLibrary>&& shaders,
                   std::vector<ShaderInputLink>&& links, std::shared_ptr<ShaderInputBlueprint>& globalSignature,
                   std::vector<ShaderHitGroup>&& hitGroups,
                   std::vector<Formats>&& rtFormats, int flags)
{
    m_shaders = std::move(shaders);
    m_links = std::move(links);
    m_globalRoot = globalSignature;
    m_shader_type = shaderType;
    m_impl = std::make_unique<Impl>();
    m_formats = std::move(rtFormats);
    m_hitGroups = std::move(hitGroups);
    m_flags = flags;

    Compile(device);
}

KS::Shader::~Shader() {}
void* KS::Shader::GetPipeline() const
{
    if (m_shader_type != PipelineType::ST_RAYTRACER)
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
    if (m_shader_type == PipelineType::ST_MESH_RENDER)
    {
        MeshRenderShader(device);
    }
    else if (m_shader_type == PipelineType::ST_COMPUTE)
    {
        ComputeShader(device);
    }
    else if (m_shader_type == PipelineType::ST_RAYTRACER)
    {
        RTShader(device);
    }
}

void KS::Shader::MeshRenderShader(const Device& device)
{
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto signature = reinterpret_cast<ID3D12RootSignature*>(m_globalRoot->GetSignature());

    auto v = DXPipelineBuilder::ShaderToBlob(m_shaders[0].path.c_str(), "vs_5_1", "mainVS");
    auto p = DXPipelineBuilder::ShaderToBlob(m_shaders[0].path.c_str(), "ps_5_1", "mainPS");

    if (!v || !p)
    {
        std::exit(EXIT_FAILURE);
    }

    auto builder = DXPipelineBuilder();
    builder.SetVertexAndPixelShaders(v.Get(), p.Get());

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
        rast.CullMode = D3D12_CULL_MODE_NONE;
        builder.SetRasterizer(rast);
    }


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
    auto signature = reinterpret_cast<ID3D12RootSignature*>(m_globalRoot->GetSignature());

    auto v = DXPipelineBuilder::ShaderToBlob(m_shaders[0].path.c_str(), "cs_5_1", "main");
    
    if (!v)
    {
        std::exit(EXIT_FAILURE);
    }

    auto builder = DXPipelineBuilder().SetComputeShader(v.Get());

    auto testPipeline = builder.Build(engineDevice, signature, L"RENDER  COMPUTE PIPELINE");

    if (testPipeline) 
        m_impl->m_pipelineSet.m_pipeline = testPipeline;
}

void KS::Shader::RTShader(const Device& device)
{
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto globalSignature = m_globalRoot ? reinterpret_cast<ID3D12RootSignature*>(m_globalRoot->GetSignature()) : nullptr;

    auto& rtPipeline = m_impl->m_pipelineSet.m_RTPipeline;
    rtPipeline = std::make_shared<DXRTPipeline>();


    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineCfg = {.MaxTraceRecursionDepth = 2};

    std::vector<Impl::DXRLibrary> libraries;
    libraries.reserve(m_shaders.size());

    size_t numSubobjects = m_shaders.size() +                        // DXIL_LIBRARY
                           m_hitGroups.size() +                   // HIT_GROUP
                           1 +                              // SHADER_CONFIG
                           (globalSignature ? 1 : 0) + 1 +  // PIPELINE_CONFIG
                           1 +                              // SHADER_CONFIG association
                           m_links.size() * 2;                    // LOCAL_ROOT_SIGNATURE + its association

    std::vector<D3D12_STATE_SUBOBJECT> subs;
    subs.reserve(numSubobjects+5);
    std::vector<LPCWSTR> shaderPayloadExports;

    for (int i = 0; i < m_shaders.size(); i++)
    {
        if (m_shaders[i].path == "")
        {
            LOG(Log::Severity::FATAL, "Passed an empty path to the shader compiler ");
            std::exit(EXIT_FAILURE);

        }

       ComPtr<IDxcBlob> blob = nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString(m_shaders[i].path.c_str()).c_str());

        if (!blob)
        {
            std::exit(EXIT_FAILURE);
        }

        Shader::Impl::DXRLibrary libDesc{};
        libraries.push_back(libDesc);
        auto& out = libraries.back();
        out.dxil = blob;
        out.exports.Name = m_shaders[i].name.c_str();  // exported name
        out.exports.ExportToRename = nullptr;
        out.exports.Flags = D3D12_EXPORT_FLAG_NONE;

        out.libDesc.DXILLibrary.pShaderBytecode = out.dxil->GetBufferPointer();
        out.libDesc.DXILLibrary.BytecodeLength = out.dxil->GetBufferSize();
        out.libDesc.NumExports = 1;
        out.libDesc.pExports = &out.exports;

        out.subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        out.subobject.pDesc = &out.libDesc;

        auto& lib = libraries.back(); 

        subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &lib.libDesc});

        if (m_shaders[i].type != ShaderType::CLOSEST_HIT_SHADER) shaderPayloadExports.push_back(m_shaders[i].name.c_str());
    }

    std::vector<D3D12_HIT_GROUP_DESC> hitGroupDescs;
    hitGroupDescs.reserve(m_hitGroups.size());

    for (const auto& hitGroup : m_hitGroups)
    {
        shaderPayloadExports.push_back(hitGroup.hitGroupName.c_str());
        D3D12_HIT_GROUP_DESC hitGroupDesc = {.HitGroupExport = hitGroup.hitGroupName.c_str(),
                                             .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
                                             .ClosestHitShaderImport = hitGroup.closestHitExport.c_str()};
        hitGroupDescs.push_back(hitGroupDesc);
        subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroupDescs.back()});

    }

    D3D12_RAYTRACING_SHADER_CONFIG shaderCfg = {
        .MaxPayloadSizeInBytes = 64,
        .MaxAttributeSizeInBytes = 8,
    };

    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderCfg});
    auto  shaderConfigId = subs.size() - 1;

    D3D12_GLOBAL_ROOT_SIGNATURE globalSig;
    if (globalSignature)
    {
        globalSig = {globalSignature};
        subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &globalSig});
    }

    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineCfg});

    // Create a list of shader entry point names that use the payload.

    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION assocShaderCfg = {};
    assocShaderCfg.NumExports = static_cast<UINT>(shaderPayloadExports.size());
    assocShaderCfg.pExports = shaderPayloadExports.data();
    assocShaderCfg.pSubobjectToAssociate = &subs[shaderConfigId];  // shaderCfg subobject
    subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &assocShaderCfg});

    std::vector<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> localSgAssoc;
    std::vector<D3D12_LOCAL_ROOT_SIGNATURE> localSgs;
    localSgs.reserve(m_links.size());
    localSgAssoc.reserve(m_links.size());
    for (int i = 0; i < m_links.size(); i++)
    {
        auto localSignature = reinterpret_cast<ID3D12RootSignature*>(m_links[i].m_input->GetSignature());

        D3D12_LOCAL_ROOT_SIGNATURE localSig = {localSignature};
        localSgs.push_back(localSig);
        subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &localSgs.back()});

        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION assocLocalRS = {};
        assocLocalRS.NumExports = static_cast<UINT>(m_links[i].shaderNames.size());
        assocLocalRS.pExports = m_links[i].shaderNames.data();
        assocLocalRS.pSubobjectToAssociate = &subs.back();  // local RS subobject
        localSgAssoc.push_back(assocLocalRS);
        subs.push_back({D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &localSgAssoc.back()});

    }

    D3D12_STATE_OBJECT_DESC desc = {.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
                                    .NumSubobjects = static_cast<UINT>(subs.size()),
                                    .pSubobjects = subs.data()};

    for (auto name : shaderPayloadExports)
    {
        OutputDebugStringW(L"Payload export: ");
        OutputDebugStringW(name);
        OutputDebugStringW(L"\n");
    }

    for (int i = 0; i < m_links.size(); ++i)
    {
        for (auto name : m_links[i].shaderNames)
        {
            OutputDebugStringW(L"Local RS export: ");
            OutputDebugStringW(name);
            OutputDebugStringW(L"\n");
        }
    }

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