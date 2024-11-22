#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <glm/vec2.hpp>
#include <gpu_resources/DXResource.hpp>
#include <optional>

// TODO: only supports committed resources
// PERFORMANCE WARNING: These allocations use a nodemask of zero for flexibility, but can incur performance costs
class DXResourceBuilder
{
public:
    DXResourceBuilder& WithHeapType(D3D12_HEAP_TYPE type);
    DXResourceBuilder& WithHeapFlags(D3D12_HEAP_FLAGS heap_flags);
    DXResourceBuilder& WithInitialState(D3D12_RESOURCE_STATES state);
    DXResourceBuilder& WithResourceFlags(D3D12_RESOURCE_FLAGS flags);

    // Reset current value by passing nullopt;
    DXResourceBuilder& WithOptimizedClearColour(std::optional<CD3DX12_CLEAR_VALUE> value);

    static inline constexpr auto DEFAULT_BUFFER_NAME = L"Buffer";
    std::optional<DXResource> MakeBuffer(ID3D12Device* device, size_t buffer_size, const wchar_t* name = DEFAULT_BUFFER_NAME) const;

    static inline constexpr auto DEFAULT_TEXTURE_NAME = L"2D Texture";
    std::optional<DXResource> MakeTexture2D(ID3D12Device* device, glm::uvec2 size, DXGI_FORMAT format, const wchar_t* name = DEFAULT_TEXTURE_NAME) const;

private:
    ComPtr<ID3D12Resource> AllocateResource(ID3D12Device* device, CD3DX12_RESOURCE_DESC& resource_desc) const;

    CD3DX12_HEAP_PROPERTIES heap_properties { D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT, 0, 0 };

    D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    std::optional<CD3DX12_CLEAR_VALUE> clear_value = {};
};
