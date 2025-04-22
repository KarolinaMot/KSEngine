#pragma once
#include <renderer/DX12/Helpers/DXIncludes.hpp>

namespace KS
{
class DXRTPipeline
{
public:
    Microsoft::WRL::ComPtr<ID3D12StateObject> m_pipeline;
    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> m_stateObjectProps;
};

}  // namespace KS
