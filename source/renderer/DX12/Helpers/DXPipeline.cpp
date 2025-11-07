#include "DXPipeline.hpp"
#include <code_utility.hpp>
#include <tools/Log.hpp>
#include <filesystem>

#ifdef _MSC_VER
#pragma comment(lib, "d3dcompiler") // Automatically link with d3dcompiler.lib as we are using D3DCompile() below.
#endif

ComPtr<ID3D12PipelineState> DXPipelineBuilder::Build(ComPtr<ID3D12Device5> device, const ComPtr<ID3D12RootSignature>& root, LPCWSTR name) const
{
    HRESULT hr;
    ComPtr<ID3D12PipelineState> pipeline;

    if (mComputeShaderBuffer == nullptr)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout.NumElements = static_cast<UINT>(mInputs.size());
        psoDesc.InputLayout.pInputElementDescs = mInputs.data();
        psoDesc.pRootSignature = root.Get();
        psoDesc.VS.BytecodeLength = mVertexShaderSize;
        psoDesc.VS.pShaderBytecode = mVertexShaderBuffer;
        psoDesc.PS.BytecodeLength = mFragmentShaderSize;
        psoDesc.PS.pShaderBytecode = mFragmentShaderBuffer;
        psoDesc.PrimitiveTopologyType = mTopology;
        psoDesc.SampleDesc.Count = mMsaaCount;
        psoDesc.SampleDesc.Quality = mMsaaQuality;
        psoDesc.SampleMask = 0xffffffff;
        psoDesc.RasterizerState = mRast;
        psoDesc.BlendState = mBlend;
        if (mRenderTargetFormats.size() <= 0)
        {
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        else
        {
            psoDesc.NumRenderTargets = static_cast<UINT>(mRenderTargetFormats.size());
            for (size_t i = 0; i < mRenderTargetFormats.size(); i++)
                psoDesc.RTVFormats[i] = mRenderTargetFormats[i];
        }
        psoDesc.DSVFormat = mDepthFormat;
        psoDesc.DepthStencilState = mDepth;
        hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
    }
    else
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = root.Get();
        psoDesc.CS = { reinterpret_cast<UINT8*>(mComputeShaderBuffer), mComputeShaderSize };
        hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
    }

    if (FAILED(hr))
    {
        MessageBox(NULL, L"Failed to create pipeline", L"FATAL ERROR!", MB_ICONERROR | MB_OK);
        ASSERT(false && "Failed to create pipeline");
        return nullptr;
    }
    pipeline->SetName(name);

    return pipeline;
}

DXPipelineBuilder& DXPipelineBuilder::AddInput(LPCSTR name, DXGI_FORMAT format, const uint32_t slot)
{
    D3D12_INPUT_ELEMENT_DESC input;
    input = { name, 0, format, slot, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    mInputs.push_back(input);
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::SetRasterizer(const CD3DX12_RASTERIZER_DESC& rasterizer)
{
    mRast = rasterizer;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::SetBlendState(const CD3DX12_BLEND_DESC& blendState)
{
    mBlend = blendState;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::SetDepthState(const CD3DX12_DEPTH_STENCIL_DESC& depthStencil)
{
    mDepth = depthStencil;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::SetDepthFormat(const DXGI_FORMAT& format)
{
    mDepthFormat = format;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::SetVertexAndPixelShaders(LPVOID vsBuffer, SIZE_T vsSize, LPVOID psBuffer, SIZE_T psSize)
{
    mVertexShaderBuffer = vsBuffer;
    mVertexShaderSize = vsSize;
    mFragmentShaderBuffer = psBuffer;
    mFragmentShaderSize = psSize;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::SetComputeShader(LPVOID computeShaderB, SIZE_T computeShaderS)
{
    mComputeShaderBuffer = computeShaderB;
    mComputeShaderSize = computeShaderS;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::SetMsaaCountAndQuality(uint32_t count, uint32_t quality)
{
    mMsaaCount = count;
    mMsaaQuality = quality;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::AddRenderTarget(DXGI_FORMAT format)
{
    mRenderTargetFormats.push_back(format);
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::SetPrimitiveTopology(const D3D12_PRIMITIVE_TOPOLOGY_TYPE& topology)
{
    mTopology = topology;
    return *this;
}

static std::wstring AnsiToWide(const char* s)
{
    if (!s) return {};
    int n = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    std::wstring w(n ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_ACP, 0, s, -1, w.data(), n);
    return w;
}

ComPtr<IDxcBlob> DXPipelineBuilder::ShaderToBlob(const char* path, const wchar_t* shaderVersion, const char* functionName)
{
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcCompiler3> compiler;
    CheckDX(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
    CheckDX(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));

    ComPtr<IDxcIncludeHandler> includeHandler;
    CheckDX(utils->CreateDefaultIncludeHandler(&includeHandler));

    std::wstring wPath = AnsiToWide(path);
    ComPtr<IDxcBlobEncoding> sourceBlob;
    CheckDX(utils->LoadFile(wPath.c_str(), nullptr, &sourceBlob));

    std::filesystem::path p(wPath);
    std::wstring inc = p.parent_path().c_str();

    DxcBuffer src{};
    src.Ptr = sourceBlob->GetBufferPointer();
    src.Size = sourceBlob->GetBufferSize();
    src.Encoding = DXC_CP_ACP;

    std::wstring wEntry = functionName ? AnsiToWide(functionName) : L"main";
    std::vector<LPCWSTR> args{
        L"-E",
        wEntry.c_str(),  // entry point
        L"-T",
        shaderVersion,     // target, e.g. L"vs_6_8", L"ps_6_8", L"lib_6_8" (for DXR)
        L"-Zi",            // debug info
        L"-Qembed_debug",  // embed debug info in DXIL
        L"-Od",            // disable optimizations (debug parity with FXC D3DCOMPILE_DEBUG)
        L"-I",
        inc.c_str()
    };

    ComPtr<IDxcResult> result;
    ComPtr<IDxcBlob> dxil;

    // Retry loop on compile errors
    for (;;)
    {
        result.Reset();
        dxil.Reset();

        HRESULT hrCompile =
            compiler->Compile(&src, args.data(), (UINT)args.size(), includeHandler.Get(), IID_PPV_ARGS(&result));
        if (FAILED(hrCompile))
        {
            std::wstring msg = std::format(L"DXC invocation failed for {}.", wPath);
            MessageBox(nullptr, msg.c_str(), L"Shader compilation error", MB_ICONERROR | MB_OK);
            CheckDX(hrCompile);
        }

        HRESULT status = S_OK;
        CheckDX(result->GetStatus(&status));

        ComPtr<IDxcBlobUtf8> errors;
        (void)result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

        if (FAILED(status))
        {
            std::string errStr = errors ? std::string((const char*)errors->GetBufferPointer(), errors->GetBufferSize())
                                        : std::string("Unknown error");
            std::wstring errW = AnsiToWide(errStr.c_str());

            std::wstring msg = std::format(L"Function {} in path {} could not compile.\nError:\n{}\nRecompile?",
                                           AnsiToWide(functionName ? functionName : "main"), wPath, errW);

            LOG(Log::Severity::FATAL, "Function {} in path {} could not compile. Error: {}",
                (functionName ? functionName : "main"), path, errStr.c_str());

            int r = MessageBox(nullptr, msg.c_str(), L"Shader compilation error", MB_ICONERROR | MB_RETRYCANCEL);
            if (r == IDRETRY) continue;
            return nullptr;
        }

        CheckDX(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxil), nullptr));
        return dxil;
    }
}
