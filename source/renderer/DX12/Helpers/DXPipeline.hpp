#pragma once
#include "DXIncludes.hpp"
#include <dxcapi.h>
#include <memory>
#include <vector>

struct ID3D12Device5;

class DXPipelineBuilder
{
public:
    DXPipelineBuilder() {};

    DXPipelineBuilder& AddInput(LPCSTR name, DXGI_FORMAT format, const uint32_t slot);
    DXPipelineBuilder& SetRasterizer(const CD3DX12_RASTERIZER_DESC& rasterizer);
    DXPipelineBuilder& SetBlendState(const CD3DX12_BLEND_DESC& blend);
    DXPipelineBuilder& SetDepthState(const CD3DX12_DEPTH_STENCIL_DESC& depth);
    DXPipelineBuilder& SetVertexAndPixelShaders(ID3DBlob* vBlob, ID3DBlob* pBlob);
    DXPipelineBuilder& SetComputeShader(ID3DBlob* cBlob);
    DXPipelineBuilder& SetMsaaCountAndQuality(uint32_t count, uint32_t quality);
    DXPipelineBuilder& AddRenderTarget(DXGI_FORMAT format);
    DXPipelineBuilder& SetPrimitiveTopology(const D3D12_PRIMITIVE_TOPOLOGY_TYPE& topology);
    DXPipelineBuilder& SetDepthFormat(const DXGI_FORMAT& format);

    ComPtr<ID3D12PipelineState> Build(ComPtr<ID3D12Device5> device, const ComPtr<ID3D12RootSignature>& root, LPCWSTR name) const;

    static ComPtr<ID3DBlob> ShaderToBlob(const char* path, const char* shaderVersion,
                                                            const char* functionName=nullptr);

private:
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputs;
    std::vector<DXGI_FORMAT> mRenderTargetFormats;

    ComPtr<ID3DBlob> mVSBlob;
    ComPtr<ID3DBlob> mPSBlob;
    ComPtr<ID3DBlob> mCSBlob;

    DXGI_FORMAT mDepthFormat = DXGI_FORMAT_D32_FLOAT;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE mTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    CD3DX12_RASTERIZER_DESC mRast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    CD3DX12_BLEND_DESC mBlend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    CD3DX12_DEPTH_STENCIL_DESC mDepth = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    uint32_t mMsaaCount = 1;
    uint32_t mMsaaQuality = 0;
};
