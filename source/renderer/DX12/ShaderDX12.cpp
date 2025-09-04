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


    struct DXRLibrary
    {
        D3D12_DXIL_LIBRARY_DESC libDesc{};
        std::vector<D3D12_EXPORT_DESC> exports;  // keep storage alive
        D3D12_STATE_SUBOBJECT subobject{};
    };

    
    DXRLibrary MakeLibrarySO(IDxcBlob* dxil, std::initializer_list<const wchar_t*> exportNames);
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
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());
    auto signature = reinterpret_cast<ID3D12RootSignature*>(m_shader_input->GetSignature());

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
            builder.Build(engineDevice,
                          signature, L"RENDER PIPELINE");
    }
    else if (shaderType == ShaderType::ST_COMPUTE)
    {
        ComPtr<ID3DBlob> v = DXPipelineBuilder::ShaderToBlob(paths.begin()->c_str(), "cs_5_0", "main");
        auto builder = DXPipelineBuilder().SetComputeShader(v->GetBufferPointer(), v->GetBufferSize());
        m_impl->m_pipelineSet.m_pipeline =
            builder.Build(engineDevice,
                          signature, L"RENDER  COMPUTE PIPELINE");
    }
    else if (shaderType == ShaderType::ST_RAYTRACER)
    {
        ID3D12Device5* engineDevice = static_cast<ID3D12Device5*>(device.GetDevice());
        auto& rtPipeline = m_impl->m_pipelineSet.m_RTPipeline;
        rtPipeline = std::make_shared<DXRTPipeline>();

        ComPtr<IDxcBlob> hitBlob =
            nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString(paths.begin()->c_str()).c_str());
        ComPtr<IDxcBlob> missBlob =
            nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString((paths.begin() + 1)->c_str()).c_str());
        ComPtr<IDxcBlob> rayGenBlob =
            nv_helpers_dx12::CompileShaderLibrary(ConvertToWideString((paths.begin() + 2)->c_str()).c_str());

        Impl::DXRLibrary hitLibrary = m_impl->MakeLibrarySO(hitBlob.Get(), {L"ClosestHit"});
        Impl::DXRLibrary missLibrary = m_impl->MakeLibrarySO(missBlob.Get(), {L"Miss"});
        Impl::DXRLibrary rayGenLibrary = m_impl->MakeLibrarySO(rayGenBlob.Get(), {L"RayGen"});

        D3D12_HIT_GROUP_DESC hitGroup = {
            .HitGroupExport = L"HitGroup", .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES, .ClosestHitShaderImport = L"ClosestHit"};

        D3D12_RAYTRACING_SHADER_CONFIG shaderCfg = {
            .MaxPayloadSizeInBytes = 16,
            .MaxAttributeSizeInBytes = 8,
        };

        D3D12_GLOBAL_ROOT_SIGNATURE globalSig = {signature};

        D3D12_RAYTRACING_PIPELINE_CONFIG pipelineCfg = {.MaxTraceRecursionDepth = 1};

          D3D12_STATE_SUBOBJECT subobjects[] = {
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, .pDesc = &hitLibrary.libDesc},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, .pDesc = &missLibrary.libDesc},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, .pDesc = &rayGenLibrary.libDesc},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, .pDesc = &hitGroup},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, .pDesc = &shaderCfg},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, .pDesc = &globalSig},
            {.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, .pDesc = &pipelineCfg}};

        D3D12_STATE_OBJECT_DESC desc = {.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
                                        .NumSubobjects = std::size(subobjects),
                                        .pSubobjects = subobjects};
        engineDevice->CreateStateObject(&desc, IID_PPV_ARGS(&rtPipeline->m_pipeline));


        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(3 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        engineDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                        nullptr, IID_PPV_ARGS(&rtPipeline->m_shaderIDs));

        rtPipeline->m_pipeline->QueryInterface(IID_PPV_ARGS(&rtPipeline->m_stateObjectProps));

        void* data;
        auto writeId = [&](const wchar_t* name)
        {
            void* id = rtPipeline->m_stateObjectProps->GetShaderIdentifier(name);
            memcpy(data, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            data = static_cast<char*>(data) + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        };

        rtPipeline->m_shaderIDs->Map(0, nullptr, &data);
        writeId(L"RayGen");
        writeId(L"Miss");
        writeId(L"HitGroup");
        rtPipeline->m_shaderIDs->Unmap(0, nullptr);

        //nv_helpers_dx12::RayTracingPipelineGenerator pipeline(engineDevice);
        //pipeline.AddLibrary(rayGenLibrary.Get(), {L"RayGen"});
        //pipeline.AddLibrary(missLibrary.Get(), {L"Miss"});
        //pipeline.AddLibrary(hitLibrary.Get(), {L"ClosestHit"});

        //pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
        //pipeline.AddRootSignatureAssociation(signature,
        //                                     {L"RayGen"});
        //pipeline.AddRootSignatureAssociation(signature, {L"Miss"});
        //pipeline.AddRootSignatureAssociation(signature,
        //                                     {L"HitGroup"});

        //pipeline.SetMaxPayloadSize(4 * sizeof(float));    // RGB + distance
        //pipeline.SetMaxAttributeSize(2 * sizeof(float));  // barycentric coordinates
        //pipeline.SetMaxRecursionDepth(1);

        //m_impl->m_pipelineSet.m_RTPipeline->m_pipeline = pipeline.Generate();

        //m_impl->m_pipelineSet.m_RTPipeline->m_pipeline->QueryInterface(
        //    IID_PPV_ARGS(&m_impl->m_pipelineSet.m_RTPipeline->m_stateObjectProps));
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

KS::Shader::Impl::DXRLibrary KS::Shader::Impl::MakeLibrarySO(IDxcBlob* dxil, std::initializer_list<const wchar_t*> exportNames)
{
    DXRLibrary out{};
    out.libDesc.DXILLibrary.pShaderBytecode = dxil->GetBufferPointer();
    out.libDesc.DXILLibrary.BytecodeLength = dxil->GetBufferSize();
    out.exports.resize(exportNames.size());

    size_t i = 0;
    for (auto* name : exportNames)
    {
        out.exports[i].Name = name;               // exported name
        out.exports[i].ExportToRename = nullptr;  // no rename; export symbol as-is
        out.exports[i].Flags = D3D12_EXPORT_FLAG_NONE;
        ++i;
    }
    out.libDesc.NumExports = (UINT)out.exports.size();
    out.libDesc.pExports = out.exports.data();

    out.subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    out.subobject.pDesc = &out.libDesc;
    return out;
}
