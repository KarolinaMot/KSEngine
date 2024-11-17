#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <glm/vec3.hpp>
#include <optional>

class DXResource
{
public:
    class MappedAddress;

    DXResource() = default;

    DXResource(ComPtr<ID3D12Resource> resource, glm::uvec3 dimensions)
        : resource(resource)
        , dimensions(dimensions)
    {
    }

    ~DXResource() = default;

    DXResource(DXResource&&) = default;
    DXResource& operator=(DXResource&&) = default;

    NON_COPYABLE(DXResource);

    ID3D12Resource* Get() { return resource.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const { return resource->GetGPUVirtualAddress(); }
    glm::uvec3 GetDimensions() const { return dimensions; }

    MappedAddress Map(uint32_t subresource = 0, std::optional<D3D12_RANGE> read_range = std::nullopt);
    void Unmap(MappedAddress&& ptr, std::optional<D3D12_RANGE> write_range = std::nullopt);

private:
    ComPtr<ID3D12Resource> resource {};
    glm::uvec3 dimensions {};

    // Helper class for memory mapping
public:
    class MappedAddress
    {
    public:
        MappedAddress() = default;
        MappedAddress(std::byte* ptr, uint32_t subresource)
            : ptr(ptr)
            , subresource(subresource)
        {
        }

        ~MappedAddress();

        MappedAddress(MappedAddress&&);
        MappedAddress& operator=(MappedAddress&&);

        NON_COPYABLE(MappedAddress);

        std::byte* Get() { return ptr; }

    private:
        friend class DXResource;
        void Release() { ptr = 0; }

        std::byte* ptr {};
        uint32_t subresource {};
    };
};