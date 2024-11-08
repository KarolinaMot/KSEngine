#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>
#include <descriptors/DXDescriptorHandle.hpp>
#include <optional>
#include <queue>
#include <variant>

// TODO: Not optimized for huge amounts of descriptors
// TODO: Free list allocator is not memory efficient
// TODO: Does not handle Samplers
class DXDescriptorHeap
{
    friend class DXDescriptorHandle;

public:
    static inline constexpr auto DEFAULT_NAME = L"Descriptor Heap";

    DXDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t size, const wchar_t* name = DEFAULT_NAME);

    NON_COPYABLE(DXDescriptorHeap);
    NON_MOVABLE(DXDescriptorHeap);

    struct CBVParameters
    {
        const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc;
    };

    struct UAVParameters
    {
        const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc;
        ID3D12Resource* counter_resource {};
    };

    struct SRVParameters
    {
        const D3D12_SHADER_RESOURCE_VIEW_DESC& desc;
    };

    struct RTVParameters
    {
        const D3D12_RENDER_TARGET_VIEW_DESC& desc;
    };

    struct DSVParameters
    {
        const D3D12_DEPTH_STENCIL_VIEW_DESC& desc;
    };

    using AllocateParameters = std::variant<CBVParameters, UAVParameters, SRVParameters, RTVParameters, DSVParameters>;

    // In the case of Constant Buffer Views, pass a nullptr resource
    std::optional<DXDescriptorHandle> Allocate(ID3D12Device* device, ID3D12Resource* resource, const AllocateParameters& parameters);

private:
    void FreeHandle(CD3DX12_CPU_DESCRIPTOR_HANDLE handle);

    ComPtr<ID3D12DescriptorHeap> descriptor_heap {};
    D3D12_DESCRIPTOR_HEAP_TYPE descriptor_type {};
    std::queue<size_t> free_list {};
    size_t descriptor_stride {};
    size_t capacity {};
};
