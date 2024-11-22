#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <shader/DXShader.hpp>

#include <deque>
#include <dxcapi.h>
#include <memory>
#include <optional>

struct DXPipeline
{
    ComPtr<ID3D12PipelineState> state {};
};

class DXPipelineBuilder
{
public:
    static inline constexpr size_t MAX_INPUTS = 16;
    static inline constexpr size_t MAX_OUTPUTS = 8;
    DXPipelineBuilder();

    // Clear methods
    DXPipelineBuilder& ClearInputDescription()
    {
        input_assembly_formats.clear();
        return *this;
    }
    DXPipelineBuilder& ClearOutputDescription()
    {
        pipeline_desc.RTVFormats = D3D12_RT_FORMAT_ARRAY { {}, 0 };
        return *this;
    }

    // Input

    DXPipelineBuilder& AddInput(uint32_t slot, const char* semantic_name, uint32_t semantic_index, DXGI_FORMAT format, bool instance_data);
    DXPipelineBuilder& WithRootSignature(ID3D12RootSignature* root_signature);

    // Rasterizer

    DXPipelineBuilder& WithRasterizer(const CD3DX12_RASTERIZER_DESC2& rasterizer);
    DXPipelineBuilder& WithBlendState(const CD3DX12_BLEND_DESC& blend);
    DXPipelineBuilder& WithPrimitiveTopology(const D3D12_PRIMITIVE_TOPOLOGY_TYPE& topology);

    // Depth stencil

    DXPipelineBuilder& WithDepthFormat(const DXGI_FORMAT& format);
    DXPipelineBuilder& WithDepthState(const CD3DX12_DEPTH_STENCIL_DESC2& depth);

    // Output

    DXPipelineBuilder& WithMSAA(uint32_t count, uint32_t quality);
    DXPipelineBuilder& AddRenderTarget(DXGI_FORMAT format);

    // Shaders
    DXPipelineBuilder& AttachShader(const DXShader& shader);

    static inline constexpr auto DEFAULT_GRAPHICS_PIPELINE_NAME = L"Graphics Pipeline State";
    static inline constexpr auto DEFAULT_COMPUTE_PIPELINE_NAME = L"Compute Pipeline State";

    std::optional<DXPipeline> BuildGraphicsPipeline(
        ID3D12Device* device,
        const wchar_t* name = DEFAULT_GRAPHICS_PIPELINE_NAME) const;

    std::optional<DXPipeline> BuildComputePipeline(
        ID3D12Device* device,
        const wchar_t* name = DEFAULT_COMPUTE_PIPELINE_NAME) const;

private:
    // TODO: Implement to improve error handling when building throws an exception
    static bool Validate(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

    // TODO: Implement to improve error handling when building throws an exception
    static bool Validate(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc);

    std::vector<D3D12_INPUT_ELEMENT_DESC> input_assembly_formats;

    ComPtr<IDxcBlob> vertex_shader {};
    ComPtr<IDxcBlob> pixel_shader {};
    ComPtr<IDxcBlob> compute_shader {};

    CD3DX12_PIPELINE_STATE_STREAM5 pipeline_desc {};
};
