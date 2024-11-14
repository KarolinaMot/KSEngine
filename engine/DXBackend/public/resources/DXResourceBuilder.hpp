#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <optional>
#include <resources/DXResource.hpp>

// TODO: only supports committed resources
// PERFORMANCE WARNING: These allocations use a nodemask of zero for flexibility, but can incur performance costs
class DXResourceBuilder
{
public:
    DXResourceBuilder& WithHeapType(D3D12_HEAP_TYPE type);
    DXResourceBuilder& WithHeapFlags(D3D12_HEAP_FLAGS heap_flags);
    DXResourceBuilder& WithInitialState(D3D12_RESOURCE_STATES state);

    // Reset current value by passing nullopt;
    DXResourceBuilder& WithOptimizedClearColour(std::optional<CD3DX12_CLEAR_VALUE> value);

    static inline constexpr auto DEFAULT_BUFFER_NAME = L"Buffer";
    std::optional<DXResource> MakeBuffer(ID3D12Device* device, size_t buffer_size, const wchar_t* name = DEFAULT_BUFFER_NAME);

private:
    CD3DX12_HEAP_PROPERTIES heap_properties { D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT, 0, 0 };

    D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
    std::optional<CD3DX12_CLEAR_VALUE> clear_value = {};
};
