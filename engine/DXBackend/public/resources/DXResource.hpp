#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <optional>

class DXResource
{
public:
    class MappedAddress;

    DXResource() = default;

    DXResource(ComPtr<ID3D12Resource> resource)
        : resource(resource)
    {
    }

    ~DXResource() = default;

    DXResource(DXResource&&) = default;
    DXResource& operator=(DXResource&&) = default;

    NON_COPYABLE(DXResource);

    ID3D12Resource* Get() { return resource.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const { return resource->GetGPUVirtualAddress(); }

    MappedAddress Map(size_t read_start, size_t read_end, uint32_t subresource = 0);
    MappedAddress Map(uint32_t subresource = 0);

    void Unmap(MappedAddress&& ptr);

private:
    ComPtr<ID3D12Resource> resource {};

    // Helper class for memory mapping
public:
    class MappedAddress
    {
    public:
        MappedAddress() = default;
        MappedAddress(void* ptr, uint32_t subresource, std::optional<D3D12_RANGE> read_range)
            : ptr(ptr)
            , subresource(subresource)
            , read_range(read_range)
        {
        }

        ~MappedAddress();

        MappedAddress(MappedAddress&&);
        MappedAddress& operator=(MappedAddress&&);

        NON_COPYABLE(MappedAddress);

        void* Get() { return ptr; }

    private:
        friend class DXResource;
        void Release() { ptr = 0; }

        void* ptr {};
        uint32_t subresource {};
        std::optional<D3D12_RANGE> read_range {};
    };
};