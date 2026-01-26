#pragma once
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <memory>

class DXShaderTable;

class DXRTPipeline
{
public:
    Microsoft::WRL::ComPtr<ID3D12StateObject> m_pipeline;
    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> m_stateObjectProps;
    std::unique_ptr<DXShaderTable> m_shaderTable[FRAME_BUFFER_COUNT];
};

