#include <Log.hpp>
#include <shader/DXPipeline.hpp>

DXPipelineBuilder::DXPipelineBuilder()
{
    input_assembly_formats.reserve(MAX_INPUTS);

    pipeline_desc.NodeMask = 0;

    pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    pipeline_desc.pRootSignature = nullptr;

    pipeline_desc.InputLayout = D3D12_INPUT_LAYOUT_DESC { nullptr, 0 };
    pipeline_desc.RTVFormats = D3D12_RT_FORMAT_ARRAY { {}, 0 };

    pipeline_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipeline_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC2(D3D12_DEFAULT);
    pipeline_desc.RasterizerState = CD3DX12_RASTERIZER_DESC2(D3D12_DEFAULT);

    pipeline_desc.SampleDesc = DXGI_SAMPLE_DESC { 1, 0 };
    pipeline_desc.SampleMask = 0;

    pipeline_desc.VS = D3D12_SHADER_BYTECODE { nullptr, 0 };
    pipeline_desc.PS = D3D12_SHADER_BYTECODE { nullptr, 0 };
    pipeline_desc.CS = D3D12_SHADER_BYTECODE { nullptr, 0 };

    // UNUSED / UNIMPLEMENTED
    {
        pipeline_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        pipeline_desc.GS = D3D12_SHADER_BYTECODE { nullptr, 0 };
        pipeline_desc.HS = D3D12_SHADER_BYTECODE { nullptr, 0 };
        pipeline_desc.DS = D3D12_SHADER_BYTECODE { nullptr, 0 };
        pipeline_desc.AS = D3D12_SHADER_BYTECODE { nullptr, 0 };
        pipeline_desc.MS = D3D12_SHADER_BYTECODE { nullptr, 0 };

        pipeline_desc.CachedPSO = D3D12_CACHED_PIPELINE_STATE { nullptr, 0 };

        pipeline_desc.StreamOutput = D3D12_STREAM_OUTPUT_DESC {};
        pipeline_desc.ViewInstancingDesc = CD3DX12_VIEW_INSTANCING_DESC(D3D12_DEFAULT);
    }
}

std::optional<DXPipeline> DXPipelineBuilder::BuildGraphicsPipeline(ID3D12Device* device, const wchar_t* name) const
{

    auto graphics_desc = pipeline_desc.GraphicsDescV0();

    ComPtr<ID3D12PipelineState> pipeline;
    HRESULT hr = device->CreateGraphicsPipelineState(&graphics_desc, IID_PPV_ARGS(&pipeline));

    if (FAILED(hr))
    {
        return std::nullopt;
    }

    pipeline->SetName(name);

    return DXPipeline { pipeline };
}

std::optional<DXPipeline> DXPipelineBuilder::BuildComputePipeline(ID3D12Device* device, const wchar_t* name) const
{
    auto compute_desc = pipeline_desc.ComputeDescV0();

    ComPtr<ID3D12PipelineState> pipeline;
    HRESULT hr = device->CreateComputePipelineState(&compute_desc, IID_PPV_ARGS(&pipeline));

    if (FAILED(hr))
    {
        return std::nullopt;
    }

    pipeline->SetName(name);

    return DXPipeline { pipeline };
}

DXPipelineBuilder& DXPipelineBuilder::AddInput(uint32_t slot, const char* semantic_name, uint32_t semantic_index, DXGI_FORMAT format, bool instance_data)
{
    if (input_assembly_formats.size() >= MAX_INPUTS)
    {
        Log("Pipeline Builder Warning: Reached limit of {} on inputs (call is ignored)", MAX_INPUTS);
        return *this;
    }

    D3D12_INPUT_ELEMENT_DESC input {
        semantic_name,
        semantic_index,
        format,
        slot,
        0,
        instance_data ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        instance_data ? 1u : 0u
    };

    input_assembly_formats.push_back(input);

    pipeline_desc.InputLayout = {
        input_assembly_formats.data(),
        static_cast<uint32_t>(input_assembly_formats.size())
    };

    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::WithRootSignature(ID3D12RootSignature* root_signature)
{
    pipeline_desc.pRootSignature = root_signature;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::WithRasterizer(const CD3DX12_RASTERIZER_DESC2& rasterizer)
{
    pipeline_desc.RasterizerState = rasterizer;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::WithBlendState(const CD3DX12_BLEND_DESC& blendState)
{
    pipeline_desc.BlendState = blendState;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::WithDepthState(const CD3DX12_DEPTH_STENCIL_DESC2& depthStencil)
{
    pipeline_desc.DepthStencilState = depthStencil;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::WithDepthFormat(const DXGI_FORMAT& format)
{
    pipeline_desc.DSVFormat = format;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::AttachShader(const DXShader& shader)
{
    D3D12_SHADER_BYTECODE shader_source = { shader.blob->GetBufferPointer(), shader.blob->GetBufferSize() };

    switch (shader.type)
    {
    case DXShader::Type::PIXEL:
        pipeline_desc.PS = shader_source;
    case DXShader::Type::VERTEX:
        pipeline_desc.VS = shader_source;
        break;
    case DXShader::Type::COMPUTE:
        pipeline_desc.CS = shader_source;
        break;
    default:
        Log("Pipeline Builder Warning: Unknown Shader Format {} (call ignored)", static_cast<uint32_t>(shader.type));
        break;
    }

    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::WithMSAA(uint32_t count, uint32_t quality)
{
    pipeline_desc.SampleDesc = { count, quality };
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::AddRenderTarget(DXGI_FORMAT format)
{
    auto* rtv_formats = &pipeline_desc.RTVFormats;
    auto current_count = rtv_formats->NumRenderTargets;

    if (current_count >= MAX_OUTPUTS)
    {
        Log("Pipeline Builder Warning: Reached limit of {} on outputs (call is ignored)", MAX_INPUTS);
        return *this;
    }

    rtv_formats->RTFormats[current_count] = format;
    rtv_formats->NumRenderTargets++;
    return *this;
}

DXPipelineBuilder& DXPipelineBuilder::WithPrimitiveTopology(const D3D12_PRIMITIVE_TOPOLOGY_TYPE& topology)
{
    pipeline_desc.PrimitiveTopologyType = topology;
    return *this;
}