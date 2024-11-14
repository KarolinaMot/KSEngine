#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>
#include <optional>

class DXDescriptorHeapBase
{
public:
    DXDescriptorHeapBase() = default;
    DXDescriptorHeapBase(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t size, const wchar_t* name);

    NON_COPYABLE(DXDescriptorHeapBase)
    DEFAULT_MOVEABLE(DXDescriptorHeapBase)

    ID3D12DescriptorHeap* Get() { return descriptor_heap.Get(); }
    std::optional<CD3DX12_CPU_DESCRIPTOR_HANDLE> GetCPUAddress(size_t index);
    std::optional<CD3DX12_GPU_DESCRIPTOR_HANDLE> GetGPUAddress(size_t index);

protected:
    ComPtr<ID3D12DescriptorHeap> descriptor_heap {};
    size_t descriptor_stride {};
    size_t max_size {};
};

struct CBV
{
    static constexpr inline auto TYPE = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    static constexpr inline auto DEFAULT_NAME = L"CBV Descriptor Heap";

    const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc;
};

struct UAV
{
    static constexpr inline auto TYPE = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    static constexpr inline auto DEFAULT_NAME = L"UAV Descriptor Heap";

    const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc;
    ID3D12Resource* resource {};
    ID3D12Resource* counter_resource {};
};

struct SRV
{
    static constexpr inline auto TYPE = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    static constexpr inline auto DEFAULT_NAME = L"SRV Descriptor Heap";

    const D3D12_SHADER_RESOURCE_VIEW_DESC& desc;
    ID3D12Resource* resource {};
};

struct RTV
{
    static constexpr inline auto TYPE = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    static constexpr inline auto DEFAULT_NAME = L"RTV Descriptor Heap";

    const D3D12_RENDER_TARGET_VIEW_DESC& desc;
    ID3D12Resource* resource {};
};

struct DSV
{
    static constexpr inline auto TYPE = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    static constexpr inline auto DEFAULT_NAME = L"DSV Descriptor Heap";

    const D3D12_DEPTH_STENCIL_VIEW_DESC& desc;
    ID3D12Resource* resource {};
};

template <typename T>
concept IsDescriptorType = std::is_same_v<T, CBV> || std::is_same_v<T, SRV> || std::is_same_v<T, UAV> || std::is_same_v<T, RTV> || std::is_same_v<T, DSV>;

template <typename T>
    requires IsDescriptorType<T>
class DXDescriptorHeap : public DXDescriptorHeapBase
{
public:
    DXDescriptorHeap() = default;

    DXDescriptorHeap(ID3D12Device* device, size_t size)
        : DXDescriptorHeapBase(device, T::TYPE, size, T::DEFAULT_NAME)
    {
    }

    bool Allocate(ID3D12Device* device, const T& description, size_t index);
};
